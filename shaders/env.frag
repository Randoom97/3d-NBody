#version 330 core

layout(location = 0) out vec4 color;

in vec3 UV;

uniform samplerCube colorSampler;

void main(){
    color = vec4(texture(colorSampler, UV).rgb, 1.0);
}