# JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
# Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#
# Build with the "Qt 4.7.4 for Symbian Anna/Belle" (SymbianSR1Qt474) kit for the phone
# (build-symbian.cmd), or with the Desktop Qt 4.7.4 MinGW kit to run it on the PC. The
# protocol core (core.pri) is shared with the desktop harness in tools/kicq-cli.

TEMPLATE = app
TARGET = JasmineKICQ
VERSION = 0.1.19

QT += core gui network declarative

include(core.pri)

INCLUDEPATH += src/app

HEADERS += \
    src/app/appcontroller.h \
    src/app/contactsmodel.h \
    src/app/messagesmodel.h \
    src/app/historystore.h \
    src/app/notifier.h

SOURCES += \
    src/main.cpp \
    src/app/appcontroller.cpp \
    src/app/contactsmodel.cpp \
    src/app/messagesmodel.cpp \
    src/app/historystore.cpp \
    src/app/notifier.cpp

RESOURCES += qml.qrc translations.qrc

# The version reaches the About page and the ICQ client capabilities as an unquoted macro
# (stringified in code), which survives every generator's quoting rules.
DEFINES += APP_VERSION=$$VERSION

TRANSLATIONS += \
    translations/jasminekicq_ru.ts \
    translations/jasminekicq_uk.ts

OTHER_FILES += qml/*.qml README.md

symbian {
    # Unprotected range: installs self-signed without Symbian Signed. Also used by the
    # notifier so that tapping a popup brings this app forward.
    TARGET.UID3 = 0xE2C9A7D1
    DEFINES += KICQ_UID3=0xE2C9A7D1
    TARGET.CAPABILITY += NetworkServices ReadUserData WriteUserData
    TARGET.EPOCHEAPSIZE = 0x020000 0x2000000
    TARGET.EPOCSTACKSIZE = 0x14000
    ICON = icon.svg

    # Notifier: discreet popups (avkon), the "new messages" global query + status-bar
    # envelope (aknnotify), vibration (hwrm), bringing the app forward (apgrfx, ws32).
    LIBS += -lavkon -laknnotify -lhwrmvibraclient -lcone -leikcore -lapgrfx -lws32
    INCLUDEPATH += $$[QT_INSTALL_PREFIX]/epoc32/include/platform/mw

    # Qt Quick Components for Symbian (built into Belle; Anna gets them through the Smart
    # Installer package, JasmineKICQ_installer.sis).
    CONFIG += qt-components
    DEPLOYMENT.installer_header = 0x2002CCCF

    vendorinfo = \
        "%{\"JasmineKICQ\"}" \
        ":\"JasmineKICQ\""
    my_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += my_deployment
}
