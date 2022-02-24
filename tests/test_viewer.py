from __future__ import (
    annotations,
)

from ctypes import (
    c_void_p,
    cast,
)
from dataclasses import (
    dataclass,
)
from pathlib import (
    Path,
)
from typing import (
    Any,
    Callable,
    List,
)

import glfw
import glm
import imgui
import OpenGL.GL as gl  # noqa: N813
import pytest
from imgui.integrations.glfw import GlfwRenderer as ImGuiGlfwRenderer
from pygltflib import (
    GLTF2,
    BufferFormat,
    Node,
)

from gltf_viewer.utils.logging import (
    get_logger,
)
from tests.conftest import (
    LOCAL_DIR,
)

logger = get_logger("tests")

gltf_sample = "TriangleWithoutIndices"
gltf_file = (
    LOCAL_DIR
    / "gltf-sample-models"
    / "2.0"
    / gltf_sample
    / "glTF"
    / f"{gltf_sample}.gltf"
)
look_at = [
    glm.vec3([-5.26056, 6.59932, 0.85661]),
    glm.vec3([-4.40144, 6.23486, 0.497347]),
    glm.vec3([0.342113, 0.931131, -0.126476]),
]

vertex_attrib_index = {
    "POSITION": 0,
    "NORMAL": 1,
    "TEXCOORD_0": 2,
}

forward_vertex_shader_src = """
#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec3 vViewSpacePosition;
out vec3 vViewSpaceNormal;
out vec2 vTexCoords;

uniform mat4 uModelViewProjMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uNormalMatrix;

void main()
{
    vViewSpacePosition = vec3(uModelViewMatrix * vec4(aPosition, 1));
    vViewSpaceNormal = normalize(vec3(uNormalMatrix * vec4(aNormal, 0)));
    vTexCoords = aTexCoords;
    gl_Position =  uModelViewProjMatrix * vec4(aPosition, 1);
}
"""

normal_fragment_shader_src = """
#version 330

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

out vec3 fColor;

void main()
{
   // Need another normalization because interpolation of vertex attributes does not maintain unit length
   vec3 viewSpaceNormal = normalize(vViewSpaceNormal);
   fColor = vec3(1,0,0);//viewSpaceNormal;
}
"""


@pytest.mark.opengl()
@pytest.mark.asyncio()
async def test_viewer_application(
    request: pytest.FixtureRequest,
) -> None:
    app_path = Path()
    width = 1080
    height = 720
    app = ViewerApplication(
        app_path,
        width,
        height,
    )
    runner_coroutine = app.run()
    if not request.config.getoption("--interactive-opengl"):
        app.stop()

    await runner_coroutine


def test_invalid_args_for_viewer_application_raises() -> None:
    with pytest.raises(ValueError, match="Invalid width"):
        ViewerApplication(
            Path(),
            -1,
            16,
        )
    with pytest.raises(ValueError, match="Invalid width"):
        ViewerApplication(
            Path(),
            0,
            16,
        )
    with pytest.raises(ValueError, match="Invalid height"):
        ViewerApplication(
            Path(),
            16,
            -1,
        )
    with pytest.raises(ValueError, match="Invalid height"):
        ViewerApplication(
            Path(),
            16,
            0,
        )


