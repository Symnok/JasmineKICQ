// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "icqpackets.h"
#include "icqbuffer.h"
#include "icqtext.h"
#include "icqtypes.h"

#include <QList>

namespace
{
    QByteArray hex(const char *s) { return QByteArray::fromHex(s); }

    // ICQ's classic password "roast" table
    const unsigned char roast[16] = {0xF3, 0x26, 0x81, 0xC4, 0x39, 0x86, 0xDB, 0x92,
                                     0x71, 0xA3, 0xB9, 0xE6, 0x53, 0x7A, 0x95, 0x7C};

    const char *clientString = "ICQ Inc. - Product of ICQ (TM).2000b.4.65.1.3281.85";
}

namespace IcqPackets
{

QByteArray flap(int channel, int seq, const QByteArray &payload)
{
    IcqWriter w;
    w.u8(0x2A).u8(channel).u16(seq & 0xFFFF).u16(payload.size()).bytes(payload);
    return w.data();
}

QByteArray snacBytes(const IcqSnac &s)
{
    IcqWriter w;
    w.u16(s.family).u16(s.subtype).u16(s.flags).u32(s.reqId).bytes(s.data);
    return w.data();
}

// -- login ----------------------------------------------------------------------------------

QByteArray xorLogin(const QString &uin, const QString &password)
{
    QByteArray pw = password.left(8).toLatin1();
    for (int i = 0; i < pw.size(); ++i) pw[i] = pw[i] ^ roast[i % 16];
    IcqWriter w;
    w.u32(1);
    w.tlvAscii(0x01, uin);
    w.tlv(0x02, pw);
    w.tlvAscii(0x03, QLatin1String(clientString));
    w.tlv(0x16, hex("010A"));
    w.tlv(0x17, hex("0004"));
    w.tlv(0x18, hex("0041"));
    w.tlv(0x19, hex("0001"));
    w.tlv(0x1A, hex("0CD1"));
    w.tlvU32(0x14, 0x00000055);
    w.tlvAscii(0x0F, QLatin1String("en"));
    w.tlvAscii(0x0E, QLatin1String("us"));
    return w.data();
}

QByteArray bosCookie(const QByteArray &cookie)
{
    IcqWriter w;
    w.u32(1).tlv(0x06, cookie);
    return w.data();
}

// -- session setup ---------------------------------------------------------------------------

namespace
{
    // family/version pairs Jasmine announces (01/04, 13/04, 02/01, 03/01, 15/01, 04/01, 06/01, 09/01, 0A/01, 0B/01)
    const quint32 families[] = {0x00220001, 0x00010004, 0x00130004, 0x00020001, 0x00030001,
                                0x00150001, 0x00040001, 0x00060001, 0x00090001, 0x000A0001, 0x000B0001};
    const int familyCount = sizeof(families) / sizeof(families[0]);
}

IcqSnac clientFamilies()
{
    IcqWriter w;
    for (int i = 0; i < familyCount; ++i) w.u32(families[i]);
    return IcqSnac(1, 0x17, 0x17, w.data());
}

IcqSnac ratesRequest() { return IcqSnac(1, 6, 6, QByteArray()); }

IcqSnac ackRates(int groups)
{
    IcqWriter w;
    for (int i = 0; i < groups; ++i) w.u16(i + 1);
    return IcqSnac(1, 8, 8, w.data());
}

IcqSnac reqSelfInfo() { return IcqSnac(1, 0x0E, 0x0E, QByteArray()); }

IcqSnac reqSsiRights()
{
    IcqWriter w;
    w.u16(0x0B).u16(2).u16(0x0F);
    return IcqSnac(0x13, 2, 2, w.data());
}

IcqSnac reqLocationRights() { return IcqSnac(2, 2, 2, QByteArray()); }

IcqSnac reqBuddyRights()
{
    IcqWriter w;
    w.u16(5).u16(2).u16(3);
    return IcqSnac(3, 2, 2, w.data());
}

IcqSnac reqIcbmParams() { return IcqSnac(4, 4, 4, QByteArray()); }
IcqSnac reqPrivacyRights() { return IcqSnac(9, 2, 2, QByteArray()); }
IcqSnac requestRoster() { return IcqSnac(0x13, 4, 0, QByteArray()); }
IcqSnac rosterActivate() { return IcqSnac(0x13, 7, 7, QByteArray()); }

IcqSnac setUserInfo(int xstatusIndex, const QString &qipGuid, int vMajor, int vMinor, int vPatch)
{
    IcqWriter caps;
    caps.bytes(hex("4a61736d696e65204943512023232323"));   // "Jasmine ICQ ####"
    caps.bytes(hex("4a61736d696e6520766572ff"));           // "Jasmine ver" + 0xff
    caps.u8(vMajor).u8(vMinor).u8(vPatch).u8(0);
    caps.bytes(hex("094600004C7F11D18222444553540000"));
    caps.bytes(hex("094613494C7F11D18222444553540000"));   // server relay
    caps.bytes(hex("0946134E4C7F11D18222444553540000"));   // UTF-8
    caps.bytes(hex("094613434C7F11D18222444553540000"));   // file transfer
    caps.bytes(hex("563FC8090B6F41BD9F79422609DFA2F3"));
    caps.bytes(hex("1A093C6CD7FD4EC59D51A6474E34F5A0"));   // Xtraz
    caps.bytes(hex("094600004C7F11D18222444553540000"));
    caps.bytes(hex("0946134D4C7F11D18222444553540000"));   // typing notifications
    if (xstatusIndex >= 0 && xstatusIndex < Icq::XStatusCount)
        caps.bytes(hex(Icq::XStatusGuids[xstatusIndex]));
    if (!qipGuid.isEmpty())
        caps.bytes(QByteArray::fromHex(qipGuid.toLatin1()));
    IcqWriter w;
    w.tlv(5, caps);
    return IcqSnac(2, 4, 4, w.data());
}

IcqSnac setIcbmParams()
{
    IcqWriter w;
    w.u16(0).u32(1803).u16(8000).u16(999).u16(999).u16(0).u16(0);
    return IcqSnac(4, 2, 2, w.data());
}

IcqSnac clientReady()
{
    IcqWriter w;
    for (int i = 0; i < familyCount; ++i) w.u32(families[i]).u32(0x0110164F);
    return IcqSnac(1, 2, 0x17, w.data());
}

IcqSnac setDcInfo(int status, int flags, int protoVersion)
{
    IcqWriter w;
    w.tlv(6, IcqWriter().u16(flags).u16(status));
    w.tlv(8, IcqWriter().u16(0));
    IcqWriter dc;
    dc.u16(0).u16(0).u32(0).u8(4).u16(protoVersion).u16(0).u16(0).u32(0).u32(0).u32(0).u32(0).u32(0).u16(0);
    w.tlv(0x0C, dc);
    w.tlv(0x1F, IcqWriter().u16(0));
    return IcqSnac(1, 0x1E, 0x1E, w.data());
}

IcqSnac setStatus(int status, int flags)
{
    IcqWriter w;
    w.u16(6).u16(4).u16(flags).u16(status);
    return IcqSnac(1, 0x1E, 0x1E, w.data());
}

IcqSnac setAwayText(const QString &text)
{
    QString away = text.size() > 253 ? text.left(249) + QLatin1String(" ...") : text;
    IcqWriter inner;
    if (!away.isEmpty()) {
        QByteArray raw = away.toUtf8();
        inner.u16(2).u8(4).u8(raw.size() + 2).u16(raw.size()).bytes(raw).u16(0);
        inner.u16(0x0E).u16(8).ascii(QLatin1String("icqmood5"));
    } else {
        inner.u16(2).u8(0).u8(0).u16(0x0E).u16(0);
    }
    IcqWriter w;
    w.tlv(0x1D, inner);
    return IcqSnac(1, 0x1E, 0x1E, w.data());
}

// -- offline messages ------------------------------------------------------------------------

IcqSnac offlineMsgsRequest(const QString &uin, int seq)
{
    IcqWriter inner;
    inner.u16(0x0800).u32le(uin.toUInt()).u16(0x3C00).u16(seq + 1);
    IcqWriter w;
    w.tlv(1, inner);
    return IcqSnac(0x15, 2, 64017, w.data());
}

IcqSnac deleteOfflineMsgs(const QString &uin, int seq)
{
    IcqWriter inner;
    inner.u16(0x0800).u32le(uin.toUInt()).u16(0x3E00).u16(seq + 1);
    IcqWriter w;
    w.tlv(1, inner);
    return IcqSnac(0x15, 2, 0, w.data());
}

IcqSnac anotherOfflineMsgsRequest() { return IcqSnac(4, 0x10, 262431, QByteArray()); }

IcqSnac userInfoRequest(const QString &myUin, const QString &uin, int metaSeq, quint32 reqId)
{
    IcqWriter inner;
    inner.u16le(14).u32le(myUin.toUInt()).u16le(0x07D0).u16le(metaSeq).u16le(0x04B2).u32le(uin.toUInt());
    IcqWriter w;
    w.tlv(1, inner);
    return IcqSnac(0x15, 2, reqId, w.data());
}

// -- messaging -------------------------------------------------------------------------------

IcqSnac messageChannel1(const QString &receiver, const QByteArray &text, int charset, const QByteArray &cookie, bool requestAck, quint32 reqId)
{
    IcqWriter main;
    main.bytes(cookie).u16(1).ascii8(receiver);
    IcqWriter tlv2;
    tlv2.u8(5).u8(1).u16(2).u16(0x0106);               // features fragment
    tlv2.u8(1).u8(1).u16(text.size() + 4).u16(charset).u16(0).bytes(text);   // text fragment
    main.tlv(2, tlv2);
    main.u16(6).u16(0);                                 // store offline
    if (requestAck) main.u16(3).u16(0);                 // request server ack
    return IcqSnac(4, 6, reqId, main.data());
}

IcqSnac messageChannel2(const QString &receiver, const QString &text, int encoding, bool peerUtf8, const QByteArray &cookie, quint32 reqId)
{
    IcqWriter main;
    main.bytes(cookie).u16(2).ascii8(receiver);
    IcqWriter tlv5;
    tlv5.u16(0).bytes(cookie);
    tlv5.bytes(hex("094613494C7F11D1822244455354000000"));
    tlv5.bytes(hex("0A00020001000F0000"));
    IcqWriter ext;
    ext.bytes(hex("1B000A00000000000000000000000000000000000000030000000000000E000000000000000000000000000000010000000100"));
    QByteArray msg;
    bool utf8 = false;
    switch (encoding) {
    case 0: msg = text.toUtf8(); utf8 = true; break;
    case 1: case 3: msg = IcqText::toCp1251(text); break;
    case 2: msg = IcqText::toUcs2(text); break;
    default:
        if (peerUtf8) { msg = text.toUtf8(); utf8 = true; }
        else msg = IcqText::toCp1251(text);
        break;
    }
    ext.u16le(msg.size() + 1).bytes(msg).u8(0);
    ext.u32(0).u32(0xFFFFFF00);
    if (utf8) {
        ext.u32le(38).ascii(QLatin1String("{0946134E-4C7F-11D1-8222-444553540000}"));
    }
    tlv5.tlv(0x2711, ext);
    main.tlv(5, tlv5);
    main.u16(3).u16(0);
    return IcqSnac(4, 6, reqId, main.data());
}

IcqSnac messageAck(const QByteArray &cookie, const QString &receiver)
{
    IcqWriter w;
    if (cookie.size() == 8) w.bytes(cookie); else w.u32(0).u32(0);
    w.u16(2).ascii8(receiver).u16(3);
    w.bytes(hex("1B00090000000000000000000000000000000000000001000000008CBE0E008CBE00000000000000000000000001000000000001000000000000FFFFFF00"));
    return IcqSnac(4, 0x0B, 0x0B, w.data());
}

IcqSnac typingNotify(const QString &uin, int type)
{
    IcqWriter w;
    w.u32(0).u32(0).u16(1).ascii8(uin).u16(type);
    return IcqSnac(4, 0x14, 0x14, w.data());
}

IcqSnac keepAliveProbe()
{
    IcqWriter w;
    w.u8(9);
    return IcqSnac(4, 6, 0x0F, w.data());
}

// -- roster ----------------------------------------------------------------------------------

IcqSnac ssiEditStart() { return IcqSnac(0x13, 0x11, 0x11, QByteArray()); }
IcqSnac ssiEditEnd() { return IcqSnac(0x13, 0x12, 0x12, QByteArray()); }

namespace
{
    QByteArray contactItem(const QString &uin, const QString &nick, int groupId, int ssiId, bool awaitingAuth)
    {
        IcqWriter extra;
        extra.tlv(0x131, nick.toUtf8());
        if (awaitingAuth) extra.tlv(0x66, QByteArray());
        IcqWriter w;
        w.ascii16(uin).u16(groupId).u16(ssiId).u16(0).u16(extra.size()).bytes(extra.data());
        return w.data();
    }
}

IcqSnac addContact(const QString &uin, const QString &nick, int groupId, int ssiId, bool awaitingAuth)
{
    return IcqSnac(0x13, 8, ReqAddContact, contactItem(uin, nick, groupId, ssiId, awaitingAuth));
}

IcqSnac deleteContact(const QString &uin, int groupId, int ssiId)
{
    IcqWriter w;
    w.ascii16(uin).u16(groupId).u16(ssiId).u16(0).u16(0);
    return IcqSnac(0x13, 0x0A, ReqDelContact, w.data());
}

IcqSnac renameContact(const QString &uin, const QString &nick, int groupId, int ssiId, bool awaitingAuth)
{
    return IcqSnac(0x13, 9, ReqRename, contactItem(uin, nick, groupId, ssiId, awaitingAuth));
}

IcqSnac addGroup(const QString &name, int groupId)
{
    IcqWriter w;
    QByteArray raw = name.toUtf8();
    w.u16(raw.size()).bytes(raw).u16(groupId).u16(0).u16(1).u16(0);
    return IcqSnac(0x13, 8, ReqAddGroup, w.data());
}

IcqSnac deleteGroup(const QString &name, int groupId)
{
    IcqWriter w;
    QByteArray raw = name.toUtf8();
    w.u16(raw.size()).bytes(raw).u16(groupId).u16(0).u16(1).u16(0);
    return IcqSnac(0x13, 0x0A, ReqDelGroup, w.data());
}

IcqSnac updateGroup(const QString &name, int groupId, const QList<int> &contactIds)
{
    IcqWriter w;
    QByteArray raw = name.toUtf8();
    w.u16(raw.size()).bytes(raw).u16(groupId).u16(0).u16(1);
    if (!contactIds.isEmpty()) {
        w.u16(contactIds.size() * 2 + 4).u16(0xC8).u16(contactIds.size() * 2);
        for (int i = 0; i < contactIds.size(); ++i) w.u16(contactIds.at(i));
    } else {
        w.u16(0);
    }
    return IcqSnac(0x13, 9, ReqRenameGroup, w.data());
}

IcqSnac authRequest(const QString &uin, const QString &reason)
{
    IcqWriter w;
    QByteArray r = reason.toUtf8();
    w.ascii8(uin).u16(r.size()).bytes(r).u16(0);
    return IcqSnac(0x13, 0x18, 0x18, w.data());
}

IcqSnac futureAuthGrant(const QString &uin)
{
    IcqWriter w;
    w.ascii8(uin).u32(0);
    return IcqSnac(0x13, 0x14, 0x14, w.data());
}

IcqSnac authReply(const QString &uin, bool granted)
{
    IcqWriter w;
    w.ascii8(uin).u8(granted ? 1 : 0).u32(0);
    return IcqSnac(0x13, 0x1A, 0x1A, w.data());
}

}
