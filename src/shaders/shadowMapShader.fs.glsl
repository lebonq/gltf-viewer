#version 330 core

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;
in vec4 vFragPosLightSpace;
in vec3 vFragPos;

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;

uniform vec4 uBaseColorFactor;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform vec3 uEmissiveFactor;
uniform float uOcclusionStrength;

uniform sampler2D uBaseColorTexture;
uniform sampler2D uMetallicRoughnessTexture;
uniform sampler2D uEmissiveTexture;
uniform sampler2D uOcclusionTexture;

uniform mat4 uLightSpaceMatrix;
uniform sampler2D uDirLightShadowMap;

uniform int uApplyOcclusion;

out vec4 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(uDirLightShadowMap, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;

    return shadow;
}

void main()
{
  vec3 color = texture(uBaseColorTexture, vTexCoords).rgb;
    vec3 normal = normalize(vViewSpaceNormal);
    vec3 lightColor = vec3(1.0);
    // ambient
    vec3 ambient = 0.15 * lightColor;
    // diffuse
    float diff = max(dot(uLightDirection, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // specular
    vec3 viewDir = normalize(vec3(-7.78261,4.45689,-0.570294) - vFragPos);
    float spec = 0.0;
    vec3 halfwayDir = normalize(uLightDirection + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;    
    // calculate shadow
    float shadow = ShadowCalculation(vFragPosLightSpace);
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;    
    
    fColor = vec4(lighting, 1.0);
}
