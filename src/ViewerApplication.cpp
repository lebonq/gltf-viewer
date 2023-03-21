#include "ViewerApplication.hpp"

#include <iostream>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "Data.hpp"
#include "utils/cameras.hpp"
#include "utils/gltf.hpp"
#include "utils/images.hpp"

#include <stb_image_write.h>
#include <tiny_gltf.h>

void keyCallback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, 1);
  }
}

int ViewerApplication::run()
{
  // Loader shaders
  m_glslProgram_shadowMap =
      compileProgram({m_ShadersRootPath / "simpleDepthShader.vs.glsl",
          m_ShadersRootPath / "simpleDepthShader.fs.glsl"});
  m_glslProgram_shadowMap.setUniform();
  m_glslProgram_fullRender =
      compileProgram({m_ShadersRootPath / "shadowMapShader.vs.glsl",
          m_ShadersRootPath / "pbr_directional_light_shadows.fs.glsl"});
  m_glslProgram_fullRender.setUniform();

  m_glslProgram_normalRender =
      compileProgram({m_ShadersRootPath / "forward.vs.glsl",
          m_ShadersRootPath / "normals.fs.glsl"});
  m_glslProgram_normalRender.setUniform();

  m_glslProgram_noShadow =
      compileProgram({m_ShadersRootPath / "shadowMapShader.vs.glsl",
          m_ShadersRootPath / "pbr_directional_light.fs.glsl"});
  m_glslProgram_noShadow.setUniform();

  m_glslProgram_debugShadowMap =
      compileProgram({m_ShadersRootPath / "shadowMapShader.vs.glsl",
          m_ShadersRootPath / "debug.fs.glsl"});
  m_glslProgram_debugShadowMap.setUniform();

  m_glslProgram_shadowMapRendered = &m_glslProgram_shadowMap;
  m_glslProgram_rendered = &m_glslProgram_fullRender;

  glm::vec3 lightDir = glm::vec3(1.0f, 1.0f, 1.0f);
  {
    const auto sinPhi = glm::sin(lightPhi);
    const auto cosPhi = glm::cos(lightPhi);
    const auto sinTheta = glm::sin(lightTheta);
    const auto cosTheta = glm::cos(lightTheta);
    lightDir = glm::vec3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
  }
  glm::vec3 lightInt = glm::vec3(1.0f, 1.0f, 1.0f);
  bool lightFromCamera = false;
  bool applyOcclusion = true;
  bool renderShadow = true;
  bool shadowNeedUpdate = true;

  // Build projection matrix
  std::cerr << "Load model" << this->m_gltfFilePath << std::endl;
  tinygltf::Model model;
  if (!loadGltfFile(model)) {
    return -1;
  }
  std::cerr << "Loaded" << std::endl;

  computeSceneBounds(model, m_bboxMin, m_bboxMax);

  const auto diag = m_bboxMax - m_bboxMin;
  auto maxDistance = glm::length(diag);

  const auto projMatrix =
      glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  std::unique_ptr<CameraController> cameraController =
      std::make_unique<TrackballCameraController>(
          m_GLFWHandle.window(), 0.5f * maxDistance);
  if (m_hasUserCamera) {
    cameraController->setCamera(m_userCamera);
  } else {
    const auto center = 0.5f * (m_bboxMax + m_bboxMin);
    const auto up = glm::vec3(0, 1, 0);
    const auto eye =
        diag.z > 0 ? center + diag : center + 2.f * glm::cross(diag, up);
    cameraController->setCamera(Camera{eye, center, up});
  }

  std::cerr << "Load texture" << std::endl;
  auto textureObjects = createTextureObjects(model);
  std::cerr << "Loadded" << std::endl;

  GLuint whiteTexture = 0;

  // Create white texture for object with no base color texture
  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  float white[] = {1, 1, 1, 1};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, white);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);

  createShdowmap();

  std::cerr << "Create Buffer Objects" << std::endl;
  auto v_bufferObjects = createBufferObjects(model);
  std::cerr << "Created" << std::endl;

  std::cerr << "Create Vertex Array Objects" << std::endl;
  std::vector<VaoRange> v_meshToVertexArrays;
  const auto vertexArrayObjects =
      createVertexArrayObjects(model, v_bufferObjects, v_meshToVertexArrays);
  std::cerr << "Created" << std::endl;

  // Setup OpenGL state for rendering
  glEnable(GL_DEPTH_TEST);

  // Lambda function to bind material
  const auto bindMaterial = [&](const auto materialIndex,
                                const GLProgram* shader) {
    if (materialIndex >= 0) {
      const auto &material = model.materials[materialIndex];
      const auto &pbrMetallicRoughness = material.pbrMetallicRoughness;
      if (shader->m_uBaseColorFactor >= 0) {
        glUniform4f(shader->m_uBaseColorFactor,
            (float)pbrMetallicRoughness.baseColorFactor[0],
            (float)pbrMetallicRoughness.baseColorFactor[1],
            (float)pbrMetallicRoughness.baseColorFactor[2],
            (float)pbrMetallicRoughness.baseColorFactor[3]);
      }

      if (shader->m_uBaseColorTexture >= 0) {
        auto textureObject = whiteTexture;
        if (pbrMetallicRoughness.baseColorTexture.index >= 0) {
          const auto &texture =
              model.textures[pbrMetallicRoughness.baseColorTexture.index];
          if (texture.source >= 0) {
            textureObject = textureObjects[texture.source];
          }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureObject);
        glUniform1i(shader->m_uBaseColorTexture, 0);
      }
      if (shader->m_uMetallicFactor >= 0) {
        glUniform1f(shader->m_uMetallicFactor,
            (float)pbrMetallicRoughness.metallicFactor);
      }
      if (shader->m_uRoughnessFactor >= 0) {
        glUniform1f(shader->m_uRoughnessFactor,
            (float)pbrMetallicRoughness.roughnessFactor);
      }
      if (shader->m_uMetallicRoughnessTexture >= 0) {
        auto textureObject = 0u;
        if (pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
          const auto &texture =
              model.textures[pbrMetallicRoughness.metallicRoughnessTexture
                                 .index];
          if (texture.source >= 0) {
            textureObject = textureObjects[texture.source];
          }
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureObject);
        glUniform1i(shader->m_uMetallicRoughnessTexture, 1);
      }
      if (shader->m_uEmissiveFactor >= 0) {
        glUniform3f(shader->m_uEmissiveFactor, (float)material.emissiveFactor[0],
            (float)material.emissiveFactor[1],
            (float)material.emissiveFactor[2]);
      }
      if (shader->m_uEmissiveTexture >= 0) {
        auto textureObject = 0u;
        if (material.emissiveTexture.index >= 0) {
          const auto &texture = model.textures[material.emissiveTexture.index];
          if (texture.source >= 0) {
            textureObject = textureObjects[texture.source];
          }
        }

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textureObject);
        glUniform1i(shader->m_uEmissiveTexture, 2);
      }
      if (shader->m_uOcclusionStrength >= 0) {
        glUniform1f(shader->m_uOcclusionStrength,
            (float)material.occlusionTexture.strength);
      }
      if (shader->m_uOcclusionTexture >= 0) {
        auto textureObject = whiteTexture;
        if (material.occlusionTexture.index >= 0) {
          const auto &texture = model.textures[material.occlusionTexture.index];
          if (texture.source >= 0) {
            textureObject = textureObjects[texture.source];
          }
        }

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, textureObject);
        glUniform1i(shader->m_uOcclusionTexture, 3);
      }

    } else {
      // Apply default material
      // Defined here:
      // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-material
      // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-pbrmetallicroughness3
      if (shader->m_uBaseColorFactor >= 0) {
        glUniform4f(shader->m_uBaseColorFactor, 1, 1, 1, 1);
      }

      if (shader->m_uBaseColorTexture >= 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(shader->m_uBaseColorTexture, whiteTexture);
      }
      if (shader->m_uMetallicFactor >= 0) {
        glUniform1f(shader->m_uMetallicFactor, 1.f);
      }
      if (shader->m_uRoughnessFactor >= 0) {
        glUniform1f(shader->m_uRoughnessFactor, 1.f);
      }
      if (shader->m_uMetallicRoughnessTexture >= 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(shader->m_uMetallicRoughnessTexture, 1);
      }
      if (shader->m_uEmissiveFactor >= 0) {
        glUniform3f(shader->m_uEmissiveFactor, 0.f, 0.f, 0.f);
      }
      if (shader->m_uEmissiveTexture >= 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(shader->m_uEmissiveTexture, 2);
      }
      if (shader->m_uOcclusionStrength >= 0) {
        glUniform1f(shader->m_uOcclusionStrength, 0.f);
      }
      if (shader->m_uOcclusionTexture >= 0) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(shader->m_uOcclusionTexture, 3);
      }
    }
  };

  // Lambda function to draw the scene
  const auto drawScene = [&](glm::mat4 viewMatrix, const GLProgram* shader) {
    // The recursive function that should draw a node
    // We use a std::function because a simple lambda cannot be recursive
    const std::function<void(int, const glm::mat4 &)> drawNode =
        [&](int nodeIdx, const glm::mat4 &parentMatrix) {
          auto v_node = model.nodes[nodeIdx];
          glm::mat4 modelMatrix = getLocalToWorldMatrix(v_node, parentMatrix);
          if (v_node.mesh >= 0) {
            // send model matrix
            if (shader->m_uModelMatrixLocation >= 0) {
              glUniformMatrix4fv(shader->m_uModelMatrixLocation, 1, GL_FALSE,
                  glm::value_ptr(modelMatrix));
            }

            if (shader->m_ulightDirection >= 0) {
              if (lightFromCamera) {
                glUniform3f(shader->m_ulightDirection, 0, 0, 1);
              } else {
                const auto lightDirectionInViewSpace = glm::normalize(
                    glm::vec3(viewMatrix * glm::vec4(lightDir, 0.)));
                glUniform3f(shader->m_ulightDirection,
                    lightDirectionInViewSpace[0], lightDirectionInViewSpace[1],
                    lightDirectionInViewSpace[2]);
              }
            }
            if (shader->m_ulightIntensity >= 0) {
              glUniform3fv(
                  shader->m_ulightIntensity, 1, glm::value_ptr(lightInt));
            }
            if (shader->m_uApplyOcclusion >= 0) {
              glUniform1i(shader->m_uApplyOcclusion, applyOcclusion);
            }

            // get mesh an draw every primitives
            auto v_mesh = model.meshes[v_node.mesh];
            auto v_vaoRange = v_meshToVertexArrays[v_node.mesh];

            for (int i = 0; i < v_mesh.primitives.size(); ++i) {
              auto v_vao = vertexArrayObjects[v_vaoRange.begin + i];
              auto primitive = v_mesh.primitives[i];
              bindMaterial(primitive.material, shader);
              glBindVertexArray(v_vao);
              if (primitive.indices >= 0) {
                const auto &accessor = model.accessors[primitive.indices];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto byteOffset =
                    accessor.byteOffset + bufferView.byteOffset;
                glDrawElements(primitive.mode, GLsizei(accessor.count),
                    accessor.componentType, (const GLvoid *)byteOffset);

              } else {
                // Take first accessor to get the count
                const auto accessorIdx = (*begin(primitive.attributes)).second;
                const auto &accessor = model.accessors[accessorIdx];
                glDrawArrays(primitive.mode, 0, GLsizei(accessor.count));
              }
            }
          }
          for (auto v_child : v_node.children) {
            drawNode(v_child, parentMatrix);
          }
        };

    // Draw the scene referenced by gltf file
    if (model.defaultScene >= 0) {
      for (auto node : model.scenes[model.defaultScene].nodes) {
        drawNode(node, glm::mat4(1));
      }
    }
  };

  const auto computeShadowMap = [&]() {
    const auto sceneCenter = 0.5f * (m_bboxMin + m_bboxMax);
    const float sceneRadius = glm::length((m_bboxMax - m_bboxMin)) * 0.5f;

    glm::mat4 dirLightViewMatrix = glm::mat4(0);

    if(lightFromCamera){ // compute the shadow from the camera
      const auto cam = cameraController->getCamera();
      dirLightViewMatrix = glm::lookAt(cam.eye(),cam.center(),cam.up());
    }else //Compute the shadow as a distant light
      {
        const auto dirLightUpVector =
            computeDirectionVectorUp(lightPhi, lightTheta);

        dirLightViewMatrix = glm::lookAt(sceneCenter + lightDir * sceneRadius, sceneCenter,
                dirLightUpVector); // Will not work if m_DirLightDirection is
                                   // colinear to lightUpVector
    }
    const auto dirLightProjMatrix = glm::ortho(-sceneRadius, sceneRadius,
        -sceneRadius, sceneRadius, 0.1f * sceneRadius, 2.f * sceneRadius);
    m_lightSpaceMatrix = dirLightProjMatrix * dirLightViewMatrix;

    m_glslProgram_shadowMapRendered->use();
    glUniformMatrix4fv(m_glslProgram_shadowMapRendered->m_uLightSpaceMatrix, 1, GL_FALSE,
        glm::value_ptr(m_lightSpaceMatrix));

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    drawScene(dirLightViewMatrix, m_glslProgram_shadowMapRendered);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  };

  const auto render = [&]() {
    const auto camera = cameraController->getCamera();

    m_glslProgram_rendered->use();
    const auto viewMatrix = camera.getViewMatrix();
    if (m_glslProgram_rendered->m_uViewMatrixLocation >= 0) {
      glUniformMatrix4fv(m_glslProgram_rendered->m_uViewMatrixLocation, 1,
          GL_FALSE, glm::value_ptr(viewMatrix));
    }
    if (m_glslProgram_rendered->m_uProjectionMatrixLocation >= 0) {
      glUniformMatrix4fv(m_glslProgram_rendered->m_uProjectionMatrixLocation, 1,
          GL_FALSE, glm::value_ptr(projMatrix));
    }
    glUniformMatrix4fv(m_glslProgram_rendered->m_uLightSpaceMatrix, 1, GL_FALSE,
        glm::value_ptr(m_lightSpaceMatrix));

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_depthMap);
    glUniform1i(m_glslProgram_rendered->m_uDirLightShadowMap, 4);

    glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawScene(viewMatrix, m_glslProgram_rendered);
  };

  if (!m_OutputPath.empty()) {
    std::vector<unsigned char> pixels(m_nWindowWidth * m_nWindowHeight * 3);

    renderToImage(m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), [&]() {
      render();
    },[&]() {
          computeShadowMap();
        });
    flipImageYAxis(m_nWindowWidth, m_nWindowHeight, 3, pixels.data());
    const auto strPath = m_OutputPath.string();
    stbi_write_png(
        strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0);
    return 0;
  }

  // Loop until the user closes the window
  for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
       ++iterationCount) {
    const auto seconds = glfwGetTime();
    const auto camera = cameraController->getCamera();

    if((shadowNeedUpdate || lightFromCamera) && renderShadow) {
      computeShadowMap();
      shadowNeedUpdate = false;
    }

    if(lightFromCamera) shadowNeedUpdate = true;
    render();

    // GUI code:
    imguiNewFrame();

    {
      ImGui::Begin("GUI");
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
          1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y,
            camera.eye().z);
        ImGui::Text("center: %.3f %.3f %.3f", camera.center().x,
            camera.center().y, camera.center().z);
        ImGui::Text(
            "up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);

        ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y,
            camera.front().z);
        ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y,
            camera.left().z);

        if (ImGui::Button("CLI camera args to clipboard")) {
          std::stringstream ss;
          ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
             << camera.eye().z << "," << camera.center().x << ","
             << camera.center().y << "," << camera.center().z << ","
             << camera.up().x << "," << camera.up().y << "," << camera.up().z;
          const auto str = ss.str();
          glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
        }
      }
      static int cameraControllerType = 0;
      auto cameraControllerTypeChanged =
          ImGui::RadioButton("Trackball", &cameraControllerType, 0) ||
          ImGui::RadioButton("First Person", &cameraControllerType, 1);
      if (cameraControllerTypeChanged) {
        const auto currentCamera = cameraController->getCamera();
        if (cameraControllerType == 0) {
          cameraController = std::make_unique<TrackballCameraController>(
              m_GLFWHandle.window(), 0.5f * maxDistance);
        } else {
          cameraController = std::make_unique<FirstPersonCameraController>(
              m_GLFWHandle.window(), 0.5f * maxDistance);
        }
        cameraController->setCamera(currentCamera);
      }

      if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::SliderFloat("theta", &lightTheta, 0, glm::pi<float>()) ||
            ImGui::SliderFloat("phi", &lightPhi, 0, 2.f * glm::pi<float>())) {
          const auto sinPhi = glm::sin(lightPhi);
          const auto cosPhi = glm::cos(lightPhi);
          const auto sinTheta = glm::sin(lightTheta);
          const auto cosTheta = glm::cos(lightTheta);
          lightDir = glm::vec3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
          shadowNeedUpdate = true; //If light direction changed, shadow map need update
        }

        static glm::vec3 lightColor(1.f, 1.f, 1.f);
        static float lightIntensityFactor = 1.f;

        if (ImGui::ColorEdit3("color", (float *)&lightColor) ||
            ImGui::InputFloat("intensity", &lightIntensityFactor)) {
          lightInt = lightColor * lightIntensityFactor;
        }
      }
      ImGui::Checkbox("light from camera", &lightFromCamera);
      ImGui::Checkbox("apply occlusion", &applyOcclusion);
      if (ImGui::CollapsingHeader("Shadow Option")) {
        if(ImGui::SliderInt("Shadow Resolution", &SHADOW_RES, 128, 4096*3)){
          glDeleteFramebuffers(1,&m_depthMapFBO);
          glDeleteTextures(1,&m_depthMap);
          createShdowmap();
          shadowNeedUpdate = true; //If shadow res changed, shadow map need update
        }
      }
      if (ImGui::CollapsingHeader("Render Type")) {
        static int renderType = 0;
        auto renderTypeChanged =
            ImGui::RadioButton("Full Render", &renderType, 0) ||
            ImGui::RadioButton("Normal render", &renderType, 1) ||
            ImGui::RadioButton("No shadow", &renderType, 2)||
            ImGui::RadioButton("Shadow Map Render", &renderType, 3);
        if (renderTypeChanged) {
          if (renderType == 0) {
            m_glslProgram_rendered = &m_glslProgram_fullRender;
            renderShadow = true;

          } else if (renderType == 1) {
            m_glslProgram_rendered = &m_glslProgram_normalRender;
            renderShadow = false;

          } else if (renderType == 2) {
            m_glslProgram_rendered = &m_glslProgram_noShadow;
            renderShadow = false;

          }
          else if (renderType == 3) {
            m_glslProgram_rendered = &m_glslProgram_debugShadowMap;
            renderShadow = true;
          }
        }
      }

      ImGui::End();
    }

    imguiRenderFrame();

    glfwPollEvents(); // Poll for and process events

    auto ellapsedTime = glfwGetTime() - seconds;
    auto guiHasFocus =
        ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    if (!guiHasFocus) {
      cameraController->update(float(ellapsedTime));
    }

    m_GLFWHandle.swapBuffers(); // Swap front and back buffers
  }

  glDeleteVertexArrays(vertexArrayObjects.size(), vertexArrayObjects.data());
  glDeleteBuffers(v_bufferObjects.size(), v_bufferObjects.data());
  glDeleteTextures(textureObjects.size(), textureObjects.data());
  glDeleteFramebuffers(1, &m_depthMapFBO);
  glDeleteTextures(1, &m_depthMap);

  return 0;
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
    uint32_t height, const fs::path &gltfFile,
    const std::vector<float> &lookatArgs, const std::string &vertexShader,
    const std::string &fragmentShader, const fs::path &output) :
    m_nWindowWidth(width),
    m_nWindowHeight(height),
    m_AppPath{appPath},
    m_AppName{m_AppPath.stem().string()},
    m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
    m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
    m_gltfFilePath{gltfFile},
    m_OutputPath{output}
{
  if (!lookatArgs.empty()) {
    m_hasUserCamera = true;
    m_userCamera =
        Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
            glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
            glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
  }

  ImGui::GetIO().IniFilename =
      m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
                                  // positions in this file

  glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

  printGLVersion();
}

