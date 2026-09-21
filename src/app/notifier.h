// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// Tells the user about a message while another application is in front: on Symbian the
// "new message" envelope in the status bar (until the app is opened again), vibration, and
// optionally a discreet popup at the top of the screen. The app itself keeps running in
// the background with its socket open, so this is all a "service" needs to be here.
// Elsewhere it just logs.
#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>
#include <QString>

class Notifier : public QObject
{
    Q_OBJECT
public:
    explicit Notifier(QObject *parent = 0);
    ~Notifier();

    void setVibrate(bool on) { m_vibrate = on; }
    void setPopups(bool on) { m_popups = on; }

    /// Shows the popup (title = who, text = what) and vibrates, according to the settings.
    void notify(const QString &title, const QString &text);
    /// Just the vibration, e.g. for an authorization request.
    void vibrate(int ms = 400);
    /// "N new messages" query with a "Show" softkey that raises the app (a global query,
    /// answered in this process), plus the status-bar envelope while count > 0.
    void setPendingCount(int count);
    int pendingCount() const { return m_pending; }

private:
    bool m_vibrate;
    bool m_popups;
    int m_pending;
    static QString pendingText(int count);

    void *m_query;   // the global query and its active object (Symbian only)
};

#endif // NOTIFIER_H
