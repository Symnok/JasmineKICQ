// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// The contact list for QML: the roster grouped as on the server, online contacts first
// within a group, with the daisy for the status, unread counters and the last message.
#ifndef CONTACTSMODEL_H
#define CONTACTSMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

class IcqSession;
class HistoryStore;

class ContactsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY countChanged)
    Q_PROPERTY(int unreadTotal READ unreadTotal NOTIFY unreadChanged)
    Q_PROPERTY(bool showOffline READ showOffline WRITE setShowOffline NOTIFY showOfflineChanged)
public:
    enum Roles {
        UinRole = Qt::UserRole + 1,
        NickRole,
        GroupIdRole,
        GroupNameRole,
        StatusRole,
        StatusIconRole,
        StatusTextRole,
        XStatusIconRole,
        AwayTextRole,
        TypingRole,
        AuthorizedRole,
        OnlineRole,
        TemporaryRole,
        UnreadRole,
        LastTextRole,
        SubtitleRole
    };

    explicit ContactsModel(IcqSession *session, QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    int onlineCount() const;
    int unreadTotal() const;
    bool showOffline() const { return m_showOffline; }
    void setShowOffline(bool on);

    void setHistory(HistoryStore *h) { m_history = h; }

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(const QString &uin) const;
    Q_INVOKABLE QVariantMap contactInfo(const QString &uin) const;
    /// [{id, name}] of the groups a contact can be added to.
    Q_INVOKABLE QVariantList groups() const;

    int unread(const QString &uin) const { return m_unread.value(uin, 0); }
    void setUnread(const QString &uin, int n);
    void setLastText(const QString &uin, const QString &text);
    /// Daisy image for a wire status: "daisy_green.png" etc.; unknown = no presence yet,
    /// not on the server list or awaiting authorization (white).
    static QString statusIcon(int status, bool unknown = false);
    static QString statusText(int status);

public slots:
    void rebuild();

signals:
    void countChanged();
    void unreadChanged();
    void showOfflineChanged();

private slots:
    void onContactChanged(const QString &uin);

private:
    struct Row { QString uin; int groupId; };
    QStringList visibleUins() const;

    IcqSession *m_session;
    HistoryStore *m_history;
    QList<Row> m_rows;
    QHash<QString, int> m_unread;
    QHash<QString, QString> m_lastText;
    QHash<QString, bool> m_lastOnline;
    bool m_showOffline;
};

#endif // CONTACTSMODEL_H
