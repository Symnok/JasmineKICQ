// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "historystore.h"

#include <QDataStream>
#include <QDesktopServices>
#include <QDir>
#include <QFile>

namespace
{
    const quint32 Magic = 0x4B494351;   // "KICQ"
    const quint16 Version = 1;

    QDataStream &operator<<(QDataStream &s, const HistoryRecord &r)
    {
        s << quint8(r.out) << r.text << r.time << quint8(r.delivered) << quint8(r.failed) << quint8(r.offline);
        return s;
    }

    QDataStream &operator>>(QDataStream &s, HistoryRecord &r)
    {
        quint8 out, delivered, failed, offline;
        s >> out >> r.text >> r.time >> delivered >> failed >> offline;
        r.out = out; r.delivered = delivered; r.failed = failed; r.offline = offline;
        return s;
    }
}

QString HistoryStore::dataDir(const QString &account)
{
    QString base = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    if (base.isEmpty()) base = QDir::homePath() + QLatin1String("/.jasminekicq");
    return base + QLatin1String("/") + account;
}

HistoryStore::HistoryStore(const QString &account)
    : m_dir(dataDir(account) + QLatin1String("/history"))
{
    QDir().mkpath(m_dir);
}

QString HistoryStore::path(const QString &uin) const
{
    return m_dir + QLatin1String("/") + uin + QLatin1String(".dat");
}

QList<HistoryRecord> HistoryStore::load(const QString &uin) const
{
    QList<HistoryRecord> out;
    QFile f(path(uin));
    if (!f.open(QIODevice::ReadOnly)) return out;
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_4_7);
    quint32 magic = 0; quint16 ver = 0;
    s >> magic >> ver;
    if (magic != Magic || ver != Version) return out;
    while (!s.atEnd()) {
        HistoryRecord r;
        s >> r;
        if (s.status() != QDataStream::Ok) break;
        out.append(r);
    }
    return out;
}

void HistoryStore::append(const QString &uin, const HistoryRecord &r)
{
    QFile f(path(uin));
    bool fresh = !f.exists() || f.size() == 0;
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_4_7);
    if (fresh) s << Magic << Version;
    s << r;
}

void HistoryStore::save(const QString &uin, const QList<HistoryRecord> &records)
{
    QFile f(path(uin));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_4_7);
    s << Magic << Version;
    for (int i = 0; i < records.size(); ++i) s << records.at(i);
}

void HistoryStore::clear(const QString &uin)
{
    QFile::remove(path(uin));
}

QString HistoryStore::lastText(const QString &uin, QDateTime *when) const
{
    QList<HistoryRecord> all = load(uin);
    if (all.isEmpty()) return QString();
    if (when) *when = all.last().time;
    return all.last().text;
}
