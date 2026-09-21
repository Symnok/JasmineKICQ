// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; see the LICENSE file. If not, see <https://www.gnu.org/licenses/>.
//
// Byte-level helpers for OSCAR: a writer that appends big-endian (and, where ICQ's old
// "meta" family insists, little-endian) fields to a QByteArray, a bounds-checked reader,
// and TLV lists. Nothing here knows about the protocol itself.
#ifndef ICQBUFFER_H
#define ICQBUFFER_H

#include <QByteArray>
#include <QList>
#include <QString>

class IcqWriter
{
public:
    IcqWriter() {}

    IcqWriter &u8(int v) { m_d.append(char(v)); return *this; }
    IcqWriter &u16(int v) { m_d.append(char(v >> 8)); m_d.append(char(v)); return *this; }
    IcqWriter &u16le(int v) { m_d.append(char(v)); m_d.append(char(v >> 8)); return *this; }
    IcqWriter &u32(quint32 v) { u16(v >> 16); u16(v & 0xFFFF); return *this; }
    IcqWriter &u32le(quint32 v) { u16le(v & 0xFFFF); u16le(v >> 16); return *this; }
    IcqWriter &bytes(const QByteArray &b) { m_d.append(b); return *this; }
    IcqWriter &ascii(const QString &s) { m_d.append(s.toLatin1()); return *this; }
    /// one-byte length followed by the ASCII string (UINs in ICBM packets)
    IcqWriter &ascii8(const QString &s) { QByteArray b = s.toLatin1(); u8(b.size()); m_d.append(b); return *this; }
    /// two-byte length followed by the ASCII string (SSI item names, login UIN)
    IcqWriter &ascii16(const QString &s) { QByteArray b = s.toLatin1(); u16(b.size()); m_d.append(b); return *this; }
    IcqWriter &tlv(int type, const QByteArray &data) { u16(type); u16(data.size()); m_d.append(data); return *this; }
    IcqWriter &tlv(int type, const IcqWriter &w) { return tlv(type, w.data()); }
    IcqWriter &tlvU16(int type, int v) { return tlv(type, IcqWriter().u16(v).data()); }
    IcqWriter &tlvU32(int type, quint32 v) { return tlv(type, IcqWriter().u32(v).data()); }
    IcqWriter &tlvAscii(int type, const QString &s) { return tlv(type, s.toLatin1()); }

    const QByteArray &data() const { return m_d; }
    int size() const { return m_d.size(); }

private:
    QByteArray m_d;
};

class IcqReader
{
public:
    explicit IcqReader(const QByteArray &d, int pos = 0) : m_d(d), m_pos(pos) {}

    int pos() const { return m_pos; }
    void seek(int p) { m_pos = qBound(0, p, m_d.size()); }
    int remaining() const { return m_d.size() - m_pos; }
    bool atEnd() const { return m_pos >= m_d.size(); }
    void skip(int n) { m_pos = qMin(m_d.size(), m_pos + qMax(0, n)); }

    int u8() { return remaining() >= 1 ? (unsigned char)m_d.at(m_pos++) : 0; }
    int u16() { int a = u8(); int b = u8(); return (a << 8) | b; }
    int u16le() { int a = u8(); int b = u8(); return a | (b << 8); }
    quint32 u32() { quint32 a = u16(); quint32 b = u16(); return (a << 16) | b; }
    quint32 u32le() { quint32 a = u16le(); quint32 b = u16le(); return a | (b << 16); }
    QByteArray bytes(int n) { n = qBound(0, n, remaining()); QByteArray r = m_d.mid(m_pos, n); m_pos += n; return r; }
    QString ascii(int n) { return QString::fromLatin1(bytes(n)); }
    /// one-byte length + ASCII
    QString ascii8() { return ascii(u8()); }
    /// two-byte length + ASCII
    QString ascii16() { return ascii(u16()); }
    QByteArray rest() { return bytes(remaining()); }

private:
    QByteArray m_d;
    int m_pos;
};

struct IcqTlv
{
    IcqTlv() : type(0) {}
    IcqTlv(int t, const QByteArray &d) : type(t), data(d) {}
    int type;
    QByteArray data;

    int u16() const { return IcqReader(data).u16(); }
    quint32 u32() const { return IcqReader(data).u32(); }
};

class IcqTlvList
{
public:
    IcqTlvList() {}
    /// Reads TLVs until the reader is exhausted, or at most `count` of them when count >= 0.
    explicit IcqTlvList(IcqReader &r, int count = -1)
    {
        while (!r.atEnd() && r.remaining() >= 4 && count != 0) {
            int type = r.u16();
            int len = r.u16();
            m_list.append(IcqTlv(type, r.bytes(len)));
            if (count > 0) --count;
        }
    }
    explicit IcqTlvList(const QByteArray &d) { IcqReader r(d); *this = IcqTlvList(r); }

    bool has(int type) const { return find(type) != 0; }
    const IcqTlv *find(int type) const
    {
        for (int i = 0; i < m_list.size(); ++i)
            if (m_list.at(i).type == type) return &m_list.at(i);
        return 0;
    }
    QByteArray value(int type) const { const IcqTlv *t = find(type); return t ? t->data : QByteArray(); }
    int count() const { return m_list.size(); }
    const IcqTlv &at(int i) const { return m_list.at(i); }
    const QList<IcqTlv> &list() const { return m_list; }

private:
    QList<IcqTlv> m_list;
};

#endif // ICQBUFFER_H
