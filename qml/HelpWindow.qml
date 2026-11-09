pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// App-wide reference: a keyboard-shortcut table followed by short sections.
// It is informational only and has no playback controls of its own.
ApplicationWindow {
    id: help
    objectName: "helpWindow"
    title: "Player Help"
    width: 600
    height: 700
    minimumWidth: 480
    minimumHeight: 360
    visible: false
    color: palette.window

    // Each row lists alternative key combinations; each combination is a list of keys.
    readonly property var shortcutGroups: [
        { title: "Playback", rows: [
            { keys: [["Space"]], action: "Play or pause" },
            { keys: [["Esc"]], action: "Stop and return to the chapter start" },
            { keys: [["←"], ["→"]], action: "Back or forward 10 seconds" },
            { keys: [["↑"], ["↓"]], action: "Previous or next chapter (pauses)" },
            { keys: [["Ctrl", "←"], ["Ctrl", "→"]], action: "Playback speed − or + 0.1×" },
            { keys: [["Ctrl", "↑"], ["Ctrl", "↓"]], action: "Volume + or − 5%" },
            { keys: [["Ctrl", "J"]], action: "Chapter list" },
            { keys: [["Ctrl", "Shift", "C"]], action: "Captions on or off" }
        ] },
        { title: "Book and windows", rows: [
            { keys: [["Ctrl", "O"]], action: "Open an audiobook" },
            { keys: [["Ctrl", "I"]], action: "Audiobook Info" },
            { keys: [["Ctrl", "N"]], action: "Notes" },
            { keys: [["Ctrl", "S"]], action: "Save the note (in Notes)" },
            { keys: [["Ctrl", "E"]], action: "EQ" },
            { keys: [["Ctrl", ","]], action: "Settings" },
            { keys: [["F11"]], action: "Full screen" },
            { keys: [["Ctrl", "Shift", "H"], ["F1"]], action: "This help" },
            { keys: [["Ctrl", "W"]], action: "Close the window" },
            { keys: [["Ctrl", "Q"]], action: "Quit" }
        ] }
    ]
    readonly property var sections: [
        { title: "Getting started", text: "Open a book with File → Open Audiobook or Open Recent. Books open paused and remember your position. Click the cover or a caption to play or pause. Scroll over the speed button to step through the speeds: up is faster, down is slower. In Fill mode, drag the cover vertically to reframe it." },
        { title: "Chapters and Stop After Chapter", text: "The slider and times cover the current chapter; the remaining time is for the whole book. Previous and Next pause and return to where you left a chapter in this session; choosing a chapter in the list starts it from the beginning. With Stop After Chapter on, playback pauses at the start of the next chapter." },
        { title: "Captions", text: "CC becomes available once embedded captions are indexed. The index is cached, so reopening a book shows them right away." },
        { title: "Notes and rating", text: "The star sets a half-star rating and opens Notes. Notes use explicit Save and Cancel; saving empty text clears the note." },
        { title: "EQ and Boost", text: "EQ starts bypassed and applies across books for this session. Tone runs from warm to clear; Hz moves its balance point. Boost adds up to 24 dB with peak limiting, and higher settings reduce dynamics. Auto loudness measures the current chapter and sets Boost. Reset restores neutral tone, 1000 Hz and zero Boost without changing bypass." },
        { title: "Export", text: "File → Export Current Chapter saves the whole chapter at the playhead. Export Current Caption Chunk saves the active caption's audio, even with captions hidden. Exports are 16-bit WAV at the original sample rate, without speed, volume or EQ." },
        { title: "Settings", text: "File → Settings holds launch resume, the Stop After Chapter default, cover display, text sizes and the caption background. Changes apply immediately." }
    ]

    function openHelp(): void {
        show()
        raise()
        requestActivate()
        scroll.forceActiveFocus()
    }

    Shortcut { sequence: "Escape"; enabled: help.visible; context: Qt.WindowShortcut; onActivated: help.close() }
    Shortcut { sequences: [StandardKey.Close]; enabled: help.visible; context: Qt.WindowShortcut; onActivated: help.close() }

    component KeyCap: Rectangle {
        id: cap
        property string key: ""
        implicitWidth: Math.max(implicitHeight, capLabel.implicitWidth + 14)
        implicitHeight: capLabel.implicitHeight + 6
        radius: 5
        color: help.palette.base
        border.color: help.palette.mid
        border.width: 1
        Rectangle {
            // A thicker lower edge reads as a physical key.
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 1
            height: 2
            radius: 2
            color: help.palette.mid
            opacity: 0.6
        }
        Label {
            id: capLabel
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            text: cap.key
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
    }
    component Combination: Row {
        id: combination
        property var keys: []
        spacing: 3
        Repeater {
            model: combination.keys
            Row {
                id: keyItem
                required property string modelData
                required property int index
                spacing: 3
                Label {
                    visible: keyItem.index > 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: "+"
                    opacity: 0.6
                }
                KeyCap { key: keyItem.modelData }
            }
        }
    }

    ReadingScrollView {
        id: scroll
        objectName: "helpScroll"
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: content.implicitHeight + 56
        focus: true
        Keys.onUpPressed: scroll.scrollBy(-1)
        Keys.onDownPressed: scroll.scrollBy(1)
        Keys.onPressed: event => {
            if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                scroll.scrollBy(event.key === Qt.Key_PageDown ? 8 : -8)
                event.accepted = true
            }
        }

        ColumnLayout {
            id: content
            x: 28
            y: 28
            width: Math.min(scroll.availableWidth - 56, 680)
            spacing: 18

            RowLayout {
                spacing: 14
                Image {
                    source: "qrc:/app-icon/io.github.amb.QtM4bPlayer.png"
                    sourceSize: Qt.size(56, 56)
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    fillMode: Image.PreserveAspectFit
                    // The app icon is not bundled when QML loads from disk (tests).
                    visible: status === Image.Ready
                }
                ColumnLayout {
                    spacing: 2
                    Label { text: "qt-m4b-player"; font.pixelSize: 24; font.bold: true; Accessible.role: Accessible.Heading }
                    Label { text: "A focused audiobook player for M4B files."; opacity: 0.75 }
                }
            }

            Label { text: "Keyboard shortcuts"; font.pixelSize: 18; font.bold: true; Accessible.role: Accessible.Heading }
            Repeater {
                model: help.shortcutGroups
                ColumnLayout {
                    id: group
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 6
                    Label { text: group.modelData.title; font.bold: true; opacity: 0.8 }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: table.implicitHeight + 2
                        radius: 8
                        color: "transparent"
                        border.color: help.palette.mid
                        border.width: 1
                        clip: true
                        Column {
                            id: table
                            objectName: "shortcutTable"
                            x: 1
                            y: 1
                            width: parent.width - 2
                            Repeater {
                                model: group.modelData.rows
                                Rectangle {
                                    id: shortcutRow
                                    required property var modelData
                                    required property int index
                                    width: table.width
                                    implicitHeight: Math.max(keysCell.implicitHeight, actionLabel.implicitHeight) + 14
                                    // Alternate row shading keeps long tables easy to follow.
                                    color: index % 2 ? Qt.rgba(help.palette.text.r, help.palette.text.g, help.palette.text.b, 0.04)
                                                     : "transparent"
                                    Accessible.role: Accessible.Row
                                    Accessible.name: shortcutRow.modelData.keys.map(k => k.join("+")).join(" or ") + ": " + shortcutRow.modelData.action
                                    Flow {
                                        id: keysCell
                                        x: 12
                                        width: Math.min(220, table.width * 0.42)
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 6
                                        Repeater {
                                            model: shortcutRow.modelData.keys
                                            Row {
                                                id: alternative
                                                required property var modelData
                                                required property int index
                                                spacing: 6
                                                Label {
                                                    visible: alternative.index > 0
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "/"
                                                    opacity: 0.5
                                                }
                                                Combination { keys: alternative.modelData }
                                            }
                                        }
                                    }
                                    Label {
                                        id: actionLabel
                                        anchors.left: keysCell.right
                                        anchors.leftMargin: 16
                                        anchors.right: parent.right
                                        anchors.rightMargin: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: shortcutRow.modelData.action
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: "Single keys work when the player has the keyboard. In Notes, menus and popups, keys type or navigate there instead; Esc closes the popup."
            }

            Repeater {
                model: help.sections
                ColumnLayout {
                    id: section
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: section.modelData.title; font.pixelSize: 16; font.bold: true; Accessible.role: Accessible.Heading }
                    Label { Layout.fillWidth: true; text: section.modelData.text; wrapMode: Text.Wrap; lineHeight: 1.15 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Close"; onClicked: help.close() }
            }
        }
    }
}
