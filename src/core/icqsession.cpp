// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "icqsession.h"
#include "icqbuffer.h"
#include "icqtext.h"

#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QStringList>
#include <cstdlib>

namespace
{
    const int ConnectTimeoutMs = 20000;
    const int KeepAliveMs = 60000;

    QString hexOf(const QByteArray &b) { return QString::fromLatin1(b.toHex()).toUpper(); }

    QString loginErrorText(int code)
    {
        switch (code) {
        case 0x01: return QLatin1String("invalid UIN");
        case 0x04: case 0x05: return QLatin1String("wrong password");
        case 0x07: case 0x08: return QLatin1String("this UIN does not exist");
        case 0x18: case 0x1D: return QLatin1String("rate limit: too many login attempts, wait a while");
        case 0x1C: return QLatin1String("client too old");
        default: return QString::fromLatin1("login refused (code %1)").arg(code);
        }
    }

    // reads text that may be UTF-8 or CP1251
    QString textOrCp1251(const QByteArray &b)
    {
        return IcqText::isUtf8(b) ? QString::fromUtf8(b.constData(), b.size()) : IcqText::fromCp1251(b);
    }
}

IcqSession::IcqSession(QObject *parent)
    : QObject(parent), m_socket(0), m_state(Disconnected), m_loginPort(5190), m_seq(1), m_bosPort(5190),
      m_probeAnswered(true), m_status(Icq::StatusOnline), m_xstatus(-1), m_vMajor(1), m_vMinor(0), m_vPatch(0),
      m_pendingGroupId(0), m_nextMsgReq(0x1000)
{
    m_keepAlive = new QTimer(this);
    m_keepAlive->setInterval(KeepAliveMs);
    connect(m_keepAlive, SIGNAL(timeout()), this, SLOT(onKeepAlive()));
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(ConnectTimeoutMs);
    connect(m_connectTimer, SIGNAL(timeout()), this, SLOT(onConnectTimeout()));
    m_pendingAdd.groupId = m_pendingAdd.ssiId = 0;
    m_pendingRename.groupId = m_pendingRename.ssiId = 0;
}

IcqSession::~IcqSession()
{
    disconnectFromServer();
}

void IcqSession::setClientVersion(int major, int minor, int patch)
{
    m_vMajor = major; m_vMinor = minor; m_vPatch = patch;
}

void IcqSession::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

// -- connection ----------------------------------------------------------------------------

void IcqSession::connectToServer(const QString &server, int port, const QString &uin, const QString &password)
{
    if (m_state != Disconnected) disconnectFromServer();
    m_uin = uin.trimmed();
    m_password = password;
    m_loginServer = server.trimmed();
    m_loginPort = port > 0 ? port : 5190;
    m_cookie.clear();
    m_inbound.clear();
    m_groups.clear();
    m_contacts.clear();
    m_order.clear();
    m_offlineSeen.clear();
    m_pendingMsgs.clear();
    m_probeAnswered = true;
    setState(ConnectingLogin);
    emit log(QString::fromLatin1("connecting to %1:%2 as %3").arg(m_loginServer).arg(m_loginPort).arg(m_uin));
    openSocket(m_loginServer, m_loginPort);
}

void IcqSession::openSocket(const QString &host, int port)
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_socket = new QTcpSocket(this);
    connect(m_socket, SIGNAL(connected()), this, SLOT(onSocketConnected()));
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onSocketReadyRead()));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(onSocketDisconnected()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError()));
    m_inbound.clear();
    m_connectTimer->start();
    m_socket->connectToHost(host, port);
}

void IcqSession::disconnectFromServer()
{
    m_keepAlive->stop();
    m_connectTimer->stop();
    if (m_socket) {
        m_socket->disconnect(this);
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->write(IcqPackets::flap(4, m_seq++, QByteArray()));
            m_socket->flush();
        }
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = 0;
    }
    for (QHash<QString, IcqContact>::iterator it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        it->status = Icq::StatusOffline;
        it->typing = false;
    }
    if (m_state != Disconnected) {
        setState(Disconnected);
        emit disconnected(QString(), 0);
    }
}

void IcqSession::fail(const QString &reason, int loginError)
{
    emit log(QLatin1String("connection failed: ") + reason);
    m_keepAlive->stop();
    m_connectTimer->stop();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = 0;
    }
    for (QHash<QString, IcqContact>::iterator it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        it->status = Icq::StatusOffline;
        it->typing = false;
    }
    setState(Disconnected);
    emit disconnected(reason, loginError);
}

void IcqSession::onSocketConnected()
{
    m_connectTimer->stop();
    emit log(QLatin1String("tcp connected, waiting for server hello"));
}

void IcqSession::onSocketDisconnected()
{
    if (m_state == ConnectingBos && !m_cookie.isEmpty()) return;   // the login server closing on us, expected
    if (m_state != Disconnected) fail(QLatin1String("connection closed by server"));
}

