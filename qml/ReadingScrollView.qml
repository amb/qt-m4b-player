import QtQuick
import QtQuick.Controls

// Scrollable reading area for Help, Settings and Info. Desktop styles such as
// Breeze hide scroll bars until the user scrolls, which leaves no hint that a
// long page continues. Here the bar stays visible whenever the content
// overflows, drawn with a visible handle in the text colour (Breeze's idle bar
// is a hairline, nearly invisible in dark schemes), its gutter is reserved so
// it never covers text, and a fade at the bottom edge shows that more content
// lies below.
Item {
    id: root
    // The single content item; ScrollView sizes itself from it.
    default property alias content: view.contentData
    property alias contentWidth: view.contentWidth
    property alias contentHeight: view.contentHeight
    readonly property alias availableWidth: view.availableWidth
    readonly property bool overflowing: view.contentHeight > view.availableHeight + 1
    readonly property var verticalBar: view.ScrollBar.vertical
    readonly property bool atEnd: verticalBar.position + verticalBar.size >= 0.999
    implicitWidth: view.implicitWidth
    implicitHeight: view.implicitHeight

    function scrollBy(steps: int): void {
        for (let i = 0; i < Math.abs(steps); ++i)
            steps > 0 ? verticalBar.increase() : verticalBar.decrease()
    }

    ScrollView {
        id: view
        objectName: "readingScrollView"
        anchors.fill: parent
        clip: true
        // A fixed gutter: following the bar's hover width would loop.
        rightPadding: root.overflowing ? 16 : 0
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            id: bar
            objectName: "readingScrollBar"
            parent: view
            x: view.width - width - 3
            y: 3
            height: view.height - 6
            policy: root.overflowing ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            minimumSize: 0.08
            padding: 0
            contentItem: Rectangle {
                implicitWidth: bar.hovered || bar.pressed ? 11 : 8
                radius: width / 2
                color: view.palette.windowText
                opacity: bar.pressed ? 0.7 : bar.hovered ? 0.55 : 0.42
                Behavior on implicitWidth { NumberAnimation { duration: 90 } }
            }
            background: Rectangle {
                radius: width / 2
                color: view.palette.windowText
                opacity: bar.hovered || bar.pressed ? 0.08 : 0.04
            }
            Accessible.name: "Scroll"
        }
    }
    Rectangle {
        objectName: "moreContentFade"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: view.rightPadding
        anchors.bottom: parent.bottom
        height: 40
        opacity: root.overflowing && !root.atEnd ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(view.palette.window.r, view.palette.window.g, view.palette.window.b, 0) }
            GradientStop { position: 1; color: view.palette.window }
        }
    }
}