class ViewerApplication:
    app_path: Path
    width: int
    height: int
    glfw_handle: GLFWHandle
    imgui_renderer: ImGuiGlfwRenderer

    def __init__(self, app_path: Path, width: int, height: int) -> None:
        if width <= 0:
            raise ValueError(f"Invalid width={width}, should be positive")
        if height <= 0:
            raise ValueError(f"Invalid height={height}, should be positive")
        self.app_path = app_path
        self.width = width
        self.height = height
        self.glfw_handle = GLFWHandle(width, height, "glTF Viewer", True)

        imgui.create_context()
        imgui.get_io().ini_file_name = b".local/imgui.ini"
        self.imgui_renderer = ImGuiGlfwRenderer(self.glfw_handle.window)

    async def run(self) -> None:
        log_gl_info()

        gltf = GLTF2().load(gltf_file)
        if gltf is None:
            raise RuntimeError(f"Unable to load {gltf_file}")
        gltf.convert_buffers(BufferFormat.BINARYBLOB)

        buffer_objects = create_buffer_objets(gltf)
        vertex_array_objects = create_vertex_array_objects(gltf, buffer_objects)

        glsl_program = compile_program(
            forward_vertex_shader_src,
            normal_fragment_shader_src,
        )

        matrix_uniforms = MatrixUniforms(
            gl.glGetUniformLocation(glsl_program, "uModelViewProjMatrix"),
            gl.glGetUniformLocation(glsl_program, "uModelViewMatrix"),
            gl.glGetUniformLocation(glsl_program, "uNormalMatrix"),
        )

        gl.glEnable(gl.GL_DEPTH_TEST)
        gl.glUseProgram(glsl_program)

        gui_window_is_open = True
        gui_camera_collapsing_header_is_open = True

        txt = ""

        max_distance = 500
        camera_controller = FirstPersonCameraController(
            self.glfw_handle.window,
            0.5 * max_distance,
            Camera(*look_at),
        )
        camera = camera_controller.camera

        proj_matrix = glm.perspective(
            70.0, self.width / self.height, 0.001 * max_distance, 1.5 * max_distance
        )

        while not self.glfw_handle.should_close():
            seconds = glfw.get_time()

            gl.glViewport(0, 0, self.width, self.height)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT | gl.GL_DEPTH_BUFFER_BIT)

            view_matrix = camera.get_view_matrix()

            draw_scene(
                gltf,
                vertex_array_objects,
                matrix_uniforms,
                proj_matrix,
                glm.mat4(),
            )

            imgui.new_frame()

            if imgui.begin_main_menu_bar():
                if imgui.begin_menu("File", True):

                    clicked_quit, selected_quit = imgui.menu_item(
                        "Quit", "Ctrl+Q", False, True
                    )

                    if clicked_quit:
                        self.stop()

                    imgui.end_menu()
                imgui.end_main_menu_bar()

            if gui_window_is_open:
                _, gui_window_is_open = imgui.begin("GUI", closable=True)
                imgui.text(
                    f"Application average {1000 / imgui.get_io().framerate: .3f} ms/frame ({imgui.get_io().framerate: .1f} FPS)"
                )

                changed, txt = imgui.input_text("Amount", txt, 256)
                imgui.text("You wrote:")
                imgui.same_line()
                imgui.text(txt)

                if gui_camera_collapsing_header_is_open:
                    (
                        expended,
                        gui_camera_collapsing_header_is_open,
                    ) = imgui.collapsing_header(
                        "Camera", gui_camera_collapsing_header_is_open
                    )
                    if expended:
                        imgui.text(f"eye: {camera.eye.x} {camera.eye.y} {camera.eye.z}")
                        imgui.text(
                            f"center: {camera.center.x} {camera.center.y} {camera.center.z}"
                        )
                        imgui.text(f"up: {camera.up.x} {camera.up.y} {camera.up.z}")

                        if imgui.button("Copy camera args to clipboard"):
                            args = (
                                f"--lookat {camera.eye.x},{camera.eye.y},{camera.eye.z},"
                                f"{camera.center.x},{camera.center.y},{camera.center.z},"
                                f"{camera.up.x},{camera.up.y},{camera.up.z}"
                            )
                            logger.info(args)
                            glfw.set_clipboard_string(self.glfw_handle.window, args)
                imgui.end()

            imgui.render()
            self.imgui_renderer.render(imgui.get_draw_data())

            self.glfw_handle.swap_buffers()

            glfw.poll_events()
            self.imgui_renderer.process_inputs()

            gui_has_focus = (
                imgui.get_io().want_capture_mouse
                or imgui.get_io().want_capture_keyboard
            )
            if not gui_has_focus:
                ellapsed_time = glfw.get_time() - seconds
                camera = camera_controller.update(ellapsed_time)

        self.imgui_renderer.shutdown()

    def stop(self) -> None:
        self.glfw_handle.set_should_close(True)


def gen_gl_objects(func: Callable, count: int) -> List[gl.GLuint]:
    return [func(count)] if count == 1 else func(count)


def create_buffer_objets(gltf: GLTF2) -> List[gl.GLuint]:
    count = len(gltf.buffers)
    data = gltf.binary_blob()
    offset = 0
    buffer_objects = gen_gl_objects(gl.glGenBuffers, count)
    for i in range(count):
        gl.glBindBuffer(gl.GL_ARRAY_BUFFER, buffer_objects[i])
        gl.glBufferStorage(
            gl.GL_ARRAY_BUFFER,
            gltf.buffers[i].byteLength,
            data[offset : offset + gltf.buffers[i].byteLength],
            0,
        )
        offset += gltf.buffers[i].byteLength
    gl.glBindBuffer(gl.GL_ARRAY_BUFFER, 0)
    return buffer_objects


