#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float angle;
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D maskSource;

// Masks source with maskSource rotated clockwise by angle (radians) about the centre, so a spinning mask needs no
// re-render. Undoing the rotation finds where each point falls on the unrotated mask
void main() {
    vec2 c = qt_TexCoord0 - 0.5;
    float s = sin(angle);
    float co = cos(angle);
    vec2 m = vec2(co * c.x + s * c.y, co * c.y - s * c.x) + 0.5;
    float inside = step(0.0, m.x) * step(0.0, m.y) * step(m.x, 1.0) * step(m.y, 1.0);
    fragColor = texture(source, qt_TexCoord0) * texture(maskSource, m).a * inside * qt_Opacity;
}
