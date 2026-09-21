// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// One conversation: the contact's daisy and status in the header, messages oldest at the
// top, and the composer. Long-press a bubble to copy or retry.
import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: page
    property variant chat: app.chat

    tools: ToolBarLayout {
        ToolButton { iconSource: "toolbar-back"; onClicked: { chat.close(); pageStack.pop() } }
        ToolButton { iconSource: "toolbar-menu"; onClicked: menu.open() }
    }

    Menu {
        id: menu
        MenuLayout {
            MenuItem {
                text: qsTr("Add to contact list")
                visible: chat.peerTemporary
                onClicked: app.addContact(chat.uin, chat.title, 0)
            }
            MenuItem { text: qsTr("Copy UIN"); onClicked: app.copyText(chat.uin) }
            MenuItem { text: qsTr("Clear history"); onClicked: clearDialog.open() }
        }
    }

    QueryDialog {
        id: clearDialog
        titleText: qsTr("Clear history")
        message: qsTr("Delete all messages of this chat from the phone?")
        acceptButtonText: qsTr("Delete")
        rejectButtonText: qsTr("Cancel")
        onAccepted: chat.clearHistory()
    }

    ContextMenu {
        id: contextMenu
        property int row: -1
        property variant item
        MenuLayout {
            MenuItem {
                text: qsTr("Copy text")
                onClicked: app.copyText(contextMenu.item.body)
            }
            MenuItem {
                text: qsTr("Retry")
                visible: contextMenu.item ? contextMenu.item.failed : false
                onClicked: chat.retry(contextMenu.row)
            }
        }
    }

    // -- header --
    Rectangle {
        id: heading
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: platformStyle.graphicSizeMedium + 2 * platformStyle.paddingMedium
        color: "#1c2a3a"
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: "#3d5a80" }
        Image {
            id: peerDaisy
            source: chat.peerStatusIcon
            anchors { left: parent.left; leftMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
        }
        Column {
            anchors { left: peerDaisy.right; leftMargin: platformStyle.paddingLarge; right: parent.right; rightMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
            Label { width: parent.width; text: chat.title; elide: Text.ElideRight; font.bold: true }
            Label {
                width: parent.width
                font.pixelSize: platformStyle.fontSizeSmall
                text: chat.title != chat.uin && chat.peerSubtitle != "" ? chat.uin + "  ·  " + chat.peerSubtitle
                    : (chat.title != chat.uin ? chat.uin : chat.peerSubtitle)
                elide: Text.ElideRight
                color: chat.peerTyping ? "#8fd18f" : platformStyle.colorNormalMid
            }
        }
    }

    // -- messages --
    ListView {
        id: list
        anchors { top: heading.bottom; left: parent.left; right: parent.right; bottom: composerRow.top }
        model: chat
        clip: true
        spacing: platformStyle.paddingSmall
        cacheBuffer: 600

        delegate: MessageDelegate {
            width: list.width
            onPressAndHold: {
                contextMenu.row = index
                contextMenu.item = chat.get(index)
                contextMenu.open()
            }
        }

        ScrollDecorator { flickableItem: list }

        function scrollToEnd() {
            if (count > 0) positionViewAtEnd()
        }
        Component.onCompleted: scrollToEnd()
    }

    Connections {
        target: chat
        onMessageAppended: list.scrollToEnd()
        onChatChanged: list.scrollToEnd()
    }

    Label {
        anchors.centerIn: list
        width: parent.width - 2 * platformStyle.paddingLarge
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: platformStyle.colorNormalMid
        visible: list.count == 0
        text: qsTr("No messages yet.")
    }

    // -- composer --
    Item {
        id: composerRow
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: composer.height + 2 * platformStyle.paddingSmall

        TextArea {
            id: composer
            anchors {
                left: parent.left; leftMargin: platformStyle.paddingSmall
                right: sendButton.left; rightMargin: platformStyle.paddingSmall
                verticalCenter: parent.verticalCenter
            }
            placeholderText: qsTr("message")
            wrapMode: TextEdit.Wrap
            platformMaxImplicitHeight: 120
            onTextChanged: chat.composing(text)
        }
        Button {
            id: sendButton
            anchors { right: parent.right; rightMargin: platformStyle.paddingSmall; verticalCenter: parent.verticalCenter }
            width: Math.max(80, implicitWidth)
            text: qsTr("Send")
            enabled: composer.text.length > 0 && chat.uin != "" && app.connection == "online"
            onClicked: {
                chat.send(composer.text)
                composer.text = ""
            }
        }
    }
}
