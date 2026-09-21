// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "messagesmodel.h"
#include "contactsmodel.h"
#include "icqsession.h"

#include <QDateTime>
#include <QHash>
#include <QTimer>

namespace
{
    const int TypingIdleMs = 6000;

    QString errorText(int code)
    {
        switch (code) {
        case 4: return MessagesModel::tr("The contact is offline and the server could not store the message.");
        case 9: return MessagesModel::tr("The contact's client does not support this message.");
        case 14: return MessagesModel::tr("Authorization required: the contact has to authorize you first.");
        case 16: return MessagesModel::tr("You are blocked by this contact.");
        default: return MessagesModel::tr("The message was not delivered (error %1).").arg(code);
        }
    }
}

MessagesModel::MessagesModel(IcqSession *session, ContactsModel *contacts, QObject *parent)
    : QAbstractListModel(parent), m_session(session), m_contacts(contacts), m_history(0), m_typingSent(false)
{
    QHash<int, QByteArray> roles;
    roles[BodyRole] = "body";
    roles[OutRole] = "out";
    roles[TimeTextRole] = "timeText";
    roles[DateTextRole] = "dateText";
    roles[ShowDateRole] = "showDate";
    roles[PendingRole] = "pending";
    roles[FailedRole] = "failed";
    roles[OfflineRole] = "offline";
    setRoleNames(roles);

    connect(session, SIGNAL(messageDelivered(QString,QByteArray)), this, SLOT(onDelivered(QString,QByteArray)));
    connect(session, SIGNAL(messageFailed(QString,QByteArray,int)), this, SLOT(onFailed(QString,QByteArray,int)));
    connect(session, SIGNAL(contactChanged(QString)), this, SLOT(onContactChanged(QString)));
    connect(session, SIGNAL(stateChanged()), this, SIGNAL(peerChanged()));

    m_typingTimer = new QTimer(this);
    m_typingTimer->setSingleShot(true);
    m_typingTimer->setInterval(TypingIdleMs);
    connect(m_typingTimer, SIGNAL(timeout()), this, SLOT(onTypingIdle()));
}

int MessagesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QString MessagesModel::timeText(const QDateTime &t)
{
    return t.toString(QLatin1String("HH:mm"));
}

QString MessagesModel::dateText(const QDateTime &t)
{
    QDate d = t.date();
    QDate today = QDate::currentDate();
    if (d == today) return tr("Today");
    if (d == today.addDays(-1)) return tr("Yesterday");
    return d.toString(QLatin1String("d MMMM yyyy"));
}

QVariant MessagesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();
    const HistoryRecord &r = m_rows.at(index.row());
    switch (role) {
    case BodyRole: return r.text;
    case OutRole: return r.out;
    case TimeTextRole: return timeText(r.time);
    case DateTextRole: return dateText(r.time);
    case ShowDateRole: return index.row() == 0 || m_rows.at(index.row() - 1).time.date() != r.time.date();
    case PendingRole: return r.out && !r.delivered && !r.failed;
    case FailedRole: return r.failed;
    case OfflineRole: return r.offline;
    default: return QVariant();
    }
}

QVariantMap MessagesModel::get(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_rows.size()) return m;
    QModelIndex idx = index(row);
    QHash<int, QByteArray> names = roleNames();
    for (QHash<int, QByteArray>::const_iterator it = names.begin(); it != names.end(); ++it)
        m.insert(QString::fromLatin1(it.value()), data(idx, it.key()));
    return m;
}

// -- peer ---------------------------------------------------------------------------------------

QString MessagesModel::title() const
{
    if (m_uin.isEmpty()) return QString();
    return m_contacts->contactInfo(m_uin).value(QLatin1String("nick")).toString();
}

QString MessagesModel::peerStatusIcon() const { return m_contacts->contactInfo(m_uin).value(QLatin1String("statusIcon")).toString(); }
QString MessagesModel::peerStatusText() const { return m_contacts->contactInfo(m_uin).value(QLatin1String("statusText")).toString(); }
bool MessagesModel::peerTyping() const { return m_contacts->contactInfo(m_uin).value(QLatin1String("typing")).toBool(); }
bool MessagesModel::peerOnline() const { return m_contacts->contactInfo(m_uin).value(QLatin1String("online")).toBool(); }
bool MessagesModel::peerTemporary() const { return m_contacts->contactInfo(m_uin).value(QLatin1String("temporary")).toBool(); }

QString MessagesModel::peerSubtitle() const
{
    QVariantMap c = m_contacts->contactInfo(m_uin);
    if (c.value(QLatin1String("typing")).toBool()) return tr("typing...");
    // the daisy shows the status itself; only an away text adds information
    return c.value(QLatin1String("awayText")).toString();
}

