# The OSCAR protocol core shared by the phone app (JasmineKICQ.pro) and the desktop harness
# (tools/kicq-cli). Pure QtCore + QtNetwork, no UI, no third-party code.

QT += core network

INCLUDEPATH += $$PWD/src/core

HEADERS += \
    $$PWD/src/core/icqbuffer.h \
    $$PWD/src/core/icqtext.h \
    $$PWD/src/core/icqtypes.h \
    $$PWD/src/core/icqpackets.h \
    $$PWD/src/core/icqsession.h

SOURCES += \
    $$PWD/src/core/icqtext.cpp \
    $$PWD/src/core/icqtypes.cpp \
    $$PWD/src/core/icqpackets.cpp \
    $$PWD/src/core/icqsession.cpp
