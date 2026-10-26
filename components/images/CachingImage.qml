import QtQuick
import Quickshell
import Caelestia.Images

Image {
    id: root

    property string path
    // Logical size to decode at; set a fixed size when the item animates so every frame doesn't trigger a decode
    property size targetSize: Qt.size(width, height)

    asynchronous: true
    fillMode: Image.PreserveAspectCrop
    source: IUtils.urlForPath(path, fillMode)
    sourceSize: {
        const dpr = (QsWindow.window as QsWindow)?.devicePixelRatio ?? 1;
        return Qt.size(targetSize.width * dpr, targetSize.height * dpr);
    }
}
