// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// A user's profile as the server has it (app.showContactInfo fills app.infoRows).
// Long-press a value to copy it.
import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: page

    tools: ToolBarLayout {
        ToolButton { iconSource: "toolbar-back"; onClicked: pageStack.pop() }
        ToolButton { iconSource: "toolbar-refresh"; onClicked: app.showContactInfo(app.infoUin) }
    }

    // -- header --
    Rectangle {
        id: heading
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: platformStyle.graphicSizeMedium + 2 * platformStyle.paddingMedium
        color: "#1c2a3a"
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: "#3d5a80" }
        Image {
            id: daisy
            source: app.infoIcon
            anchors { left: parent.left; leftMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
        }
        Column {
            anchors { left: daisy.right; leftMargin: platformStyle.paddingLarge; right: parent.right; rightMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
            Label { width: parent.width; text: app.infoTitle; elide: Text.ElideRight; font.bold: true }
            Label {
                width: parent.width
                font.pixelSize: platformStyle.fontSizeSmall
                text: app.infoTitle != app.infoUin ? app.infoUin : qsTr("profile")
                elide: Text.ElideRight
                color: platformStyle.colorNormalMid
            }
        }
    }

    ListView {
        id: list
        anchors { top: heading.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        model: app.infoRows
        clip: true
        delegate: ListItem {
            id: row
            subItemIndicator: false
            height: valueText.height + labelText.height + 2 * platformStyle.paddingLarge
            Column {
                anchors { left: row.paddingItem.left; right: row.paddingItem.right; verticalCenter: parent.verticalCenter }
                ListItemText {
                    id: labelText
                    width: parent.width
                    role: "SubTitle"
                    text: modelData.label
                    color: platformStyle.colorNormalMid
                }
                ListItemText {
                    id: valueText
                    width: parent.width
                    role: "Title"
                    text: modelData.value
                    wrapMode: Text.Wrap
                    color: platformStyle.colorNormalLight
                }
            }
            onPressAndHold: app.copyText(modelData.value)
        }
        footer: Item {
            width: list.width
            height: note.visible ? note.height + 2 * platformStyle.paddingLarge : 0
            Label {
                id: note
                anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: platformStyle.colorNormalMid
                visible: !app.infoLoading && app.infoNote != ""
                text: app.infoNote
            }
        }
        ScrollDecorator { flickableItem: list }
    }

    BusyIndicator {
        anchors.centerIn: list
        running: app.infoLoading
        visible: running
        width: platformStyle.graphicSizeLarge
        height: platformStyle.graphicSizeLarge
    }

}