void IcqSession::onSocketError()
{
    if (m_state == Disconnected) return;
    QString err = m_socket ? m_socket->errorString() : QLatin1String("socket error");
    if (m_state == ConnectingBos && m_socket && m_socket->error() == QAbstractSocket::RemoteHostClosedError) return;
    fail(err);
}

void IcqSession::onConnectTimeout()
{
    if (m_state == ConnectingLogin || m_state == ConnectingBos) fail(QLatin1String("connection timed out"));
}

void IcqSession::onKeepAlive()
{
    if (m_state != Online) return;
    if (!m_probeAnswered) {
        fail(QLatin1String("server stopped answering"));
        return;
    }
    m_probeAnswered = false;
    send(IcqPackets::keepAliveProbe());
}

void IcqSession::send(int channel, const QByteArray &payload)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    m_socket->write(IcqPackets::flap(channel, m_seq, payload));
    m_seq = (m_seq + 1) & 0xFFFF;
}

void IcqSession::send(const IcqSnac &snac)
{
    send(2, IcqPackets::snacBytes(snac));
}

// -- inbound framing --------------------------------------------------------------------------

void IcqSession::onSocketReadyRead()
{
    m_inbound.append(m_socket->readAll());
    while (m_inbound.size() >= 6) {
        if ((unsigned char)m_inbound.at(0) != 0x2A) {
            fail(QLatin1String("protocol error: not a FLAP frame"));
            return;
        }
        int channel = (unsigned char)m_inbound.at(1);
        int len = ((unsigned char)m_inbound.at(4) << 8) | (unsigned char)m_inbound.at(5);
        if (m_inbound.size() < 6 + len) return;
        QByteArray data = m_inbound.mid(6, len);
        m_inbound.remove(0, 6 + len);
        handleFlap(channel, data);
        if (!m_socket) return;   // handler tore the connection down
    }
}

void IcqSession::handleFlap(int channel, const QByteArray &data)
{
    switch (channel) {
    case 1:
        if (m_state == ConnectingLogin) handleLoginHello();
        else if (m_state == ConnectingBos) handleBosHello();
        break;
    case 2:
        handleSnac(data);
        break;
    case 4:
        handleChannel4(data);
        break;
    case 5:
        break;   // keep-alive from the server
    default:
        break;
    }
}

void IcqSession::handleLoginHello()
{
    setState(Authenticating);
    emit log(QLatin1String("login server hello, sending XOR login"));
    send(1, IcqPackets::xorLogin(m_uin, m_password));
}

void IcqSession::handleBosHello()
{
    setState(Negotiating);
    emit log(QLatin1String("BOS hello, sending cookie"));
    send(1, IcqPackets::bosCookie(m_cookie));
}

void IcqSession::handleChannel4(const QByteArray &data)
{
    IcqTlvList tlvs(data);
    const IcqTlv *server = tlvs.find(5);
    const IcqTlv *cookie = tlvs.find(6);
    if (server && cookie) {
        QString bos = QString::fromLatin1(server->data);
        m_cookie = cookie->data;
        m_bosHost = bos.section(QLatin1Char(':'), 0, 0);
        m_bosPort = bos.section(QLatin1Char(':'), 1, 1).toInt();
        if (m_bosPort <= 0) m_bosPort = m_loginPort;
        emit log(QString::fromLatin1("login accepted, BOS at %1:%2, cookie %3 bytes").arg(m_bosHost).arg(m_bosPort).arg(m_cookie.size()));
        setState(ConnectingBos);
        send(4, QByteArray());
        openSocket(m_bosHost, m_bosPort);
        return;
    }
    const IcqTlv *error = tlvs.find(8);
    if (error) {
        int code = error->u16();
        fail(loginErrorText(code), code);
        return;
    }
    const IcqTlv *t9 = tlvs.find(9);
    if (t9 && t9->u16() == 1) {
        fail(QLatin1String("this UIN signed on from another place"));
        return;
    }
    fail(QLatin1String("server closed the session"));
}

// -- SNAC dispatch ----------------------------------------------------------------------------

void IcqSession::handleSnac(const QByteArray &data)
{
    if (data.size() < 10) return;
    IcqReader r(data);
    int family = r.u16();
    int subtype = r.u16();
    int flags = r.u16();
    quint32 reqId = r.u32();
    QByteArray body = r.rest();
    m_probeAnswered = true;
    // an optional block of extra data precedes the payload when bit 15 of the flags is set
    if (flags & 0x8000) {
        IcqReader e(body);
        int len = e.u16();
        e.skip(len);
        body = e.rest();
    }

    switch (family) {
    case 0x01:
        switch (subtype) {
        case 0x03: case 0x15: send(IcqPackets::clientFamilies()); break;
        case 0x18: send(IcqPackets::ratesRequest()); break;
        case 0x07: handleRates(body); break;
        case 0x0F: case 0x13: case 0x21: break;   // self info, MOTD, ext status
        default: break;
        }
        break;
    case 0x03:
        if (subtype == 0x0B) handleUserOnline(body);
        else if (subtype == 0x0C) handleUserOffline(body);
        break;
    case 0x04:
        switch (subtype) {
        case 0x01: handleMessageError(body, reqId); break;
        case 0x07: handleIncomingMessage(body); break;
        case 0x0B: case 0x0C: handleMessageAck(body); break;
        case 0x14: handleTyping(body); break;
        default: break;
        }
        break;
    case 0x13:
        switch (subtype) {
        case 0x06: handleRoster(body, flags); break;
        case 0x08: handleSsiAdd(body); break;
        case 0x09: handleSsiUpdate(body); break;
        case 0x0A: handleSsiDelete(body); break;
        case 0x0E: handleSsiResult(body, reqId); break;
        case 0x19: handleAuthRequest(body); break;
        case 0x1B: handleAuthReply(body); break;
        case 0x1C: handleYouWereAdded(body); break;
        default: break;
        }
        break;
    case 0x15:
        if (subtype == 0x03 && reqId == 64017) handleOfflineMessage(body);
        break;
    default:
        break;
    }
}

