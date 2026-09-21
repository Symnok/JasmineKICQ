// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "icqtext.h"

#include <QTextCodec>

namespace
{
    QTextCodec *cp1251()
    {
        static QTextCodec *c = QTextCodec::codecForName("windows-1251");
        return c;
    }
}

namespace IcqText
{

QByteArray toUcs2(const QString &s)
{
    QByteArray out;
    out.reserve(s.size() * 2);
    for (int i = 0; i < s.size(); ++i) {
        ushort u = s.at(i).unicode();
        out.append(char(u >> 8));
        out.append(char(u & 0xFF));
    }
    return out;
}

QString fromUcs2(const QByteArray &b)
{
    QString s;
    s.reserve(b.size() / 2);
    for (int i = 0; i + 1 < b.size(); i += 2)
        s.append(QChar(((unsigned char)b.at(i) << 8) | (unsigned char)b.at(i + 1)));
    return s;
}

QByteArray toCp1251(const QString &s)
{
    QTextCodec *c = cp1251();
    return c ? c->fromUnicode(s) : s.toLatin1();
}

QString fromCp1251(const QByteArray &b)
{
    QTextCodec *c = cp1251();
    return c ? c->toUnicode(b) : QString::fromLatin1(b);
}

QByteArray toUtf8(const QString &s)
{
    return s.toUtf8();
}

bool isUtf8(const QByteArray &b)
{
    int i = 0, n = b.size();
    bool sawMultibyte = false;
    while (i < n) {
        unsigned char c = b.at(i);
        int extra;
        if (c < 0x80) extra = 0;
        else if ((c & 0xE0) == 0xC0) extra = 1;
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0) extra = 3;
        else return false;
        for (int k = 1; k <= extra; ++k) {
            if (i + k >= n) return false;
            if (((unsigned char)b.at(i + k) & 0xC0) != 0x80) return false;
        }
        if (extra > 0) sawMultibyte = true;
        i += extra + 1;
    }
    Q_UNUSED(sawMultibyte);
    return true;
}

bool isUcs2(const QByteArray &b)
{
    if (b.size() & 1) return false;
    bool result = true;
    for (int i = 0; i < b.size(); i += 2) {
        signed char hi = b.at(i);
        if (hi > 0 && hi < 9) return true;                  // high byte of a non-Latin char
        if (hi == 0 && b.at(i + 1) != 0) return true;      // ASCII char in UCS-2 form
        if (hi > 32 || hi < 0) result = false;
    }
    return result;
}

QString decodeGuess(const QByteArray &in)
{
    QByteArray b = in;
    while (!b.isEmpty() && b.at(b.size() - 1) == 0) b.chop(1);
    if (b.isEmpty()) return QString();
    if (isUcs2(b)) return fromUcs2(b).remove(QLatin1Char('\r'));
    if (isUtf8(b)) return QString::fromUtf8(b.constData(), b.size());
    return fromCp1251(b);
}

QString decodeCharset(const QByteArray &b, int charset)
{
    switch (charset) {
    case 2: return fromUcs2(b).remove(QLatin1Char('\r'));
    case 3: return QString::fromLatin1(b);
    case 0: // "ASCII", which in practice means the sender's ANSI codepage - or UTF-8
        return isUtf8(b) ? QString::fromUtf8(b.constData(), b.size()) : fromCp1251(b);
    default:
        return decodeGuess(b);
    }
}

QString stripHtml(const QString &s)
{
    if (!s.startsWith(QLatin1String("<HTML>"), Qt::CaseInsensitive) || !s.endsWith(QLatin1String("</HTML>"), Qt::CaseInsensitive))
        return s;
    QString out;
    bool tag = false;
    for (int i = 0; i < s.size(); ++i) {
        QChar c = s.at(i);
        if (c == QLatin1Char('<')) tag = true;
        else if (c == QLatin1Char('>')) { tag = false; continue; }
        if (!tag) out.append(c);
    }
    return out;
}

QString unescape(const QString &s)
{
    if (!s.contains(QLatin1Char('&'))) return s;
    QString r = s;
    r.replace(QLatin1String("&lt;"), QLatin1String("<"));
    r.replace(QLatin1String("&gt;"), QLatin1String(">"));
    r.replace(QLatin1String("&quot;"), QLatin1String("\""));
    r.replace(QLatin1String("&apos;"), QLatin1String("'"));
    r.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return r;
}

}
