#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec4 aTangent;

out vec3 vViewSpacePosition;
out vec3 vViewSpaceNormal;
out vec2 vTexCoords;
out vec4 vFragPosLightSpace;
out vec3 vFragPos;
out vec3 vTangents;
out vec3 vBitengants;

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
	vTexCoords = aTexCoords;

    //Shadow Mapping
    vFragPos = vec3(uModelMatrix * vec4(aPosition, 1.0));
    vFragPosLightSpace = uLightSpaceMatrix * vec4(vFragPos, 1.0);
    //NormalMapping

    if(aTangent.x == 0.0 && aTangent.y == 0.0 && aTangent.z == 0.0)
    {
        vTangents = vec3(0.0, 0.0, 0.0);
        vBitengants = vec3(0.0, 0.0, 0.0);
    }
    else
    {
        vTangents = normalize(vec3(uModelMatrix * aTangent));
        vBitengants = cross(vViewSpaceNormal, vTangents) * aTangent.w;
    }

    gl_Position =  vModelViewProjMatrix * vec4(aPosition, 1.);
}