void IcqSession::handleRates(const QByteArray &data)
{
    int groups = IcqReader(data).u16();
    emit log(QString::fromLatin1("rates: %1 classes; requesting roster").arg(groups));
    send(IcqPackets::ackRates(groups));
    send(IcqPackets::reqSelfInfo());
    send(IcqPackets::reqSsiRights());
    send(IcqPackets::reqLocationRights());
    send(IcqPackets::reqBuddyRights());
    send(IcqPackets::reqIcbmParams());
    send(IcqPackets::reqPrivacyRights());
    m_groups.clear();
    m_contacts.clear();
    m_order.clear();
    IcqGroup notInList;
    notInList.id = -1;
    notInList.name = QString();
    notInList.notInList = true;
    m_groups.append(notInList);
    send(IcqPackets::requestRoster());
}

// -- roster -------------------------------------------------------------------------------------

void IcqSession::handleRoster(const QByteArray &data, int flags)
{
    IcqReader r(data);
    r.skip(1);                       // SSI version
    int count = r.u16();
    for (int i = 0; i < count && !r.atEnd(); ++i) {
        int nameLen = r.u16();
        QByteArray rawName = r.bytes(nameLen);
        QString name = textOrCp1251(rawName);
        int groupId = r.u16();
        int itemId = r.u16();
        int type = r.u16();
        int extraLen = r.u16();
        QByteArray extra = r.bytes(extraLen);
        IcqTlvList tlvs(extra);
        if (type == 0 && groupId != 0) {
            IcqContact &c = ensureContact(name);
            c.groupId = groupId;
            c.ssiId = itemId;
            c.temporary = false;
            c.authorized = !tlvs.has(0x66);
            const IcqTlv *nick = tlvs.find(0x131);
            if (nick && !nick->data.isEmpty()) c.nick = textOrCp1251(nick->data);
            if (c.nick.isEmpty()) c.nick = name;
        } else if (type == 1 && groupId != 0) {
            IcqGroup g;
            g.id = groupId;
            g.name = name;
            g.notInList = tlvs.has(0x6A);
            bool replaced = false;
            for (int k = 0; k < m_groups.size(); ++k)
                if (m_groups[k].id == g.id) { m_groups[k] = g; replaced = true; }
            if (!replaced) m_groups.append(g);
        }
        // types 2/3/14 (visible/invisible/ignore lists), 4 (privacy), 20 (icon) are not used
    }
    // contacts whose group is the "not in list" group are temporary
    for (QHash<QString, IcqContact>::iterator it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        IcqGroup g = group(it->groupId);
        if (g.notInList) it->temporary = true;
    }
    if ((flags & 0x0001) == 0) {
        emit log(QString::fromLatin1("roster: %1 groups, %2 contacts").arg(m_groups.size()).arg(m_contacts.size()));
        emit groupsChanged();
        emit rosterLoaded();
        finishLogin();
    }
}

void IcqSession::finishLogin()
{
    sendUserInfo();
    send(IcqPackets::setIcbmParams());
    send(IcqPackets::clientReady());
    int wireStatus = qipGuid().isEmpty() ? m_status : Icq::StatusOnline;
    send(IcqPackets::setDcInfo(wireStatus, 256, 11));
    if (!m_awayText.isEmpty()) send(IcqPackets::setAwayText(m_awayText));
    send(IcqPackets::rosterActivate());
    send(IcqPackets::offlineMsgsRequest(m_uin, m_seq));
    setState(Online);
    m_probeAnswered = true;
    m_keepAlive->start();
    emit log(QLatin1String("online"));
    emit connected();
}

IcqGroup IcqSession::group(int id) const
{
    for (int i = 0; i < m_groups.size(); ++i)
        if (m_groups.at(i).id == id) return m_groups.at(i);
    IcqGroup none;
    none.id = id;
    return none;
}

QList<IcqContact> IcqSession::contacts() const
{
    QList<IcqContact> out;
    for (int i = 0; i < m_order.size(); ++i)
        if (m_contacts.contains(m_order.at(i))) out.append(m_contacts.value(m_order.at(i)));
    return out;
}

