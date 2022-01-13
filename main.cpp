#include <QCoreApplication>
#include <QFileInfo>
#include <QMap>
#include <QDebug>


#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/io.h>
#include <semaphore.h>


#if !defined(DEBUG) && !defined(RELEASE)
#define DEBUG
#endif

#define F_INPUT 1
#pragma pack(1)
struct sysio_message_t {
    uint8_t umask; // D0 - 1 ввод, 0 - вывод; D1 - 1 успешно
    uint8_t value;
    uint16_t port;
};

struct mmap_data {
    sem_t event1;
    sem_t event2;
    sysio_message_t d;
};

union iodata {
    sysio_message_t msg;
    std::array<uint8_t, sizeof (sysio_message_t)> d;
};
#pragma pack()



int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

#ifndef DEBUG
    if(iopl(3) == -1)
    {
        perror("iopl");
        return 1;
    }
#endif


    const char * fname = "/tmp/sysioctl";

    remove(fname);

    if(mkfifo(fname, 0777))
    {
        unlink(fname);

        if(mkfifo(fname, 0777))
        {
            perror("File pipe for sysioctl is not created");
            return 1;
        }
    }

    int fd = open(fname, O_RDONLY);
    if(!fd)
    {
        remove(fname);
        return 1;
    }


    char buf[4096];
    do {
        memset(buf, '\0', sizeof buf);
        int r = read(fd, buf, sizeof buf);
        if(r)
        {
            QString memory_name(buf);

            new std::thread([memory_name]{
                int shm = 0;

                if((shm = shm_open(memory_name.toStdString().c_str(), O_CREAT | O_RDWR, 0777)) == -1) {
                    perror("shm_open");
                    return;
                }

                if(ftruncate(shm, sizeof (mmap_data)) == -1) {
                    perror("ftruncate");
                    return;
                }

                void * addr = mmap(0, sizeof(mmap_data), PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
                if(addr == MAP_FAILED)
                {
                    perror("mmap");
                    return;
                }

                mmap_data * mdata = (mmap_data*)addr;
                sysio_message_t * shm_data = &mdata->d;
                sem_t * event1 = &mdata->event1;
                sem_t * event2 = &mdata->event2;

                if(sem_init(event1, 1, 0) != 0)
                {
                    perror("sem_init");
                    return;
                }

                if(sem_init(event2, 1, 0) != 0)
                {
                    perror("sem_init");
                    return;
                }

                qDebug() << "memory name:" << memory_name;

                int cnt = 0;
                do {
                    if(sem_wait(event1) == 0)
                    {
                        if(shm_data->port > 0)
                        {
                            if(shm_data->umask & F_INPUT)
                            {
#ifdef DEBUG
                                qDebug() << "i:" << shm_data->port;
                                shm_data->value = ++cnt;
#else
                                shm_data->value = inb(shm_data->port);
#endif
                            }
                            else
                            {  
#ifdef DEBUG
                                qDebug() << "o:" << shm_data->value << shm_data->port;
#else
                                outb(shm_data->value, shm_data->port);
#endif
                            }
                        }

                        sem_post(event2);
                    }
                } while(1);

                munmap(addr, sizeof(mmap_data));
                close(shm);
            });

            // not join, threads will be killed by OS, couse threads include infinity's cicle's
        }
    }
    while (1);

    close(fd);
    remove(fname);

    return a.exec();
}
