TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -Wall -Wextra -Werror -Wno-deprecated -std=gnu99
QMAKE_LFLAGS += -lreadline
INCLUDEPATH += $$PWD/3rdparty/libcfu


SOURCES += main.c \
    lexer.c \
    parser.c \
    utils.c \
    reader.c \
    il.c \
    alias.c \
    3rdparty/libcfu/cfu.c \
    3rdparty/libcfu/cfuconf.c \
    3rdparty/libcfu/cfuhash.c \
    3rdparty/libcfu/cfulist.c \
    3rdparty/libcfu/cfuopt.c \
    3rdparty/libcfu/cfustring.c \
    vm.c \
    vm_stack.c \
    vm_entry.c \
    exec.c \
    builtin.c \
    states.c

HEADERS += \
    lexer.h \
    parser.h \
    utils.h \
    reader.h \
    il.h \
    alias.h \
    3rdparty/libcfu/cfu.h \
    3rdparty/libcfu/cfuconf.h \
    3rdparty/libcfu/cfuhash.h \
    3rdparty/libcfu/cfulist.h \
    3rdparty/libcfu/cfuopt.h \
    3rdparty/libcfu/cfustring.h \
    vm.h \
    vm_entry.h \
    vm_stack.h \
    parser_t.inc.h \
    il_t.inc.h \
    exec.h \
    builtin.h \
    states.h