IcqContact &IcqSession::ensureContact(const QString &uin)
{
    if (!m_contacts.contains(uin)) {
        IcqContact c;
        c.uin = uin;
        c.nick = uin;
        c.groupId = -1;
        c.temporary = true;
        m_contacts.insert(uin, c);
        m_order.append(uin);
    }
    return m_contacts[uin];
}

int IcqSession::newSsiId() const
{
    // Jasmine: random 16-bit id above 4096, avoiding what the roster already uses
    for (int tries = 0; tries < 100; ++tries) {
        int id = (qrand() & 0x6FFF) + 4096;
        bool used = false;
        for (QHash<QString, IcqContact>::const_iterator it = m_contacts.begin(); it != m_contacts.end(); ++it)
            if (it->ssiId == id) used = true;
        for (int i = 0; i < m_groups.size(); ++i) if (m_groups.at(i).id == id) used = true;
        if (!used) return id;
    }
    return (qrand() & 0x6FFF) + 4096;
}

// -- roster edits --------------------------------------------------------------------------------

void IcqSession::addContact(const QString &uinIn, const QString &nick, int groupId)
{
    QString uin = uinIn.trimmed();
    if (m_state != Online || uin.isEmpty()) return;
    if (m_contacts.contains(uin) && !m_contacts.value(uin).temporary) {
        emit ssiFinished(IcqPackets::ReqAddContact, uin, false, -1);
        return;
    }
    m_pendingAdd.uin = uin;
    m_pendingAdd.nick = nick.isEmpty() ? uin : nick;
    m_pendingAdd.groupId = groupId;
    m_pendingAdd.ssiId = newSsiId();
    send(IcqPackets::ssiEditStart());
    send(IcqPackets::addContact(uin, m_pendingAdd.nick, groupId, m_pendingAdd.ssiId, false));
    send(IcqPackets::ssiEditEnd());
}

void IcqSession::removeContact(const QString &uin)
{
    if (m_state != Online || !m_contacts.contains(uin)) return;
    const IcqContact c = m_contacts.value(uin);
    if (c.temporary || c.groupId <= 0) {
        m_contacts.remove(uin);
        m_order.removeAll(uin);
        emit contactRemoved(uin);
        return;
    }
    m_pendingDelete = uin;
    send(IcqPackets::ssiEditStart());
    send(IcqPackets::deleteContact(uin, c.groupId, c.ssiId));
    send(IcqPackets::ssiEditEnd());
}

void IcqSession::renameContact(const QString &uin, const QString &nick)
{
    if (m_state != Online || !m_contacts.contains(uin)) return;
    const IcqContact c = m_contacts.value(uin);
    if (c.temporary) {
        m_contacts[uin].nick = nick;
        emit contactChanged(uin);
        return;
    }
    m_pendingRename.uin = uin;
    m_pendingRename.nick = nick;
    m_pendingRename.groupId = c.groupId;
    m_pendingRename.ssiId = c.ssiId;
    send(IcqPackets::ssiEditStart());
    send(IcqPackets::renameContact(uin, nick, c.groupId, c.ssiId, !c.authorized));
    send(IcqPackets::ssiEditEnd());
}

void IcqSession::addGroup(const QString &name)
{
    if (m_state != Online || name.trimmed().isEmpty()) return;
    for (int i = 0; i < m_groups.size(); ++i)
        if (m_groups.at(i).name.compare(name, Qt::CaseInsensitive) == 0) {
            emit ssiFinished(IcqPackets::ReqAddGroup, name, false, -1);
            return;
        }
    m_pendingGroup = name.trimmed();
    m_pendingGroupId = newSsiId();
    send(IcqPackets::ssiEditStart());
    send(IcqPackets::addGroup(m_pendingGroup, m_pendingGroupId));
    send(IcqPackets::ssiEditEnd());
}

void IcqSession::requestAuthorization(const QString &uin, const QString &reason)
{
    if (m_state != Online) return;
    send(IcqPackets::futureAuthGrant(uin));
    send(IcqPackets::authRequest(uin, reason));
}

void IcqSession::replyAuthorization(const QString &uin, bool granted)
{
    if (m_state != Online) return;
    send(IcqPackets::authReply(uin, granted));
}

