#version 330 core

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

uniform sampler2D uBaseColorTexture;
uniform sampler2D uMetallicRoughnessTexture;
uniform sampler2D uEmissiveTexture;
uniform sampler2D uOcclusionTexture;
uniform sampler2D uDirLightShadowMap;
uniform sampler2D uNormalTexture;

uniform float uNormalScale;

in vec3 vBitengants;

out vec3 fColor;

void main()
{
   // Need another normalization because interpolation of vertex attributes does not maintain unit length
   vec3 viewSpaceNormal = normalize(vBitengants);
   fColor = viewSpaceNormal;
}