#version 330 core

layout(location = 0) in vec3 pos;

uniform mat4 viewMatrix;

out vec3 UV;

void main(){
    gl_Position = vec4(pos,1);
    UV = (viewMatrix*vec4(pos, 1)).rgb;
}