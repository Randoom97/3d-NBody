#version 430 core
#define PI 3.14159265358

layout(location = 0) in int index;

struct Particle {
    vec4 pos;
    vec4 vel;
    double mass;
    double padding;
};

layout(std430, binding = 1) buffer particleInputBuffer{
    Particle particles[];
} inBuffer;

uniform float maxVelocity;

out vec3 vColor;
out float vMass;

void main(){
    gl_Position = vec4(inBuffer.particles[index].pos.xyz, 1);
    vMass = float(inBuffer.particles[index].mass);
    // color computation
    float colorRotation = max(0, min(PI, PI*(length(inBuffer.particles[index].vel.xyz)/maxVelocity)));
    vColor = vec3(max(0, -cos(colorRotation)), sin(colorRotation), max(0, cos(colorRotation)));
}