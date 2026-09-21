// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: page

    tools: ToolBarLayout {
        ToolButton { iconSource: "toolbar-back"; onClicked: Qt.quit() }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 2 * platformStyle.paddingLarge
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: column
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: platformStyle.paddingLarge }
            spacing: platformStyle.paddingMedium

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: platformStyle.paddingMedium
                Image { source: "qrc:/images/daisy_green.png"; anchors.verticalCenter: parent.verticalCenter }
                Label {
                    text: "JasmineKICQ"
                    font.pixelSize: platformStyle.fontSizeLarge * 1.5
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Label {
                text: qsTr("sign in to ICQ (kicq.ru)")
                color: platformStyle.colorNormalMid
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Item { width: 1; height: platformStyle.paddingLarge }

            Label { text: qsTr("UIN"); font.pixelSize: platformStyle.fontSizeSmall }
            TextField {
                id: uinField
                width: parent.width
                text: app.savedUin
                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
                placeholderText: qsTr("UIN")
                enabled: !app.busy
            }
            Label { text: qsTr("password"); font.pixelSize: platformStyle.fontSizeSmall }
            TextField {
                id: passwordField
                width: parent.width
                text: app.savedPassword
                echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("password")
                enabled: !app.busy
                Keys.onReturnPressed: signIn()
                Keys.onEnterPressed: signIn()
            }
            CheckBox { id: showPassword; text: qsTr("show password") }
            CheckBox { id: rememberBox; text: qsTr("sign in automatically"); checked: app.autoConnect }

            Row {
                width: parent.width
                spacing: platformStyle.paddingMedium
                Column {
                    width: parent.width * 0.65
                    Label { text: qsTr("server"); font.pixelSize: platformStyle.fontSizeSmall }
                    TextField {
                        id: serverField
                        width: parent.width
                        text: app.server
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhUrlCharactersOnly
                        enabled: !app.busy
                    }
                }
                Column {
                    width: parent.width * 0.35 - platformStyle.paddingMedium
                    Label { text: qsTr("port"); font.pixelSize: platformStyle.fontSizeSmall }
                    TextField {
                        id: portField
                        width: parent.width
                        text: app.port
                        inputMethodHints: Qt.ImhDigitsOnly
                        enabled: !app.busy
                    }
                }
            }

            Button {
                text: app.busy ? qsTr("signing in...") : qsTr("sign in")
                width: parent.width
                enabled: !app.busy
                onClicked: signIn()
            }

            Label {
                width: parent.width
                wrapMode: Text.Wrap
                text: app.loginError
                visible: text != ""
                color: "#ff6b6b"
                font.pixelSize: platformStyle.fontSizeSmall
            }

            Item { width: 1; height: platformStyle.paddingLarge }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: platformStyle.fontSizeSmall
                color: platformStyle.colorNormalMid
                text: qsTr("The password travels in the classic ICQ form (obscured, not encrypted) and only its first 8 characters count - that is how this server works.")
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: app.busy
        visible: app.busy
        width: platformStyle.graphicSizeLarge
        height: platformStyle.graphicSizeLarge
    }

    function signIn() {
        passwordField.closeSoftwareInputPanel()
        app.server = serverField.text
        app.port = parseInt(portField.text)
        app.login(uinField.text, passwordField.text, rememberBox.checked)
    }
}
