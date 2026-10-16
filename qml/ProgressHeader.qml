import QtQuick
import QtQuick.Controls

Item {
    id: header
    property string chapterText: ""
    property string positionText: ""
    property string remainingText: ""
    property color textColor: "white"
    readonly property real spacing: 8
    // Measure full labels before falling back, matching the reference layout.
    readonly property bool wide: width >= position.implicitWidth
        + 2 * (Math.max(chapter.implicitWidth, remaining.implicitWidth) + spacing)
    readonly property real sideWidth: Math.max(0, (width - position.implicitWidth) / 2 - spacing)
    implicitHeight: wide ? Math.max(chapter.implicitHeight, position.implicitHeight, remaining.implicitHeight)
        : chapter.implicitHeight + 4 + Math.max(position.implicitHeight, remaining.implicitHeight)

    Label {
        id: chapter
        objectName: "chapterReadout"
        text: header.chapterText
        width: header.wide ? header.sideWidth : header.width
        y: header.wide ? (header.height - height) / 2 : 0
        color: header.textColor
        font.pixelSize: 16
        font.bold: true
        elide: Text.ElideRight
    }
    Label {
        id: position
        objectName: "positionReadout"
        text: header.positionText
        // Only the text's natural width is reserved at the exact midpoint.
        width: implicitWidth
        x: header.wide ? (header.width - width) / 2 : 0
        y: header.wide ? (header.height - height) / 2 : chapter.height + 4
        color: header.textColor
        opacity: 0.75
        font.pixelSize: 14
    }
    Label {
        id: remaining
        objectName: "remainingReadout"
        text: header.remainingText
        width: header.wide ? header.sideWidth : Math.max(0, header.width - position.width - header.spacing)
        x: header.width - width
        y: header.wide ? (header.height - height) / 2 : chapter.height + 4
        color: header.textColor
        opacity: 0.75
        font.pixelSize: 14
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
    }
}