def create_vertex_array_objects(
    gltf: GLTF2, buffer_objects: List[gl.GLuint]
) -> List[List[gl.GLuint]]:
    mesh_idx_to_vertex_arrays = []
    for mesh_idx, mesh_def in enumerate(gltf.meshes):
        mesh_idx_to_vertex_arrays.append(
            gen_gl_objects(gl.glGenVertexArrays, len(mesh_def.primitives))
        )
        for prim_idx, prim_def in enumerate(mesh_def.primitives):
            vao = mesh_idx_to_vertex_arrays[mesh_idx][prim_idx]
            gl.glBindVertexArray(vao)
            for attr_name, attr_idx in vertex_attrib_index.items():
                accessor_idx = getattr(prim_def.attributes, attr_name)
                if accessor_idx is not None:
                    accessor = gltf.accessors[accessor_idx]
                    if accessor.bufferView is None:
                        raise ValueError("accessor.bufferView cannot be None")
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    gl.glEnableVertexAttribArray(attr_idx)
                    gl.glBindBuffer(
                        gl.GL_ARRAY_BUFFER, buffer_objects[buffer_view.buffer]
                    )
                    gl.glVertexAttribPointer(
                        attr_idx,
                        int(accessor.type.split("VEC")[1]),
                        accessor.componentType,
                        gl.GL_FALSE,
                        buffer_view.byteStride or 0,
                        accessor.byteOffset or 0 + buffer_view.byteOffset or 0,
                    )
            if prim_def.indices is not None:
                accessor = gltf.accessors[prim_def.indices]
                if accessor.bufferView is None:
                    raise ValueError("accessor.bufferView cannot be None")
                buffer_view = gltf.bufferViews[accessor.bufferView]
                gl.glBindBuffer(
                    gl.GL_ELEMENT_ARRAY_BUFFER, buffer_objects[buffer_view.buffer]
                )

    gl.glBindBuffer(gl.GL_ARRAY_BUFFER, 0)
    gl.glBindVertexArray(0)
    return mesh_idx_to_vertex_arrays


def draw_scene(
    gltf: GLTF2,
    vertex_arrays: List[List[gl.GLuint]],
    matrix_uniforms: MatrixUniforms,
    proj_matrix: glm.mat4,
    view_matrix: glm.mat4,
) -> None:
    if gltf.scene < 0:
        return
    for node_idx in gltf.scenes[gltf.scene].nodes:
        draw_node(
            gltf,
            vertex_arrays,
            matrix_uniforms,
            proj_matrix,
            view_matrix,
            glm.mat4(),
            node_idx,
        )


@dataclass
class MatrixUniforms:
    model_view_proj: gl.GLuint
    model_view: gl.GLuint
    normal: gl.GLuint


def draw_node(
    gltf: GLTF2,
    vertex_arrays: List[List[gl.GLuint]],
    matrix_uniforms: MatrixUniforms,
    proj_matrix: glm.mat4,
    view_matrix: glm.mat4,
    parent_model_matrix: glm.mat4,
    node_idx: int,
) -> None:
    node = gltf.nodes[node_idx]
    model_matrix = get_local_to_world_matrix(node, parent_model_matrix)

    for child_node_idx in node.children:
        draw_node(
            gltf,
            vertex_arrays,
            matrix_uniforms,
            proj_matrix,
            view_matrix,
            model_matrix,
            child_node_idx,
        )

    if node.mesh is None:
        return

    model_view = view_matrix * model_matrix
    model_view_proj = proj_matrix * model_view
    normal_matrix = glm.transpose(glm.inverse(model_view))

    gl.glUniformMatrix4fv(
        matrix_uniforms.model_view_proj, 1, gl.GL_FALSE, glm.value_ptr(model_view_proj)
    )
    gl.glUniformMatrix4fv(
        matrix_uniforms.model_view, 1, gl.GL_FALSE, glm.value_ptr(model_view)
    )
    gl.glUniformMatrix4fv(
        matrix_uniforms.normal, 1, gl.GL_FALSE, glm.value_ptr(normal_matrix)
    )

    mesh = gltf.meshes[node.mesh]
    mesh_vertex_arrays = vertex_arrays[node.mesh]
    for prim_idx, prim_def in enumerate(mesh.primitives):
        vao = mesh_vertex_arrays[prim_idx]
        gl.glBindVertexArray(vao)
        if prim_def.indices is not None:
            accessor = gltf.accessors[prim_def.indices]
            if accessor.bufferView is None:
                raise ValueError("accessor.bufferView cannot be None")
            buffer_view = gltf.bufferViews[accessor.bufferView]
            byte_offset = accessor.byteOffset or 0 + buffer_view.byteOffset or 0
            gl.glDrawElements(
                prim_def.mode,
                accessor.count,
                accessor.componentType,
                cast(byte_offset, c_void_p),
            )
        else:
            accessor = gltf.accessors[prim_def.attributes.POSITION]
            gl.glDrawArrays(prim_def.mode, 0, accessor.count)


