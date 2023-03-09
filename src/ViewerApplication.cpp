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
  const auto glslProgram = compileProgram({m_ShadersRootPath / m_vertexShader,
      m_ShadersRootPath / m_fragmentShader});

  const auto modelViewProjMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
  const auto modelViewMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
  const auto normalMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");

  // Build projection matrix
  auto maxDistance = 500.f; // TODO use scene bounds instead to compute this
  maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
  const auto projMatrix =
      glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  // TODO Implement a new CameraController model and use it instead. Propose the
  // choice from the GUI
  FirstPersonCameraController cameraController{
      m_GLFWHandle.window(), 0.5f * maxDistance};
  if (m_hasUserCamera) {
    cameraController.setCamera(m_userCamera);
  } else {
    // TODO Use scene bounds to compute a better default camera
    cameraController.setCamera(
        Camera{glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)});
  }

  tinygltf::Model model;

  std::cerr << "Load " << this->m_gltfFilePath << std::endl;
  loadGltfFile(model);
  std::cerr << "Loaded" << std::endl;

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
  glslProgram.use();

  // Lambda function to draw the scene
  const auto drawScene = [&](const Camera &camera) {
    glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto viewMatrix = camera.getViewMatrix();

    // The recursive function that should draw a node
    // We use a std::function because a simple lambda cannot be recursive
    const std::function<void(int, const glm::mat4 &)> drawNode =
        [&](int nodeIdx, const glm::mat4 &parentMatrix) {
          auto v_node = model.nodes[nodeIdx];
          glm::mat4 modelMatrix = getLocalToWorldMatrix(v_node, parentMatrix);
          if (v_node.mesh >= 0) {
            glm::mat4 modelViewMatrix, modelViewProjectionMatrix, normalMatrix;
            modelViewMatrix = viewMatrix * modelMatrix;
            modelViewProjectionMatrix = projMatrix * modelViewMatrix;
            normalMatrix = glm::transpose(glm::inverse(modelViewMatrix));
            glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE,
                glm::value_ptr(modelViewProjectionMatrix));
            glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE,
                glm::value_ptr(modelViewMatrix));
            glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE,
                glm::value_ptr(normalMatrix));

            // get mesh an draw every primitives
            auto v_mesh = model.meshes[v_node.mesh];
            auto v_vaoRange = v_meshToVertexArrays[v_node.mesh];

            for (int i = 0; i < v_mesh.primitives.size(); ++i) {
              auto v_vao = vertexArrayObjects[v_vaoRange.begin + i];
              auto primitive = v_mesh.primitives[i];
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

  if (!m_OutputPath.empty()) {
    std::vector<unsigned char> pixels(m_nWindowWidth * m_nWindowHeight * 3);
    renderToImage(m_nWindowWidth, m_nWindowHeight, 3, pixels.data(),
        [&]() { drawScene(cameraController.getCamera()); });
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

    const auto camera = cameraController.getCamera();
    drawScene(camera);

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
      ImGui::End();
    }

    imguiRenderFrame();

    glfwPollEvents(); // Poll for and process events

    auto ellapsedTime = glfwGetTime() - seconds;
    auto guiHasFocus =
        ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    if (!guiHasFocus) {
      cameraController.update(float(ellapsedTime));
    }

    m_GLFWHandle.swapBuffers(); // Swap front and back buffers
  }

  // TODO clean up allocated GL data

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

  if (!vertexShader.empty()) {
    m_vertexShader = vertexShader;
  }

  if (!fragmentShader.empty()) {
    m_fragmentShader = fragmentShader;
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