#pragma once

#include "tiny_gltf.h"
#include "utils/GLFWHandle.hpp"
#include "utils/cameras.hpp"
#include "utils/filesystem.hpp"
#include "utils/shaders.hpp"

  static float lightTheta = 0.f;
  static float lightPhi = 0.f;

class ViewerApplication
{
public:
  ViewerApplication(const fs::path &appPath, uint32_t width, uint32_t height,
      const fs::path &gltfFile, const std::vector<float> &lookatArgs,
      const std::string &vertexShader, const std::string &fragmentShader,
      const fs::path &output);

  int run();

private:
  // A range of indices in a vector containing Vertex Array Objects
  struct VaoRange
  {
    GLsizei begin; // Index of first element in vertexArrayObjects
    GLsizei count; // Number of elements in range
  };

  GLsizei m_nWindowWidth = 1280;
  GLsizei m_nWindowHeight = 720;

  const fs::path m_AppPath;
  const std::string m_AppName;
  const fs::path m_ShadersRootPath;

  fs::path m_gltfFilePath;
  std::string m_vertexShader = "shadowMapShader.vs.glsl";
  std::string m_fragmentShader = "pbr_directional_light.fs.glsl";

  GLint m_uViewMatrixLocation;
  GLint m_uProjectionMatrixLocation;
  GLint m_uModelMatrixLocation;
  GLint m_ulightDirection;
  GLint m_ulightIntensity;
  GLint m_uBaseColorTexture;
  GLint m_uBaseColorFactor;
  GLint m_uMetallicRoughnessTexture;
  GLint m_uMetallicFactor;
  GLint m_uRoughnessFactor;
  GLint m_uEmissiveTexture;
  GLint m_uEmissiveFactor;
  GLint m_uOcclusionTexture;
  GLint m_uOcclusionStrength;
  GLint m_uApplyOcclusion;
  GLint m_uLightSpaceMatrix;

  const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
  GLuint m_depthMapFBO;
  GLuint m_depthMap;

  glm::vec3 m_bboxMin, m_bboxMax;

  bool m_hasUserCamera = false;
  Camera m_userCamera;

  fs::path m_OutputPath;

  // Order is important here, see comment below
  const std::string m_ImGuiIniFilename;
  // Last to be initialized, first to be destroyed:
  GLFWHandle m_GLFWHandle{int(m_nWindowWidth), int(m_nWindowHeight),
      "glTF Viewer",
      m_OutputPath.empty()}; // show the window only if m_OutputPath is empty

  GLProgram m_glslProgram;

  bool loadGltfFile(tinygltf::Model & model);
  std::vector<GLuint> createTextureObjects(const tinygltf::Model &model) const;
  std::vector<GLuint> createBufferObjects(const tinygltf::Model &model);
  std::vector<GLuint> createVertexArrayObjects(const tinygltf::Model &model,
  const std::vector<GLuint> &bufferObjects,
  std::vector<VaoRange> &meshIndexToVaoRange);
  void loadShaderPrograms();
  /*
    ! THE ORDER OF DECLARATION OF MEMBER VARIABLES IS IMPORTANT !
    - m_ImGuiIniFilename.c_str() will be used by ImGUI in ImGui::Shutdown, which
    will be called in destructor of m_GLFWHandle. So we must declare
    m_ImGuiIniFilename before m_GLFWHandle so that m_ImGuiIniFilename
    destructor is called after.
    - m_GLFWHandle must be declared before the creation of any object managing
    OpenGL resources (e.g. GLProgram, GLShader) because it is responsible for
    the creation of a GLFW windows and thus a GL context which must exists
    before most of OpenGL function calls.
  */
  void createShdowmap();
};

static const auto computeDirectionVectorUp = [](float phiRadians, float thetaRadians)
{
    const auto cosPhi = glm::cos(phiRadians);
    const auto sinPhi = glm::sin(phiRadians);
    const auto cosTheta = glm::cos(thetaRadians);
    return -glm::normalize(glm::vec3(sinPhi * cosTheta, -glm::sin(thetaRadians), cosPhi * cosTheta));
};