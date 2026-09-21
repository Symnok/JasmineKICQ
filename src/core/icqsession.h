// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// One ICQ session: the TCP link to the login server and then the BOS server, the OSCAR
// negotiation, the server-side roster, presence, and messaging. Pure Qt (QTcpSocket),
// single-threaded, no UI - the app models and the desktop harness both sit on top of it.
//
// The flow, as Jasmine IM does it against kicq.ru:
//   connect login server -> server hello (FLAP ch1) -> XOR login (ch1)
//   -> ch4 with BOS address + cookie -> reconnect to BOS -> hello -> cookie (ch1)
//   -> 01/03 families -> 01/17 versions -> 01/18 -> 01/06 rates -> 01/07 -> 01/08 ack,
//      01/0E, 13/02, 02/02, 03/02, 04/04, 09/02, 13/04 roster request
//   -> 13/06 roster -> 02/04 caps, 04/02 icbm, 01/02 client ready, 01/1E status,
//      13/07 activate, 15/02 offline messages
//   -> online: 03/0B,0C presence; 04/07 messages; 04/0B acks; 04/14 typing; 13/19,1B auth ...
#ifndef ICQSESSION_H
#define ICQSESSION_H

#include "icqtypes.h"
#include "icqpackets.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class QTcpSocket;
class QTimer;

class IcqSession : public QObject
{
    Q_OBJECT
public:
    enum State { Disconnected, ConnectingLogin, Authenticating, ConnectingBos, Negotiating, Online };

    explicit IcqSession(QObject *parent = 0);
    ~IcqSession();

    State state() const { return m_state; }
    bool isOnline() const { return m_state == Online; }
    QString uin() const { return m_uin; }
    /// The status we asked for (wire value), and the Xtraz index (-1 none).
    int ownStatus() const { return m_status; }
    int ownXStatus() const { return m_xstatus; }
    QString ownAwayText() const { return m_awayText; }

    /// Client version announced in the capabilities (shows up in other clients' detectors).
    void setClientVersion(int major, int minor, int patch);

    // -- connection --------------------------------------------------------------------------
    void connectToServer(const QString &server, int port, const QString &uin, const QString &password);
    void disconnectFromServer();

    // -- roster (read side) -------------------------------------------------------------------
    QList<IcqGroup> groups() const { return m_groups; }
    /// Contacts in roster order (groups first, then by nick), including temporary ones.
    QList<IcqContact> contacts() const;
    bool hasContact(const QString &uin) const { return m_contacts.contains(uin); }
    IcqContact contact(const QString &uin) const { return m_contacts.value(uin); }
    IcqGroup group(int id) const;
    QString groupName(int id) const { return group(id).name; }

    // -- roster (write side, server-side SSI) -------------------------------------------------
    /// Adds a contact to a group; the result arrives as ssiFinished(ReqAddContact, uin, ok).
    /// A contact that requires authorization is added flagged and an auth request is sent.
    void addContact(const QString &uin, const QString &nick, int groupId);
    void removeContact(const QString &uin);
    void renameContact(const QString &uin, const QString &nick);
    void addGroup(const QString &name);
    void renameGroup(int groupId, const QString &name);
    void requestAuthorization(const QString &uin, const QString &reason);
    /// Asks the server for the user's details; answered by userInfoReceived (also for
    /// strangers and for our own UIN).
    void requestUserInfo(const QString &uin);
    void replyAuthorization(const QString &uin, bool granted);

    // -- presence -------------------------------------------------------------------------------
    void setStatus(int status);
    void setXStatus(int index);
    void setAwayText(const QString &text);

