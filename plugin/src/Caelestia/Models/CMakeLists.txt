qml_module(caelestia-models
    URI Caelestia.Models
    SOURCES
        filesystemmodel.cpp
        appdb.cpp
        fuzzy.cpp
    LIBRARIES
        Qt::Gui
        Qt::Concurrent
        Qt::Sql
        vicinae-fuzzy
)
