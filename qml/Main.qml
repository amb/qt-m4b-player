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
    property string buildTimestamp: ""
    readonly property bool wideControls: width >= 740
    readonly property var playbackRates: [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3]
    readonly property real minimumSpeed: playbackRates[0]
    readonly property real maximumSpeed: playbackRates[playbackRates.length - 1]
    // Ctrl+Left/Right step speed by 0.1×, kept to hundredths and the preset range.
    function changeSpeed(delta: real): void {
        const next = Math.round((window.player.speed + delta) * 100) / 100
        window.player.setSpeed(Math.max(window.minimumSpeed, Math.min(window.maximumSpeed, next)))
    }
    readonly property bool utilityPopupOpen: chapterSheet.visible || ratingPopup.visible || speedPopup.visible || volumePopup.visible
    // Separate windows and utility popups own the keyboard; transport keys must not reach playback.
    readonly property bool transportBlocked: notesEditor.visible || bookInfoWindow.visible || utilityPopupOpen
    property url popupBookUrl
    // Follow the platform colour scheme; the application ships no palette of its own.
    color: palette.window
    width: 520
    height: 800
    minimumWidth: 390
    minimumHeight: 600
    // C++ restores the saved size/state before showing the window.
    visible: false
    title: window.player.loaded ? window.player.title + (window.player.author ? " — " + window.player.author : "") : "qt-m4b-player"
    onClosing: { notesEditor.close(); bookInfoWindow.close(); settingsWindow.close(); equalizerWindow.close() }

    SettingsWindow {
        id: settingsWindow
        preferences: window.preferences
        transientParent: window
        palette: window.palette
    }
    NotesEditor {
        id: notesEditor
        player: window.player
        textSize: window.preferences.playerTextSize
        transientParent: window
        palette: window.palette
    }
    BookInfoWindow {
        id: bookInfoWindow
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

    function closeUtilityPopups(exceptPopup: var): void {
        for (let popup of [chapterSheet, ratingPopup, speedPopup, volumePopup])
            if (popup !== exceptPopup) popup.close()
    }
    function prepareUtilityPopup(popup: var): void {
        closeUtilityPopups(popup)
        popupBookUrl = window.player.fileUrl
    }
    Connections {
        target: window.player
        function onStateChanged() {
            if (window.utilityPopupOpen && window.popupBookUrl !== window.player.fileUrl)
                window.closeUtilityPopups(null)
            if (bookInfoWindow.visible && bookInfoWindow.bookUrl !== window.player.fileUrl)
                bookInfoWindow.close()
        }
        function onExportFinished(error: string) {
            if (error.length > 0)
                window.showAlert("Unable to Export Audio", error)
        }
    }
    function showAlert(title: string, message: string): void {
        alertDialog.title = title
        alertDialog.message = message
        alertDialog.open()
    }
    // The range is captured before the save dialog opens, so later playback,
    // seeks or book changes do not change what is exported.
    function beginExport(range: var): void {
        if (!range.path) return
        window.closeUtilityPopups(null)
        exportDialog.range = range
        exportDialog.currentFolder = range.folder
        exportDialog.selectedFile = range.folder + "/" + encodeURIComponent(range.fileName)
        exportDialog.open()
    }

    FileDialog {
        id: fileDialog
        title: "Open audiobook"
        nameFilters: ["Audiobooks (*.m4b *.m4a *.mp4)", "All files (*)"]
        onAccepted: window.player.openBook(selectedFile)
    }
    FileDialog {
        id: exportDialog
        objectName: "exportDialog"
        property var range: ({})
        title: "Export Audio as WAV"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: window.player.exportAudio(range, selectedFile)
    }
    Action {
        id: openAction
        text: "&Open Audiobook…"
        shortcut: StandardKey.Open
        onTriggered: { window.closeUtilityPopups(null); fileDialog.open() }
    }
    Action {
        id: playAction
        text: window.player.playing ? "&Pause" : "&Play"
        shortcut: "Space"
        enabled: window.player.loaded && !window.transportBlocked
        onTriggered: window.player.togglePlayback()
    }
    Action {
        id: stopAction
        text: "&Stop and Return to Chapter Start"
        shortcut: "Escape"
        enabled: window.player.loaded && !window.transportBlocked
        onTriggered: window.player.stop()
    }
    Action {
        id: previousAction
        text: "&Previous Chapter"
        shortcut: "Up"
        // Stays enabled at the first chapter, where it pauses without moving.
        enabled: window.player.loaded && !window.transportBlocked
        onTriggered: window.player.navigateChapter(-1)
    }
    Action {
        id: nextAction
        text: "&Next Chapter"
        shortcut: "Down"
        enabled: window.player.loaded && !window.transportBlocked
        onTriggered: window.player.navigateChapter(1)
    }
    Action {
        id: backAction
        text: "&Back 10 Seconds"
        shortcut: "Left"
        enabled: window.player.loaded && !window.transportBlocked
        onTriggered: window.player.skip(-10)
    }
    Action {
        id: forwardAction
        text: "&Forward 10 Seconds"
        shortcut: "Right"
        enabled: window.player.loaded && !window.transportBlocked
        onTriggered: window.player.skip(10)
    }
    Action {
        id: slowerAction
        text: "&Decrease Playback Speed"
        shortcut: "Ctrl+Left"
        enabled: window.player.loaded && !window.transportBlocked && window.player.speed > window.minimumSpeed + 0.001
        onTriggered: window.changeSpeed(-0.1)
    }
    Action {
        id: fasterAction
        text: "&Increase Playback Speed"
        shortcut: "Ctrl+Right"
        enabled: window.player.loaded && !window.transportBlocked && window.player.speed < window.maximumSpeed - 0.001
        onTriggered: window.changeSpeed(0.1)
    }
    Action {
        id: closeAction
        text: "&Close Window"
        shortcut: StandardKey.Close
        onTriggered: window.close()
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
        // The popup owns this shortcut while open; avoid registering it twice.
        shortcut: ratingPopup.visible ? "" : "Ctrl+N"
        enabled: window.player.notesAvailable && !bookInfoWindow.visible
        onTriggered: {
            window.closeUtilityPopups(null)
            notesEditor.openDocument(window.player.notesForEditing())
        }
    }
    Action {
        id: infoAction
        text: "Audiobook &Info…"
        shortcut: "Ctrl+I"
        enabled: window.player.loaded && !notesEditor.visible
        onTriggered: { window.closeUtilityPopups(null); bookInfoWindow.openInfo() }
    }
    Action {
        id: exportChapterAction
        text: "Export Current &Chapter as WAV…"
        enabled: window.player.chapterExportAvailable && !window.player.exporting && !window.transportBlocked
        onTriggered: window.beginExport(window.player.chapterExportRange())
    }
    Action {
        id: exportChunkAction
        text: "Export Current Caption C&hunk as WAV…"
        enabled: window.player.chunkExportAvailable && !window.player.exporting && !window.transportBlocked
        onTriggered: {
            const range = window.player.chunkExportRange()
            if (range.gap)
                window.showAlert("No Chunk at Current Position",
                                 "The current playback position is in a gap between subtitle chunks.")
            else
                window.beginExport(range)
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
        shortcut: volumePopup.visible ? "" : "Ctrl+E"
        onTriggered: { window.closeUtilityPopups(null); equalizerWindow.openPanel() }
    }
    Action {
        id: settingsAction
        text: "&Settings…"
        shortcut: "Ctrl+,"
        onTriggered: { window.closeUtilityPopups(null); settingsWindow.openSettings() }
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

    // Menus group commands by what they act on, and each command appears once:
    // File = the book and its files; Playback = position, chapters, speed and
    // sound; View = what is shown; Help. Global preferences live in Settings.
    menuBar: MenuBar {
        Menu {
            objectName: "fileMenu"
            title: "&File"
            width: Math.min(window.width - 16, 380)
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
            MenuCommand { action: infoAction }
            MenuCommand { action: notesAction }
            MenuSeparator {}
            MenuCommand { action: exportChapterAction }
            MenuCommand { action: exportChunkAction }
            MenuItem {
                text: "Exporting Audio…"
                enabled: false
                visible: window.player.exporting
                height: visible ? implicitHeight : 0
            }
            MenuSeparator {}
            MenuCommand { action: settingsAction }
            MenuSeparator {}
            MenuCommand { action: closeAction; keyHint: "Ctrl+W" }
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
            MenuCommand { action: backAction }
            MenuCommand { action: forwardAction }
            MenuSeparator {}
            MenuCommand { action: previousAction }
            MenuCommand { action: nextAction }
            MenuCommand { action: chaptersAction }
            MenuCommand { action: stopAfterAction }
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
            MenuCommand { action: slowerAction }
            MenuCommand { action: fasterAction }
            MenuSeparator {}
            MenuCommand { action: volumeAction }
            MenuCommand {
                action: Action {
                    text: "&Increase Volume"
                    enabled: !window.transportBlocked
                    shortcut: "Ctrl+Up"
                    onTriggered: window.player.setVolume(Math.min(100, window.player.volume + 5))
                }
            }
            MenuCommand {
                action: Action {
                    text: "&Decrease Volume"
                    enabled: !window.transportBlocked
                    shortcut: "Ctrl+Down"
                    onTriggered: window.player.setVolume(Math.max(0, window.player.volume - 5))
                }
            }
            MenuCommand { action: equalizerAction }
        }
        Menu {
            title: "&View"
            width: Math.min(window.width - 16, 300)
            MenuCommand { action: captionsAction }
            MenuCommand { action: fullScreenAction }
        }
        Menu {
            title: "&Help"
            width: Math.min(window.width - 16, 260)
            MenuCommand {
                action: Action {
                    text: "Player &Help"
                    shortcut: "Ctrl+Shift+H"
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
            font.pixelSize: Math.round(window.preferences.playerTextSize * 1.75)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Label {
            width: parent.width
            text: "Open an M4B file to listen and read its embedded captions."
            font.pixelSize: window.preferences.playerTextSize
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
            // Transparency fades only the panel; outlined text stays opaque.
            color: Qt.rgba(0, 0, 0, 1 - window.preferences.captionTransparency / 100)
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
                    style: Text.Outline
                    styleColor: "#000000"
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
                    textSize: window.preferences.playerTextSize
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
                        TransportButton { id: chaptersButton; objectName: "chaptersButton"; action: chaptersAction; iconName: "chapters"; description: "Chapters"; popupOpen: chapterSheet.visible }
                        TransportButton { action: stopAfterAction; iconName: "stopAfter"; description: "Stop after chapter" }
                        TransportButton { action: captionsAction; text: "CC"; description: window.player.indexingCaptions ? "Indexing captions" : "Embedded captions"; font.pixelSize: 17; font.bold: true }
                        TransportButton {
                            id: ratingButton
                            objectName: "ratingButton"
                            action: ratingAction
                            popupOpen: ratingPopup.visible
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
                        TransportButton { id: speedButton; objectName: "speedButton"; text: window.player.speed + "×"; description: "Playback speed"; enabled: window.player.loaded; popupOpen: speedPopup.visible; onClicked: speedPopup.open() }
                        TransportButton { id: volumeButton; objectName: "volumeButton"; action: volumeAction; iconName: "volume"; description: "Volume"; popupOpen: volumePopup.visible }
                    }
                }
            }
        }
    }

    TransportPopup {
        id: speedPopup
        objectName: "speedPopup"
        anchorItem: speedButton
        boundsItem: window.contentItem
        palette: window.palette
        onAboutToShow: window.prepareUtilityPopup(speedPopup)
        onOpened: {
            speedList.currentIndex = Math.max(0, window.playbackRates.indexOf(window.player.speed))
            speedList.positionViewAtIndex(speedList.currentIndex, ListView.Contain)
            speedList.forceActiveFocus()
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Playback speed"; font.pixelSize: 18 }
            ListView {
                id: speedList
                objectName: "speedList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                Layout.preferredHeight: window.playbackRates.length * 44
                clip: true
                keyNavigationEnabled: true
                boundsBehavior: Flickable.StopAtBounds
                model: window.playbackRates
                ScrollBar.vertical: ScrollBar {}
                function chooseCurrent(): void {
                    if (currentIndex < 0) return
                    window.player.setSpeed(window.playbackRates[currentIndex])
                    speedPopup.close()
                }
                Keys.onReturnPressed: chooseCurrent()
                Keys.onEnterPressed: chooseCurrent()
                delegate: ItemDelegate {
                    required property real modelData
                    required property int index
                    width: ListView.view.width
                    height: 44
                    text: (Math.abs(window.player.speed - modelData) < 0.001 ? "✓  " : "") + modelData + "×"
                    highlighted: ListView.isCurrentItem
                    onClicked: { window.player.setSpeed(modelData); speedPopup.close() }
                }
            }
        }
    }
    TransportPopup {
        id: ratingPopup
        objectName: "ratingPopup"
        anchorItem: ratingButton
        boundsItem: window.contentItem
        palette: window.palette
        onAboutToShow: window.prepareUtilityPopup(ratingPopup)
        onOpened: ratingSlider.forceActiveFocus()
        Shortcut { sequence: "Ctrl+N"; enabled: ratingPopup.opened; onActivated: notesAction.trigger() }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Rating and notes"; font.pixelSize: 18 }
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
                                color: ratingSlider.activeFocus ? ratingSlider.palette.highlight : ratingValueLabel.color
                            }
                        }
                    }
                }
            }
            Label {
                id: ratingValueLabel
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
    TransportPopup {
        id: volumePopup
        objectName: "volumePopup"
        anchorItem: volumeButton
        boundsItem: window.contentItem
        palette: window.palette
        onAboutToShow: window.prepareUtilityPopup(volumePopup)
        onOpened: volumeSlider.forceActiveFocus()
        Shortcut { sequence: "Ctrl+E"; enabled: volumePopup.opened; onActivated: equalizerAction.trigger() }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Volume"; font.pixelSize: 18 }
            Slider { id: volumeSlider; objectName: "volumeSlider"; Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: window.player.volume; Accessible.name: "Volume"; onMoved: window.player.setVolume(value) }
            Button { action: equalizerAction; text: "Open EQ (Ctrl+E)" }
        }
    }
    TransportPopup {
        id: chapterSheet
        objectName: "chapterSheet"
        anchorItem: chaptersButton
        boundsItem: window.contentItem
        palette: window.palette
        preferredWidth: 420
        onAboutToShow: window.prepareUtilityPopup(chapterSheet)
        onOpened: {
            chapterList.currentIndex = Math.max(0, window.player.currentChapter)
            chapterList.positionViewAtIndex(chapterList.currentIndex, ListView.Contain)
            chapterList.forceActiveFocus()
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Chapters"; font.pixelSize: 18 }
            ListView {
                id: chapterList
                objectName: "chapterList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                Layout.preferredHeight: Math.min(440, window.player.chapters.length * 44)
                clip: true
                keyNavigationEnabled: true
                boundsBehavior: Flickable.StopAtBounds
                model: window.player.chapters
                ScrollBar.vertical: ScrollBar {}
                function chooseCurrent(): void {
                    if (currentIndex < 0) return
                    window.player.selectChapter(currentIndex)
                    chapterSheet.close()
                }
                Keys.onReturnPressed: chooseCurrent()
                Keys.onEnterPressed: chooseCurrent()
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: 44
                    text: (index === window.player.currentChapter ? "▶  " : "") + modelData.title
                    highlighted: ListView.isCurrentItem
                    onClicked: { window.player.selectChapter(index); chapterSheet.close() }
                }
            }
        }
    }
    // F1 is the usual Linux help key; Ctrl+Shift+H matches the macOS app.
    Shortcut { sequence: "F1"; onActivated: helpDialog.open() }
    InfoPopup {
        id: helpDialog
        title: "Player Help"
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 440)
        message: "Open a book with File → Open Audiobook or Open Recent. Books open paused and remember your position.\n\n"
            + "Space: Play / pause\nEscape: Stop and return to the chapter start\nUp / Down: Previous / next chapter\nLeft / Right: Back / forward 10 seconds\nCtrl+Left / Right: Playback speed − / + 0.1×\nCtrl+Up / Down: Volume + / − 5%\nCtrl+J: Chapters\nCtrl+Shift+C: Toggle captions\nCtrl+I: Audiobook info\nCtrl+N: Notes (Ctrl+S saves)\nCtrl+E: EQ\nCtrl+,: Settings\nCtrl+W: Close window\nCtrl+Shift+H or F1: Help\nF11: Full screen\n\n"
            + "Notes use explicit Save/Cancel; an empty note clears it. EQ starts bypassed and applies across books for this session. Tone runs from warm to clear; Hz changes its balance point. Boost adds up to 24 dB with peak limiting; higher settings reduce dynamics. Reset restores neutral tone, 1000 Hz and zero Boost without changing bypass. Automatic headroom makes room for boosted frequencies.\n\n"
            + "File has book details, notes and WAV export. Export Current Chapter saves the whole chapter at the playhead; Export Current Caption Chunk saves the active caption's audio, even with captions hidden. Exports are 16-bit WAV at the original sample rate, without speed, volume or EQ.\n\n"
            + "Playback has transport, chapters, speed, Stop After Chapter, volume and EQ. View toggles captions once indexing finishes and full screen. File → Settings holds launch resume, the Stop After Chapter default, cover display, text sizes and caption background. In Fill mode, drag vertically on the cover to reframe it."
    }
    InfoPopup {
        id: alertDialog
        objectName: "alertDialog"
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 400)
    }
    InfoPopup {
        id: aboutDialog
        title: "About qt-m4b-player"
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 32, 360)
        message: "qt-m4b-player " + Qt.application.version
            + (window.buildTimestamp ? "\nBuilt: " + window.buildTimestamp : "")
            + "\n\nAn audiobook player with chapters, cover art, and embedded captions."
            + "\n\nBuilt with Qt, libmpv, FFmpeg, and SQLite."
            + "\n\nIcons: © 2023 Phosphor Icons (MIT license)."
    }
}
