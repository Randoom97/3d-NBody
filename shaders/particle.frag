#version 430 core
layout(location = 0) out vec4 color;

in vec4 gColor;

void main(){
    color = gColor;
}