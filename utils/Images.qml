pragma Singleton

import Quickshell

Singleton {
    readonly property list<string> validImageExtensions: ["jpg", "jpeg", "png", "webp", "tif", "tiff", "svg"]
    // Matches util::videoExtensions in the plugin and VIDEO_EXTENSIONS in the CLI
    readonly property list<string> validVideoExtensions: ["mp4", "mkv", "webm", "mov", "avi"]
    readonly property list<string> validWallpaperExtensions: [...validImageExtensions, ...validVideoExtensions]

    function isVideo(path: string): bool {
        return validVideoExtensions.includes(path.slice(path.lastIndexOf(".") + 1).toLowerCase());
    }
}
