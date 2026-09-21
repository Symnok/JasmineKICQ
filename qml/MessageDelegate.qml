// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
//
// One message bubble: date separator, text, time and delivery mark. Outgoing on the
// right, incoming on the left.
import QtQuick 1.1
import com.nokia.symbian 1.1

Item {
    id: root
    signal pressAndHold

    property int maxBubbleWidth: width * 0.8

    height: column.height + platformStyle.paddingSmall

    Column {
        id: column
        anchors { left: parent.left; right: parent.right }
        spacing: platformStyle.paddingSmall

        Item {
            width: parent.width
            height: model.showDate ? dateLabel.height + platformStyle.paddingMedium : 0
            visible: model.showDate
            Label {
                id: dateLabel
                anchors.centerIn: parent
                text: model.dateText
                font.pixelSize: platformStyle.fontSizeSmall
                color: platformStyle.colorNormalMid
            }
        }

        Rectangle {
            id: bubble
            property real innerWidth: Math.max(bodyLabel.paintedWidth, timeRow.width)
            width: Math.min(maxBubbleWidth, innerWidth + 2 * platformStyle.paddingMedium)
            height: bodyLabel.height + timeRow.height + 2 * platformStyle.paddingMedium + platformStyle.paddingSmall
            radius: 8
            color: model.failed ? "#6b2b2b" : (model.out ? "#1f5e8a" : "#3a3a3a")
            opacity: model.pending ? 0.6 : 1
            anchors { right: model.out ? parent.right : undefined; left: model.out ? undefined : parent.left; margins: platformStyle.paddingMedium }

            MouseArea {
                anchors.fill: parent
                onPressAndHold: root.pressAndHold()
            }

            Label {
                id: bodyLabel
                anchors { left: parent.left; top: parent.top; margins: platformStyle.paddingMedium }
                width: maxBubbleWidth - 2 * platformStyle.paddingMedium
                text: model.body
                wrapMode: Text.Wrap
                color: "white"
            }

            Row {
                id: timeRow
                anchors { right: parent.right; bottom: parent.bottom; margins: platformStyle.paddingSmall }
                spacing: platformStyle.paddingSmall
                Label {
                    text: (model.offline ? qsTr("offline") + ", " : "") + model.timeText
                    font.pixelSize: platformStyle.fontSizeSmall * 0.85
                    color: "#c0c0c0"
                }
                Label {
                    visible: model.out
                    text: model.failed ? "!" : (model.pending ? "..." : "✓")
                    font.pixelSize: platformStyle.fontSizeSmall * 0.85
                    color: model.failed ? "#ff9b9b" : "#c0c0c0"
                }
            }
        }
    }
}
