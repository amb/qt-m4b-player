import QtQuick
import QtQuick.Controls

// Measure text at the chosen width instead of relying on Basic.Dialog's
// implicit content/header/footer sizing, which can loop during layout changes.
Popup {
    id: popup
    property string title: ""
    property string message: ""
    readonly property real naturalHeight: topPadding + bottomPadding
        + heading.implicitHeight + body.implicitHeight + okay.implicitHeight + 32
    implicitHeight: naturalHeight
    height: Math.min(naturalHeight, Overlay.overlay ? Math.max(0, Overlay.overlay.height - 32) : naturalHeight)
    modal: true
    focus: true

    contentItem: Item {
        focus: true
        Accessible.role: Accessible.Dialog
        Accessible.name: popup.title
        Keys.onReturnPressed: popup.close()
        Keys.onEnterPressed: popup.close()
        Label {
            id: heading
            width: parent.width
            text: popup.title
            font.bold: true
            elide: Text.ElideRight
        }
        Flickable {
            id: scroll
            y: heading.height + 16
            width: parent.width
            height: Math.max(0, parent.height - heading.height - okay.height - 32)
            contentWidth: width
            contentHeight: body.implicitHeight
            clip: true
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            Label {
                id: body
                width: scroll.width
                text: popup.message
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            ScrollBar.vertical: ScrollBar {}
        }
        Button {
            id: okay
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            text: "OK"
            onClicked: popup.close()
        }
    }
}