void IcqSession::handleSsiResult(const QByteArray &data, quint32 reqId)
{
    int result = IcqReader(data).u16();
    switch (reqId) {
    case IcqPackets::ReqAddContact: {
        if (m_pendingAdd.uin.isEmpty()) return;
        if (result == 0) {
            IcqContact &c = ensureContact(m_pendingAdd.uin);
            bool wasThere = !c.temporary;
            c.nick = m_pendingAdd.nick;
            c.groupId = m_pendingAdd.groupId;
            c.ssiId = m_pendingAdd.ssiId;
            c.temporary = false;
            QString uin = m_pendingAdd.uin;
            m_pendingAdd.uin.clear();
            if (wasThere) emit contactChanged(uin); else emit contactAdded(uin);
            emit ssiFinished(reqId, uin, true, 0);
        } else if (result == 0x0E) {
            // the contact requires authorization: add it flagged, then ask
            IcqContact &c = ensureContact(m_pendingAdd.uin);
            c.authorized = false;
            send(IcqPackets::ssiEditStart());
            send(IcqPackets::addContact(m_pendingAdd.uin, m_pendingAdd.nick, m_pendingAdd.groupId, m_pendingAdd.ssiId, true));
            send(IcqPackets::ssiEditEnd());
            requestAuthorization(m_pendingAdd.uin, QLatin1String("Please authorize me"));
        } else {
            QString uin = m_pendingAdd.uin;
            m_pendingAdd.uin.clear();
            emit ssiFinished(reqId, uin, false, result);
        }
        break;
    }
    case IcqPackets::ReqDelContact: {
        QString uin = m_pendingDelete;
        m_pendingDelete.clear();
        if (result == 0 && !uin.isEmpty()) {
            m_contacts.remove(uin);
            m_order.removeAll(uin);
            emit contactRemoved(uin);
        }
        emit ssiFinished(reqId, uin, result == 0, result);
        break;
    }
    case IcqPackets::ReqRename: {
        QString uin = m_pendingRename.uin;
        if (result == 0 && m_contacts.contains(uin)) {
            m_contacts[uin].nick = m_pendingRename.nick;
            emit contactChanged(uin);
        }
        m_pendingRename.uin.clear();
        emit ssiFinished(reqId, uin, result == 0, result);
        break;
    }
    case IcqPackets::ReqAddGroup: {
        if (result == 0 && !m_pendingGroup.isEmpty()) {
            IcqGroup g;
            g.id = m_pendingGroupId;
            g.name = m_pendingGroup;
            m_groups.append(g);
            emit groupsChanged();
        }
        QString name = m_pendingGroup;
        m_pendingGroup.clear();
        emit ssiFinished(reqId, name, result == 0, result);
        break;
    }
    default:
        break;
    }
}

void IcqSession::handleSsiAdd(const QByteArray &data)
{
    IcqReader r(data);
    QString name = textOrCp1251(r.bytes(r.u16()));
    int groupId = r.u16();
    int itemId = r.u16();
    int type = r.u16();
    IcqTlvList tlvs(r.bytes(r.u16()));
    if (type == 0) {
        bool isNew = !m_contacts.contains(name) || m_contacts.value(name).temporary;
        IcqContact &c = ensureContact(name);
        c.groupId = groupId;
        c.ssiId = itemId;
        c.temporary = group(groupId).notInList;
        c.authorized = !tlvs.has(0x66);
        const IcqTlv *nick = tlvs.find(0x131);
        if (nick && !nick->data.isEmpty()) c.nick = textOrCp1251(nick->data);
        if (isNew) emit contactAdded(name); else emit contactChanged(name);
    } else if (type == 1 && groupId != 0) {
        bool known = false;
        for (int i = 0; i < m_groups.size(); ++i) if (m_groups.at(i).id == groupId) known = true;
        if (!known) {
            IcqGroup g;
            g.id = groupId;
            g.name = name;
            g.notInList = tlvs.has(0x6A);
            m_groups.append(g);
            emit groupsChanged();
        }
    }
}

void IcqSession::handleSsiUpdate(const QByteArray &data)
{
    IcqReader r(data);
    QString name = textOrCp1251(r.bytes(r.u16()));
    int groupId = r.u16();
    int itemId = r.u16();
    int type = r.u16();
    IcqTlvList tlvs(r.bytes(r.u16()));
    if (type == 0 && m_contacts.contains(name)) {
        IcqContact &c = m_contacts[name];
        c.ssiId = itemId;
        c.groupId = groupId;
        c.authorized = !tlvs.has(0x66);
        const IcqTlv *nick = tlvs.find(0x131);
        if (nick && !nick->data.isEmpty()) c.nick = textOrCp1251(nick->data);
        emit contactChanged(name);
    } else if (type == 1) {
        for (int i = 0; i < m_groups.size(); ++i)
            if (m_groups[i].id == groupId) { m_groups[i].name = name; emit groupsChanged(); }
    }
}

void IcqSession::handleSsiDelete(const QByteArray &data)
{
    IcqReader r(data);
    QString name = textOrCp1251(r.bytes(r.u16()));
    r.u16(); r.u16();
    int type = r.u16();
    if (type == 0 && m_contacts.contains(name)) {
        m_contacts.remove(name);
        m_order.removeAll(name);
        emit contactRemoved(name);
    }
}

// -- presence ------------------------------------------------------------------------------------

void IcqSession::sendUserInfo()
{
    send(IcqPackets::setUserInfo(m_xstatus, qipGuid(), m_vMajor, m_vMinor, m_vPatch));
}

QString IcqSession::qipGuid() const
{
    return Icq::qipGuidForStatus(m_status);
}