    // -- messaging -------------------------------------------------------------------------------
    /// How to put text on the wire; Auto picks per contact (see sendMessage).
    enum Encoding { Auto, Channel1Ucs2, Channel1Cp1251, Channel1Utf8, Channel2Utf8, Channel2Cp1251 };
    /// Sends text; returns the 8-byte cookie messageDelivered/messageFailed will carry.
    QByteArray sendMessage(const QString &uin, const QString &text, Encoding enc = Auto);
    /// Channel 1 with explicit bytes and charset id (protocol experiments; the harness uses it).
    QByteArray sendMessageRaw(const QString &uin, const QByteArray &text, int charset);
    void sendTyping(const QString &uin, bool typing);

signals:
    void stateChanged();
    void connected();
    /// The link is gone. loginError is a server code when the login itself was refused
    /// (1/4/5 bad UIN or password, 24 rate limited...), 0 otherwise.
    void disconnected(const QString &reason, int loginError);
    void rosterLoaded();
    void groupsChanged();
    void contactAdded(const QString &uin);
    void contactRemoved(const QString &uin);
    /// Presence, nick, typing or authorization state of a contact changed.
    void contactChanged(const QString &uin);
    void messageReceived(const QString &uin, const QString &text, const QDateTime &when, bool offline);
    /// The server (or the peer's client) acknowledged a message we sent to uin. kicq.ru
    /// rewrites message cookies on the way back, so acks are matched to the oldest message
    /// still pending for that uin; the cookie is ours.
    void messageDelivered(const QString &uin, const QByteArray &cookie);
    void messageFailed(const QString &uin, const QByteArray &cookie, int errorCode);
    void authRequested(const QString &uin, const QString &reason);
    void authReplied(const QString &uin, bool granted);
    void youWereAdded(const QString &uin);
    void ssiFinished(int request, const QString &name, bool ok, int code);
    void userInfoReceived(const IcqUserInfo &info);
    /// Debug/diagnostic line (the harness prints these; the app ignores them).
    void log(const QString &line);

private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError();
    void onKeepAlive();
    void onConnectTimeout();
    void onUserInfoTimeout();

private:
    void setState(State s);
    void openSocket(const QString &host, int port);
    void send(int channel, const QByteArray &payload);
    void send(const IcqSnac &snac);
    void fail(const QString &reason, int loginError = 0);

    void handleFlap(int channel, const QByteArray &data);
    void handleLoginHello();
    void handleBosHello();
    void handleChannel4(const QByteArray &data);
    void handleSnac(const QByteArray &data);
    void handleRates(const QByteArray &data);
    void handleRoster(const QByteArray &data, int flags);
    void finishLogin();
    void handleUserOnline(const QByteArray &data);
    void handleUserOffline(const QByteArray &data);
    void handleIncomingMessage(const QByteArray &data);
    void handleOfflineMessage(const QByteArray &data);
    void handleUserInfo(const QByteArray &data, quint32 reqId);
    void handleMessageAck(const QByteArray &data);
    void handleMessageError(const QByteArray &data, quint32 reqId);
    void handleTyping(const QByteArray &data);
    void handleAuthRequest(const QByteArray &data);
    void handleAuthReply(const QByteArray &data);
    void handleYouWereAdded(const QByteArray &data);
    void handleSsiAdd(const QByteArray &data);
    void handleSsiUpdate(const QByteArray &data);
    void handleSsiDelete(const QByteArray &data);
    void handleSsiResult(const QByteArray &data, quint32 reqId);
    void deliverMessage(const QString &uin, const QString &text, const QDateTime &when, bool offline);

    IcqContact &ensureContact(const QString &uin);
    int newSsiId() const;
    void sendUserInfo();
    QString qipGuid() const;

    QTcpSocket *m_socket;
    QTimer *m_keepAlive;
    QTimer *m_connectTimer;
    State m_state;
    QString m_uin, m_password, m_loginServer;
    int m_loginPort;
    QByteArray m_inbound;
    int m_seq;
    QByteArray m_cookie;
    QString m_bosHost;
    int m_bosPort;
    bool m_probeAnswered;
    int m_status;
    int m_xstatus;
    QString m_awayText;
    int m_vMajor, m_vMinor, m_vPatch;

    QList<IcqGroup> m_groups;
    QHash<QString, IcqContact> m_contacts;
    QStringList m_order;               // contacts in arrival order (stable listing)
    QStringList m_offlineSeen;         // texts already delivered from the offline store
    struct Pending { QString uin; QString nick; int groupId; int ssiId; };
    Pending m_pendingAdd;
    QString m_pendingDelete;
    Pending m_pendingRename;
    QString m_pendingGroup;
    int m_pendingGroupId;
    QString m_pendingGroupRename;
    int m_pendingGroupRenameId;
    QMap<quint32, IcqUserInfo> m_infoRequests;   // request id -> details collected so far
    quint32 m_nextInfoReq;
    QTimer *m_infoTimer;
    struct PendingMsg { QString uin; QByteArray cookie; };
    QMap<quint32, PendingMsg> m_pendingMsgs;   // request id -> message awaiting ack/error
    quint32 m_nextMsgReq;
};

#endif // ICQSESSION_H
