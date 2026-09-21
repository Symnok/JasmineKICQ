# JasmineKICQ

An ICQ (OSCAR) client for Symbian Anna/Belle that talks to the kicq.ru server, ported from
[Jasmine IM](../JasmineIM) for Android. Qt 4.7.4 / Qt Quick 1.1, the same shell as
SimpleOKM-Symbian and SimpleVKM-Symbian.

What it does: XOR ("roasted") login, the server-side contact list with groups, add / remove /
rename contacts, authorization requests both ways, presence with the daisies (green online, yellow
away, green with a badge busy, red offline, white unknown) and the contacts' X-status icons, messaging on
channel 1 and the server-relay channel with delivery marks, typing notifications, offline
messages, local history, reconnection, and Symbian discreet popups + vibration for messages
that arrive while another app is in front. No TLS (the server has none), no file transfer.

## Layout

- `src/core/` - the protocol: `icqsession` (state machine, roster, presence, messaging),
  `icqpackets` (every packet, byte for byte as Jasmine sends it), `icqbuffer`, `icqtext`,
  `icqtypes`. Pure QtCore + QtNetwork; shared with the desktop harness.
- `src/app/` - `AppController` (network session, reconnection, settings, notifications),
  `ContactsModel`, `MessagesModel`, `HistoryStore`, `Notifier`.
- `qml/` - Login, Contacts, Chat, Settings, About pages.
- `tools/kicq-cli/` - console harness that logs in and executes commands from `kicq-cmd.txt`;
  the way the protocol was verified against kicq.ru.

## Building

Phone: `build-symbian.cmd` (bumps the patch version, builds ARMv5 release, packages
`JasmineKICQ_<ver>.sis` for Belle and `JasmineKICQ_installer_<ver>.sis` for Anna/S^3).

Desktop (Qt 4.7.4 MinGW from the Qt SDK, which ships the Symbian components):

    mkdir build-desktop && cd build-desktop
    qmake ../JasmineKICQ.pro -spec win32-g++ CONFIG+=release && mingw32-make

Environment for desktop testing: `KICQ_CREDS_FILE` (a file with `UIN password` per line) and
`KICQ_ACCOUNT` (line index) sign in automatically; `KICQ_LOG_FILE` writes qDebug and QML
errors to a file; `KICQ_SHOT_DIR` makes main.qml walk the pages and save screenshots.
After changing QML in the shadow build delete `release/qrc_qml.cpp` before `mingw32-make`.

Translations: `lupdate -extensions qml,cpp,h -no-obsolete src qml -ts translations/*.ts`
(the Simulator kit's lupdate), then `lrelease translations/*.ts`.

## kicq.ru quirks the code works around

- Message cookies are rewritten by the server; delivery acks are matched by UIN.
- The offline-message store keeps only bytes 0x01-0x7F and stops at the first 0x00: ASCII
  text is sent as charset 0, anything else as UCS-2 with spaces mapped to U+2002. Cyrillic
  sentences survive; digits and Latin letters mixed into Cyrillic text do not.
- Presence is rebroadcast only on a status change; capability (X-status) changes made while
  online are not seen by contacts until the next login, so there is no X-status picker.
- The keep-alive is a deliberately invalid ICBM the server answers with an error (Jasmine's trick).
