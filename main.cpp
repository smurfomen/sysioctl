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

#define F_INPUT 1
#define F_HANDLED 4
#define F_SUCCESS 2
#pragma pack(1)
struct sysio_message_t {
    uint8_t umask; // D0 - 1 ввод, 0 - вывод; D1 - 1 успешно
    uint8_t value;
    uint16_t port;
};
#pragma pack()

union iodata {
    sysio_message_t msg;
    std::array<uint8_t, sizeof (sysio_message_t)> d;
};


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    const char * fname = "/tmp/sysioctl";

    remove(fname);

    if(mkfifo(fname, 0777))
    {
        unlink(fname);

        if(mkfifo(fname, 0777))
        {
            perror("File pipe for sysioctl is not created");
            return 0;
        }
    }

    int fd = open(fname, O_RDONLY);
    if(!fd)
    {
        remove(fname);
        return 1;
    }

    iopl(3);

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

                if(ftruncate(shm, sizeof (sysio_message_t)) == -1) {
                    perror("ftruncate");
                    return;
                }

                void * addr = mmap(0, sizeof(sysio_message_t), PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
                if(addr == MAP_FAILED)
                {
                    perror("mmap");
                    return;
                }

                sysio_message_t * msg = (sysio_message_t*)addr;
                do {
                    if(!(msg->umask & F_HANDLED))
                    {
                        if(msg->port > 0)
                        {
                            msg->umask &= ~F_SUCCESS;
                            if(msg->umask & F_INPUT)
                            {
                                qDebug() << "input:" << msg->port;
                                msg->value = inb(msg->port);
                                msg->umask |= F_SUCCESS;
                            }
                            else
                            {
                                qDebug() << "output:" << msg->value << msg->port;
                                outb(msg->value, msg->port);
                                msg->umask |= F_SUCCESS;
                            }
                        }
                        msg->umask |= F_HANDLED;
                    }
                } while(1);

                munmap(addr, sizeof(sysio_message_t));
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
