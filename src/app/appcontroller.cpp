// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "appcontroller.h"
#include "contactsmodel.h"
#include "messagesmodel.h"
#include "historystore.h"
#include "notifier.h"
#include "icqsession.h"
#include "icqtypes.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDeclarativeView>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QNetworkConfigurationManager>
#include <QNetworkSession>
#include <QPixmap>
#include <QRegExp>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QEvent>

#ifndef APP_VERSION
#define APP_VERSION 0.0.0
#endif
#define KICQ_STR2(x) #x
#define KICQ_STR(x) KICQ_STR2(x)

namespace
{
    const char *const KeyUin = "account/uin";
    const char *const KeyPassword = "account/password";
    const char *const KeyServer = "account/server";
    const char *const KeyPort = "account/port";
    const char *const KeyAutoConnect = "account/autoConnect";
    const char *const KeyStatus = "account/status";
    const char *const KeyLanguage = "ui/language";
    const char *const KeyVibrate = "ui/vibrate";
    const char *const KeyPopups = "ui/popups";
    const char *const KeyShowOffline = "ui/showOffline";
    const char *const DefaultServer = "195.66.114.37";
    const int DefaultPort = 5190;
    const int ReconnectMinMs = 5000;
    const int ReconnectMaxMs = 60000;

    QStringList &logLines() { static QStringList lines; return lines; }
    AppController *logOwner = 0;
}

AppController::AppController(QObject *parent)
    : QObject(parent),
      m_settings(QLatin1String("JasmineKICQ"), QLatin1String("JasmineKICQ")),
      m_history(0), m_netMgr(0), m_netSession(0), m_view(0),
      m_state(QLatin1String("starting")), m_busy(false), m_wantOnline(false), m_everOnline(false),
      m_reconnectDelay(ReconnectMinMs), m_infoLoading(false)
{
    m_session = new IcqSession(this);
    QStringList v = QString::fromLatin1(KICQ_STR(APP_VERSION)).split(QLatin1Char('.'));
    m_session->setClientVersion(v.value(0).toInt(), v.value(1).toInt(), v.value(2).toInt());
    m_contacts = new ContactsModel(m_session, this);
    m_chat = new MessagesModel(m_session, m_contacts, this);
    m_notifier = new Notifier(this);
    m_notifier->setVibrate(vibrate());
    m_notifier->setPopups(popups());
    m_contacts->setShowOffline(m_settings.value(QLatin1String(KeyShowOffline), true).toBool());
    connect(m_contacts, SIGNAL(showOfflineChanged()), this, SLOT(onShowOfflineChanged()));

    connect(m_session, SIGNAL(stateChanged()), this, SLOT(onSessionState()));
    connect(m_session, SIGNAL(connected()), this, SLOT(onSessionConnected()));
    connect(m_session, SIGNAL(disconnected(QString,int)), this, SLOT(onSessionDisconnected(QString,int)));
    connect(m_session, SIGNAL(messageReceived(QString,QString,QDateTime,bool)), this, SLOT(onMessage(QString,QString,QDateTime,bool)));
    connect(m_session, SIGNAL(authRequested(QString,QString)), this, SLOT(onAuthRequested(QString,QString)));
    connect(m_session, SIGNAL(authReplied(QString,bool)), this, SLOT(onAuthReplied(QString,bool)));
    connect(m_session, SIGNAL(youWereAdded(QString)), this, SLOT(onYouWereAdded(QString)));
    connect(m_session, SIGNAL(userInfoReceived(IcqUserInfo)), this, SLOT(onUserInfo(IcqUserInfo)));
    connect(m_session, SIGNAL(ssiFinished(int,QString,bool,int)), this, SLOT(onSsiFinished(int,QString,bool,int)));
    connect(m_session, SIGNAL(log(QString)), this, SLOT(onSessionLog(QString)));
    connect(m_chat, SIGNAL(sendFailed(QString)), this, SLOT(onSendFailed(QString)));

    m_reconnect = new QTimer(this);
    m_reconnect->setSingleShot(true);
    connect(m_reconnect, SIGNAL(timeout()), this, SLOT(onReconnectTimer()));

    qApp->installEventFilter(this);
    logOwner = this;
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    // Back in front: whatever was announced in the notification panel has been seen.
    if (event->type() == QEvent::ApplicationActivate && m_notifier->pendingCount() > 0)
        m_notifier->setPendingCount(0);
    return QObject::eventFilter(watched, event);
}

