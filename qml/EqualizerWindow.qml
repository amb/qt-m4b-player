pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: equalizer
    required property var player
    objectName: "equalizerWindow"
    title: "Audiobook EQ"
    width: 480
    height: 620
    minimumWidth: 390
    minimumHeight: 600
    flags: Qt.Tool
    visible: false
    function openPanel(): void { show(); raise(); requestActivate() }
    Shortcut { sequence: "Escape"; enabled: equalizer.visible; context: Qt.WindowShortcut; onActivated: equalizer.close() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 8
        RowLayout {
            Label { text: "Speech tone"; font.pixelSize: 22; Layout.fillWidth: true }
            Switch {
                text: "EQ"
                checked: equalizer.player.equalizerEnabled
                onToggled: equalizer.player.setEqualizerEnabled(checked)
                Accessible.name: "Enable EQ"
            }
        }
        Canvas {
            id: graph
            objectName: "equalizerGraph"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 64
            property var response: equalizer.player.equalizerResponse
            onResponseChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Connections { target: equalizer; function onPaletteChanged() { graph.requestPaint() } }
            onPaint: {
                let ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = equalizer.palette.mid.toString()
                ctx.lineWidth = 1
                for (let gain of [-12, 0, 12]) {
                    let y = height * (0.5 - gain / 32)
                    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                }
                ctx.strokeStyle = equalizer.palette.highlight.toString()
                ctx.lineWidth = 2
                ctx.beginPath()
                for (let i = 0; i < response.length; ++i) {
                    let x = i * width / (response.length - 1)
                    let y = height * (0.5 - response[i] / 32)
                    if (i === 0) ctx.moveTo(x, y)
                    else ctx.lineTo(x, y)
                }
                ctx.stroke()
            }
            Accessible.role: Accessible.Graphic
            Accessible.name: "Tone response before headroom, boost and limiting, 20 Hz to 20 kHz"
        }
        RowLayout {
            Label { text: "20 Hz" }
            Item { Layout.fillWidth: true }
            Label { text: "20 kHz" }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16
            ColumnLayout {
                Layout.minimumWidth: 104
                Layout.preferredWidth: 104
                Layout.maximumWidth: 104
                Dial {
                    id: tone
                    implicitWidth: 96; implicitHeight: 96
                    Layout.alignment: Qt.AlignHCenter
                    from: -12; to: 12; stepSize: 0.1
                    value: equalizer.player.equalizerTone
                    enabled: equalizer.player.equalizerEnabled
                    onMoved: equalizer.player.setEqualizerTone(value)
                    Keys.onSpacePressed: equalizer.player.setEqualizerTone(0)
                    TapHandler { onDoubleTapped: equalizer.player.setEqualizerTone(0) }
                    Accessible.name: "Tone: warm to clear"
                    Accessible.description: "Minus 12 to plus 12 decibels; zero is neutral"
                }
                Label { Layout.alignment: Qt.AlignHCenter; text: "Tone " + equalizer.player.equalizerTone.toFixed(1) + " dB" }
                Label { Layout.alignment: Qt.AlignHCenter; text: "Warm ↔ Clear" }
            }
            ColumnLayout {
                Layout.minimumWidth: 104
                Layout.preferredWidth: 104
                Layout.maximumWidth: 104
                Dial {
                    implicitWidth: 96; implicitHeight: 96
                    Layout.alignment: Qt.AlignHCenter
                    from: 0; to: 1; stepSize: 0.01
                    value: Math.log(equalizer.player.equalizerFrequency / 200) / Math.log(20)
                    enabled: equalizer.player.equalizerEnabled
                    onMoved: equalizer.player.setEqualizerFrequency(200 * Math.pow(20, value))
                    Keys.onSpacePressed: equalizer.player.setEqualizerFrequency(1000)
                    TapHandler { onDoubleTapped: equalizer.player.setEqualizerFrequency(1000) }
                    Accessible.name: "EQ pivot frequency"
                    Accessible.description: "200 to 4000 Hz on a logarithmic scale"
                }
                Label { Layout.alignment: Qt.AlignHCenter; text: Math.round(equalizer.player.equalizerFrequency) + " Hz" }
                Label { Layout.alignment: Qt.AlignHCenter; text: "Balance point" }
            }
            ColumnLayout {
                Layout.minimumWidth: 104
                Layout.preferredWidth: 104
                Layout.maximumWidth: 104
                Dial {
                    objectName: "boostKnob"
                    implicitWidth: 96; implicitHeight: 96
                    Layout.alignment: Qt.AlignHCenter
                    from: 0; to: 24; stepSize: 0.5
                    value: equalizer.player.equalizerBoost
                    enabled: equalizer.player.equalizerEnabled
                    onMoved: equalizer.player.setEqualizerBoost(value)
                    Keys.onSpacePressed: equalizer.player.setEqualizerBoost(0)
                    TapHandler { onDoubleTapped: equalizer.player.setEqualizerBoost(0) }
                    Accessible.name: "Boost"
                    Accessible.description: "Zero to 24 decibels with peak limiting; zero adds no gain"
                }
                Label { Layout.alignment: Qt.AlignHCenter; text: "Boost +" + equalizer.player.equalizerBoost.toFixed(1) + " dB" }
                Label { Layout.alignment: Qt.AlignHCenter; text: "Peak-limited" }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Boost lifts quiet passages and limits loud peaks. Higher settings reduce dynamics."
        }
        TextMetrics { id: autoLabelMetrics; font: autoButton.font; text: autoButton.idleText }
        TextMetrics { id: cancelLabelMetrics; font: autoButton.font; text: autoButton.busyText }
        FontMetrics { id: autoButtonFontMetrics; font: autoButton.font }
        Button {
            id: autoButton
            objectName: "autoLoudnessButton"
            readonly property string idleText: "Auto loudness: −18 LKFS"
            readonly property string busyText: "Cancel measurement"
            Layout.alignment: Qt.AlignHCenter
            Layout.minimumWidth: Math.max(autoLabelMetrics.width, cancelLabelMetrics.width) + leftPadding + rightPadding
            Layout.preferredWidth: Layout.minimumWidth
            Layout.maximumWidth: Layout.minimumWidth
            // Breeze gives these labels different natural text heights. Keep
            // both button dimensions stable so the expanding graph cannot move
            // the knobs when measurement starts or finishes.
            Layout.minimumHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                Math.max(autoButtonFontMetrics.height, autoLabelMetrics.height, cancelLabelMetrics.height) + topPadding + bottomPadding)
            Layout.preferredHeight: Layout.minimumHeight
            Layout.maximumHeight: Layout.minimumHeight
            text: equalizer.player.loudnessAnalyzing ? busyText : idleText
            enabled: equalizer.player.loudnessAvailable || equalizer.player.loudnessAnalyzing
            onClicked: {
                if (equalizer.player.loudnessAnalyzing) equalizer.player.cancelLoudness()
                else equalizer.player.autoLoudness()
            }
        }
        FontMetrics { id: statusMetrics; font: statusText.font }
        Flickable {
            id: statusArea
            Layout.fillWidth: true
            // Status messages reserve the same three lines in every state.
            // Longer errors scroll within this area instead of moving controls.
            Layout.minimumHeight: statusMetrics.lineSpacing * 3
            Layout.preferredHeight: Layout.minimumHeight
            Layout.maximumHeight: Layout.minimumHeight
            clip: true
            contentWidth: width
            contentHeight: statusText.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Label {
                id: statusText
                width: statusArea.width
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                text: equalizer.player.loudnessStatus.length > 0 ? equalizer.player.loudnessStatus
                    : "Measures the current chapter when it is longer than one minute."
                onTextChanged: statusArea.contentY = 0
            }
        }
        RowLayout {
            objectName: "equalizerFooter"
            Label {
                Layout.fillWidth: true
                text: equalizer.player.equalizerEnabled
                    ? "Auto headroom −" + Math.abs(equalizer.player.equalizerTone).toFixed(1) + " dB" : "Bypassed"
            }
            Button { text: "Reset"; onClicked: equalizer.player.resetEqualizer() }
        }
    }
}
