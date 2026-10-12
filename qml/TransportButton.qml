import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string description: ""
    property string iconName: ""
    implicitWidth: 48
    implicitHeight: 48
    font.pixelSize: 20
    Accessible.name: description
    ToolTip.visible: hovered
    ToolTip.text: description
    background: Rectangle {
        radius: 12
        color: control.down ? "#5b5b64" : control.checked ? "#e1e1e5" : control.hovered ? "#4b4b53" : "transparent"
        border.color: control.activeFocus ? "#b5dbe4" : "transparent"
        border.width: 2
    }
    contentItem: Item {
        TransportIcon {
            anchors.centerIn: parent
            width: 28
            height: 28
            visible: control.iconName.length > 0
            name: control.iconName
            dark: control.checked
            opacity: control.enabled ? 1 : 0.35
        }
        Text {
            anchors.fill: parent
            visible: control.iconName.length === 0
            text: control.text
            font: control.font
            color: !control.enabled ? "#686870" : control.checked ? "#25252a" : "#ededf0"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