AppController::~AppController()
{
    m_session->disconnectFromServer();
    if (m_netSession) m_netSession->close();
    delete m_history;
}

// -- properties ----------------------------------------------------------------------------------

void AppController::setState(const QString &s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void AppController::setBusy(bool b)
{
    if (m_busy == b) return;
    m_busy = b;
    emit busyChanged();
}

void AppController::setNotice(const QString &n)
{
    m_notice = n;
    emit noticeChanged();
}

void AppController::clearNotice() { setNotice(QString()); }

QString AppController::connection() const
{
    switch (m_session->state()) {
    case IcqSession::Disconnected: return QLatin1String("offline");
    case IcqSession::Online: return QLatin1String("online");
    default: return QLatin1String("connecting");
    }
}

QString AppController::savedUin() const { return m_settings.value(QLatin1String(KeyUin)).toString(); }
QString AppController::savedPassword() const { return m_settings.value(QLatin1String(KeyPassword)).toString(); }
QString AppController::server() const { return m_settings.value(QLatin1String(KeyServer), QLatin1String(DefaultServer)).toString(); }
void AppController::setServer(const QString &s) { m_settings.setValue(QLatin1String(KeyServer), s.trimmed().isEmpty() ? QLatin1String(DefaultServer) : s.trimmed()); emit settingsChanged(); }
int AppController::port() const { return m_settings.value(QLatin1String(KeyPort), DefaultPort).toInt(); }
void AppController::setPort(int p) { m_settings.setValue(QLatin1String(KeyPort), p > 0 ? p : DefaultPort); emit settingsChanged(); }
QString AppController::version() const { return QLatin1String(KICQ_STR(APP_VERSION)); }
bool AppController::vibrate() const { return m_settings.value(QLatin1String(KeyVibrate), true).toBool(); }
void AppController::setVibrate(bool on) { m_settings.setValue(QLatin1String(KeyVibrate), on); m_notifier->setVibrate(on); emit settingsChanged(); }
bool AppController::popups() const { return m_settings.value(QLatin1String(KeyPopups), true).toBool(); }
void AppController::setPopups(bool on) { m_settings.setValue(QLatin1String(KeyPopups), on); m_notifier->setPopups(on); emit settingsChanged(); }
bool AppController::autoConnect() const { return m_settings.value(QLatin1String(KeyAutoConnect), true).toBool(); }
void AppController::setAutoConnect(bool on) { m_settings.setValue(QLatin1String(KeyAutoConnect), on); emit settingsChanged(); }
int AppController::myStatus() const { return m_session->isOnline() ? m_session->ownStatus() : int(Icq::StatusOffline); }
QString AppController::myStatusIcon() const { return QLatin1String("qrc:/images/") + ContactsModel::statusIcon(myStatus()); }
QString AppController::myStatusText() const { return ContactsModel::statusText(myStatus()); }
QString AppController::myUin() const { return m_uin; }

QString AppController::language() const
{
    return effectiveLanguage(m_settings);
}

void AppController::setLanguage(const QString &lang)
{
    m_settings.setValue(QLatin1String(KeyLanguage), lang);
    emit settingsChanged();
}

QString AppController::effectiveLanguage(const QSettings &settings)
{
    const QString chosen = settings.value(QLatin1String(KeyLanguage)).toString();
    if (!chosen.isEmpty()) return chosen;
    const QString sys = QLocale::system().name().left(2).toLower();
    return (sys == QLatin1String("ru") || sys == QLatin1String("uk")) ? sys : QString::fromLatin1("en");
}

// -- start / network ---------------------------------------------------------------------------------

void AppController::start()
{
    setState(QLatin1String("starting"));
    setBusy(true);
    // On the phone a socket without an open QNetworkSession either fails or prompts for an
    // access point every time. Open the default configuration once, up front.
    m_netMgr = new QNetworkConfigurationManager(this);
    const QNetworkConfiguration cfg = m_netMgr->defaultConfiguration();
    if (!cfg.isValid() || !(m_netMgr->capabilities() & QNetworkConfigurationManager::NetworkSessionRequired)) {
        continueStart();
        return;
    }
    m_netSession = new QNetworkSession(cfg, this);
    connect(m_netSession, SIGNAL(opened()), this, SLOT(onNetworkOpened()));
    connect(m_netSession, SIGNAL(error(QNetworkSession::SessionError)), this, SLOT(onNetworkError()));
    m_netSession->open();
}

void AppController::onNetworkOpened() { continueStart(); }

void AppController::onNetworkError()
{
    qWarning() << "network session error:" << (m_netSession ? m_netSession->errorString() : QString());
    continueStart();
}

void AppController::continueStart()
{
    static bool started = false;
    if (started) return;
    started = true;

    QString uin = savedUin();
    QString pw = savedPassword();
#ifndef Q_OS_SYMBIAN
    // Desktop testing: KICQ_CREDS_FILE (one "UIN password" per line) and KICQ_ACCOUNT (line index).
    const QByteArray credsFile = qgetenv("KICQ_CREDS_FILE");
    if (!credsFile.isEmpty()) {
        QFile f(QString::fromLocal8Bit(credsFile));
        if (f.open(QIODevice::ReadOnly)) {
            QStringList lines = QString::fromUtf8(f.readAll()).split(QRegExp(QLatin1String("\r?\n")), QString::SkipEmptyParts);
            int idx = qgetenv("KICQ_ACCOUNT").toInt();
            QStringList acc = lines.value(idx).split(QRegExp(QLatin1String("[ \t/]+")), QString::SkipEmptyParts);
            if (acc.size() >= 2) { uin = acc.at(0); pw = acc.at(1); }
        }
    }
#endif
    if (uin.isEmpty() || pw.isEmpty() || !autoConnect()) {
        setBusy(false);
        setState(QLatin1String("login"));
        return;
    }
    m_uin = uin;
    m_password = pw;
    openHistory(uin);
    m_wantOnline = true;
    m_session->setStatus(m_settings.value(QLatin1String(KeyStatus), int(Icq::StatusOnline)).toInt());
    setState(QLatin1String("ready"));
    connectSession();
}

void AppController::openHistory(const QString &uin)
{
    delete m_history;
    m_history = new HistoryStore(uin);
    m_contacts->setHistory(m_history);
    m_chat->setHistory(m_history);
}

void AppController::connectSession()
{
    setBusy(true);
    m_session->connectToServer(server(), port(), m_uin, m_password);
}

// -- sign in / out ----------------------------------------------------------------------------------

void AppController::login(const QString &uin, const QString &password, bool remember)
{
    QString u = uin.trimmed();
    if (u.isEmpty() || password.isEmpty()) {
        m_loginError = tr("Enter your UIN and password.");
        emit loginErrorChanged();
        return;
    }
    m_loginError.clear();
    emit loginErrorChanged();
    m_uin = u;
    m_password = password;
    m_settings.setValue(QLatin1String(KeyUin), u);
    m_settings.setValue(QLatin1String(KeyPassword), remember ? password : QString());
    m_settings.setValue(QLatin1String(KeyAutoConnect), remember);
    openHistory(u);
    m_wantOnline = true;
    m_everOnline = false;
    m_reconnectDelay = ReconnectMinMs;
    m_session->setStatus(m_settings.value(QLatin1String(KeyStatus), int(Icq::StatusOnline)).toInt());
    connectSession();
}

void AppController::signOut()
{
    m_wantOnline = false;
    m_reconnect->stop();
    m_chat->close();
    m_session->disconnectFromServer();
    m_settings.remove(QLatin1String(KeyPassword));
    setBusy(false);
    setState(QLatin1String("login"));
}

void AppController::goOffline()
{
    m_wantOnline = false;
    m_reconnect->stop();
    m_session->disconnectFromServer();
    setBusy(false);
    emit myStatusChanged();
    emit connectionChanged();
}

void AppController::reconnect()
{
    if (m_uin.isEmpty()) return;
    m_wantOnline = true;
    m_reconnect->stop();
    m_reconnectDelay = ReconnectMinMs;
    if (m_session->state() == IcqSession::Disconnected) connectSession();
}

void AppController::onReconnectTimer()
{
    if (m_wantOnline && m_session->state() == IcqSession::Disconnected) connectSession();
}

// -- session events -------------------------------------------------------------------------------

void AppController::onSessionState()
{
    emit connectionChanged();
    emit myStatusChanged();
}

void AppController::onSessionConnected()
{
    setBusy(false);
    m_everOnline = true;
    m_reconnectDelay = ReconnectMinMs;
    setState(QLatin1String("ready"));
    emit myStatusChanged();
}

void AppController::onSessionDisconnected(const QString &reason, int loginError)
{
    setBusy(false);
    emit myStatusChanged();
    if (loginError != 0 || (!m_everOnline && m_state == QLatin1String("login"))) {
        // refused by the server: back to the sign-in page with the reason
        m_wantOnline = false;
        m_loginError = reason;
        emit loginErrorChanged();
        setState(QLatin1String("login"));
        return;
    }
    if (!m_everOnline && !reason.isEmpty()) {
        m_loginError = reason;
        emit loginErrorChanged();
        setState(QLatin1String("login"));
        m_wantOnline = false;
        return;
    }
    if (m_wantOnline && !reason.isEmpty()) {
        setNotice(tr("Connection lost: %1. Reconnecting...").arg(reason));
        m_reconnect->start(m_reconnectDelay);
        m_reconnectDelay = qMin(m_reconnectDelay * 2, ReconnectMaxMs);
    }
}

void AppController::onShowOfflineChanged()
{
    m_settings.setValue(QLatin1String(KeyShowOffline), m_contacts->showOffline());
}

void AppController::onSessionLog(const QString &line)
{
    qDebug() << "icq:" << line;
}

bool AppController::appInForeground() const
{
    return m_view && m_view->isActiveWindow();
}

void AppController::onMessage(const QString &uin, const QString &text, const QDateTime &when, bool offline)
{
    HistoryRecord r;
    r.out = false;
    r.text = text;
    r.time = when;
    r.offline = offline;
    m_chat->appendIncoming(uin, r);
    bool chatOpen = m_chat->uin() == uin && appInForeground();
    if (!chatOpen) {
        m_contacts->setUnread(uin, m_contacts->unread(uin) + 1);
        emit messageArrived(uin);
    }
    if (!appInForeground()) {
        QString who = m_contacts->contactInfo(uin).value(QLatin1String("nick")).toString();
        m_notifier->notify(who, text);
        m_notifier->setPendingCount(m_notifier->pendingCount() + 1);
    }
}

void AppController::onAuthRequested(const QString &uin, const QString &reason)
{
    m_authUin = uin;
    QString who = m_contacts->contactInfo(uin).value(QLatin1String("nick")).toString();
    m_authText = reason.isEmpty() ? tr("%1 (%2) asks for your authorization.").arg(who).arg(uin)
                                  : tr("%1 (%2) asks for your authorization: %3").arg(who).arg(uin).arg(reason);
    emit authRequestChanged();
    if (!appInForeground()) m_notifier->notify(who, tr("asks for your authorization"));
}

void AppController::answerAuthorization(bool grant)
{
    if (m_authUin.isEmpty()) return;
    m_session->replyAuthorization(m_authUin, grant);
    m_authUin.clear();
    m_authText.clear();
    emit authRequestChanged();
}

void AppController::onAuthReplied(const QString &uin, bool granted)
{
    QString who = m_contacts->contactInfo(uin).value(QLatin1String("nick")).toString();
    setNotice(granted ? tr("%1 authorized you.").arg(who) : tr("%1 declined your authorization request.").arg(who));
}

void AppController::onYouWereAdded(const QString &uin)
{
    setNotice(tr("%1 added you to their contact list.").arg(uin));
}

void AppController::onSsiFinished(int request, const QString &name, bool ok, int code)
{
    switch (request) {
    case IcqPackets::ReqAddContact:
        setNotice(ok ? tr("Contact %1 added.").arg(name)
                     : (code == -1 ? tr("%1 is already in the list.").arg(name) : tr("Could not add %1 (error %2).").arg(name).arg(code)));
        break;
    case IcqPackets::ReqDelContact:
        setNotice(ok ? tr("Contact removed.") : tr("Could not remove the contact (error %1).").arg(code));
        break;
    case IcqPackets::ReqRename:
        if (!ok) setNotice(tr("Could not rename the contact (error %1).").arg(code));
        break;
    case IcqPackets::ReqAddGroup:
        setNotice(ok ? tr("Group \"%1\" created.").arg(name)
                     : (code == -1 ? tr("Group \"%1\" already exists.").arg(name) : tr("Could not create the group (error %1).").arg(code)));
        break;
    case IcqPackets::ReqRenameGroup:
        if (!ok) setNotice(tr("Could not rename the group (error %1).").arg(code));
        break;
    default:
        break;
    }
}

void AppController::onSendFailed(const QString &error)
{
    setNotice(error);
}

// -- actions ---------------------------------------------------------------------------------------------

void AppController::setStatus(int status)
{
    m_settings.setValue(QLatin1String(KeyStatus), status);
    m_session->setStatus(status);
    emit myStatusChanged();
}

QVariantList AppController::statusChoices() const
{
    static const int list[] = {Icq::StatusOnline, Icq::StatusFfc, Icq::StatusAway, Icq::StatusNa,
                               Icq::StatusOccupied, Icq::StatusDnd, Icq::StatusInvisible};
    QVariantList out;
    for (unsigned i = 0; i < sizeof(list) / sizeof(list[0]); ++i) {
        QVariantMap m;
        m.insert(QLatin1String("status"), list[i]);
        m.insert(QLatin1String("name"), ContactsModel::statusText(list[i]));
        m.insert(QLatin1String("icon"), QLatin1String("qrc:/images/") + ContactsModel::statusIcon(list[i]));
        out.append(m);
    }
    return out;
}

void AppController::addContact(const QString &uin, const QString &nick, int groupId)
{
    QString u = uin.trimmed();
    if (u.isEmpty() || !QRegExp(QLatin1String("\\d{1,12}")).exactMatch(u)) {
        setNotice(tr("A UIN is a number."));
        return;
    }
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return; }
    m_session->addContact(u, nick.trimmed(), groupId);
}

