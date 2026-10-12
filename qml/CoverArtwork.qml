import QtQuick

Item {
    id: cover
    property url source
    property bool fill: true
    property real fittedHeight: height
    property bool playbackEnabled: false
    // Normalized framing survives resizing; a new book starts centered.
    property real verticalPosition: 0
    readonly property real imageScale: image.implicitWidth > 0 && image.implicitHeight > 0
        ? (fill ? Math.max(width / image.implicitWidth, height / image.implicitHeight)
                : Math.min(width / image.implicitWidth, fittedHeight / image.implicitHeight)) : 0
    readonly property real maxVerticalOffset: Math.max(0, (image.height - height) / 2)
    signal clicked()
    clip: true
    onSourceChanged: verticalPosition = 0
    onPlaybackEnabledChanged: if (!playbackEnabled) verticalPosition = 0

    Image {
        id: image
        objectName: "coverImage"
        source: cover.source
        asynchronous: true
        width: implicitWidth * cover.imageScale
        height: implicitHeight * cover.imageScale
        x: (cover.width - width) / 2
        y: cover.fill ? (cover.height - height) / 2 + cover.verticalPosition * cover.maxVerticalOffset
                      : cover.fittedHeight - height
    }
    MouseArea {
        id: gesture
        anchors.fill: parent
        enabled: cover.playbackEnabled
        property real pressX: 0
        property real pressY: 0
        property real initialPosition: 0
        property bool moved: false
        onPressed: mouse => {
            pressX = mouse.x
            pressY = mouse.y
            initialPosition = cover.verticalPosition
            moved = false
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            if (Math.hypot(mouse.x - pressX, mouse.y - pressY) >= 8) moved = true
            if (moved && cover.fill && cover.maxVerticalOffset > 0)
                cover.verticalPosition = Math.max(-1, Math.min(1,
                    initialPosition + (mouse.y - pressY) / cover.maxVerticalOffset))
        }
        onClicked: if (!moved) cover.clicked()
        onCanceled: cover.verticalPosition = initialPosition
        Accessible.name: "Artwork: play or pause"
        Accessible.description: cover.fill ? "Click to play or pause; drag vertically to reframe the cover" : "Click to play or pause"
    }
}
