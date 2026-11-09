pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Read-only inspector for the loaded book. Nothing here edits metadata or the file.
ApplicationWindow {
    id: info
    required property var player
    readonly property var details: player.bookInfo
    readonly property bool loading: player.bookInfoLoading
    property url bookUrl
    objectName: "bookInfoWindow"
    title: "Audiobook Info"
    width: 560
    height: 640
    minimumWidth: 420
    minimumHeight: 360
    flags: Qt.Dialog
    modality: Qt.WindowModal
    visible: false
    color: palette.window

    function openInfo(): void {
        if (visible) {
            raise()
            requestActivate()
            return
        }
        bookUrl = player.fileUrl
        player.loadBookInfo()
        show()
        raise()
        requestActivate()
        doneButton.forceActiveFocus()
    }
    function formatDuration(seconds: real): string {
        let value = Math.max(0, Math.round(seconds))
        let hours = Math.floor(value / 3600)
        let minutes = Math.floor((value % 3600) / 60)
        let secs = value % 60
        return (hours ? hours + ":" + String(minutes).padStart(2, "0") : minutes) + ":" + String(secs).padStart(2, "0")
    }
    function trackList(key: string): var {
        return info.details && info.details[key] ? info.details[key] : []
    }

    Shortcut { sequences: ["Escape", "Return", "Enter"]; enabled: info.visible; context: Qt.WindowShortcut; onActivated: info.close() }

    component SectionHeading: Label {
        Layout.fillWidth: true
        Layout.topMargin: 8
        font.pixelSize: 17
        font.bold: true
        Accessible.role: Accessible.Heading
    }
    component Field: ColumnLayout {
        id: field
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        spacing: 2
        Label {
            text: field.label
            opacity: 0.7
            font.pixelSize: 13
        }
        TextEdit {
            Layout.fillWidth: true
            text: field.value.length > 0 ? field.value : "Unknown"
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            textFormat: TextEdit.PlainText
            color: info.palette.windowText
            selectionColor: info.palette.highlight
            selectedTextColor: info.palette.highlightedText
            font.pixelSize: 15
            Accessible.name: field.label
        }
    }
    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        opacity: 0.8
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        Label { text: "Audiobook Info"; font.pixelSize: 22; Accessible.role: Accessible.Heading }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 8

                SectionHeading { text: "Book" }
                Field { label: "Title"; value: info.player.title }
                Field { label: "Author"; value: info.player.author }
                Field { label: "Duration"; value: info.player.duration > 0 ? info.formatDuration(info.player.duration) : "" }
                Field { label: "Chapters"; value: String(info.player.chapters.length) }

                SectionHeading { text: "File" }
                Note { visible: info.loading; text: "Loading file details…" }
                Note {
                    visible: !info.loading && !!info.details.fileError
                    text: "File details unavailable: " + (info.details.fileError || "")
                }
                ColumnLayout {
                    visible: !info.loading && !info.details.fileError && !!info.details.path
                    Layout.fillWidth: true
                    spacing: 8
                    Field { label: "Name"; value: info.details.fileName || "" }
                    Field { objectName: "infoPath"; label: "Location"; value: info.details.path || "" }
                    Field { label: "Kind"; value: info.details.kind || "" }
                    Field { label: "Size"; value: info.details.size || "" }
                    Field { label: "Created"; value: info.details.created || "" }
                    Field { label: "Modified"; value: info.details.modified || "" }
                }

                SectionHeading { text: "Audio" }
                Note { visible: info.loading; text: "Loading audio details…" }
                Note {
                    visible: !info.loading && !!info.details.trackError
                    text: "Track details unavailable: " + (info.details.trackError || "")
                }
                Note {
                    visible: !info.loading && !info.details.trackError && info.trackList("audioTracks").length === 0
                    text: "No audio tracks found."
                }
                Repeater {
                    model: info.loading ? [] : info.trackList("audioTracks")
                    ColumnLayout {
                        id: audioTrack
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 8
                        Label {
                            visible: info.trackList("audioTracks").length > 1
                            text: "Track " + (audioTrack.index + 1)
                            font.bold: true
                        }
                        Field { label: "Codec"; value: audioTrack.modelData.codec }
                        Field { label: "Average bitrate"; value: audioTrack.modelData.bitrate }
                        Field { label: "Sample rate"; value: audioTrack.modelData.sampleRate }
                        Field { label: "Channels"; value: audioTrack.modelData.channels }
                    }
                }

                SectionHeading { text: "Subtitles" }
                Note { visible: info.loading; text: "Checking embedded subtitles…" }
                ColumnLayout {
                    visible: !info.loading && !info.details.trackError
                    Layout.fillWidth: true
                    spacing: 8
                    Field {
                        label: "Embedded tracks"
                        value: info.trackList("subtitleTracks").length > 0 ? String(info.trackList("subtitleTracks").length) : "None"
                    }
                    Repeater {
                        model: info.loading ? [] : info.trackList("subtitleTracks")
                        ColumnLayout {
                            id: subtitleTrack
                            required property var modelData
                            readonly property bool selected: modelData.index === info.player.captionStream
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: "Track " + subtitleTrack.modelData.id; font.bold: true }
                            Field { label: "Language"; value: subtitleTrack.modelData.language }
                            Field { label: "Format"; value: subtitleTrack.modelData.format }
                            Field {
                                visible: subtitleTrack.selected
                                label: "Used for captions"
                                value: info.player.captionsEnabled ? "Yes · captions On" : "Yes · captions Off"
                            }
                        }
                    }
                    Note {
                        visible: info.player.indexingCaptions
                        text: "Captions are still being indexed."
                    }
                    Note {
                        visible: !info.player.indexingCaptions && info.trackList("subtitleTracks").length > 0
                            && info.player.captionStream < 0
                        text: "None of these tracks can be shown as captions. Supported timed-text (mov_text) tracks must contain non-empty cues."
                    }
                }
            }
        }
        RowLayout {
            objectName: "bookInfoFooter"
            Layout.fillWidth: true
            Button { text: "Copy Path"; onClicked: info.player.copyPath() }
            Button { text: "Show in File Manager"; onClicked: info.player.showInFileManager() }
            Item { Layout.fillWidth: true }
            Button { id: doneButton; text: "Done"; highlighted: true; onClicked: info.close() }
        }
    }
}
