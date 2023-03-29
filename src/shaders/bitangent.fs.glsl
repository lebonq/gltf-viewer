#version 330 core

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;
in vec3 vTangents;
in vec3 vBitengants;

in mat4 vModelMatrix;

out vec3 fColor;
//stolen from here http://www.thetenthplanet.de/archives/1180
vec3 computeTangent(vec3 position, vec3 normal, vec2 texCoord)
{
    vec3 dp1 = dFdx(position);
    vec3 dp2 = dFdy(position);
    vec2 duv1 = dFdx(texCoord);
    vec2 duv2 = dFdy(texCoord);

    // solve the linear system
    vec3 dp2perp = cross( dp2, normal );
    vec3 dp1perp = cross( normal, dp1 );
    vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;

    return tangent;
}

void main()
{
     vec3 N;
   //Compute tangent if nor precomputed
      if((vTangents.x == 0.0 && vTangents.y == 0.0 && vTangents.z == 0.0) && (vBitengants.x == 0.0 && vBitengants.y == 0.0 && vBitengants.z == 0.0)){
        vec3 T = computeTangent(vViewSpacePosition, vViewSpaceNormal, vTexCoords);
        vec3 T_space = normalize(vec3(vModelMatrix * vec4(T,1.0)));
        vec3 B = cross(vViewSpaceNormal, T_space) * -1.0;
        N = normalize(B);
      }
      else{
        N = normalize(vBitengants);
      }
   fColor = N;
}