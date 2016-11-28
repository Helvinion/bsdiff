TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    bsdiff.c \
    bspatch.c

HEADERS += \
    bsdiff.h \
    bspatch.h \
    config.h
