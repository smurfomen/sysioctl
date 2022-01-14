LIBS += -lrt

#DEFINES += SYS_IO     # using instead sys/io.h
#DEFINES += SYS_IO_CTL # using service sysioctl using shared memory


SOURCES += \
        $$PWD/sysioctl.cpp

HEADERS += \
    $$PWD/sysioctl.h
