import QtQuick
import QtQuick.Effects

// Recolours a vendored Phosphor glyph to the current theme colour so the icon
// follows the platform (KDE) palette instead of shipping a fixed tint.
Item {
    id: icon
    property string name: ""
    property color color: "white"

    Image {
        id: image
        objectName: "transportIconImage"
        anchors.fill: parent
        visible: false
        source: icon.name ? "qrc:/icons/" + icon.name + "-light.svg" : ""
        sourceSize.width: width * Screen.devicePixelRatio
        sourceSize.height: height * Screen.devicePixelRatio
    }
    MultiEffect {
        anchors.fill: parent
        source: image
        visible: icon.name.length > 0
        colorization: 1.0
        colorizationColor: icon.color
    }
    Text {
        anchors.centerIn: parent
        visible: icon.name === "back" || icon.name === "forward"
        text: "10"
        color: icon.color
        font.pixelSize: 10
        font.bold: true
    }
}
