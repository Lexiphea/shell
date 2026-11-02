import QtQuick
import Quickshell

FontLoader {
    // Skip the bundled copy when the font is installed, so it isn't loaded twice
    source: Qt.fontFamilies().includes("Google Sans Flex") ? "" : Quickshell.shellPath("assets/google-sans-flex/GoogleSansFlex-VariableFont_GRAD,ROND,opsz,slnt,wdth,wght.ttf")
}
