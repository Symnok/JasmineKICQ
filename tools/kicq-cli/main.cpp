// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// Desktop harness for the protocol core: logs in with an account from a credentials file,
// prints everything the session reports, and executes commands appended to a command file
// (Windows console stdin does not mix with the Qt event loop, a polled file does).
//
//   kicq-cli <creds.txt> [account index] [server] [port]
//
// creds.txt: one "UIN password" per line. Commands, one per line in kicq-cmd.txt next to
// the working directory (the file is emptied after each read):
//   msg <uin> <text...>     status <online|away|na|dnd|occupied|ffc|invisible>
//   xstatus <index|-1>     away <text...>     add <uin> <groupId> [nick...]
//   del <uin>              rename <uin> <nick...>    group <name>
//   auth <uin>             grant <uin>   deny <uin>  typing <uin> <0|1>
//   roster                 quit
#include "icqsession.h"
#include "icqtext.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QStringList>
#include <cstdio>

class Harness : public QObject
{
    Q_OBJECT
public:
    Harness(IcqSession *s, const QString &cmdFile) : m_s(s), m_cmdFile(cmdFile)
    {
        connect(s, SIGNAL(log(QString)), this, SLOT(onLog(QString)));
        connect(s, SIGNAL(stateChanged()), this, SLOT(onState()));
        connect(s, SIGNAL(connected()), this, SLOT(onConnected()));
        connect(s, SIGNAL(disconnected(QString,int)), this, SLOT(onDisconnected(QString,int)));
        connect(s, SIGNAL(rosterLoaded()), this, SLOT(printRoster()));
        connect(s, SIGNAL(contactChanged(QString)), this, SLOT(onContact(QString)));
        connect(s, SIGNAL(contactAdded(QString)), this, SLOT(onContactAdded(QString)));
        connect(s, SIGNAL(contactRemoved(QString)), this, SLOT(onContactRemoved(QString)));
        connect(s, SIGNAL(messageReceived(QString,QString,QDateTime,bool)), this, SLOT(onMessage(QString,QString,QDateTime,bool)));
        connect(s, SIGNAL(messageDelivered(QString,QByteArray)), this, SLOT(onDelivered(QString,QByteArray)));
        connect(s, SIGNAL(messageFailed(QString,QByteArray,int)), this, SLOT(onFailed(QString,QByteArray,int)));
        connect(s, SIGNAL(authRequested(QString,QString)), this, SLOT(onAuthRequested(QString,QString)));
        connect(s, SIGNAL(authReplied(QString,bool)), this, SLOT(onAuthReplied(QString,bool)));
        connect(s, SIGNAL(youWereAdded(QString)), this, SLOT(onYouWereAdded(QString)));
        connect(s, SIGNAL(ssiFinished(int,QString,bool,int)), this, SLOT(onSsi(int,QString,bool,int)));
        connect(s, SIGNAL(userInfoReceived(IcqUserInfo)), this, SLOT(onUserInfo(IcqUserInfo)));
        QTimer *t = new QTimer(this);
        connect(t, SIGNAL(timeout()), this, SLOT(pollCommands()));
        t->start(500);
    }

private slots:
    void onLog(const QString &l) { out(QLatin1String("LOG  ") + l); }
    void onState() { out(QString::fromLatin1("STATE %1").arg(m_s->state())); }
    void onConnected() { out(QLatin1String("ONLINE")); }
    void onDisconnected(const QString &r, int code) { out(QString::fromLatin1("DISCONNECTED %1 (login error %2)").arg(r).arg(code)); }
    void onContact(const QString &uin)
    {
        IcqContact c = m_s->contact(uin);
        out(QString::fromLatin1("PRESENCE %1 (%2) status=%3 xstatus=%4 utf8=%5 relay=%6 typing=%7 away=\"%8\"")
            .arg(uin).arg(c.nick).arg(c.status).arg(c.xstatus).arg(c.utf8).arg(c.serverRelay).arg(c.typing).arg(c.awayText));
    }
    void onContactAdded(const QString &uin) { out(QLatin1String("CONTACT-ADDED ") + uin + QLatin1String(" ") + m_s->contact(uin).nick); }
    void onContactRemoved(const QString &uin) { out(QLatin1String("CONTACT-REMOVED ") + uin); }
    void onMessage(const QString &uin, const QString &text, const QDateTime &when, bool offline)
    {
        out(QString::fromLatin1("MSG from %1 at %2%3: %4").arg(uin).arg(when.toString(Qt::ISODate)).arg(offline ? QLatin1String(" [offline]") : QString()).arg(text));
    }
    void onDelivered(const QString &uin, const QByteArray &c) { out(QString::fromLatin1("DELIVERED to %1 cookie=%2").arg(uin).arg(QString::fromLatin1(c.toHex()))); }
    void onFailed(const QString &uin, const QByteArray &, int code) { out(QString::fromLatin1("MSG-FAILED to %1 code=%2").arg(uin).arg(code)); }
    void onAuthRequested(const QString &uin, const QString &reason) { out(QString::fromLatin1("AUTH-REQUEST from %1: %2").arg(uin).arg(reason)); }
    void onAuthReplied(const QString &uin, bool ok) { out(QString::fromLatin1("AUTH-REPLY from %1: %2").arg(uin).arg(ok ? QLatin1String("granted") : QLatin1String("denied"))); }
    void onYouWereAdded(const QString &uin) { out(QLatin1String("YOU-WERE-ADDED by ") + uin); }
    void onUserInfo(const IcqUserInfo &i)
    {
        out(QString::fromLatin1("INFO %1 complete=%2 nick=\"%3\" first=\"%4\" last=\"%5\" email=\"%6\" city=\"%7\" state=\"%8\" phone=\"%9\" cell=\"%10\"")
            .arg(i.uin).arg(i.complete).arg(i.nick).arg(i.firstName).arg(i.lastName).arg(i.email).arg(i.city).arg(i.state).arg(i.phone).arg(i.cell));
        out(QString::fromLatin1("INFO age=%1 gender=%2 birth=%3-%4-%5 homepage=\"%6\" work=\"%7/%8/%9\" about=\"%10\"")
            .arg(i.age).arg(i.gender).arg(i.birthYear).arg(i.birthMonth).arg(i.birthDay).arg(i.homepage).arg(i.workCompany).arg(i.workDepartment).arg(i.workPosition).arg(i.about));
    }
    void onSsi(int req, const QString &name, bool ok, int code) { out(QString::fromLatin1("SSI req=%1 %2 ok=%3 code=%4").arg(req).arg(name).arg(ok).arg(code)); }

