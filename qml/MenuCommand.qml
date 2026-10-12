import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: command
    property string keyHint: action && typeof action.shortcut === "string" ? action.shortcut : ""
    implicitWidth: Math.max(200, label.implicitWidth + hint.implicitWidth + 64)
    contentItem: RowLayout {
        spacing: 24
        Label {
            id: label
            Layout.leftMargin: command.checkable && command.indicator ? command.indicator.width + command.spacing : 0
            Layout.fillWidth: true
            text: command.text.replace(/&/g, "")
            font: command.font
            color: command.palette.windowText
        }
        Label {
            id: hint
            text: command.keyHint
            font: command.font
            color: command.palette.windowText
            opacity: 0.7
        }
    }
}
