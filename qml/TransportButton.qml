import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string description: ""
    property string iconName: ""
    // Marks a button drawn on the highlight colour, e.g. the primary action.
    property bool emphasized: false
    property bool popupOpen: false
    readonly property color foregroundColor: !control.enabled ? control.palette.placeholderText
        : (control.checked || control.emphasized || control.popupOpen) ? control.palette.highlightedText
        : control.palette.buttonText
    implicitWidth: 48
    implicitHeight: 48
    font.pixelSize: 20
    Accessible.name: description
    ToolTip.visible: hovered
    ToolTip.text: description
    background: Rectangle {
        radius: 12
        color: control.down ? Qt.darker(control.palette.button, 1.15)
            : (control.checked || control.popupOpen) ? control.palette.highlight
            : control.hovered ? control.palette.button
            : "transparent"
        border.color: control.activeFocus ? control.palette.highlight : "transparent"
        border.width: 2
    }
    contentItem: Item {
        TransportIcon {
            anchors.centerIn: parent
            width: 28
            height: 28
            visible: control.iconName.length > 0
            name: control.iconName
            color: control.foregroundColor
            opacity: control.enabled ? 1 : 0.5
        }
        Text {
            anchors.fill: parent
            visible: control.iconName.length === 0
            text: control.text
            font: control.font
            color: control.foregroundColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
