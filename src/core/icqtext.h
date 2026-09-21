// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// Text encodings on the ICQ wire: UCS-2BE (channel 1), UTF-8 and CP1251 (channel 2, by
// the peer's capabilities), and the guesswork needed for incoming text whose declared
// charset cannot be trusted - the same heuristics Jasmine IM uses against kicq.ru.
#ifndef ICQTEXT_H
#define ICQTEXT_H

#include <QByteArray>
#include <QString>

namespace IcqText
{
    QByteArray toUcs2(const QString &s);
    QString fromUcs2(const QByteArray &b);
    QByteArray toCp1251(const QString &s);
    QString fromCp1251(const QByteArray &b);
    QByteArray toUtf8(const QString &s);
    /// Strict UTF-8 check: false on any malformed sequence.
    bool isUtf8(const QByteArray &b);
    /// Looks like UCS-2BE text (even length, high bytes mostly zero or control).
    bool isUcs2(const QByteArray &b);
    /// Best-effort decode of incoming text: UCS-2 if it looks like it, then UTF-8 if valid,
    /// else CP1251. Trailing NULs are dropped.
    QString decodeGuess(const QByteArray &b);
    /// Decode with the charset id from an ICBM 0x0101 fragment (0 ASCII/ANSI, 2 UCS-2, 3 latin1),
    /// falling back to the guess when the id is unknown.
    QString decodeCharset(const QByteArray &b, int charset);
    /// Removes the <HTML>...</HTML> wrapping some clients put around channel-1 text and
    /// unescapes the handful of entities ICQ clients emit.
    QString stripHtml(const QString &s);
    QString unescape(const QString &s);
}

#endif // ICQTEXT_H