void AppController::removeContact(const QString &uin)
{
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return; }
    if (m_chat->uin() == uin) m_chat->close();
    m_session->removeContact(uin);
}

void AppController::renameContact(const QString &uin, const QString &nick)
{
    if (nick.trimmed().isEmpty()) return;
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return; }
    m_session->renameContact(uin, nick.trimmed());
}

void AppController::addGroup(const QString &name)
{
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return; }
    m_session->addGroup(name);
}

void AppController::renameGroup(int groupId, const QString &name)
{
    if (name.trimmed().isEmpty()) return;
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return; }
    const IcqGroup g = m_session->group(groupId);
    if (g.id != groupId || g.notInList || groupId <= 0) { setNotice(tr("This group cannot be renamed.")); return; }
    m_session->renameGroup(groupId, name.trimmed());
}

// -- profile ------------------------------------------------------------------------------------------

bool AppController::showContactInfo(const QString &uin)
{
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return false; }
    m_infoUin = uin;
    m_infoRows.clear();
    m_infoNote.clear();
    m_infoLoading = true;
    emit infoChanged();
    m_session->requestUserInfo(uin);
    return true;
}

QString AppController::infoTitle() const
{
    if (m_infoUin.isEmpty()) return QString();
    if (m_infoUin == m_session->uin()) return tr("My profile");
    if (m_session->hasContact(m_infoUin)) return m_session->contact(m_infoUin).nick;
    return m_infoUin;
}

