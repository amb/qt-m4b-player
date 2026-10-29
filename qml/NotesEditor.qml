pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

ApplicationWindow {
    id: editor
    required property var player
    property string bookFingerprint: ""
    property string bookTitle: ""
    property string saveError: ""
    readonly property alias draft: textArea.text
    objectName: "notesEditor"
    title: "Notes — " + bookTitle
    width: 560
    height: 460
    minimumWidth: 390
    minimumHeight: 320
    flags: Qt.Dialog
    modality: Qt.WindowModal
    visible: false

    function openDocument(document: var): void {
        if (visible) {
            raise()
            requestActivate()
            return
        }
        if (!document.fingerprint) return
        bookFingerprint = document.fingerprint
        bookTitle = document.title
        textArea.text = document.text
        saveError = ""
        show()
        raise()
        requestActivate()
        textArea.forceActiveFocus()
    }
    function saveDocument(): void {
        saveError = player.saveNotes(bookFingerprint, textArea.text)
        if (saveError.length === 0) close()
    }
    Shortcut { sequence: "Ctrl+S"; enabled: editor.visible; context: Qt.WindowShortcut; onActivated: editor.saveDocument() }
    Shortcut { sequence: "Escape"; enabled: editor.visible; context: Qt.WindowShortcut; onActivated: editor.close() }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        Label { text: "Notes"; font.pixelSize: 24 }
        Label { text: editor.bookTitle; Layout.fillWidth: true; elide: Text.ElideRight }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Breeze's mobile text toolbar currently requires TextInput, which
            // rejects its own multiline TextArea. Keep this editor palette-aware
            // without instantiating that broken style helper.
            Basic.TextArea {
                id: textArea
                objectName: "notesText"
                palette: editor.palette
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                background: Rectangle {
                    color: editor.palette.base
                    border.color: editor.palette.mid
                    radius: 6
                }
                Accessible.name: "Notes for " + editor.bookTitle
            }
        }
        Label {
            objectName: "notesSaveError"
            visible: editor.saveError.length > 0
            text: editor.saveError
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Saved with this audiobook."; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Button { text: "Cancel"; onClicked: editor.close() }
            Button { text: "Save"; highlighted: true; onClicked: editor.saveDocument() }
        }
    }
}
