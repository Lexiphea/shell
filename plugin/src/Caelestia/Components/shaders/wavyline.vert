#version 440

layout(location = 0) in vec4 qt_VertexPosition;
layout(location = 1) in vec2 qt_VertexTexCoord;

layout(location = 0) out vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float dpr;
    vec2 size;
    vec4 color;
    float lineWidth;
    float amplitude;
    float waveK;
    float phase0;
    float start;
    float end;
    float radius;
    float arc;
};

void main() {
    gl_Position = qt_Matrix * qt_VertexPosition;
    qt_TexCoord0 = qt_VertexTexCoord;
}
