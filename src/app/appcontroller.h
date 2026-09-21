// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// Process-wide state, exposed to QML as "app": the network session (the Symbian access
// point), the ICQ session with automatic reconnection, the models the pages bind to,
// settings, notifications, and the little host services QML cannot do itself.
#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariantList>

class IcqSession;
class ContactsModel;
class MessagesModel;
class HistoryStore;
class Notifier;
class QNetworkConfigurationManager;
class QNetworkSession;
class QTimer;
class QDeclarativeView;
class QDateTime;
struct IcqUserInfo;

class AppController : public QObject
{
    Q_OBJECT
    /// "starting" (opening the network), "login" (sign-in page) or "ready" (contact list).
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    /// "offline", "connecting" or "online" - the ICQ link, shown on the contact list.
    Q_PROPERTY(QString connection READ connection NOTIFY connectionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString loginError READ loginError NOTIFY loginErrorChanged)
    Q_PROPERTY(QString savedUin READ savedUin CONSTANT)
    Q_PROPERTY(QString savedPassword READ savedPassword CONSTANT)
    Q_PROPERTY(QString server READ server WRITE setServer NOTIFY settingsChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY settingsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY settingsChanged)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY settingsChanged)
    Q_PROPERTY(bool vibrate READ vibrate WRITE setVibrate NOTIFY settingsChanged)
    Q_PROPERTY(bool popups READ popups WRITE setPopups NOTIFY settingsChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY settingsChanged)
    Q_PROPERTY(int myStatus READ myStatus NOTIFY myStatusChanged)
    Q_PROPERTY(QString myStatusIcon READ myStatusIcon NOTIFY myStatusChanged)
    Q_PROPERTY(QString myStatusText READ myStatusText NOTIFY myStatusChanged)
    Q_PROPERTY(QString myUin READ myUin NOTIFY stateChanged)
    Q_PROPERTY(ContactsModel *contacts READ contacts CONSTANT)
    Q_PROPERTY(MessagesModel *chat READ chat CONSTANT)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString authRequestUin READ authRequestUin NOTIFY authRequestChanged)
    Q_PROPERTY(QString authRequestText READ authRequestText NOTIFY authRequestChanged)
    /// The profile page: whose details, and the [{label, value}] rows once they arrived.
    Q_PROPERTY(QString infoUin READ infoUin NOTIFY infoChanged)
    Q_PROPERTY(QString infoTitle READ infoTitle NOTIFY infoChanged)
    Q_PROPERTY(QString infoIcon READ infoIcon NOTIFY infoChanged)
    Q_PROPERTY(QVariantList infoRows READ infoRows NOTIFY infoChanged)
    Q_PROPERTY(bool infoLoading READ infoLoading NOTIFY infoChanged)
    Q_PROPERTY(QString infoNote READ infoNote NOTIFY infoChanged)
    /// Desktop testing: true when KICQ_SHOT_DIR is set; main.qml then walks the pages and shoots them.
    Q_PROPERTY(bool autotest READ autotest CONSTANT)
    /// The contact the autotest talks to (KICQ_TEST_UIN, default 93444 - never a bot).
    Q_PROPERTY(QString autotestUin READ autotestUin CONSTANT)
    /// The last log lines (warnings, QML errors, notifier results) for the About page.
    Q_PROPERTY(QString logTail READ logTail NOTIFY logChanged)
