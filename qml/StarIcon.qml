import QtQuick

Canvas {
    id: star
    property real fraction: 0
    property color color: "black"
    onFractionChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        let ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        let radius = Math.max(0, Math.min(width, height) / 2 - 2)
        ctx.beginPath()
        for (let i = 0; i < 10; ++i) {
            let angle = -Math.PI / 2 + i * Math.PI / 5
            let r = i % 2 === 0 ? radius : radius * 0.43
            let x = width / 2 + r * Math.cos(angle)
            let y = height / 2 + r * Math.sin(angle)
            if (i === 0) ctx.moveTo(x, y)
            else ctx.lineTo(x, y)
        }
        ctx.closePath()
        ctx.strokeStyle = color.toString()
        ctx.lineWidth = 1.5
        ctx.stroke()
        ctx.save()
        ctx.clip()
        ctx.fillStyle = color.toString()
        ctx.fillRect(0, 0, width * Math.max(0, Math.min(1, fraction)), height)
        ctx.restore()
    }
}
