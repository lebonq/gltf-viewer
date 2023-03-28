#version 330 core

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;
in vec3 vTangents;
in vec3 vBitengants;

in mat4 vModelMatrix;

out vec3 fColor;
//free;y inspired from here http://www.thetenthplanet.de/archives/1180
vec4 computeTangent(vec3 position, vec3 normal, vec2 texCoord)
{
  vec3 dp1 = dFdx(position);
  vec3 dp2 = dFdy(position);
  vec2 duv1 = dFdx(texCoord);
  vec2 duv2 = dFdy(texCoord);

  // Solve the linear system of equations to get the tangent and bitangent vectors
  vec3 tangent = normalize(dp2 * duv1.y - dp1 * duv2.y);
  vec3 bitangent = normalize(dp1 * duv2.x - dp2 * duv1.x);

  // Calculate the handedness of the tangent basis
  vec3 tangentCrossBitangent = cross(tangent, bitangent);
  float handedness = (dot(tangentCrossBitangent, normal) < 0.0) ? -1.0 : 1.0;

  return vec4(tangent, handedness);
}

void main()
{
     vec3 N;
   //Compute tangent if nor precomputed
      if((vTangents.x == 0.0 && vTangents.y == 0.0 && vTangents.z == 0.0) && (vBitengants.x == 0.0 && vBitengants.y == 0.0 && vBitengants.z == 0.0)){
        vec4 T = computeTangent(vViewSpacePosition, vViewSpaceNormal, vTexCoords);
        vec3 T_space = normalize(vec3(vModelMatrix * T));
        vec3 B = cross(vViewSpaceNormal, T_space) * T.w;
        N = normalize(B);
      }
      else{
        N = normalize(vBitengants);
      }
   fColor = N;
}