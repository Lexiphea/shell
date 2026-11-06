qml_module(caelestia-components
    URI Caelestia.Components
    SOURCES
        animatedrepeater.cpp
        lazylistview.cpp
        wavyline.cpp
        buttonrow.cpp
        circularindicatormanager.cpp
        linearindicatormanager.cpp
        sparklineitem.cpp
        visualiserbars.cpp
        roundedmask.cpp
    LIBRARIES
        Qt::Gui
        Qt::Quick
        caelestia-core
    DEPENDENCIES
        Caelestia
)

qt_add_shaders(caelestia-components "components_shaders"
    BATCHABLE OPTIMIZED NOHLSL NOMSL
    GLSL "300es,330"
    PREFIX "/"
    FILES
        shaders/roundedmask.frag
        shaders/roundedmask.vert
        shaders/wavyline.frag
        shaders/wavyline.vert
)

# Used from QML ShaderEffects, so built for the same GLSL versions as Qt's default vertex shader
qt_add_shaders(caelestia-components "components_effect_shaders"
    BATCHABLE OPTIMIZED NOHLSL NOMSL
    GLSL "100es,120,150"
    PREFIX "/"
    FILES
        shaders/covermask.frag
)
