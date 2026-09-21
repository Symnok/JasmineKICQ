// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// The window: a page stack that follows app.state (starting -> login -> ready), the notice
// banner every page shares, and the authorization-request dialog that can pop up anywhere.
import QtQuick 1.1
import com.nokia.symbian 1.1
import com.nokia.extras 1.1

PageStackWindow {
    id: window
    showStatusBar: true
    showToolBar: true
    platformSoftwareInputPanelEnabled: true
    initialPage: startPage

    Page {
        id: startPage
        BusyIndicator {
            anchors.centerIn: parent
            running: true
            width: platformStyle.graphicSizeLarge
            height: platformStyle.graphicSizeLarge
        }
        Label {
            anchors { top: parent.top; topMargin: parent.height / 4; horizontalCenter: parent.horizontalCenter }
            text: "JasmineKICQ"
            font.pixelSize: platformStyle.fontSizeLarge * 1.5
        }
        tools: ToolBarLayout {
            ToolButton { iconSource: "toolbar-back"; onClicked: Qt.quit() }
        }
    }

    Component { id: loginPage; LoginPage {} }
    Component { id: contactsPage; ContactsPage {} }
    Component { id: chatPage; ChatPage {} }

    function route() {
        if (app.state == "login") {
            pageStack.clear()
            pageStack.push(loginPage)
        } else if (app.state == "ready") {
            if (pageStack.depth == 0 || pageStack.currentPage != contactsPage) {
                pageStack.clear()
                pageStack.push(contactsPage)
            }
        }
    }

    function openChat(uin) {
        app.chat.open(uin)
        pageStack.push(chatPage)
    }

    Connections {
        target: app
        onStateChanged: route()
        onNoticeChanged: {
            if (app.notice != "") {
                banner.text = app.notice
                banner.open()
            }
        }
        onAuthRequestChanged: {
            if (app.authRequestUin != "") authDialog.open()
        }
    }

    Component.onCompleted: route()

    // Desktop testing (KICQ_SHOT_DIR): screenshots of the pages once online.
    Timer {
        id: autotest
        property int step: 0
        interval: 1500
        repeat: true
        running: app.autotest
        onTriggered: {
            if (step == 0 && app.state == "login") { app.takeScreenshot("login"); return }
            if (app.connection != "online") return
            step++
            if (step == 1) app.takeScreenshot("contacts")
            else if (step == 2) { if (app.contacts.count > 0) window.openChat(app.contacts.get(0).uin) }
            else if (step == 3) app.takeScreenshot("chat")
            else if (step == 4) { app.chat.send("Autotest: привет от JasmineKICQ") }
            else if (step == 5) app.takeScreenshot("chat-sent")
            else if (step == 6) { app.chat.close(); pageStack.pop(); statusDialogProbe() }
            else if (step == 7) app.takeScreenshot("contacts-again")
            else if (step == 8) Qt.quit()
        }
        function statusDialogProbe() { }
    }

    InfoBanner {
        id: banner
        timeout: 4000
        onClicked: app.clearNotice()
    }

    QueryDialog {
        id: authDialog
        titleText: qsTr("Authorization request")
        message: app.authRequestText
        acceptButtonText: qsTr("Authorize")
        rejectButtonText: qsTr("Decline")
        onAccepted: app.answerAuthorization(true)
        onRejected: app.answerAuthorization(false)
    }
}
