#ifndef SYSIOCTL_H
#define SYSIOCTL_H


#ifdef SYS_IO
bool sysioctl_init();

#ifdef SYS_IO_CTL
unsigned char inb (unsigned short int __port);

void outb (unsigned char __value, unsigned short int __port);
#else
#include <sys/io.h>
#endif
#endif

#endif // SYSIOCTL_H
