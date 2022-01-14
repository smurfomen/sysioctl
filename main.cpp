#include <QCoreApplication>

#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/io.h>
#include <semaphore.h>
#include <iostream>

namespace  {
    std::map<QString, std::thread*> memo;
}

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
#pragma pack()

void log(const QString & str) {
    std::cout << str.toStdString() << std::endl;
}

void log(const QString & mname, const QString & s) {
    log(QString("[%1] %2").arg(mname).arg(s));
}

void log(const QString & mname, const QString & s, const QString & errstr) {
    log(mname, QString("%1 (%2)").arg(s).arg(errstr));
}



void init_shm(const QString & memory_name) {
    memo.insert({ memory_name, new std::thread([memory_name]{
        int shm = 0;

        if((shm = shm_open(memory_name.toStdString().c_str(), O_CREAT | O_RDWR | O_TRUNC, 0777)) == -1) {
            log(memory_name, "shm_open", strerror(errno));
            return;
        }

        if(ftruncate(shm, sizeof (mmap_data)) == -1) {
            log(memory_name, "ftruncate", strerror(errno));
            return;
        }

        fchmod(shm, S_IRWXU | S_IRWXG | S_IRWXO);

        void * addr = mmap(0, sizeof(mmap_data), PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
        if(addr == MAP_FAILED)
        {
            log(memory_name, "mmap", strerror(errno));
            return;
        }

        mmap_data * mdata = (mmap_data*)addr;
        sysio_message_t * shm_data = &mdata->d;
        sem_t * event1 = &mdata->event1;
        sem_t * event2 = &mdata->event2;

        if(sem_init(event1, 1, 0) != 0 || sem_init(event2, 1, 0) != 0)
        {
            log(memory_name, "sem_init", strerror(errno));
            return;
        }

        log(memory_name, "initialized shared memory");

#ifdef DEBUG
        int cnt = 0;
#endif
        while(1)
        {
            if(sem_wait(event1) == 0)
            {
                if(shm_data->port > 0)
                {
                    if(shm_data->umask & F_INPUT)
                    {
                        log(memory_name, "input", QString("port(%1)").arg(shm_data->port));
#ifdef DEBUG
                        shm_data->value = cnt++;
#else
                        shm_data->value = inb(shm_data->port);
#endif
                    }
                    else
                    {
                        log(memory_name, "output", QString("val(%1) : port(%2)").arg(shm_data->value).arg(shm_data->port));
#ifdef RELEASE
                        outb(shm_data->value, shm_data->port);
#endif
                    }
                }

                sem_post(event2);
            }
        }

        munmap(addr, sizeof(mmap_data));
        close(shm);

        log(memory_name, "shared memory destroyed");
    })});

    log(memory_name, "new handler created");
}

#include <QProcess>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

#ifndef DEBUG
    if(iopl(3) == -1)
    {
        log(QString("iopl: %1 (%2)").arg("privelege is not taken").arg(strerror(errno)));
        return 1;
    }
#endif

    for(int i = 1; i < argc; i++)
    {
        init_shm(argv[i]);
    }

    const char * fname = "/tmp/sysioctl";

    remove(fname);

    if(mkfifo(fname, 0777))
    {
        unlink(fname);

        if(mkfifo(fname, 0777))
        {
            log(QString("FATAL: failure to create FIFO /tmp/sysioctl file (%1)").arg(strerror(errno)));
            return 1;
        }
    }

    chmod("/tmp/sysioctl", S_IRWXU | S_IRWXG | S_IRWXO);

    int fd = open(fname, O_RDONLY);
    if(!fd)
    {
        remove(fname);
        return 1;
    }

    char buf[4096];
    do {
        memset(buf, '\0', sizeof buf);
        if(read(fd, buf, sizeof buf))
        {
            if(memo.count(buf)) {
                log(buf, "skip shared memory allocation", "shared memory already exists");
                continue;
            }

            init_shm(buf);
        }
    }
    while (1);

    close(fd);
    remove(fname);

    return a.exec();
}
