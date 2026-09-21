// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "contactsmodel.h"
#include "historystore.h"
#include "icqsession.h"

#include <QCoreApplication>
#include <QHash>

namespace
{
    struct SortKey
    {
        int groupOrder;
        bool offline;
        QString nick;
        QString uin;
        bool operator<(const SortKey &o) const
        {
            if (groupOrder != o.groupOrder) return groupOrder < o.groupOrder;
            if (offline != o.offline) return !offline;
            int c = nick.compare(o.nick, Qt::CaseInsensitive);
            if (c != 0) return c < 0;
            return uin < o.uin;
        }
    };
}

ContactsModel::ContactsModel(IcqSession *session, QObject *parent)
    : QAbstractListModel(parent), m_session(session), m_history(0), m_showOffline(true)
{
    QHash<int, QByteArray> roles;
    roles[UinRole] = "uin";
    roles[NickRole] = "nick";
    roles[GroupIdRole] = "groupId";
    roles[GroupNameRole] = "groupName";
    roles[StatusRole] = "status";
    roles[StatusIconRole] = "statusIcon";
    roles[StatusTextRole] = "statusText";
    roles[XStatusIconRole] = "xstatusIcon";
    roles[AwayTextRole] = "awayText";
    roles[TypingRole] = "typing";
    roles[AuthorizedRole] = "authorized";
    roles[OnlineRole] = "online";
    roles[TemporaryRole] = "temporary";
    roles[UnreadRole] = "unread";
    roles[LastTextRole] = "lastText";
    roles[SubtitleRole] = "subtitle";
    setRoleNames(roles);

    connect(session, SIGNAL(rosterLoaded()), this, SLOT(rebuild()));
    connect(session, SIGNAL(groupsChanged()), this, SLOT(rebuild()));
    connect(session, SIGNAL(contactAdded(QString)), this, SLOT(rebuild()));
    connect(session, SIGNAL(contactRemoved(QString)), this, SLOT(rebuild()));
    connect(session, SIGNAL(contactChanged(QString)), this, SLOT(onContactChanged(QString)));
    connect(session, SIGNAL(stateChanged()), this, SLOT(rebuild()));
}

int ContactsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QString ContactsModel::statusIcon(int status, bool unknown)
{
    if (unknown) return QLatin1String("daisy_white.png");
    switch (Icq::statusColor(status)) {
    case Icq::Green: return QLatin1String("daisy_green.png");
    case Icq::Yellow: return QLatin1String("daisy_yellow.png");
    case Icq::Dnd: return QLatin1String("daisy_dnd.png");
    case Icq::Occupied: return QLatin1String("daisy_occupied.png");
    case Icq::Red: return QLatin1String("daisy_red.png");
    default: return QLatin1String("daisy_white.png");
    }
}

/// White daisy: the server will never report this contact's status - not on the server-side
/// list, or still awaiting authorization. (Offline contacts get no presence packet at all,
/// so "nothing received" simply means offline: red.)
static bool statusUnknown(const IcqContact &c, bool)
{
    return c.temporary || !c.authorized;
}

QString ContactsModel::statusText(int status)
{
    if (status == Icq::StatusOffline) return QCoreApplication::translate("Status", "Offline");
    switch (status & 0xFFFF) {
    case Icq::StatusOnline: return QCoreApplication::translate("Status", "Online");
    case Icq::StatusAway: return QCoreApplication::translate("Status", "Away");
    case Icq::StatusNa: case 0x0005: return QCoreApplication::translate("Status", "Not available");
    case Icq::StatusOccupied: case 0x0011: return QCoreApplication::translate("Status", "Occupied");
    case Icq::StatusDnd: case 0x0013: return QCoreApplication::translate("Status", "Do not disturb");
    case Icq::StatusFfc: return QCoreApplication::translate("Status", "Free for chat");
    case Icq::StatusInvisible: return QCoreApplication::translate("Status", "Invisible");
    case Icq::StatusLunch: return QCoreApplication::translate("Status", "Lunch");
    case Icq::StatusEvil: return QCoreApplication::translate("Status", "Evil");
    case Icq::StatusDepress: return QCoreApplication::translate("Status", "Depressed");
    case Icq::StatusHome: return QCoreApplication::translate("Status", "At home");
    case Icq::StatusWork: return QCoreApplication::translate("Status", "At work");
    default: return QCoreApplication::translate("Status", "Online");
    }
}

