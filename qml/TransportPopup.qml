pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Popup {
    id: popup
    required property Item anchorItem
    required property Item boundsItem
    property real preferredWidth: 300
    property real preferredHeight: contentItem ? contentItem.implicitHeight + topPadding + bottomPadding : 0
    readonly property real gap: 8
    property real availableAbove: 0
    parent: anchorItem
    width: Math.min(preferredWidth, Math.max(1, boundsItem.width - 16))
    height: Math.min(preferredHeight, availableAbove)
    padding: 16
    margins: 8
    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function reposition(): void {
        let point = anchorItem.mapToItem(boundsItem, 0, 0)
        availableAbove = Math.max(1, point.y - gap - margins)
        x = (anchorItem.width - width) / 2
        y = -height - gap
    }
    onAboutToShow: reposition()
    onHeightChanged: if (visible) y = -height - gap
    onWidthChanged: if (visible) x = (anchorItem.width - width) / 2
    onClosed: {
        // Opening Notes/EQ activates another window; never steal its focus.
        if (anchorItem.enabled && anchorItem.Window.window && anchorItem.Window.window.active)
            anchorItem.forceActiveFocus()
    }
    Connections {
        target: popup.boundsItem
        function onWidthChanged() { if (popup.visible) Qt.callLater(popup.reposition) }
        function onHeightChanged() { if (popup.visible) Qt.callLater(popup.reposition) }
    }
    Connections {
        target: popup.anchorItem.parent
        function onXChanged() { if (popup.visible) Qt.callLater(popup.reposition) }
        function onYChanged() { if (popup.visible) Qt.callLater(popup.reposition) }
    }
    // All four popups share the native theme surface, including its foreground
    // contrast and shadow. A palette-only replacement can clash with Breeze's
    // Kirigami text colours when the platform and controls themes differ.
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.32) }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 } }
}