void MessagesModel::onContactChanged(const QString &uin)
{
    if (uin == m_uin) emit peerChanged();
}

// -- open/close ------------------------------------------------------------------------------------

void MessagesModel::open(const QString &uin)
{
    if (m_uin == uin) return;
    close();
    beginResetModel();
    m_uin = uin;
    m_rows = m_history ? m_history->load(uin) : QList<HistoryRecord>();
    endResetModel();
    m_contacts->setUnread(uin, 0);
    emit chatChanged();
    emit peerChanged();
    emit countChanged();
}

void MessagesModel::close()
{
    if (m_uin.isEmpty()) return;
    if (m_typingSent) { m_session->sendTyping(m_uin, false); m_typingSent = false; }
    m_typingTimer->stop();
    beginResetModel();
    m_uin.clear();
    m_rows.clear();
    endResetModel();
    emit chatChanged();
    emit peerChanged();
    emit countChanged();
}

void MessagesModel::clearHistory()
{
    if (m_uin.isEmpty()) return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    if (m_history) m_history->clear(m_uin);
    m_contacts->setLastText(m_uin, QString());
    emit countChanged();
}

// -- sending -------------------------------------------------------------------------------------------

void MessagesModel::send(const QString &textIn)
{
    QString text = textIn.trimmed();
    if (m_uin.isEmpty() || text.isEmpty()) return;
    if (m_typingSent) { m_session->sendTyping(m_uin, false); m_typingSent = false; }
    m_typingTimer->stop();
    HistoryRecord r;
    r.out = true;
    r.text = text;
    r.time = QDateTime::currentDateTime();
    r.delivered = false;
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
    m_rows.append(r);
    endInsertRows();
    sendRecord(m_rows.size() - 1);
    if (m_history) m_history->append(m_uin, m_rows.last());
    m_contacts->setLastText(m_uin, text);
    emit countChanged();
    emit messageAppended();
}

void MessagesModel::sendRecord(int row)
{
    HistoryRecord &r = m_rows[row];
    r.failed = false;
    r.delivered = false;
    if (!m_session->isOnline()) {
        r.failed = true;
        emit sendFailed(tr("Not connected."));
    } else {
        r.cookie = m_session->sendMessage(m_uin, r.text);
    }
    emit dataChanged(index(row), index(row));
}

void MessagesModel::retry(int row)
{
    if (row < 0 || row >= m_rows.size() || !m_rows.at(row).out) return;
    sendRecord(row);
    if (m_history) m_history->save(m_uin, m_rows);
}

void MessagesModel::onDelivered(const QString &uin, const QByteArray &cookie)
{
    if (uin != m_uin) return;
    for (int i = m_rows.size() - 1; i >= 0; --i) {
        HistoryRecord &r = m_rows[i];
        if (r.out && r.cookie == cookie) {
            r.delivered = true;
            emit dataChanged(index(i), index(i));
            if (m_history) m_history->save(m_uin, m_rows);
            return;
        }
    }
}

void MessagesModel::onFailed(const QString &uin, const QByteArray &cookie, int code)
{
    if (uin == m_uin) {
        for (int i = m_rows.size() - 1; i >= 0; --i) {
            HistoryRecord &r = m_rows[i];
            if (r.out && r.cookie == cookie) {
                r.failed = true;
                emit dataChanged(index(i), index(i));
                if (m_history) m_history->save(m_uin, m_rows);
                break;
            }
        }
    }
    emit sendFailed(errorText(code));
}

// -- incoming ---------------------------------------------------------------------------------------------

void MessagesModel::appendIncoming(const QString &uin, const HistoryRecord &r)
{
    if (m_history) m_history->append(uin, r);
    m_contacts->setLastText(uin, r.text);
    if (uin != m_uin) return;
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
    m_rows.append(r);
    endInsertRows();
    emit countChanged();
    emit messageAppended();
}

// -- typing ---------------------------------------------------------------------------------------------

void MessagesModel::composing(const QString &text)
{
    if (m_uin.isEmpty() || !m_session->isOnline()) return;
    if (text.isEmpty()) {
        if (m_typingSent) { m_session->sendTyping(m_uin, false); m_typingSent = false; }
        m_typingTimer->stop();
        return;
    }
    if (!m_typingSent) { m_session->sendTyping(m_uin, true); m_typingSent = true; }
    m_typingTimer->start();
}

void MessagesModel::onTypingIdle()
{
    if (m_typingSent && !m_uin.isEmpty()) { m_session->sendTyping(m_uin, false); m_typingSent = false; }
}