QString AppController::infoIcon() const
{
    if (m_infoUin == m_session->uin()) return myStatusIcon();
    return m_contacts->contactInfo(m_infoUin).value(QLatin1String("statusIcon")).toString();
}

namespace
{
    void infoRow(QVariantList &rows, const QString &label, const QString &value)
    {
        if (value.trimmed().isEmpty()) return;
        QVariantMap m;
        m.insert(QLatin1String("label"), label);
        m.insert(QLatin1String("value"), value.trimmed());
        rows.append(m);
    }
}

void AppController::onUserInfo(const IcqUserInfo &info)
{
    if (info.uin != m_infoUin) return;
    QVariantList rows;
    infoRow(rows, tr("UIN"), info.uin);
    infoRow(rows, tr("Nickname"), info.nick);
    infoRow(rows, tr("First name"), info.firstName);
    infoRow(rows, tr("Last name"), info.lastName);
    infoRow(rows, tr("Gender"), info.gender == 1 ? tr("female") : (info.gender == 2 ? tr("male") : QString()));
    if (info.age > 0) infoRow(rows, tr("Age"), QString::number(info.age));
    if (info.birthYear > 0 || info.birthMonth > 0) {
        QDate d(info.birthYear > 0 ? info.birthYear : 2000, info.birthMonth, info.birthDay);
        QString text = d.isValid() ? (info.birthYear > 0 ? d.toString(QLatin1String("d MMMM yyyy")) : d.toString(QLatin1String("d MMMM")))
                                   : QString::fromLatin1("%1-%2-%3").arg(info.birthYear).arg(info.birthMonth).arg(info.birthDay);
        infoRow(rows, tr("Birthday"), text);
    }
    infoRow(rows, tr("E-mail"), info.email);
    infoRow(rows, tr("City"), info.city);
    infoRow(rows, tr("Region"), info.state);
    infoRow(rows, tr("Phone"), info.phone);
    infoRow(rows, tr("Mobile"), info.cell);
    infoRow(rows, tr("Homepage"), info.homepage);
    infoRow(rows, tr("Company"), info.workCompany);
    infoRow(rows, tr("Department"), info.workDepartment);
    infoRow(rows, tr("Position"), info.workPosition);
    infoRow(rows, tr("About"), info.about);
    m_infoRows = rows;
    m_infoLoading = false;
    // only the UIN row: the server knows nothing (or no such user)
    m_infoNote = rows.size() <= 1 ? (info.complete ? tr("The server has no details about this user.") : tr("No reply from the server."))
                                  : QString();
    emit infoChanged();
}

