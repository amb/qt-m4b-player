pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AudiobookPlayer

// The single home for global preferences. Changes apply and save immediately;
// closing the window never reverts them. Book choices, EQ and volume stay in
// the player.
ApplicationWindow {
    id: settings
    required property AppPreferences preferences
    objectName: "settingsWindow"
    title: "Settings"
    width: 520
    height: Math.min(content.implicitHeight + 40, 760)
    minimumWidth: 440
    minimumHeight: 360
    flags: Qt.Dialog
    visible: false
    color: palette.window

    function openSettings(): void {
        show()
        raise()
        requestActivate()
        resumeSwitch.forceActiveFocus()
    }

    Shortcut { sequence: "Escape"; enabled: settings.visible; context: Qt.WindowShortcut; onActivated: settings.close() }
    Shortcut { sequences: [StandardKey.Close]; enabled: settings.visible; context: Qt.WindowShortcut; onActivated: settings.close() }

    component Section: ColumnLayout {
        id: section
        default property alias rows: body.data
        property string heading: ""
        Layout.fillWidth: true
        spacing: 6
        Label {
            text: section.heading
            font.bold: true
            Accessible.role: Accessible.Heading
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: body.implicitHeight + 24
            radius: 10
            color: settings.palette.base
            border.color: settings.palette.mid
            border.width: 1
            ColumnLayout {
                id: body
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
            }
        }
    }
    component Explanation: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        opacity: 0.7
        font.pixelSize: Math.max(11, settings.font.pixelSize - 1)
    }
    component SizeRow: RowLayout {
        id: sizeRow
        property string label: ""
        property int value: 0
        property int from: 0
        property int to: 0
        signal changed(int value)
        Layout.fillWidth: true
        Label { text: sizeRow.label; Layout.fillWidth: true; wrapMode: Text.Wrap }
        SpinBox {
            from: sizeRow.from
            to: sizeRow.to
            value: sizeRow.value
            editable: true
            onValueModified: sizeRow.changed(value)
            textFromValue: (value, locale) => value + " px"
            valueFromText: (text, locale) => parseInt(text)
            Accessible.name: sizeRow.label
        }
    }

    ReadingScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: content.implicitHeight + 40
        ColumnLayout {
            id: content
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 16

            Section {
                heading: "Playback"
                Switch {
                    id: resumeSwitch
                    objectName: "resumeAtLaunch"
                    Layout.fillWidth: true
                    text: "Resume the most recently played audiobook at launch"
                    checked: settings.preferences.resumeAtLaunch
                    onToggled: settings.preferences.resumeAtLaunch = checked
                }
                Explanation { text: "Opens paused at its saved position. A book opened from the file manager or command line always takes priority." }
                Switch {
                    objectName: "stopAfterChapterDefault"
                    Layout.fillWidth: true
                    text: "Enable Stop After Chapter for books without a saved preference"
                    checked: settings.preferences.stopAfterChapterDefault
                    onToggled: settings.preferences.stopAfterChapterDefault = checked
                }
                Explanation { text: "A book's own On or Off choice always wins. Changing this does not affect the book that is already open." }
            }

            Section {
                heading: "Player appearance"
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Cover display"; Layout.fillWidth: true }
                    ComboBox {
                        objectName: "coverDisplay"
                        model: ["Fit", "Fill"]
                        currentIndex: settings.preferences.fillCover ? 1 : 0
                        onActivated: index => settings.preferences.fillCover = index === 1
                        Accessible.name: "Cover display"
                    }
                }
                Explanation { text: "Fit shows the whole cover above the controls. Fill covers the player behind translucent controls; drag the cover vertically to reframe it. Captions always stay above the controls." }
                SizeRow {
                    objectName: "playerTextSize"
                    label: "Player text size"
                    from: settings.preferences.minimumPlayerTextSize
                    to: settings.preferences.maximumPlayerTextSize
                    value: settings.preferences.playerTextSize
                    onChanged: value => settings.preferences.playerTextSize = value
                }
                SizeRow {
                    objectName: "captionSize"
                    label: "Subtitle text size"
                    from: settings.preferences.minimumCaptionSize
                    to: settings.preferences.maximumCaptionSize
                    value: settings.preferences.captionSize
                    onChanged: value => settings.preferences.captionSize = value
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Subtitle background transparency"; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Slider {
                        id: transparencySlider
                        objectName: "captionTransparency"
                        Layout.preferredWidth: 160
                        from: 0
                        to: 100
                        stepSize: 1
                        snapMode: Slider.SnapAlways
                        value: settings.preferences.captionTransparency
                        onMoved: settings.preferences.captionTransparency = Math.round(value)
                        Accessible.name: "Subtitle background transparency"
                    }
                    Label {
                        text: settings.preferences.captionTransparency + "%"
                        Layout.preferredWidth: 44
                        horizontalAlignment: Text.AlignRight
                    }
                }
                Explanation { text: "0% is a solid background; 100% removes it. Caption text keeps its black outline." }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: preview.implicitHeight + 24
                    radius: 8
                    color: settings.palette.window
                    border.color: settings.palette.mid
                    Accessible.role: Accessible.StaticText
                    Accessible.name: "Appearance preview"
                    Label {
                        id: preview
                        anchors.centerIn: parent
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: "Audiobook title · 12:34 remaining"
                        font.pixelSize: settings.preferences.playerTextSize
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Explanation { text: "Changes are saved immediately." }
                Button { text: "Close"; onClicked: settings.close() }
            }
        }
    }
}
