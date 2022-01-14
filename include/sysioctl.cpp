#include "sysioctl.h"

#ifdef SYS_IO
#ifdef SYS_IO_CTL
#include <QCoreApplication>

#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <sys/mman.h>
#include <semaphore.h>
#include <mutex>

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

namespace  {
    sysio_message_t * shm_data = nullptr;
    sem_t * event1 = nullptr;
    sem_t * event2 = nullptr;
    int shm = 0;
    std::mutex mtx;
}

bool initialized() {
    return shm_data && event1;
}

unsigned char inb (unsigned short int __port)
{
    if(initialized())
    {
        std::lock_guard<std::mutex> lck(mtx);
        shm_data->port = __port;
        shm_data->value = 0;
        shm_data->umask = F_INPUT;

        if(sem_post(event1) == 0)
        {
            sem_wait(event2);
            return shm_data->value;
        }
    }

    return 0;
}

void outb (unsigned char __value, unsigned short int __port)
{
    if(initialized())
    {
        std::lock_guard<std::mutex> lck(mtx);
        shm_data->port = __port;
        shm_data->value = __value;
        shm_data->umask = 0;

        if(sem_post(event1) == 0)
        {
            sem_wait(event2);
        }
    }
}

bool sysioctl_init()
{
    std::lock_guard<std::mutex> lck(mtx);
    if(!initialized())
    {
        int fdioctl = open("/tmp/sysioctl", O_WRONLY);
        if(fdioctl)
        {
            std::string appname = QString(QCoreApplication::applicationName()+"_shmt").toStdString();
            write(fdioctl, appname.c_str(), appname.size());

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            if((shm = shm_open(appname.c_str(), O_RDWR, 0777)) == -1)
            {
                perror("shm_open");
            }

            else if(ftruncate(shm, sizeof (mmap_data)) == -1) {
                perror("ftruncate");
            }

            else
            {
                void * addr = mmap(0, sizeof(mmap_data), PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
                if(addr == MAP_FAILED)
                {
                    perror("mmap");
                }
                else
                {
                    mmap_data * mdata = (mmap_data*)addr;
                    shm_data = &mdata->d;
                    event1 = &mdata->event1;
                    event2 = &mdata->event2;
                }
            }
        }
    }

    return initialized();
}

#else
#include <sys/io.h>
bool sysioctl_init() {
    return iopl(3) != -1;
}

#endif

#endif