void AppController::requestAuthorization(const QString &uin)
{
    if (!m_session->isOnline()) { setNotice(tr("Not connected.")); return; }
    m_session->requestAuthorization(uin, tr("Please authorize me"));
    setNotice(tr("Authorization request sent."));
}

void AppController::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void AppController::copyText(const QString &text)
{
    QApplication::clipboard()->setText(text);
    setNotice(tr("Copied."));
}

void AppController::appendLog(const QString &line)
{
    QStringList &lines = logLines();
    lines.append(QDateTime::currentDateTime().toString(QLatin1String("HH:mm:ss ")) + line);
    while (lines.size() > 40) lines.removeFirst();
    if (logOwner) emit logOwner->logChanged();
}

QString AppController::logTail() const
{
    return logLines().join(QLatin1String("\n"));
}

bool AppController::autotest() const
{
    return !qgetenv("KICQ_SHOT_DIR").isEmpty();
}

QString AppController::autotestUin() const
{
    QByteArray uin = qgetenv("KICQ_TEST_UIN");
    return uin.isEmpty() ? QLatin1String("93444") : QString::fromLatin1(uin);
}

void AppController::takeScreenshot(const QString &name)
{
    QByteArray dir = qgetenv("KICQ_SHOT_DIR");
    if (dir.isEmpty() || !m_view) return;
    QDir().mkpath(QString::fromLocal8Bit(dir));
    QPixmap::grabWidget(m_view).save(QString::fromLocal8Bit(dir) + QLatin1String("/") + name + QLatin1String(".png"));
}