def get_local_to_world_matrix(
    node: Node, parent_local_to_world_matrix: glm.mat4
) -> glm.mat4:
    # Extract model matrix
    # https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#transformations
    if node.matrix is not None:
        return parent_local_to_world_matrix * glm.mat4(*node.matrix)
    translation_vector = (
        glm.vec3(node.translation) if node.translation is not None else glm.vec3(0)
    )
    translation = glm.translate(translation_vector)
    rotation_quaternion = (
        glm.quat(node.rotation[3], *node.rotation[0:3])
        if node.rotation is not None
        else glm.quat(1, 0, 0, 0)
    )
    translation_rotation = translation * glm.mat4_cast(rotation_quaternion)
    scale_vector = glm.vec3(node.scale) if node.scale is not None else glm.vec3(1)
    return glm.scale(translation_rotation, scale_vector)


def compile_program(vertex_shader_src: str, fragment_shader_src: str) -> gl.GLuint:
    program = gl.glCreateProgram()

    gl.glAttachShader(program, compile_shader(gl.GL_VERTEX_SHADER, vertex_shader_src))
    gl.glAttachShader(
        program, compile_shader(gl.GL_FRAGMENT_SHADER, fragment_shader_src)
    )

    gl.glLinkProgram(program)
    if gl.GL_TRUE != gl.glGetProgramiv(program, gl.GL_LINK_STATUS):
        raise RuntimeError(gl.glGetProgramInfoLog(program))

    return program


def compile_shader(
    type: gl.GLenum,
    shader_src: str,
) -> gl.GLuint:
    shader = gl.glCreateShader(type)

    gl.glShaderSource(shader, shader_src)
    gl.glCompileShader(shader)
    if gl.GL_TRUE != gl.glGetShaderiv(shader, gl.GL_COMPILE_STATUS):
        raise RuntimeError(gl.glGetShaderInfoLog(shader))

    return shader


class GLFWHandle:
    window: Any
    imgui_renderer: ImGuiGlfwRenderer

    def __init__(
        self,
        width: int,
        height: int,
        title: str,
        show_window: bool,
    ) -> None:
        if not glfw.init():
            raise RuntimeError("Unable to init GLFW")

        if not show_window:
            glfw.window_hint(glfw.VISIBLE, glfw.FALSE)

        glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 4)
        glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 4)
        glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
        glfw.window_hint(glfw.OPENGL_DEBUG_CONTEXT, glfw.TRUE)
        glfw.window_hint(glfw.RESIZABLE, glfw.FALSE)
        glfw.window_hint(glfw.SAMPLES, 4)

        self.window = glfw.create_window(width, height, title, None, None)
        if self.window is None:
            glfw.terminate()
            raise RuntimeError("Unable to open window")

        glfw.make_context_current(self.window)
        glfw.swap_interval(0)  # no vsync

    def __del__(self) -> None:
        if hasattr(self, "window"):
            glfw.destroy_window(self.window)
        glfw.terminate()

    def should_close(self) -> bool:
        return glfw.window_should_close(self.window)

    def set_should_close(self, value: bool) -> None:
        return glfw.set_window_should_close(self.window, value)

    def framebuffer_size(self) -> glm.ivec2:
        return glm.ivec2(*glfw.get_framebuffer_size(self.window))

    def swap_buffers(self) -> None:
        glfw.swap_buffers(self.window)


@dataclass
class Camera:
    eye: glm.vec3 = glm.vec3(0, 0, 1)
    center: glm.vec3 = glm.vec3(0, 0, 0)
    up: glm.vec3 = glm.vec3(0, 1, 0)

    def __post_init__(self) -> None:
        front = self.center - self.eye
        left = glm.cross(self.up, front)
        if left == glm.vec3(0):
            raise ValueError("up and front vectors of camera should not be aligned")
        self.up = glm.normalize(glm.cross(front, left))

    def get_view_matrix(self) -> glm.mat4:
        return glm.lookAt(self.eye, self.center, self.up)

    def front(self, *, normalized: bool = True) -> glm.vec3:
        front = self.center - self.eye
        return glm.normalize(front) if normalized else front

    def left(self, *, normalized: bool = True) -> glm.vec3:
        front = self.front(normalized=False)
        left = glm.cross(self.up, front)
        return glm.normalize(left) if normalized else left


class FirstPersonCameraController:
    camera: Camera

    def __init__(self, window: Any, speed: float, camera: Camera = Camera()) -> None:
        self.camera = camera

    def update(self, ellapsed_time: float) -> Camera:
        return self.camera


def log_gl_info() -> None:
    logger.info(f"Vendor: {gl.glGetString(gl.GL_VENDOR).decode()}")
    logger.info(f"Opengl version: {gl.glGetString(gl.GL_VERSION).decode()}")
    logger.info(
        f"GLSL Version: {gl.glGetString(gl.GL_SHADING_LANGUAGE_VERSION).decode()}"
    )
    logger.info(f"Renderer: {gl.glGetString(gl.GL_RENDERER).decode()}")
