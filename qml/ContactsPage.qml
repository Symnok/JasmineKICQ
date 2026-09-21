// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// The contact list: my status in the header, the roster grouped as on the server, a
// daisy per contact (green online, yellow away, red busy, white offline), unread badges.
// Tap opens the chat; long-press for profile / rename / delete / authorization. Tap a group
// heading to rename the group.
import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: page

    Component { id: settingsPage; SettingsPage {} }
    Component { id: aboutPage; AboutPage {} }
    Component { id: infoPage; ContactInfoPage {} }

    tools: ToolBarLayout {
        ToolButton { iconSource: "toolbar-back"; onClicked: Qt.quit() }
        ToolButton { iconSource: "toolbar-add"; onClicked: addDialog.open() }
        ToolButton {
            iconSource: "toolbar-settings"
            onClicked: statusDialog.open()
        }
        ToolButton { iconSource: "toolbar-menu"; onClicked: menu.open() }
    }

    Menu {
        id: menu
        MenuLayout {
            MenuItem {
                text: app.contacts.showOffline ? qsTr("Hide offline contacts") : qsTr("Show offline contacts")
                onClicked: app.contacts.showOffline = !app.contacts.showOffline
            }
            MenuItem { text: qsTr("New group"); onClicked: groupDialog.open() }
            MenuItem { text: qsTr("My profile"); onClicked: if (app.showContactInfo(app.myUin)) pageStack.push(infoPage) }
            MenuItem {
                text: app.connection == "offline" ? qsTr("Connect") : qsTr("Disconnect")
                onClicked: app.connection == "offline" ? app.reconnect() : app.goOffline()
            }
            MenuItem { text: qsTr("Settings"); onClicked: pageStack.push(settingsPage) }
            MenuItem { text: qsTr("About"); onClicked: pageStack.push(aboutPage) }
            MenuItem { text: qsTr("Sign out"); onClicked: signOutDialog.open() }
        }
    }

    // -- dialogs --
    QueryDialog {
        id: signOutDialog
        titleText: qsTr("Sign out")
        message: qsTr("Sign out? The saved password will be removed from this phone.")
        acceptButtonText: qsTr("Sign out")
        rejectButtonText: qsTr("Cancel")
        onAccepted: app.signOut()
    }

    SelectionDialog {
        id: statusDialog
        titleText: qsTr("My status")
        model: ListModel { id: statusModel }
        // Own rows (the stock delegate expects a string list, and the theme's dialog text is dim).
        delegate: Item {
            width: parent ? parent.width : 300
            height: (typeof privateStyle != "undefined") ? privateStyle.menuItemHeight : 56
            Rectangle { anchors.fill: parent; color: statusMouse.pressed ? "#3d5a80" : "transparent" }
            Row {
                anchors { left: parent.left; leftMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
                spacing: platformStyle.paddingMedium
                Image { source: model.icon; anchors.verticalCenter: parent.verticalCenter }
                Label {
                    text: model.name + (model.status == app.myStatus ? "   *" : "")
                    color: "white"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                id: statusMouse
                anchors.fill: parent
                onClicked: { statusDialog.selectedIndex = index; statusDialog.accept() }
            }
        }
        onAccepted: if (selectedIndex >= 0) app.setStatus(statusModel.get(selectedIndex).status)
        Component.onCompleted: {
            var choices = app.statusChoices()
            for (var i = 0; i < choices.length; ++i) statusModel.append(choices[i])
        }
    }

    CommonDialog {
        id: addDialog
        titleText: qsTr("Add contact")
        buttonTexts: [qsTr("Add"), qsTr("Cancel")]
        property int groupId: 0
        content: Column {
            width: parent.width
            spacing: platformStyle.paddingMedium
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            Label { text: qsTr("UIN"); font.pixelSize: platformStyle.fontSizeSmall }
            TextField { id: addUin; width: parent.width; inputMethodHints: Qt.ImhDigitsOnly }
            Label { text: qsTr("nickname (optional)"); font.pixelSize: platformStyle.fontSizeSmall }
            TextField { id: addNick; width: parent.width }
            Label { text: qsTr("group"); font.pixelSize: platformStyle.fontSizeSmall }
            Button {
                id: groupButton
                width: parent.width
                text: qsTr("choose...")
                onClicked: groupPicker.open()
            }
        }
        onButtonClicked: {
            if (index == 0) {
                app.addContact(addUin.text, addNick.text, addDialog.groupId)
                addUin.text = ""
                addNick.text = ""
            }
        }
        onStatusChanged: {
            if (status == DialogStatus.Opening) {
                // pick the first real group by default
                var g = app.contacts.groups()
                if (g.length > 0 && addDialog.groupId == 0) { addDialog.groupId = g[0].id; groupButton.text = g[0].name }
            }
        }
    }

    SelectionDialog {
        id: groupPicker
        titleText: qsTr("Group")
        model: ListModel { id: groupModel }
        delegate: Item {
            width: parent ? parent.width : 300
            height: (typeof privateStyle != "undefined") ? privateStyle.menuItemHeight : 56
            Rectangle { anchors.fill: parent; color: groupMouse.pressed ? "#3d5a80" : "transparent" }
            Label {
                anchors { left: parent.left; leftMargin: platformStyle.paddingLarge; right: parent.right; verticalCenter: parent.verticalCenter }
                text: model.name
                color: "white"
                elide: Text.ElideRight
            }
            MouseArea {
                id: groupMouse
                anchors.fill: parent
                onClicked: { groupPicker.selectedIndex = index; groupPicker.accept() }
            }
        }
        onStatusChanged: {
            if (status == DialogStatus.Opening) {
                groupModel.clear()
                var g = app.contacts.groups()
                for (var i = 0; i < g.length; ++i) groupModel.append(g[i])
            }
        }
        onAccepted: {
            if (selectedIndex >= 0) {
                addDialog.groupId = groupModel.get(selectedIndex).id
                groupButton.text = groupModel.get(selectedIndex).name
            }
        }
    }

    CommonDialog {
        id: groupDialog
        titleText: qsTr("New group")
        buttonTexts: [qsTr("Create"), qsTr("Cancel")]
        content: Column {
            width: parent.width
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            TextField { id: groupName; width: parent.width; placeholderText: qsTr("group name") }
        }
        onButtonClicked: if (index == 0) { app.addGroup(groupName.text); groupName.text = "" }
    }

    CommonDialog {
        id: renameGroupDialog
        property int groupId
        titleText: qsTr("Rename group")
        buttonTexts: [qsTr("Rename"), qsTr("Cancel")]
        content: Column {
            width: parent.width
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            TextField { id: renameGroupField; width: parent.width }
        }
        onButtonClicked: if (index == 0) app.renameGroup(renameGroupDialog.groupId, renameGroupField.text)
    }

    // tap on a group heading
    ContextMenu {
        id: groupMenu
        property int groupId
        property string name
        MenuLayout {
            MenuItem {
                text: qsTr("Rename group")
                onClicked: { renameGroupDialog.groupId = groupMenu.groupId; renameGroupField.text = groupMenu.name; renameGroupDialog.open() }
            }
        }
    }

    function openGroupMenu(name) {
        var g = app.contacts.groups()
        for (var i = 0; i < g.length; ++i)
            if (g[i].name == name) { groupMenu.groupId = g[i].id; groupMenu.name = name; groupMenu.open(); return }
    }

    CommonDialog {
        id: renameDialog
        property string uin
        titleText: qsTr("Rename contact")
        buttonTexts: [qsTr("Rename"), qsTr("Cancel")]
        content: Column {
            width: parent.width
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            TextField { id: renameField; width: parent.width }
        }
        onButtonClicked: if (index == 0) app.renameContact(renameDialog.uin, renameField.text)
    }

    QueryDialog {
        id: deleteDialog
        property string uin
        property string nick
        titleText: qsTr("Remove contact")
        message: qsTr("Remove \"%1\" from the contact list on the server?").arg(nick)
        acceptButtonText: qsTr("Remove")
        rejectButtonText: qsTr("Cancel")
        onAccepted: app.removeContact(uin)
    }

    ContextMenu {
        id: contextMenu
        property variant item
        MenuLayout {
            MenuItem {
                text: qsTr("Contact info")
                onClicked: if (app.showContactInfo(contextMenu.item.uin)) pageStack.push(infoPage)
            }
            MenuItem {
                text: qsTr("Rename")
                onClicked: { renameDialog.uin = contextMenu.item.uin; renameField.text = contextMenu.item.nick; renameDialog.open() }
            }
            MenuItem {
                text: qsTr("Request authorization")
                visible: contextMenu.item ? !contextMenu.item.authorized : false
                onClicked: app.requestAuthorization(contextMenu.item.uin)
            }
            MenuItem {
                text: qsTr("Add to contact list")
                visible: contextMenu.item ? contextMenu.item.temporary : false
                onClicked: { addUin.text = contextMenu.item.uin; addNick.text = contextMenu.item.nick; addDialog.open() }
            }
            MenuItem {
                text: qsTr("Copy UIN")
                onClicked: app.copyText(contextMenu.item.uin)
            }
            MenuItem {
                text: qsTr("Remove")
                onClicked: { deleteDialog.uin = contextMenu.item.uin; deleteDialog.nick = contextMenu.item.nick; deleteDialog.open() }
            }
        }
    }

    // -- header: my status (tap to change it) --
    Rectangle {
        id: heading
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: platformStyle.graphicSizeMedium + 2 * platformStyle.paddingMedium
        color: "#1c2a3a"
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: "#3d5a80" }
        Image {
            id: myDaisy
            source: app.myStatusIcon
            anchors { left: parent.left; leftMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
        }
        Column {
            anchors { left: myDaisy.right; leftMargin: platformStyle.paddingLarge; right: busy.left; verticalCenter: parent.verticalCenter }
            Label { width: parent.width; text: app.myUin; elide: Text.ElideRight; font.bold: true }
            Label {
                width: parent.width
                font.pixelSize: platformStyle.fontSizeSmall
                color: app.connection == "online" ? "#8fd18f" : platformStyle.colorNormalMid
                text: app.connection == "online" ? app.myStatusText
                    : (app.connection == "connecting" ? qsTr("connecting...") : qsTr("offline"))
                elide: Text.ElideRight
            }
        }
        BusyIndicator {
            id: busy
            anchors { right: parent.right; rightMargin: platformStyle.paddingLarge; verticalCenter: parent.verticalCenter }
            running: app.connection == "connecting"
            visible: running
            width: visible ? platformStyle.graphicSizeSmall : 0
        }
        MouseArea { anchors.fill: parent; onClicked: statusDialog.open() }
    }

    // -- list --
    ListView {
        id: list
        anchors { top: heading.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        model: app.contacts
        clip: true
        cacheBuffer: 400
        section.property: "groupName"
        section.criteria: ViewSection.FullString
        section.delegate: ListHeading {
            width: list.width
            ListItemText { anchors.fill: parent.paddingItem; role: "Heading"; text: section; horizontalAlignment: Text.AlignLeft }
            MouseArea { anchors.fill: parent; onClicked: page.openGroupMenu(section) }
        }

        delegate: ListItem {
            id: item
            subItemIndicator: false
            height: platformStyle.graphicSizeMedium + 2 * platformStyle.paddingLarge

            Image {
                id: daisy
                anchors { left: item.paddingItem.left; verticalCenter: parent.verticalCenter }
                source: model.statusIcon
            }
            Image {
                anchors { left: daisy.right; leftMargin: -8; bottom: daisy.bottom; bottomMargin: -4 }
                source: model.xstatusIcon
                visible: model.xstatusIcon != ""
                width: 16; height: 16
                smooth: true
            }
            Column {
                anchors {
                    left: daisy.right; leftMargin: platformStyle.paddingLarge
                    right: badge.visible ? badge.left : item.paddingItem.right; rightMargin: platformStyle.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                ListItemText {
                    width: parent.width
                    role: "Title"
                    text: model.nick
                    elide: Text.ElideRight
                    color: model.online ? platformStyle.colorNormalLight : platformStyle.colorNormalMid
                }
                ListItemText {
                    width: parent.width
                    role: "SubTitle"
                    // the UIN, then what is going on: typing / away text / last message / status
                    text: model.nick != model.uin && model.subtitle != "" ? model.uin + "  ·  " + model.subtitle
                        : (model.nick != model.uin ? model.uin : model.subtitle)
                    elide: Text.ElideRight
                    visible: text != ""
                    color: model.typing ? "#8fd18f" : platformStyle.colorNormalMid
                }
            }
            Rectangle {
                id: badge
                anchors { right: item.paddingItem.right; verticalCenter: parent.verticalCenter }
                width: Math.max(badgeLabel.width + 12, 24)
                height: 24
                radius: 12
                color: "#e8722a"
                visible: model.unread > 0
                Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: model.unread
                    font.pixelSize: platformStyle.fontSizeSmall
                    color: "white"
                }
            }

            onClicked: window.openChat(model.uin)
            onPressAndHold: {
                contextMenu.item = app.contacts.get(index)
                contextMenu.open()
            }
        }

        ScrollDecorator { flickableItem: list }
    }

    Label {
        anchors.centerIn: list
        width: parent.width - 2 * platformStyle.paddingLarge
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: platformStyle.colorNormalMid
        visible: list.count == 0
        text: app.connection == "online"
              ? (app.contacts.showOffline ? qsTr("The contact list is empty. Tap + to add someone.") : qsTr("Nobody is online."))
              : (app.connection == "connecting" ? qsTr("Connecting...") : qsTr("Offline. Use the menu to connect."))
    }
}
