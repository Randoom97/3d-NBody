#version 430 core
layout (points) in;
layout (triangle_strip, max_vertices = 8) out;

in float vMass[];
in vec3 vColor[];

out vec4 gColor;

uniform mat4 viewMatrix;
uniform mat3 rotationMatrix;

void main(){
    vec4 centerColor = vec4(1,1,1,1);
    vec4 edgeColor = vec4(vColor[0],0);

    float size = 0.1*pow(vMass[0],1./3.);
    vec4 center = gl_in[0].gl_Position;
    vec4 left = viewMatrix*(center + vec4(rotationMatrix*vec3(-size,0,0),0));
    vec4 right = viewMatrix*(center + vec4(rotationMatrix*vec3(size,0,0),0));
    vec4 bottom = viewMatrix*(center + vec4(rotationMatrix*vec3(0,-size,0),0));
    vec4 top = viewMatrix*(center + vec4(rotationMatrix*vec3(0,size,0),0));
    center = viewMatrix*center;
    
    gl_Position = left;
    gColor = edgeColor;
    EmitVertex();
    gl_Position = bottom;
    gColor = edgeColor;
    EmitVertex();
    gl_Position = center;
    gColor = centerColor;
    EmitVertex();
    gl_Position = right;
    gColor = edgeColor;
    EmitVertex();
    gl_Position = top;
    gColor = edgeColor;
    EmitVertex();
    gl_Position = top;
    gColor = edgeColor;
    EmitVertex();
    gl_Position = center;
    gColor = centerColor;
    EmitVertex();
    gl_Position = left;
    gColor = edgeColor;
    EmitVertex();
    EndPrimitive();
}