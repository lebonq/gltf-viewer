from __future__ import (
    annotations,
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
import numpy as np
import OpenGL.GL as gl  # noqa: N813
import pytest
from imgui.integrations.glfw import GlfwRenderer as ImGuiGlfwRenderer
from pygltflib import (
    GLTF2,
)

from gltf_viewer.utils.logging import (
    get_logger,
)
from tests.conftest import (
    LOCAL_DIR,
)

logger = get_logger("tests")

gltf_file = LOCAL_DIR / "gltf-sample-models" / "2.0" / "Sponza" / "glTF" / "Sponza.gltf"


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

        buffer_objects = create_buffer_objets(gltf)

        gl.glEnable(gl.GL_DEPTH_TEST)

        gui_window_is_open = True
        gui_camera_collapsing_header_is_open = True

        txt = ""

        camera = Camera()
        max_distance = 500
        camera_controller = FirstPersonCameraController(
            self.glfw_handle.window, 0.5 * max_distance
        )

        while not self.glfw_handle.should_close():
            seconds = glfw.get_time()

            gl.glViewport(0, 0, self.width, self.height)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT | gl.GL_DEPTH_BUFFER_BIT)

            view_matrix = camera.get_view_matrix()

            # draw scene here

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
        )
        offset += gltf.buffers[i].byteLength
    gl.glBindBuffer(gl.GL_ARRAY_BUFFER, 0)
    return buffer_objects


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

    def __init__(self, window: Any, speed: float) -> None:
        self.camera = Camera()

    def update(self, ellapsed_time: float) -> Camera:
        return self.camera


def log_gl_info() -> None:
    logger.info(f"Vendor: {gl.glGetString(gl.GL_VENDOR).decode()}")
    logger.info(f"Opengl version: {gl.glGetString(gl.GL_VERSION).decode()}")
    logger.info(
        f"GLSL Version: {gl.glGetString(gl.GL_SHADING_LANGUAGE_VERSION).decode()}"
    )
    logger.info(f"Renderer: {gl.glGetString(gl.GL_RENDERER).decode()}")