    void printRoster()
    {
        QList<IcqGroup> groups = m_s->groups();
        QList<IcqContact> contacts = m_s->contacts();
        out(QString::fromLatin1("ROSTER %1 groups, %2 contacts").arg(groups.size()).arg(contacts.size()));
        for (int g = 0; g < groups.size(); ++g) {
            out(QString::fromLatin1("  GROUP %1 \"%2\"%3").arg(groups.at(g).id).arg(groups.at(g).name).arg(groups.at(g).notInList ? QLatin1String(" [not in list]") : QString()));
            for (int i = 0; i < contacts.size(); ++i) {
                const IcqContact &c = contacts.at(i);
                if (c.groupId != groups.at(g).id) continue;
                out(QString::fromLatin1("    %1 \"%2\" ssi=%3 auth=%4 status=%5%6").arg(c.uin).arg(c.nick).arg(c.ssiId).arg(c.authorized).arg(c.status).arg(c.temporary ? QLatin1String(" [temp]") : QString()));
            }
        }
    }

    void pollCommands()
    {
        QFile f(m_cmdFile);
        if (!f.exists() || f.size() == 0) return;
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), QString::SkipEmptyParts);
        f.close();
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.close();
        for (int i = 0; i < lines.size(); ++i) execute(lines.at(i).trimmed());
    }

