// displayDepth.fs.glsl
#version 330

uniform sampler2D uDirLightShadowMap;

out vec3 fColor;

void main()
{
    float depth = texelFetch(uDirLightShadowMap, ivec2(gl_FragCoord.xy), 0).r;
    fColor = vec3(depth);
}