QVariant ContactsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();
    const Row &row = m_rows.at(index.row());
    const IcqContact c = m_session->contact(row.uin);
    switch (role) {
    case UinRole: return c.uin;
    case NickRole: return c.nick.isEmpty() ? c.uin : c.nick;
    case GroupIdRole: return row.groupId;
    case GroupNameRole: {
        IcqGroup g = m_session->group(row.groupId);
        if (g.notInList || row.groupId <= 0) return tr("Not in list");
        return g.name;
    }
    case StatusRole: return c.status;
    case StatusIconRole: return QLatin1String("qrc:/images/") + statusIcon(c.status, statusUnknown(c, m_session->isOnline()));
    case StatusTextRole: return statusText(c.status);
    case XStatusIconRole:
        return (c.xstatus >= 0 && c.xstatus < Icq::XStatusCount)
            ? QString::fromLatin1("qrc:/images/x_%1.png").arg(QLatin1String(Icq::XStatusNames[c.xstatus])) : QString();
    case AwayTextRole: return c.awayText;
    case TypingRole: return c.typing;
    case AuthorizedRole: return c.authorized;
    case OnlineRole: return c.online();
    case TemporaryRole: return c.temporary;
    case UnreadRole: return m_unread.value(c.uin, 0);
    case LastTextRole: return m_lastText.value(c.uin);
    case SubtitleRole: {
        if (c.typing) return tr("typing...");
        if (!c.authorized) return tr("awaiting authorization");
        if (!c.awayText.isEmpty()) return c.awayText;
        QString last = m_lastText.value(c.uin);
        if (!last.isEmpty()) return last.simplified();
        return QString();   // the plain status is the daisy's job
    }
    default: return QVariant();
    }
}

int ContactsModel::onlineCount() const
{
    int n = 0;
    QList<IcqContact> all = m_session->contacts();
    for (int i = 0; i < all.size(); ++i) if (all.at(i).online()) ++n;
    return n;
}

int ContactsModel::unreadTotal() const
{
    int n = 0;
    for (QHash<QString, int>::const_iterator it = m_unread.begin(); it != m_unread.end(); ++it) n += it.value();
    return n;
}

void ContactsModel::setShowOffline(bool on)
{
    if (m_showOffline == on) return;
    m_showOffline = on;
    emit showOfflineChanged();
    rebuild();
}

void ContactsModel::setUnread(const QString &uin, int n)
{
    if (n <= 0) m_unread.remove(uin); else m_unread.insert(uin, n);
    int row = indexOf(uin);
    if (row >= 0) emit dataChanged(index(row), index(row));
    emit unreadChanged();
    if (row < 0 && n > 0) rebuild();   // an offline contact with news must become visible
}

void ContactsModel::setLastText(const QString &uin, const QString &text)
{
    m_lastText.insert(uin, text);
    int row = indexOf(uin);
    if (row >= 0) emit dataChanged(index(row), index(row));
}

int ContactsModel::indexOf(const QString &uin) const
{
    for (int i = 0; i < m_rows.size(); ++i) if (m_rows.at(i).uin == uin) return i;
    return -1;
}

QVariantMap ContactsModel::get(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_rows.size()) return m;
    QModelIndex idx = index(row);
    QHash<int, QByteArray> names = roleNames();
    for (QHash<int, QByteArray>::const_iterator it = names.begin(); it != names.end(); ++it)
        m.insert(QString::fromLatin1(it.value()), data(idx, it.key()));
    return m;
}