public:
    explicit AppController(QObject *parent = 0);
    ~AppController();

    void setView(QDeclarativeView *view) { m_view = view; }

    QString state() const { return m_state; }
    QString connection() const;
    bool busy() const { return m_busy; }
    QString loginError() const { return m_loginError; }
    QString savedUin() const;
    QString savedPassword() const;
    QString server() const;
    void setServer(const QString &s);
    int port() const;
    void setPort(int p);
    QString version() const;
    QString language() const;
    void setLanguage(const QString &lang);
    bool notifications() const;
    void setNotifications(bool on);
    bool vibrate() const;
    void setVibrate(bool on);
    bool popups() const;
    void setPopups(bool on);
    bool autoConnect() const;
    void setAutoConnect(bool on);
    int myStatus() const;
    QString myStatusIcon() const;
    QString myStatusText() const;
    QString myUin() const;
    ContactsModel *contacts() const { return m_contacts; }
    MessagesModel *chat() const { return m_chat; }
    QString notice() const { return m_notice; }
    QString authRequestUin() const { return m_authUin; }
    QString authRequestText() const { return m_authText; }
    bool autotest() const;
    QString autotestUin() const;
    QString logTail() const;
    QString infoUin() const { return m_infoUin; }
    QString infoTitle() const;
    QString infoIcon() const;
    QVariantList infoRows() const { return m_infoRows; }
    bool infoLoading() const { return m_infoLoading; }
    QString infoNote() const { return m_infoNote; }
    static void appendLog(const QString &line);

    /// Opens the network, then signs in with the saved account or shows the sign-in page.
    void start();
    static QString effectiveLanguage(const QSettings &settings);

    /// Watches the application coming to the foreground (clears the panel notification).
    bool eventFilter(QObject *watched, QEvent *event);

public slots:
    void login(const QString &uin, const QString &password, bool remember);
    void signOut();
    void reconnect();
    void goOffline();
    void setStatus(int status);
    /// [{status, name, icon}] for the status picker.
    QVariantList statusChoices() const;
    void addContact(const QString &uin, const QString &nick, int groupId);
    void removeContact(const QString &uin);
    void renameContact(const QString &uin, const QString &nick);
    void addGroup(const QString &name);
    void renameGroup(int groupId, const QString &name);
    /// Asks the server for the user's profile; returns false (with a notice) when offline.
    bool showContactInfo(const QString &uin);
    void requestAuthorization(const QString &uin);
    void answerAuthorization(bool grant);
    void openUrl(const QString &url);
    void copyText(const QString &text);
    void clearNotice();
    /// Desktop testing: screenshots of the pages (KICQ_SHOT_DIR).
    void takeScreenshot(const QString &name);

signals:
    void stateChanged();
    void connectionChanged();
    void busyChanged();
    void loginErrorChanged();
    void settingsChanged();
    void myStatusChanged();
    void noticeChanged();
    void authRequestChanged();
    /// A message arrived for a chat that is not open (the list page may want to react).
    void messageArrived(const QString &uin);
    void logChanged();
    void infoChanged();

private slots:
    void onNetworkOpened();
    void onNetworkError();
    void onSessionState();
    void onSessionConnected();
    void onSessionDisconnected(const QString &reason, int loginError);
    void onMessage(const QString &uin, const QString &text, const QDateTime &when, bool offline);
    void onAuthRequested(const QString &uin, const QString &reason);
    void onAuthReplied(const QString &uin, bool granted);
    void onYouWereAdded(const QString &uin);
    void onSsiFinished(int request, const QString &name, bool ok, int code);
    void onUserInfo(const IcqUserInfo &info);
    void onSendFailed(const QString &error);
    void onReconnectTimer();
    void onSessionLog(const QString &line);
    void onShowOfflineChanged();

private:
    void setState(const QString &s);
    void setBusy(bool b);
    void setNotice(const QString &n);
    void continueStart();
    void connectSession();
    bool appInForeground() const;
    void openHistory(const QString &uin);

    QSettings m_settings;
    IcqSession *m_session;
    ContactsModel *m_contacts;
    MessagesModel *m_chat;
    HistoryStore *m_history;
    Notifier *m_notifier;
    QNetworkConfigurationManager *m_netMgr;
    QNetworkSession *m_netSession;
    QTimer *m_reconnect;
    QDeclarativeView *m_view;
    QString m_state;
    bool m_busy;
    QString m_loginError;
    QString m_notice;
    QString m_uin, m_password;
    bool m_wantOnline;
    bool m_everOnline;
    int m_reconnectDelay;
    QString m_authUin, m_authText;
    QString m_infoUin, m_infoNote;
    QVariantList m_infoRows;
    bool m_infoLoading;
};

#endif // APPCONTROLLER_H