void IcqSession::setStatus(int status)
{
    m_status = status;
    if (m_state != Online) return;
    int wireStatus = qipGuid().isEmpty() ? m_status : Icq::StatusOnline;
    send(IcqPackets::setStatus(wireStatus, 256));
    if (!m_awayText.isEmpty()) send(IcqPackets::setAwayText(m_awayText));
    sendUserInfo();
}

void IcqSession::setXStatus(int index)
{
    m_xstatus = index;
    if (m_state != Online) return;
    sendUserInfo();
    // the server only broadcasts presence on a status change, so repeat the status to make
    // the new capabilities visible to contacts (Jasmine does the same)
    int wireStatus = qipGuid().isEmpty() ? m_status : Icq::StatusOnline;
    send(IcqPackets::setStatus(wireStatus, 256));
}

void IcqSession::setAwayText(const QString &text)
{
    m_awayText = text;
    if (m_state == Online) send(IcqPackets::setAwayText(text));
}

void IcqSession::handleUserOnline(const QByteArray &data)
{
    IcqReader r(data);
    QString uin = r.ascii8();
    if (!m_contacts.contains(uin)) return;
    r.skip(2);   // warning level
    r.skip(2);   // TLV count
    IcqTlvList tlvs(r);
    IcqContact &c = m_contacts[uin];
    const IcqTlv *st = tlvs.find(6);
    c.status = st ? IcqReader(st->data, 2).u16() : Icq::StatusOnline;
    c.awayText.clear();
    const IcqTlv *since = tlvs.find(3);
    if (since) c.onlineSince = QDateTime::fromTime_t(since->u32());
    const IcqTlv *caps = tlvs.find(0x0D);
    const IcqTlv *shortCaps = tlvs.find(0x19);
    if (caps || shortCaps) {
        // a full update: rebuild the capability set (an abbreviated status update carries none,
        // and must not wipe what we know)
        c.caps.clear();
        c.utf8 = c.serverRelay = false;
        c.xstatus = -1;
    }
    if (caps) {
        IcqReader cr(caps->data);
        bool moodFound = false;
        while (cr.remaining() >= 16) {
            QString cap = hexOf(cr.bytes(16));
            c.caps.insert(cap);
            if (!moodFound) {
                int mood = Icq::qipStatusFromGuid(cap);
                if (mood) { c.status = mood; moodFound = true; }
            }
            if (c.xstatus < 0) c.xstatus = Icq::xstatusIndex(cap);
        }
    }
    if (shortCaps) {
        IcqReader cr(shortCaps->data);
        while (cr.remaining() >= 2) {
            int sc = cr.u16();
            c.caps.insert(QString::fromLatin1("0946%1%2").arg(sc, 4, 16, QLatin1Char('0')).toUpper() + QLatin1String("4C7F11D18222444553540000"));
        }
    }
    if (caps || shortCaps) {
        c.utf8 = c.caps.contains(QLatin1String(Icq::CapUtf8));
        c.serverRelay = c.caps.contains(QLatin1String(Icq::CapServerRelay));
    }
    const IcqTlv *avail = tlvs.find(0x1D);
    if (avail) {
        IcqTlvList inner(avail->data);
        const IcqTlv *t2 = inner.find(2);
        if (t2) {
            IcqReader ar(t2->data);
            int len = ar.u16();
            QString text = textOrCp1251(ar.bytes(len)).trimmed();
            if (!text.isEmpty()) c.awayText = text;
        }
    }
    c.presenceKnown = true;
    emit contactChanged(uin);
}

void IcqSession::handleUserOffline(const QByteArray &data)
{
    IcqReader r(data);
    QString uin = r.ascii8();
    if (!m_contacts.contains(uin)) return;
    IcqContact &c = m_contacts[uin];
    c.status = Icq::StatusOffline;
    c.xstatus = -1;
    c.awayText.clear();
    c.typing = false;
    c.presenceKnown = true;
    emit contactChanged(uin);
}

// -- messaging ------------------------------------------------------------------------------------

