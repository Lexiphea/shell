#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

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

const float TAU = 6.28318530718;

// Distance to y = amplitude * sin(waveK * x + phase0) for x in [start, end], with round caps at both ends. Inside the
// span it's the vertical gap scaled by the slope, which is close enough for the gentle waves this draws
float linearDistance(vec2 p) {
    float cy = size.y * 0.5;
    vec2 a = vec2(start, cy + amplitude * sin(waveK * start + phase0));
    vec2 b = vec2(end, cy + amplitude * sin(waveK * end + phase0));
    float d = min(length(p - a), length(p - b));
    if (p.x >= start && p.x <= end) {
        float t = waveK * p.x + phase0;
        float slope = amplitude * waveK * cos(t);
        d = min(d, abs(p.y - cy - amplitude * sin(t)) / sqrt(1.0 + slope * slope));
    }
    return d;
}

// The same along an arc, where the wave displaces the radius and its phase advances with arc length
float arcDistance(vec2 p) {
    vec2 c = p - size * 0.5;
    float rho = length(c);
    float ra = radius + amplitude * sin(phase0);
    float rb = radius + amplitude * sin(waveK * end * radius + phase0);
    float d = min(length(c - ra * vec2(cos(start), sin(start))), length(c - rb * vec2(cos(start + end), sin(start + end))));
    float t = mod(atan(c.y, c.x) - start, TAU);
    if (t <= end) {
        float ph = waveK * t * radius + phase0;
        float slope = amplitude * waveK * radius * cos(ph) / max(rho, 1e-3);
        d = min(d, abs(rho - radius - amplitude * sin(ph)) / sqrt(1.0 + slope * slope));
    }
    return d;
}

void main() {
    vec2 p = qt_TexCoord0 * size;
    float d = arc > 0.5 ? arcDistance(p) : linearDistance(p);
    float coverage = clamp(0.5 - (d - lineWidth * 0.5) * dpr, 0.0, 1.0);
    fragColor = color * coverage * qt_Opacity;
}
