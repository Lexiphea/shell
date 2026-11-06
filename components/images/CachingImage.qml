import QtQuick
import Caelestia.Images

Image {
    id: root

    property string path
    // Logical size to decode at; set a fixed size when the item animates so every frame doesn't trigger a decode
    property size targetSize: Qt.size(width, height)

    asynchronous: true
    fillMode: Image.PreserveAspectCrop
    // Wait for a size, as each change starts a decode and one without a size decodes the full image
    source: sourceSize.width > 0 && sourceSize.height > 0 ? IUtils.urlForPath(path, fillMode) : ""
    // Qt scales this by the screen's pixel ratio itself
    sourceSize: targetSize
}
