// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// Message history on the phone: one small binary file per contact under the app's data
// folder, appended as messages come and go, read whole when a chat opens (ICQ histories
// on a phone are tiny). Unread counters live in QSettings next to it.
#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

struct HistoryRecord
{
    HistoryRecord() : out(false), delivered(true), failed(false), offline(false) {}
    bool out;
    QString text;
    QDateTime time;
    bool delivered;
    bool failed;
    bool offline;
    QByteArray cookie;   // outgoing: the send cookie, for matching acks in memory only
};

class HistoryStore
{
public:
    explicit HistoryStore(const QString &account);

    QList<HistoryRecord> load(const QString &uin) const;
    void append(const QString &uin, const HistoryRecord &r);
    /// Rewrites the file (after delivered/failed flags changed).
    void save(const QString &uin, const QList<HistoryRecord> &records);
    void clear(const QString &uin);
    /// The last message text of every contact that has a history, for the contact list.
    QString lastText(const QString &uin, QDateTime *when = 0) const;

    static QString dataDir(const QString &account);

private:
    QString path(const QString &uin) const;
    QString m_dir;
};

#endif // HISTORYSTORE_H
