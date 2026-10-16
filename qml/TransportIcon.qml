import QtQuick

// Recolours a vendored Phosphor glyph to the current theme colour so the icon
// follows the platform (KDE) palette on both software and GPU renderers.
Item {
    id: icon
    property string name: ""
    property color color: "white"

    Image {
        id: image
        objectName: "transportIconImage"
        anchors.fill: parent
        source: icon.name ? "image://transport/" + encodeURIComponent(icon.name)
            + "/" + encodeURIComponent(icon.color.toString()) : ""
        sourceSize.width: width * Screen.devicePixelRatio
        sourceSize.height: height * Screen.devicePixelRatio
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
