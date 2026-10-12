import QtQuick

Item {
    id: icon
    property string name: ""
    property bool dark: false

    Image {
        anchors.fill: parent
        source: icon.name ? "qrc:/icons/" + icon.name + (icon.dark ? "-dark.svg" : "-light.svg") : ""
        sourceSize.width: width * Screen.devicePixelRatio
        sourceSize.height: height * Screen.devicePixelRatio
    }
    Text {
        anchors.centerIn: parent
        visible: icon.name === "back" || icon.name === "forward"
        text: "10"
        color: icon.dark ? "#25252a" : "#ededf0"
        font.pixelSize: 10
        font.bold: true
    }
}