QVariantMap ContactsModel::contactInfo(const QString &uin) const
{
    QVariantMap m;
    if (!m_session->hasContact(uin)) {
        m.insert(QLatin1String("uin"), uin);
        m.insert(QLatin1String("nick"), uin);
        m.insert(QLatin1String("status"), int(Icq::StatusOffline));
        m.insert(QLatin1String("statusIcon"), QLatin1String("qrc:/images/") + statusIcon(Icq::StatusOffline, true));
        m.insert(QLatin1String("statusText"), statusText(Icq::StatusOffline));
        m.insert(QLatin1String("online"), false);
        m.insert(QLatin1String("typing"), false);
        m.insert(QLatin1String("awayText"), QString());
        m.insert(QLatin1String("authorized"), true);
        m.insert(QLatin1String("temporary"), true);
        return m;
    }
    const IcqContact c = m_session->contact(uin);
    m.insert(QLatin1String("uin"), c.uin);
    m.insert(QLatin1String("nick"), c.nick.isEmpty() ? c.uin : c.nick);
    m.insert(QLatin1String("status"), c.status);
    m.insert(QLatin1String("statusIcon"), QLatin1String("qrc:/images/") + statusIcon(c.status, statusUnknown(c, m_session->isOnline())));
    m.insert(QLatin1String("statusText"), statusText(c.status));
    m.insert(QLatin1String("online"), c.online());
    m.insert(QLatin1String("typing"), c.typing);
    m.insert(QLatin1String("awayText"), c.awayText);
    m.insert(QLatin1String("authorized"), c.authorized);
    m.insert(QLatin1String("temporary"), c.temporary);
    return m;
}

QVariantList ContactsModel::groups() const
{
    QVariantList out;
    QList<IcqGroup> gs = m_session->groups();
    for (int i = 0; i < gs.size(); ++i) {
        if (gs.at(i).notInList || gs.at(i).id <= 0) continue;
        QVariantMap m;
        m.insert(QLatin1String("id"), gs.at(i).id);
        m.insert(QLatin1String("name"), gs.at(i).name);
        out.append(m);
    }
    return out;
}

void ContactsModel::rebuild()
{
    QList<IcqGroup> gs = m_session->groups();
    QHash<int, int> groupOrder;
    for (int i = 0; i < gs.size(); ++i) groupOrder.insert(gs.at(i).id, gs.at(i).notInList ? 100000 : i);

    QList<IcqContact> all = m_session->contacts();
    QMap<SortKey, Row> sorted;
    for (int i = 0; i < all.size(); ++i) {
        const IcqContact &c = all.at(i);
        bool interesting = c.online() || m_unread.value(c.uin, 0) > 0 || c.typing;
        if (!m_showOffline && !interesting) continue;
        SortKey k;
        k.groupOrder = groupOrder.value(c.groupId, 99999);
        k.offline = !c.online();
        k.nick = c.nick.isEmpty() ? c.uin : c.nick;
        k.uin = c.uin;
        Row r;
        r.uin = c.uin;
        r.groupId = c.groupId;
        sorted.insert(k, r);
    }
    beginResetModel();
    m_rows = sorted.values();
    endResetModel();
    for (int i = 0; i < all.size(); ++i) m_lastOnline.insert(all.at(i).uin, all.at(i).online());
    emit countChanged();
}

void ContactsModel::onContactChanged(const QString &uin)
{
    const IcqContact c = m_session->contact(uin);
    bool prevOnline = m_lastOnline.value(uin, false);
    m_lastOnline.insert(uin, c.online());
    int row = indexOf(uin);
    // online/offline flips change the order and the visibility; anything else updates in place
    if (row < 0 || prevOnline != c.online()) { rebuild(); return; }
    emit dataChanged(index(row), index(row));
}
