// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// The data the session keeps about the roster, and the status vocabulary.
#ifndef ICQTYPES_H
#define ICQTYPES_H

#include <QByteArray>
#include <QDateTime>
#include <QSet>
#include <QString>

namespace Icq
{
    /// Status words as they travel in TLV 0x06 (low 16 bits). Offline is our own marker.
    enum Status {
        StatusOffline   = -1,
        StatusOnline    = 0x0000,
        StatusAway      = 0x0001,
        StatusDnd       = 0x0002,
        StatusNa        = 0x0004,
        StatusOccupied  = 0x0010,
        StatusFfc       = 0x0020,
        StatusInvisible = 0x0100,
        // QIP "moods" carried as capabilities; kept in the same field like Jasmine does
        StatusLunch     = 0x2001,
        StatusEvil      = 0x3000,
        StatusDepress   = 0x4000,
        StatusHome      = 0x5000,
        StatusWork      = 0x6000
    };

    /// The daisies the UI shows: green online, yellow away, green with a badge for busy,
    /// red offline (the classic ICQ convention), white when the status is not known.
    enum StatusColor { Green, Yellow, Dnd, Occupied, Red, White };

    inline StatusColor statusColor(int status)
    {
        if (status < 0) return Red;
        switch (status & 0xFFFF) {
        case StatusAway: case StatusNa: case 0x0005: return Yellow;
        case StatusDnd: case 0x0013: return Dnd;
        case StatusOccupied: case 0x0011: return Occupied;
        default: return Green;
        }
    }

    /// Capabilities that matter for choosing the message channel/encoding.
    const char * const CapServerRelay = "094613494C7F11D18222444553540000";
    const char * const CapUtf8        = "0946134E4C7F11D18222444553540000";

    /// Xtraz status GUIDs in Jasmine's order; the index is what the UI and the icons use.
    extern const char * const XStatusGuids[];
    extern const int XStatusCount;
    /// Icon names (images/x_<name>.png) in the same order.
    extern const char * const XStatusNames[];
    int xstatusIndex(const QString &guidHex);

    /// QIP mood GUIDs -> status words.
    int qipStatusFromGuid(const QString &guidHex);
    QString qipGuidForStatus(int status);
}

struct IcqGroup
{
    IcqGroup() : id(0), notInList(false) {}
    int id;
    QString name;
    /// the "not in list" pseudo-group (SSI group with TLV 0x6A): temporary contacts
    bool notInList;
};

struct IcqContact
{
    IcqContact() : groupId(0), ssiId(0), authorized(true), status(Icq::StatusOffline), xstatus(-1),
                   utf8(false), serverRelay(false), typing(false), presenceKnown(false), temporary(false) {}
    QString uin;
    QString nick;
    int groupId;
    int ssiId;
    bool authorized;
    int status;
    int xstatus;
    QString awayText;
    bool utf8;
    bool serverRelay;
    bool typing;
    bool presenceKnown;
    /// not on the server-side list (a stranger who wrote to us, or an add in progress)
    bool temporary;
    QSet<QString> caps;
    QDateTime onlineSince;

    bool online() const { return status != Icq::StatusOffline; }
};

/// What the server knows about a user (meta family 0x15, "full user info"); empty strings
/// and zero numbers mean "not filled in".
struct IcqUserInfo
{
    IcqUserInfo() : age(0), gender(0), birthYear(0), birthMonth(0), birthDay(0), complete(false) {}
    QString uin;
    QString nick, firstName, lastName, email, city, state, phone, cell, homepage, about;
    QString workCompany, workDepartment, workPosition;
    int age;
    int gender;         // 1 female, 2 male, 0 unknown
    int birthYear, birthMonth, birthDay;
    /// the server sent the final chunk (otherwise the request timed out with what arrived)
    bool complete;
};

#endif // ICQTYPES_H
