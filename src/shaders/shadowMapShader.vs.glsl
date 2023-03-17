#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec3 vViewSpacePosition;
out vec3 vViewSpaceNormal;
out vec2 vTexCoords;
//out vec4 vFragPosLightSpace;
out vec3 vFragPos;

uniform mat4 uViewMatrix;
uniform mat4 uModelMatrix;
uniform mat4 uProjectionMatrix;
uniform mat4 uLightSpaceMatrix;

void main()
{
    mat4 vModelViewMatrix = uViewMatrix * uModelMatrix;
    mat4 vModelViewProjMatrix = uProjectionMatrix * vModelViewMatrix;
    mat4 vNormalMatrix = transpose(inverse(vModelViewMatrix));
    vViewSpacePosition = vec3(vModelViewMatrix * vec4(aPosition, 1.));
	vViewSpaceNormal = normalize(vec3(vNormalMatrix * vec4(aNormal, 0.)));
    //vFragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
	vTexCoords = aTexCoords;
    vFragPos = vec3(uModelMatrix * vec4(aPosition, 1.0));
    gl_Position =  vModelViewProjMatrix * vec4(aPosition, 1.);
}