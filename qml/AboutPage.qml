// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: page

    tools: ToolBarLayout {
        ToolButton { iconSource: "toolbar-back"; onClicked: pageStack.pop() }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.height + 2 * platformStyle.paddingLarge
        clip: true

        Column {
            id: column
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: platformStyle.paddingLarge }
            spacing: platformStyle.paddingMedium

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: platformStyle.paddingMedium
                Image { source: "qrc:/images/daisy_green.png"; anchors.verticalCenter: parent.verticalCenter }
                Label { text: "JasmineKICQ"; font.pixelSize: platformStyle.fontSizeLarge * 1.5; anchors.verticalCenter: parent.verticalCenter }
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("version %1").arg(app.version)
                color: platformStyle.colorNormalMid
            }
            Item { width: 1; height: platformStyle.paddingLarge }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTr("An ICQ client for the kicq.ru server, ported from Jasmine IM for Android to Symbian Anna/Belle.")
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: platformStyle.fontSizeSmall
                color: platformStyle.colorNormalMid
                text: qsTr("Statuses: green - online, yellow - away, green with a badge - busy, red - offline, white - unknown (not on the server list or not yet authorized). Messages to offline contacts are stored by the server only when they are pure Latin, or Cyrillic without digits and punctuation - a limitation of the server.")
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: platformStyle.fontSizeSmall
                color: platformStyle.colorNormalMid
                text: "GPL-2.0-or-later. Status daisies and X-status icons from Jasmine IM."
            }
            Item { width: 1; height: platformStyle.paddingLarge }
            Label { text: qsTr("Log"); font.bold: true }
            Label {
                width: parent.width
                wrapMode: Text.WrapAnywhere
                font.pixelSize: platformStyle.fontSizeSmall * 0.85
                font.family: "monospace"
                color: platformStyle.colorNormalMid
                text: app.logTail
            }
            Button {
                width: parent.width
                text: qsTr("Copy log")
                onClicked: app.copyText(app.logTail)
            }
        }
    }
}