QByteArray IcqSession::sendMessage(const QString &uin, const QString &text, Encoding enc)
{
    if (m_state != Online) return QByteArray();
    QByteArray cookie;
    quint64 id = QDateTime::currentMSecsSinceEpoch();
    for (int i = 7; i >= 0; --i) cookie.append(char(id >> (8 * i)));
    const IcqContact c = m_contacts.value(uin);
    bool useRelay = m_contacts.contains(uin) && !c.temporary && c.authorized && c.online() && c.serverRelay;
    quint32 reqId = m_nextMsgReq++;
    if (m_nextMsgReq > 0x7FFFFFF0) m_nextMsgReq = 0x1000;
    PendingMsg p;
    p.uin = uin;
    p.cookie = cookie;
    m_pendingMsgs.insert(reqId, p);
    if (m_pendingMsgs.size() > 64) m_pendingMsgs.erase(m_pendingMsgs.begin());
    if (enc == Auto) {
        if (useRelay) enc = c.utf8 ? Channel2Utf8 : Channel2Cp1251;
        else enc = Channel1Ucs2;
    }
    if (enc == Channel1Ucs2 && !c.online()) {
        // kicq.ru's offline store keeps only bytes 0x01-0x7F and stops at the first 0x00:
        // ASCII text survives as charset 0; anything else only as UCS-2, and there only
        // characters >= U+0100 (Cyrillic is 0x04xx). Spaces are mapped to EN SPACE (U+2002)
        // so whole Cyrillic sentences get through; digits and Latin letters cannot.
        bool ascii = true;
        for (int i = 0; i < text.size() && ascii; ++i) if (text.at(i).unicode() >= 0x80) ascii = false;
        if (ascii) {
            enc = Channel1Cp1251;   // pure ASCII bytes, charset 0
        } else {
            QString t = text;
            t.replace(QLatin1Char(' '), QChar(0x2002));
            send(IcqPackets::messageChannel1(uin, IcqText::toUcs2(t), 2, cookie, true, reqId));
            return cookie;
        }
    }
    switch (enc) {
    case Channel2Utf8: send(IcqPackets::messageChannel2(uin, text, 0, true, cookie, reqId)); break;
    case Channel2Cp1251: send(IcqPackets::messageChannel2(uin, text, 1, false, cookie, reqId)); break;
    case Channel1Cp1251: send(IcqPackets::messageChannel1(uin, IcqText::toCp1251(text), 0, cookie, true, reqId)); break;
    case Channel1Utf8: send(IcqPackets::messageChannel1(uin, text.toUtf8(), 0, cookie, true, reqId)); break;
    default: send(IcqPackets::messageChannel1(uin, IcqText::toUcs2(text), 2, cookie, true, reqId)); break;
    }
    return cookie;
}

QByteArray IcqSession::sendMessageRaw(const QString &uin, const QByteArray &text, int charset)
{
    if (m_state != Online) return QByteArray();
    QByteArray cookie;
    quint64 id = QDateTime::currentMSecsSinceEpoch();
    for (int i = 7; i >= 0; --i) cookie.append(char(id >> (8 * i)));
    quint32 reqId = m_nextMsgReq++;
    PendingMsg p;
    p.uin = uin;
    p.cookie = cookie;
    m_pendingMsgs.insert(reqId, p);
    send(IcqPackets::messageChannel1(uin, text, charset, cookie, true, reqId));
    return cookie;
}

void IcqSession::sendTyping(const QString &uin, bool typing)
{
    if (m_state != Online || (m_status & 0xFFFF) == Icq::StatusInvisible) return;
    send(IcqPackets::typingNotify(uin, typing ? 2 : 0));
}

void IcqSession::deliverMessage(const QString &uin, const QString &text, const QDateTime &when, bool offline)
{
    if (!m_contacts.contains(uin)) {
        ensureContact(uin);
        emit contactAdded(uin);
    }
    m_contacts[uin].typing = false;
    emit messageReceived(uin, text, when, offline);
}

void IcqSession::handleIncomingMessage(const QByteArray &data)
{
    IcqReader r(data);
    QByteArray cookie = r.bytes(8);
    int channel = r.u16();
    QString sender = r.ascii8();
    r.skip(2);                     // warning level
    int infoCount = r.u16();
    IcqTlvList info(r, infoCount); // sender's presence TLVs
    Q_UNUSED(info);
    IcqTlvList msgTlvs(r);

    QString text;
    bool haveText = false;
    QDateTime when = QDateTime::currentDateTime();
    bool offline = false;

    if (channel == 1) {
        const IcqTlv *t2 = msgTlvs.find(2);
        if (!t2) return;
        IcqReader m(t2->data);
        while (m.remaining() >= 4) {
            int fragId = m.u8();
            m.u8();                // fragment version
            int len = m.u16();
            if (fragId == 1 && len >= 4) {
                int charset = m.u16();
                m.u16();           // sub-charset
                QByteArray raw = m.bytes(len - 4);
                text = IcqText::unescape(IcqText::stripHtml(IcqText::decodeCharset(raw, charset)));
                haveText = true;
            } else {
                m.skip(len);
            }
        }
        const IcqTlv *ts = msgTlvs.find(0x16);
        if (ts) { when = QDateTime::fromTime_t(ts->u32()); offline = true; }
    } else if (channel == 2) {
        const IcqTlv *t5 = msgTlvs.find(5);
        if (!t5) return;
        IcqReader m(t5->data);
        int msgType = m.u16();
        m.skip(8);                 // cookie again
        QString guid = hexOf(m.bytes(16));
        if (guid == QLatin1String("094613434C7F11D18222444553540000")) return;   // file transfer: unsupported
        if (msgType != 0) return;  // cancel/accept of a rendezvous
        IcqTlvList inner(m);
        const IcqTlv *ext = inner.find(0x2711);
        if (!ext) return;
        IcqReader e(ext->data);
        int len1 = e.u16le(); e.skip(len1);     // protocol/plugin block
        int len2 = e.u16le(); e.skip(len2);     // second block
        int type = e.u8();
        e.u8();                                  // flags
        e.u16le();                               // status
        e.u16le();                               // priority
        int len = e.u16le();
        if (type != 1) return;                   // only plain text (0x1A is an Xtraz plugin request)
        QByteArray raw = e.bytes(len > 0 ? len - 1 : 0);
        e.skip(1);
        // fg/bg colours, then an optional encoding GUID
        bool utf8 = false;
        if (e.remaining() >= 8) {
            e.u32(); e.u32();
            if (e.remaining() >= 4) {
                int glen = e.u32le();
                QString g = e.ascii(glen);
                if (g.contains(QLatin1String("0946134E"), Qt::CaseInsensitive)) utf8 = true;
            }
        }
        if (utf8 && IcqText::isUtf8(raw)) text = QString::fromUtf8(raw.constData(), raw.size());
        else text = IcqText::decodeGuess(raw);
        text = IcqText::unescape(text);
        haveText = true;
    } else {
        return;
    }
    if (!haveText) return;
    if (!offline) send(IcqPackets::messageAck(cookie, sender));
    deliverMessage(sender, text, when, offline);
}

