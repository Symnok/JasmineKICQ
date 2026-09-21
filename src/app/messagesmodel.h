// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// The open chat for QML: its history oldest-first, sending with delivery marks, typing
// notifications in both directions, and the peer's live presence for the header.
#ifndef MESSAGESMODEL_H
#define MESSAGESMODEL_H

#include "historystore.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantMap>

class IcqSession;
class ContactsModel;
class QTimer;

class MessagesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString uin READ uin NOTIFY chatChanged)
    Q_PROPERTY(QString title READ title NOTIFY peerChanged)
    Q_PROPERTY(QString peerStatusIcon READ peerStatusIcon NOTIFY peerChanged)
    Q_PROPERTY(QString peerStatusText READ peerStatusText NOTIFY peerChanged)
    Q_PROPERTY(QString peerSubtitle READ peerSubtitle NOTIFY peerChanged)
    Q_PROPERTY(bool peerTyping READ peerTyping NOTIFY peerChanged)
    Q_PROPERTY(bool peerOnline READ peerOnline NOTIFY peerChanged)
    Q_PROPERTY(bool peerTemporary READ peerTemporary NOTIFY peerChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        BodyRole = Qt::UserRole + 1,
        OutRole,
        TimeTextRole,
        DateTextRole,
        ShowDateRole,
        PendingRole,
        FailedRole,
        OfflineRole
    };

    MessagesModel(IcqSession *session, ContactsModel *contacts, QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    QString uin() const { return m_uin; }
    QString title() const;
    QString peerStatusIcon() const;
    QString peerStatusText() const;
    QString peerSubtitle() const;
    bool peerTyping() const;
    bool peerOnline() const;
    bool peerTemporary() const;

    void setHistory(HistoryStore *h) { m_history = h; }

    Q_INVOKABLE void open(const QString &uin);
    Q_INVOKABLE void close();
    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void retry(int row);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE QVariantMap get(int row) const;
    /// Called by the composer as the text changes: drives the typing notification.
    Q_INVOKABLE void composing(const QString &text);

    /// A message for this or another chat arrived (the controller routes it here).
    void appendIncoming(const QString &uin, const HistoryRecord &r);

signals:
    void chatChanged();
    void peerChanged();
    void countChanged();
    void messageAppended();
    void sendFailed(const QString &error);

private slots:
    void onDelivered(const QString &uin, const QByteArray &cookie);
    void onFailed(const QString &uin, const QByteArray &cookie, int code);
    void onContactChanged(const QString &uin);
    void onTypingIdle();

private:
    void sendRecord(int row);
    static QString timeText(const QDateTime &t);
    static QString dateText(const QDateTime &t);

    IcqSession *m_session;
    ContactsModel *m_contacts;
    HistoryStore *m_history;
    QString m_uin;
    QList<HistoryRecord> m_rows;
    QTimer *m_typingTimer;
    bool m_typingSent;
};

#endif // MESSAGESMODEL_H