bool ViewerApplication::loadGltfFile(tinygltf::Model &model)
{

  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;
  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath);

  if (!warn.empty()) {
    printf("Warn: %s\n", warn.c_str());
  }

  if (!err.empty()) {
    printf("Err: %s\n", err.c_str());
  }

  return ret;
}

std::vector<GLuint> ViewerApplication::createTextureObjects(
    const tinygltf::Model &model) const
{
  std::vector<GLuint> textureObjects(model.textures.size(), 0);

  // default sampler:
  // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#texturesampler
  // "When undefined, a sampler with repeat wrapping and auto filtering should
  // be used."
  tinygltf::Sampler defaultSampler;
  defaultSampler.minFilter = GL_LINEAR;
  defaultSampler.magFilter = GL_LINEAR;
  defaultSampler.wrapS = GL_REPEAT;
  defaultSampler.wrapT = GL_REPEAT;
  defaultSampler.wrapR = GL_REPEAT;

  glActiveTexture(GL_TEXTURE0);

  glGenTextures(GLsizei(model.textures.size()), textureObjects.data());
  for (size_t i = 0; i < model.textures.size(); ++i) {
    const auto &texture = model.textures[i];
    assert(texture.source >= 0);
    const auto &image = model.images[texture.source];

    const auto &sampler =
        texture.sampler >= 0 ? model.samplers[texture.sampler] : defaultSampler;
    glBindTexture(GL_TEXTURE_2D, textureObjects[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
        GL_RGBA, image.pixel_type, image.image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, sampler.wrapR);

    if (sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
        sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
        sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
        sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR) {
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  return textureObjects;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(
    const tinygltf::Model &model)
{

  std::vector<GLuint> bufferObjects(model.buffers.size(), 0);

  glGenBuffers(GLsizei(bufferObjects.size()), bufferObjects.data());

  for (size_t i = 0; i < model.buffers.size(); ++i) {
    const auto &buffer = model.buffers[i].data;
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
    glBufferStorage(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), 0);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return bufferObjects;
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects(
    const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects,
    std::vector<VaoRange> &meshIndexToVaoRange)
{

  std::vector<GLuint> vertexArrayObjects;

  // For each mesh of model we keep its range of VAOs
  meshIndexToVaoRange.resize(model.meshes.size());

  for (size_t idx_mesh = 0; idx_mesh < model.meshes.size(); ++idx_mesh) {
    const auto &mesh = model.meshes[idx_mesh];

    auto &vaoRange = meshIndexToVaoRange[idx_mesh];
    vaoRange.begin =
        GLsizei(vertexArrayObjects.size()); // Range for this mesh will be at
                                            // the end of vertexArrayObjects
    vaoRange.count =
        GLsizei(mesh.primitives.size()); // One VAO for each primitive

    // Add enough elements to store our VAOs identifiers
    vertexArrayObjects.resize(
        vertexArrayObjects.size() + mesh.primitives.size());
    glGenVertexArrays(vaoRange.count, &vertexArrayObjects[vaoRange.begin]);
    for (size_t pIdx = 0; pIdx < mesh.primitives.size(); ++pIdx) {
      const auto vao = vertexArrayObjects[vaoRange.begin + pIdx];
      const auto &primitive = mesh.primitives[pIdx];
      glBindVertexArray(vao);
      //=============== Position ====================
      { // I'm opening a scope because I want to reuse the variable iterator in
        // the code for NORMAL and TEXCOORD_0
        const auto iterator = primitive.attributes.find("POSITION");
        if (iterator !=
            end(primitive
                    .attributes)) { // If "POSITION" has been found in the map
          // (*iterator).first is the key "POSITION", (*iterator).second is the
          // value, ie. the index of the accessor for this attribute
          const auto accessorIdx = (*iterator).second;
          const auto &accessor = model.accessors[accessorIdx];
          const auto &bufferView = model.bufferViews[accessor.bufferView];
          const auto bufferIdx = bufferView.buffer;

          const auto bufferObject = bufferObjects[bufferIdx];

          glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX); // Enable array
          glBindBuffer(GL_ARRAY_BUFFER,
              bufferObject); // Bind the buffer object to GL_ARRAY_BUFFER

          const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
          ; // Compute the total byte offset using the accessor and the buffer
            // view
          glVertexAttribPointer(VERTEX_ATTRIB_POSITION_IDX, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)byteOffset);
        }
      }
      //=============== Normal ====================
      { // I'm opening a scope because I want to reuse the variable iterator in
        // the code for NORMAL and TEXCOORD_0
        const auto iterator = primitive.attributes.find("NORMAL");
        if (iterator !=
            end(primitive
                    .attributes)) { // If "POSITION" has been found in the map
          // (*iterator).first is the key "POSITION", (*iterator).second is the
          // value, ie. the index of the accessor for this attribute
          const auto accessorIdx = (*iterator).second;
          const auto &accessor = model.accessors[accessorIdx];
          const auto &bufferView = model.bufferViews[accessor.bufferView];
          const auto bufferIdx = bufferView.buffer;

          const auto bufferObject = bufferObjects[bufferIdx];

          glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL_IDX); // Enable array
          glBindBuffer(GL_ARRAY_BUFFER,
              bufferObject); // Bind the buffer object to GL_ARRAY_BUFFER

          const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
          ; // Compute the total byte offset using the accessor and the buffer
            // view
          glVertexAttribPointer(VERTEX_ATTRIB_NORMAL_IDX, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)byteOffset);
        }
      }
      //=============== Texture ====================
      { // I'm opening a scope because I want to reuse the variable iterator in
        // the code for NORMAL and TEXCOORD_0
        const auto iterator = primitive.attributes.find("TEXCOORD_0");
        if (iterator !=
            end(primitive
                    .attributes)) { // If "POSITION" has been found in the map
          // (*iterator).first is the key "POSITION", (*iterator).second is the
          // value, ie. the index of the accessor for this attribute
          const auto accessorIdx = (*iterator).second;
          const auto &accessor = model.accessors[accessorIdx];
          const auto &bufferView = model.bufferViews[accessor.bufferView];
          const auto bufferIdx = bufferView.buffer;

          const auto bufferObject = bufferObjects[bufferIdx];

          glEnableVertexAttribArray(
              VERTEX_ATTRIB_TEXCOORD0_IDX); // Enable array
          glBindBuffer(GL_ARRAY_BUFFER,
              bufferObject); // Bind the buffer object to GL_ARRAY_BUFFER

          const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
          ; // Compute the total byte offset using the accessor and the buffer
            // view
          glVertexAttribPointer(VERTEX_ATTRIB_TEXCOORD0_IDX, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)byteOffset);
        }
      }
      // Index array if defined
      if (primitive.indices >= 0) {
        const auto accessorIdx = primitive.indices;
        const auto &accessor = model.accessors[accessorIdx];
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto bufferIdx = bufferView.buffer;

        assert(GL_ELEMENT_ARRAY_BUFFER == bufferView.target);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
            bufferObjects[bufferIdx]); // Binding the index buffer to
                                       // GL_ELEMENT_ARRAY_BUFFER while the VAO
                                       // is bound is enough to tell OpenGL we
                                       // want to use that index buffer for that
                                       // VAO
      }
    }
  }
  glBindVertexArray(0);
  std::clog << "Number of VAOs: " << vertexArrayObjects.size() << std::endl;

  return vertexArrayObjects;
}

void ViewerApplication::createShdowmap()
{
  glGenFramebuffers(1, &m_depthMapFBO);
  // create depth texture
  glGenTextures(1, &m_depthMap);
  glBindTexture(GL_TEXTURE_2D, m_depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_RES,
      SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float borderColor[] = {1.0, 1.0, 1.0, 1.0};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  // attach depth texture as FBO's depth buffer
  glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}