void IcqSession::handleOfflineMessage(const QByteArray &data)
{
    emit log(QLatin1String("offline meta: ") + QString::fromLatin1(data.toHex()));
    IcqReader r(data);
    r.skip(10);
    int type = r.u16();
    if (type == 0x4100) {
        r.skip(2);
        QString uin = QString::number(r.u32le());
        int year = r.u16le();
        int month = r.u8();
        int day = r.u8();
        int hour = r.u8();
        int minute = r.u8();
        r.skip(2);
        int len = r.u16le();
        QByteArray raw = r.bytes(len);
        QString text = IcqText::unescape(IcqText::stripHtml(IcqText::decodeGuess(raw)));
        QString key = uin + QLatin1Char('|') + text;
        if (m_offlineSeen.contains(key)) return;
        m_offlineSeen.append(key);
        // the server's timestamp is in some private timezone (off by hours from UTC); the
        // date is kept, the time of day is not trusted
        Q_UNUSED(hour); Q_UNUSED(minute);
        QDateTime when = QDateTime::currentDateTime();
        QDate d(year, month, day);
        if (d.isValid() && d < when.date()) when = QDateTime(d, QTime(23, 59));
        deliverMessage(uin, text, when, true);
    } else if (type == 0x4200) {
        send(IcqPackets::deleteOfflineMsgs(m_uin, m_seq));
        send(IcqPackets::anotherOfflineMsgsRequest());
    }
}

void IcqSession::handleMessageAck(const QByteArray &data)
{
    IcqReader r(data);
    r.bytes(8);              // cookie - not ours on kicq.ru, see the signal's note
    r.u16();                 // channel
    QString uin = r.ascii8();
    for (QMap<quint32, PendingMsg>::iterator it = m_pendingMsgs.begin(); it != m_pendingMsgs.end(); ++it) {
        if (it->uin == uin) {
            QByteArray cookie = it->cookie;
            m_pendingMsgs.erase(it);
            emit messageDelivered(uin, cookie);
            return;
        }
    }
}

void IcqSession::handleMessageError(const QByteArray &data, quint32 reqId)
{
    int code = IcqReader(data).u16();
    // Errors answer the request id they refer to: our keep-alive probe (error 9, by design),
    // an ack we sent for a stranger's message (error 14), or a message of ours.
    QMap<quint32, PendingMsg>::iterator it = m_pendingMsgs.find(reqId);
    if (it == m_pendingMsgs.end()) return;
    PendingMsg p = *it;
    m_pendingMsgs.erase(it);
    emit log(QString::fromLatin1("message to %1 failed, error %2").arg(p.uin).arg(code));
    emit messageFailed(p.uin, p.cookie, code);
}

void IcqSession::handleTyping(const QByteArray &data)
{
    IcqReader r(data);
    r.skip(10);
    QString uin = r.ascii8();
    int type = r.u16();
    if (!m_contacts.contains(uin)) return;
    bool typing = type == 2;
    if (m_contacts[uin].typing != typing) {
        m_contacts[uin].typing = typing;
        emit contactChanged(uin);
    }
}

// -- authorization ------------------------------------------------------------------------------

void IcqSession::handleAuthRequest(const QByteArray &data)
{
    IcqReader r(data);
    QString uin = r.ascii8();
    int len = r.u16();
    QString reason = textOrCp1251(r.bytes(len));
    if (!m_contacts.contains(uin)) {
        ensureContact(uin);
        emit contactAdded(uin);
    }
    emit authRequested(uin, reason);
}

void IcqSession::handleAuthReply(const QByteArray &data)
{
    IcqReader r(data);
    QString uin = r.ascii8();
    int reply = r.u8();
    if (m_contacts.contains(uin) && reply == 1) {
        m_contacts[uin].authorized = true;
        emit contactChanged(uin);
    }
    emit authReplied(uin, reply == 1);
}

void IcqSession::handleYouWereAdded(const QByteArray &data)
{
    IcqReader r(data);
    emit youWereAdded(r.ascii8());
}
