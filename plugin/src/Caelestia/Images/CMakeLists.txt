qml_module(caelestia-images
    URI Caelestia.Images
    SOURCES
        cachingimageprovider.cpp
        decoder.cpp
        imagecacher.cpp
        imageanalyser.cpp
        iutils.cpp
    LIBRARIES
        Qt::Gui
        Qt::Quick
        Qt::Concurrent
        JPEG::JPEG
        wuffs
        caelestia-util
)
