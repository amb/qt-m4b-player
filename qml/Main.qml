pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AudiobookPlayer

ApplicationWindow {
    id: window
    required property PlayerController player
    required property AppPreferences preferences
    readonly property bool wideControls: width >= 740
    readonly property var playbackRates: [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3]
    // Follow the platform colour scheme; the application ships no palette of its own.
    color: palette.window
    width: 520
    height: 800
    minimumWidth: 390
    minimumHeight: 600
    // C++ restores the saved size/state before showing the window.
    visible: false
    title: window.player.loaded ? window.player.title + (window.player.author ? " — " + window.player.author : "") : "qt-m4b-player"
    onClosing: { notesEditor.close(); equalizerWindow.close() }

    NotesEditor {
        id: notesEditor
        player: window.player
        transientParent: window
        palette: window.palette
    }
    EqualizerWindow {
        id: equalizerWindow
        player: window.player
        transientParent: window
        palette: window.palette
    }

    function formatTime(seconds: real): string {
        let value = Math.max(0, Math.floor(seconds))
        let hours = Math.floor(value / 3600)
        let minutes = Math.floor((value % 3600) / 60)
        let secs = value % 60
        return (hours ? hours + ":" + String(minutes).padStart(2, "0") : minutes) + ":" + String(secs).padStart(2, "0")
    }

    FileDialog {
        id: fileDialog
        title: "Open audiobook"
        nameFilters: ["Audiobooks (*.m4b *.m4a *.mp4)", "All files (*)"]
        onAccepted: window.player.openBook(selectedFile)
    }
    Action {
        id: openAction
        text: "&Open Audiobook…"
        shortcut: StandardKey.Open
        onTriggered: fileDialog.open()
    }
    Action {
        id: playAction
        text: window.player.playing ? "&Pause" : "&Play"
        shortcut: "Space"
        enabled: window.player.loaded && !notesEditor.visible && !ratingPopup.visible
        onTriggered: window.player.togglePlayback()
    }
    Action {
        id: stopAction
        text: "&Stop and Return to Chapter Start"
        shortcut: "Ctrl+S"
        enabled: window.player.loaded && !notesEditor.visible && !ratingPopup.visible
        onTriggered: window.player.stop()
    }
    Action {
        id: previousAction
        text: "&Previous Chapter"
        shortcut: "Ctrl+Left"
        enabled: window.player.loaded && !notesEditor.visible && !ratingPopup.visible && window.player.currentChapter > 0
        onTriggered: window.player.navigateChapter(-1)
    }
    Action {
        id: nextAction
        text: "&Next Chapter"
        shortcut: "Ctrl+Right"
        enabled: window.player.loaded && !notesEditor.visible && !ratingPopup.visible && window.player.currentChapter + 1 < window.player.chapters.length
        onTriggered: window.player.navigateChapter(1)
    }
    Action {
        id: backAction
        text: "&Back 10 Seconds"
        shortcut: "Left"
        enabled: window.player.loaded && !notesEditor.visible && !ratingPopup.visible
        onTriggered: window.player.skip(-10)
    }
    Action {
        id: forwardAction
        text: "&Forward 10 Seconds"
        shortcut: "Right"
        enabled: window.player.loaded && !notesEditor.visible && !ratingPopup.visible
        onTriggered: window.player.skip(10)
    }
    Action {
        id: chaptersAction
        text: "&Chapters…"
        shortcut: "Ctrl+J"
        enabled: window.player.chapters.length > 0
        onTriggered: chapterSheet.open()
    }
    Action {
        id: stopAfterAction
        text: "Stop &After Chapter"
        enabled: window.player.loaded
        checkable: true
        checked: window.player.stopAfterChapter
        onTriggered: window.player.setStopAfterChapter(checked)
    }
    Action {
        id: captionsAction
        text: window.player.indexingCaptions ? "&Captions (indexing…)" : "&Captions"
        shortcut: "Ctrl+Shift+C"
        enabled: window.player.captionsAvailable
        checkable: true
        checked: window.player.captionsEnabled
        onTriggered: window.player.setCaptions(checked)
    }
    Action {
        id: volumeAction
        text: "&Volume…"
        onTriggered: volumePopup.open()
    }
    Action {
        id: notesAction
        text: "&Notes…"
        shortcut: "Ctrl+N"
        enabled: window.player.notesAvailable
        onTriggered: {
            ratingPopup.close()
            notesEditor.openDocument(window.player.notesForEditing())
        }
    }
    Action {
        id: ratingAction
        text: "&Rating and Notes…"
        enabled: window.player.notesAvailable
        onTriggered: ratingPopup.open()
    }
    Action {
        id: equalizerAction
        text: "Open &EQ…"
        shortcut: "Ctrl+E"
        onTriggered: equalizerWindow.openPanel()
    }
    Action {
        id: fillCoverAction
        text: "&Fill Cover Background"
        checkable: true
        checked: window.preferences.fillCover
        onTriggered: window.preferences.fillCover = checked
    }
    Action {
        id: appearanceAction
        text: "&Appearance…"
        shortcut: "Ctrl+,"
        onTriggered: settings.open()
    }
    Action {
        id: fullScreenAction
        text: "&Full Screen"
        shortcut: "F11"
        checkable: true
        checked: window.visibility === Window.FullScreen
        onTriggered: {
            if (checked) window.showFullScreen()
            else window.showNormal()
        }
    }

    menuBar: MenuBar {
        Menu {
            title: "&File"
            width: Math.min(window.width - 16, 320)
            MenuCommand { action: openAction; keyHint: "Ctrl+O" }
            Menu {
                id: recentMenu
                title: "Open &Recent"
                width: Math.min(window.width - 16, 420)
                MenuItem { text: "No recent books"; enabled: false; visible: window.player.recentBooks.length === 0; height: visible ? implicitHeight : 0 }
                Instantiator {
                    model: window.player.recentBooks
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.title.replace(/&/g, "&&")
                        onTriggered: window.player.openLocalPath(modelData.path)
                    }
                    onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => recentMenu.removeItem(object)
                }
            }
            MenuSeparator {}
            MenuCommand { action: notesAction }
            MenuCommand { action: appearanceAction }
            MenuSeparator {}
            MenuCommand {
                keyHint: "Ctrl+Q"
                action: Action {
                    text: "&Quit"
                    shortcut: StandardKey.Quit
                    onTriggered: window.close()
                }
            }
        }
        Menu {
            title: "&Playback"
            width: Math.min(window.width - 16, 360)
            MenuCommand { action: playAction }
            MenuCommand { action: stopAction }
            MenuSeparator {}
            MenuCommand { action: previousAction }
            MenuCommand { action: nextAction }
            MenuCommand { action: chaptersAction }
            MenuSeparator {}
            MenuCommand { action: backAction }
            MenuCommand { action: forwardAction }
            MenuSeparator {}
            Menu {
                title: "Playback &Speed"
                enabled: window.player.loaded
                Repeater {
                    model: window.playbackRates
                    MenuItem {
                        required property real modelData
                        text: modelData + "×"
                        checkable: true
                        checked: Math.abs(window.player.speed - modelData) < 0.001
                        onTriggered: window.player.setSpeed(modelData)
                    }
                }
            }
            MenuCommand { action: stopAfterAction }
            MenuSeparator {}
            MenuCommand { action: volumeAction }
            MenuCommand { action: equalizerAction }
            MenuCommand {
                action: Action {
                    text: "&Increase Volume"
                    enabled: !notesEditor.visible
                    shortcut: "Ctrl+Up"
                    onTriggered: window.player.setVolume(Math.min(100, window.player.volume + 5))
                }
            }
            MenuCommand {
                action: Action {
                    text: "&Decrease Volume"
                    enabled: !notesEditor.visible
                    shortcut: "Ctrl+Down"
                    onTriggered: window.player.setVolume(Math.max(0, window.player.volume - 5))
                }
            }
        }
        Menu {
            title: "&View"
            width: Math.min(window.width - 16, 300)
            MenuCommand { action: captionsAction }
            Menu {
                id: captionSizeMenu
                title: "Caption &Text Size"
                Repeater {
                    model: [18, 22, 26, 30, 34, 38, 42]
                    MenuItem {
                        required property int modelData
                        text: modelData + " px"
                        checkable: true
                        checked: window.preferences.captionSize === modelData
                        onTriggered: window.preferences.captionSize = modelData
                    }
                }
            }
            MenuCommand { action: fillCoverAction }
            MenuCommand { action: appearanceAction }
            MenuSeparator {}
            MenuCommand { action: fullScreenAction }
        }
        Menu {
            title: "&Help"
            width: Math.min(window.width - 16, 260)
            MenuCommand {
                action: Action {
                    text: "Player &Help"
                    shortcut: "F1"
                    onTriggered: helpDialog.open()
                }
            }
            MenuItem { text: "&About qt-m4b-player"; onTriggered: aboutDialog.open() }
        }
    }

    CoverArtwork {
        id: artwork
        anchors.fill: parent
        source: window.player.coverUrl
        fill: window.preferences.fillCover
        fittedHeight: panels.y + controlsPanel.y
        playbackEnabled: window.player.loaded
        onClicked: window.player.togglePlayback()
    }
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        visible: !window.player.loaded
        width: parent.width - 64
        spacing: 16
        Label {
            width: parent.width
            text: window.player.fileUrl.toString() ? "Opening audiobook…" : "Your next chapter"
            color: window.palette.windowText
            font.pixelSize: 28
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Label {
            width: parent.width
            text: "Open an M4B file to listen and read its embedded captions."
            color: window.palette.windowText
            opacity: 0.75
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    Column {
        id: panels
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        spacing: 12

        Rectangle {
            width: parent.width
            visible: window.player.error.length > 0
            height: visible ? errorText.implicitHeight + 32 : 0
            radius: 12
            // No palette role exists for errors; a fixed red stays legible in both schemes.
            color: "#b3261e"
            Label {
                id: errorText
                anchors.left: parent.left
                anchors.right: dismiss.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                text: window.player.error
                color: "white"
                wrapMode: Text.Wrap
            }
            ToolButton { id: dismiss; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "×"; onClicked: window.player.clearError() }
        }
        Rectangle {
            id: captionBubble
            width: parent.width
            visible: window.player.captionText.length > 0
            height: visible ? Math.min(caption.implicitHeight + 36, window.contentItem.height * 0.43) : 0
            radius: 18
            color: "#000000"
            Item {
                id: captionViewport
                anchors.fill: parent
                anchors.margins: 18
                clip: true
                Text {
                    id: caption
                    width: parent.width
                    y: -Math.max(0, implicitHeight - captionViewport.height) * window.player.captionScroll
                    text: window.player.captionText
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffffff"
                    font.pixelSize: window.preferences.captionSize
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }
            MouseArea { anchors.fill: parent; onClicked: window.player.togglePlayback() }
        }
        Item {
            id: controlsPanel
            objectName: "transportPanel"
            width: parent.width
            height: controls.implicitHeight + 32
            FrostedPanel {
                anchors.fill: parent
                source: window.preferences.fillCover ? window.player.coverUrl : ""
                artworkRect: Qt.rect(artwork.imageRect.x - panels.x - controlsPanel.x,
                                     artwork.imageRect.y - panels.y - controlsPanel.y,
                                     artwork.imageRect.width, artwork.imageRect.height)
                tint: Qt.rgba(window.palette.window.r, window.palette.window.g,
                              window.palette.window.b, window.preferences.fillCover ? 0.76 : 1)
            }
            ColumnLayout {
                id: controls
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 8
                ProgressHeader {
                    id: progressHeader
                    Layout.fillWidth: true
                    textColor: window.palette.windowText
                    chapterText: window.player.currentChapter >= 0 ? window.player.chapters[window.player.currentChapter].title : "Whole book"
                    positionText: window.formatTime(window.player.position - window.player.chapterStart) + " of " + window.formatTime(window.player.chapterEnd - window.player.chapterStart)
                    readonly property real remaining: Math.max(0, window.player.duration - window.player.position)
                    remainingText: Math.floor(remaining / 3600) + "h " + Math.floor((remaining % 3600) / 60) + "m remaining ("
                        + (window.player.duration > 0 ? Math.ceil(remaining / window.player.duration * 100) : 0) + "%)"
                }
                Slider {
                    id: progress
                    property real capturedStart: 0
                    property real capturedEnd: 0
                    property bool wasPlaying: false
                    Layout.fillWidth: true
                    from: pressed ? capturedStart : window.player.chapterStart
                    to: Math.max(from + 0.001, pressed ? capturedEnd : window.player.chapterEnd)
                    value: window.player.position
                    enabled: window.player.loaded
                    Accessible.name: "Chapter progress"
                    onPressedChanged: {
                        if (pressed) {
                            capturedStart = window.player.chapterStart
                            capturedEnd = window.player.chapterEnd
                            wasPlaying = window.player.playing
                            if (wasPlaying) window.player.togglePlayback()
                        } else {
                            window.player.seek(Math.min(value, capturedEnd - 0.000001))
                            if (wasPlaying) window.player.togglePlayback()
                        }
                    }
                    onMoved: if (!pressed) window.player.seek(value)
                    // Keep the native style slider so its accent follows the platform scheme.
                }
                GridLayout {
                    columns: window.wideControls ? 2 : 1
                    Layout.alignment: Qt.AlignHCenter
                    columnSpacing: 12
                    rowSpacing: 6
                    RowLayout {
                        objectName: "primaryTransport"
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 4
                        TransportButton { action: previousAction; iconName: "previous"; description: "Previous chapter" }
                        TransportButton { action: backAction; iconName: "back"; description: "Back ten seconds" }
                        TransportButton {
                            id: playButton
                            action: playAction
                            iconName: window.player.playing ? "pause" : "play"
                            description: window.player.playing ? "Pause" : "Play"
                            emphasized: true
                            implicitWidth: 56
                            implicitHeight: 56
                            background: Rectangle {
                                radius: 28
                                color: playButton.down ? Qt.darker(playButton.palette.highlight, 1.2)
                                    : playButton.hovered ? Qt.lighter(playButton.palette.highlight, 1.1)
                                    : playButton.palette.highlight
                                border.width: playButton.activeFocus ? 2 : 0
                                border.color: playButton.palette.highlightedText
                            }
                        }
                        TransportButton { action: forwardAction; iconName: "forward"; description: "Forward ten seconds" }
                        TransportButton { action: nextAction; iconName: "next"; description: "Next chapter" }
                        TransportButton { action: stopAction; iconName: "stop"; description: "Stop at chapter start" }
                    }
                    RowLayout {
                        objectName: "secondaryTransport"
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 4
                        TransportButton { action: chaptersAction; iconName: "chapters"; description: "Chapters" }
                        TransportButton { action: stopAfterAction; iconName: "stopAfter"; description: "Stop after chapter" }
                        TransportButton { action: captionsAction; text: "CC"; description: window.player.indexingCaptions ? "Indexing captions" : "Embedded captions"; font.pixelSize: 17; font.bold: true }
                        TransportButton {
                            id: ratingButton
                            objectName: "ratingButton"
                            action: ratingAction
                            description: window.player.rating === 0 ? "Rating and notes: unrated"
                                : "Rating and notes: " + (window.player.rating / 10).toFixed(1) + " out of 5 stars"
                            contentItem: Item {
                                StarIcon {
                                    anchors.centerIn: parent
                                    width: 24; height: 24
                                    fraction: window.player.rating > 0 ? 1 : 0
                                    color: ratingButton.foregroundColor
                                }
                            }
                        }
                        TransportButton { text: window.player.speed + "×"; description: "Playback speed"; enabled: window.player.loaded; onClicked: speedMenu.open() }
                        TransportButton { id: volumeButton; objectName: "volumeButton"; action: volumeAction; iconName: "volume"; description: "Volume" }
                    }
                }
            }
        }
    }

    Menu {
        id: speedMenu
        x: (window.width - width) / 2
        y: window.contentItem.height - height - 100
        Repeater {
            model: window.playbackRates
            MenuItem {
                required property real modelData
                text: modelData + "×"
                checkable: true
                checked: Math.abs(window.player.speed - modelData) < 0.001
                onTriggered: window.player.setSpeed(modelData)
            }
        }
    }
    Popup {
        id: ratingPopup
        objectName: "ratingPopup"
        parent: Overlay.overlay
        width: 240
        modal: true
        focus: true
        property url bookUrl
        onAboutToShow: {
            bookUrl = window.player.fileUrl
            let point = ratingButton.mapToItem(Overlay.overlay, 0, 0)
            x = Math.max(8, Math.min(window.width - width - 8, point.x + (ratingButton.width - width) / 2))
            y = Math.max(8, point.y - height - 8)
        }
        onOpened: ratingSlider.forceActiveFocus()
        onClosed: if (!notesEditor.visible) ratingButton.forceActiveFocus()
        Connections {
            target: window.player
            function onStateChanged() {
                if (ratingPopup.visible && (!window.player.notesAvailable || ratingPopup.bookUrl !== window.player.fileUrl))
                    ratingPopup.close()
            }
        }
        contentItem: ColumnLayout {
            spacing: 10
            Slider {
                id: ratingSlider
                objectName: "ratingSlider"
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 180
                implicitHeight: 32
                padding: 0
                from: 0; to: 5; stepSize: 0.5
                snapMode: Slider.SnapAlways
                value: window.player.rating / 10
                onMoved: window.player.setRating(Math.round(value * 10))
                Keys.onPressed: event => {
                    event.accepted = false
                    if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                        window.player.setRating(event.key === Qt.Key_Home ? 0 : 50)
                        event.accepted = true
                    }
                }
                Accessible.name: "Audiobook rating"
                Accessible.description: "Half-star steps. Left and Right adjust; Home clears the rating."
                handle: Item { width: 0; height: 0 }
                background: Row {
                    width: ratingSlider.availableWidth
                    height: ratingSlider.availableHeight
                    Repeater {
                        model: 5
                        Item {
                            id: ratingStar
                            required property int index
                            width: ratingSlider.availableWidth / 5
                            height: ratingSlider.availableHeight
                            StarIcon {
                                anchors.centerIn: parent
                                width: 28; height: 28
                                fraction: Math.max(0, Math.min(1, window.player.rating / 10 - ratingStar.index))
                                color: ratingSlider.activeFocus ? ratingSlider.palette.highlight : ratingSlider.palette.buttonText
                            }
                        }
                    }
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: window.player.rating === 0 ? "Unrated" : (window.player.rating / 10).toFixed(1) + " / 5"
            }
            Button {
                objectName: "openNotesButton"
                Layout.alignment: Qt.AlignHCenter
                text: "Open notes (Ctrl+N)"
                onClicked: notesAction.trigger()
            }
        }
    }
    Popup {
        id: volumePopup
        objectName: "volumePopup"
        parent: Overlay.overlay
        width: 300
        modal: true
        focus: true
        onAboutToShow: {
            let point = volumeButton.mapToItem(Overlay.overlay, 0, 0)
            x = Math.max(8, Math.min(window.width - width - 8, point.x + (volumeButton.width - width) / 2))
            y = Math.max(8, point.y - height - 8)
        }
        contentItem: ColumnLayout {
            Label { text: "Volume" }
            Slider { Layout.fillWidth: true; from: 0; to: 100; value: window.player.volume; Accessible.name: "Volume"; onMoved: window.player.setVolume(value) }
            Button { action: equalizerAction; text: "Open EQ (Ctrl+E)"; onClicked: volumePopup.close() }
        }
    }
    Popup {
        id: chapterSheet
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 480)
        height: Math.min(window.height - 120, 500)
        modal: true
        focus: true
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Chapters"; font.pixelSize: 22 }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: window.player.chapters
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    text: (index === window.player.currentChapter ? "▶  " : "") + modelData.title
                    highlighted: index === window.player.currentChapter
                    onClicked: { window.player.selectChapter(index); chapterSheet.close() }
                }
            }
            Button { text: "Close"; onClicked: chapterSheet.close() }
        }
    }
    Popup {
        id: settings
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 360)
        modal: true
        focus: true
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Appearance"; font.pixelSize: 22 }
            CheckBox { action: fillCoverAction; text: "Fill cover background" }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "In Fill mode, drag the cover vertically to adjust its crop." }
            Label { text: "Caption size: " + window.preferences.captionSize }
            Slider {
                id: captionSizeSlider
                Layout.fillWidth: true
                from: 18
                to: 42
                stepSize: 1
                value: window.preferences.captionSize
                Accessible.name: "Caption text size"
                onMoved: window.preferences.captionSize = value
            }
            Button { text: "Close"; onClicked: settings.close() }
        }
    }
    InfoPopup {
        id: helpDialog
        title: "Player Help"
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 440)
        message: "Open a book with File → Open Audiobook or Open Recent. Books open paused and remember your position.\n\n"
            + "Space: Play / pause\nLeft / Right: Back / forward 10 seconds\nCtrl+Left / Right: Previous / next chapter\nCtrl+J: Chapters\nCtrl+N: Notes\nCtrl+E: EQ\nCtrl+S: Stop (Save while editing notes)\nCtrl+Up / Down: Volume + / − 5%\nCtrl+Shift+C: Toggle captions\nCtrl+,: Appearance\nF11: Full screen\n\n"
            + "Notes use explicit Save/Cancel; an empty note clears it. EQ starts bypassed and applies across books for this session. Tone runs from warm to clear; Hz changes its balance point. Boost adds up to 24 dB with peak limiting; higher settings reduce dynamics. Reset restores neutral tone, 1000 Hz and zero Boost without changing bypass. Automatic headroom makes room for boosted frequencies.\n\n"
            + "Use Playback for speed and Stop After Chapter. Enable embedded captions in View once indexing finishes. In Fill mode, drag vertically on the cover to reframe it."
    }
    InfoPopup {
        id: aboutDialog
        title: "About qt-m4b-player"
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 360)
        message: "qt-m4b-player " + Qt.application.version
            + "\n\nAn audiobook player with chapters, cover art, and embedded captions."
            + "\n\nBuilt with Qt, libmpv, FFmpeg, and SQLite."
            + "\n\nIcons: © 2023 Phosphor Icons (MIT license)."
    }
}