private:
    void execute(const QString &line)
    {
        if (line.isEmpty()) return;
        out(QLatin1String("CMD  ") + line);
        QStringList a = line.split(QLatin1Char(' '), QString::SkipEmptyParts);
        QString cmd = a.value(0);
        if (cmd == QLatin1String("quit")) { m_s->disconnectFromServer(); QCoreApplication::quit(); }
        else if (cmd == QLatin1String("roster")) printRoster();
        else if ((cmd == QLatin1String("msg") || cmd.startsWith(QLatin1String("msg-"))) && a.size() >= 3) {
            IcqSession::Encoding e = IcqSession::Auto;
            if (cmd == QLatin1String("msg-ucs2")) e = IcqSession::Channel1Ucs2;
            else if (cmd == QLatin1String("msg-1251")) e = IcqSession::Channel1Cp1251;
            else if (cmd == QLatin1String("msg-utf8")) e = IcqSession::Channel1Utf8;
            else if (cmd == QLatin1String("msg-relay")) e = IcqSession::Channel2Utf8;
            else if (cmd == QLatin1String("msg-relay1251")) e = IcqSession::Channel2Cp1251;
            QByteArray c = m_s->sendMessage(a.at(1), QStringList(a.mid(2)).join(QLatin1String(" ")), e);
            out(QLatin1String("SENT cookie=") + QString::fromLatin1(c.toHex()));
        }
        else if (cmd == QLatin1String("raw1") && a.size() >= 5) {
            // raw1 <charset> <ucs2|1251|utf8> <uin> <text...>
            QString text = QStringList(a.mid(4)).join(QLatin1String(" "));
            QByteArray b = a.at(2) == QLatin1String("ucs2") ? IcqText::toUcs2(text)
                         : a.at(2) == QLatin1String("1251") ? IcqText::toCp1251(text) : text.toUtf8();
            QByteArray c = m_s->sendMessageRaw(a.at(3), b, a.at(1).toInt());
            out(QLatin1String("SENT cookie=") + QString::fromLatin1(c.toHex()));
        }
        else if (cmd == QLatin1String("status") && a.size() >= 2) {
            QString s = a.at(1);
            int st = Icq::StatusOnline;
            if (s == QLatin1String("away")) st = Icq::StatusAway;
            else if (s == QLatin1String("na")) st = Icq::StatusNa;
            else if (s == QLatin1String("dnd")) st = Icq::StatusDnd;
            else if (s == QLatin1String("occupied")) st = Icq::StatusOccupied;
            else if (s == QLatin1String("ffc")) st = Icq::StatusFfc;
            else if (s == QLatin1String("invisible")) st = Icq::StatusInvisible;
            m_s->setStatus(st);
        }
        else if (cmd == QLatin1String("xstatus") && a.size() >= 2) m_s->setXStatus(a.at(1).toInt());
        else if (cmd == QLatin1String("away")) m_s->setAwayText(QStringList(a.mid(1)).join(QLatin1String(" ")));
        else if (cmd == QLatin1String("add") && a.size() >= 3) m_s->addContact(a.at(1), QStringList(a.mid(3)).join(QLatin1String(" ")), a.at(2).toInt());
        else if (cmd == QLatin1String("del") && a.size() >= 2) m_s->removeContact(a.at(1));
        else if (cmd == QLatin1String("rename") && a.size() >= 3) m_s->renameContact(a.at(1), QStringList(a.mid(2)).join(QLatin1String(" ")));
        else if (cmd == QLatin1String("group") && a.size() >= 2) m_s->addGroup(QStringList(a.mid(1)).join(QLatin1String(" ")));
        else if (cmd == QLatin1String("rengroup") && a.size() >= 3) m_s->renameGroup(a.at(1).toInt(), QStringList(a.mid(2)).join(QLatin1String(" ")));
        else if (cmd == QLatin1String("info") && a.size() >= 2) m_s->requestUserInfo(a.at(1));
        else if (cmd == QLatin1String("auth") && a.size() >= 2) m_s->requestAuthorization(a.at(1), QLatin1String("Please authorize me"));
        else if (cmd == QLatin1String("grant") && a.size() >= 2) m_s->replyAuthorization(a.at(1), true);
        else if (cmd == QLatin1String("deny") && a.size() >= 2) m_s->replyAuthorization(a.at(1), false);
        else if (cmd == QLatin1String("typing") && a.size() >= 3) m_s->sendTyping(a.at(1), a.at(2).toInt() != 0);
        else out(QLatin1String("?? unknown command"));
    }

    void out(const QString &s)
    {
        QByteArray b = s.toUtf8();
        std::fprintf(stdout, "%s\n", b.constData());
        std::fflush(stdout);
    }

    IcqSession *m_s;
    QString m_cmdFile;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    if (args.size() < 2) {
        std::fprintf(stderr, "usage: kicq-cli <creds.txt> [account index] [server] [port]\n");
        return 2;
    }
    QFile f(args.at(1));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "cannot read %s\n", qPrintable(args.at(1)));
        return 2;
    }
    QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), QString::SkipEmptyParts);
    int idx = args.size() > 2 ? args.at(2).toInt() : 0;
    if (idx < 0 || idx >= lines.size()) {
        std::fprintf(stderr, "no account #%d in %s\n", idx, qPrintable(args.at(1)));
        return 2;
    }
    QStringList acc = lines.at(idx).trimmed().split(QRegExp(QLatin1String("[ \\t/]+")), QString::SkipEmptyParts);
    if (acc.size() < 2) {
        std::fprintf(stderr, "account line must be: UIN password\n");
        return 2;
    }
    QString server = args.size() > 3 ? args.at(3) : QLatin1String("195.66.114.37");
    int port = args.size() > 4 ? args.at(4).toInt() : 5190;

    IcqSession session;
    session.setClientVersion(0, 1, 0);
    Harness h(&session, QLatin1String("kicq-cmd.txt"));
    session.connectToServer(server, port, acc.at(0), acc.at(1));
    return app.exec();
}

#include "main.moc"
