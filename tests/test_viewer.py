from __future__ import (
    annotations,
)

from pathlib import (
    Path,
)
from typing import (
    Any,
)

import glfw
import glm
import OpenGL.GL as gl  # noqa: N813
import pytest

from gltf_viewer.utils.logging import (
    get_logger,
)

logger = get_logger("tests")


@pytest.mark.opengl()
@pytest.mark.asyncio()
async def test_viewer_application() -> None:
    app_path = Path()
    width = 1080
    height = 720
    app = ViewerApplication(
        app_path,
        width,
        height,
    )
    runner_coroutine = app.run()
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

    def __init__(self, app_path: Path, width: int, height: int) -> None:
        if width <= 0:
            raise ValueError(f"Invalid width={width}, should be positive")
        if height <= 0:
            raise ValueError(f"Invalid height={height}, should be positive")
        self.app_path = app_path
        self.width = width
        self.height = height
        self.glfw_handle = GLFWHandle(width, height, "glTF Viewer", True)

    async def run(self) -> None:
        log_gl_info()
        while not self.glfw_handle.should_close():
            glfw.poll_events()
            self.glfw_handle.swap_buffers()

    def stop(self) -> None:
        self.glfw_handle.set_should_close(True)


class GLFWHandle:
    window: Any

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

        # todo: setup GL debug output
        # todo: setup dear imgui

    def __del__(self) -> None:
        logger.info("Buy !")
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


def log_gl_info() -> None:
    logger.info(f"Vendor: {gl.glGetString(gl.GL_VENDOR).decode()}")
    logger.info(f"Opengl version: {gl.glGetString(gl.GL_VERSION).decode()}")
    logger.info(
        f"GLSL Version: {gl.glGetString(gl.GL_SHADING_LANGUAGE_VERSION).decode()}"
    )
    logger.info(f"Renderer: {gl.glGetString(gl.GL_RENDERER).decode()}")
