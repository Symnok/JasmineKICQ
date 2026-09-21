// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// Builders for every packet the client sends. They reproduce, byte for byte, what Jasmine
// IM (the Android client this is ported from) sends to kicq.ru - that server is a third
// party OSCAR implementation and Jasmine is known to work with it, so its exact framing
// is the specification here, not the AOL documents.
#ifndef ICQPACKETS_H
#define ICQPACKETS_H

#include <QByteArray>
#include <QString>

/// A SNAC ready to be framed: family, subtype, flags, request id and payload.
struct IcqSnac
{
    IcqSnac() : family(0), subtype(0), flags(0), reqId(0) {}
    IcqSnac(int f, int s, quint32 id, const QByteArray &d, int fl = 0) : family(f), subtype(s), flags(fl), reqId(id), data(d) {}
    int family;
    int subtype;
    int flags;
    quint32 reqId;
    QByteArray data;
};

namespace IcqPackets
{
    // -- framing -------------------------------------------------------------------------
    QByteArray flap(int channel, int seq, const QByteArray &payload);
    QByteArray snacBytes(const IcqSnac &s);

    // -- login (FLAP channel 1 / 4) ----------------------------------------------------------
    /// The "roasted" (XOR) login, FLAP channel 1 payload. The password is cut to 8 characters
    /// like every ICQ client of that era.
    QByteArray xorLogin(const QString &uin, const QString &password);
    QByteArray bosCookie(const QByteArray &cookie);

    // -- session setup ---------------------------------------------------------------------
    IcqSnac clientFamilies();
    IcqSnac ratesRequest();
    IcqSnac ackRates(int groups);
    IcqSnac reqSelfInfo();
    IcqSnac reqSsiRights();
    IcqSnac reqLocationRights();
    IcqSnac reqBuddyRights();
    IcqSnac reqIcbmParams();
    IcqSnac reqPrivacyRights();
    IcqSnac requestRoster();
    IcqSnac rosterActivate();
    IcqSnac setUserInfo(int xstatusIndex, const QString &qipGuid, int vMajor, int vMinor, int vPatch);
    IcqSnac setIcbmParams();
    IcqSnac clientReady();
    IcqSnac setDcInfo(int status, int flags, int protoVersion);
    IcqSnac setStatus(int status, int flags);
    IcqSnac setAwayText(const QString &text);

    // -- offline messages (old ICQ "meta" family 0x15) ------------------------------------------
    IcqSnac offlineMsgsRequest(const QString &uin, int seq);
    IcqSnac deleteOfflineMsgs(const QString &uin, int seq);
    IcqSnac anotherOfflineMsgsRequest();
    /// Meta "full user info" request (0x04B2); the reply chunks carry reqId back.
    IcqSnac userInfoRequest(const QString &myUin, const QString &uin, int metaSeq, quint32 reqId);

    // -- messaging ----------------------------------------------------------------------------
    /// Channel 1 text with an offline-store TLV and optionally an ack request.
    ///   charset 2: UCS-2BE; charset 0: 8-bit bytes as given (CP1251 or UTF-8) - kicq.ru's offline
    ///   store cuts UCS-2 at the first ASCII character (a NUL high byte), so offline contacts get 8-bit.
    IcqSnac messageChannel1(const QString &receiver, const QByteArray &text, int charset, const QByteArray &cookie, bool requestAck, quint32 reqId);
    /// Channel 2 (server relay): text in the peer's encoding.
    ///   encoding: -1 auto (UTF-8 when the peer advertises it, else CP1251), 0 UTF-8, 1/3 CP1251, 2 UCS-2
    IcqSnac messageChannel2(const QString &receiver, const QString &text, int encoding, bool peerUtf8, const QByteArray &cookie, quint32 reqId);
    IcqSnac messageAck(const QByteArray &cookie, const QString &receiver);
    IcqSnac typingNotify(const QString &uin, int type);
    /// The keep-alive Jasmine uses: a deliberately malformed ICBM that the server always
    /// answers with an error, which proves the link is alive.
    IcqSnac keepAliveProbe();

    // -- roster (SSI, family 0x13) ------------------------------------------------------------
    IcqSnac ssiEditStart();
    IcqSnac ssiEditEnd();
    IcqSnac addContact(const QString &uin, const QString &nick, int groupId, int ssiId, bool awaitingAuth);
    IcqSnac deleteContact(const QString &uin, int groupId, int ssiId);
    IcqSnac renameContact(const QString &uin, const QString &nick, int groupId, int ssiId, bool awaitingAuth);
    IcqSnac addGroup(const QString &name, int groupId);
    IcqSnac deleteGroup(const QString &name, int groupId);
    IcqSnac updateGroup(const QString &name, int groupId, const QList<int> &contactIds);
    IcqSnac authRequest(const QString &uin, const QString &reason);
    IcqSnac futureAuthGrant(const QString &uin);
    IcqSnac authReply(const QString &uin, bool granted);

    // request ids the SSI results come back with (Jasmine's values)
    enum SsiRequest {
        ReqVisibility  = 131073,
        ReqRename      = 131075,
        ReqAddContact  = 131080,
        ReqDelContact  = 131082,
        ReqRenameGroup = 131104,
        ReqDelGroup    = 131105,
        ReqAddGroup    = 131106
    };
}

#endif // ICQPACKETS_H
