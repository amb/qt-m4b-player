import QtQuick
import "../../qml" as PlayerUi

Rectangle {
    id: fixture
    width: 96
    height: 96
    property color foreground: "white"
    property bool iconChecked: false
    color: "black"

    PlayerUi.TransportButton {
        anchors.centerIn: parent
        width: 56
        height: 56
        iconName: "play"
        description: "Play"
        hoverEnabled: false
        checked: fixture.iconChecked
        palette.buttonText: fixture.foreground
        palette.highlightedText: fixture.foreground
        palette.highlight: "#306090"
    }
}
