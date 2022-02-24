"""
From https://www.metamost.com/opengl-with-python/ with some improvements
"""

import contextlib
import ctypes
import logging
import sys
from typing import (
    Any,
    Generator,
)

import glfw
import numpy as np
import pytest
from OpenGL import GL as gl  # noqa: N811
from OpenGL.arrays import (
    ArrayDatatype,
)

log = logging.getLogger(__name__)


@contextlib.contextmanager
def create_main_window() -> Generator[Any, None, None]:
    if not glfw.init():
        log.error("failed to initialize GLFW")
        sys.exit(1)
    try:
        log.debug("requiring modern OpenGL without any legacy features")
        glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
        glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
        glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, True)
        glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)

        log.debug("opening window")
        title = "Tutorial 2: First Triangle"
        window = glfw.create_window(500, 400, title, None, None)
        if not window:
            log.error("failed to open GLFW window.")
            sys.exit(2)
        glfw.make_context_current(window)

        log.debug("set background to dark blue")
        gl.glClearColor(0, 0, 0.4, 0)

        yield window

    finally:
        log.debug("terminating window context")
        glfw.terminate()


@contextlib.contextmanager
def create_vertex_array_object() -> Generator[None, None, None]:
    log.debug("creating and binding the vertex array (VAO)")
    vertex_array_id = gl.glGenVertexArrays(1)
    try:
        gl.glBindVertexArray(vertex_array_id)
        yield
    finally:
        log.debug("cleaning up vertex array")
        gl.glDeleteVertexArrays(1, [vertex_array_id])


@contextlib.contextmanager
def create_vertex_buffer() -> Generator[None, None, None]:
    with create_vertex_array_object():
        # A triangle
        vertex_data = [
            -0.5,
            -0.5,
            1.0,
            0.0,
            0.0,  # Premier vertex
            0.5,
            -0.5,
            0.0,
            1.0,
            0.0,  # Deuxième vertex
            0.0,
            0.5,
            0.0,
            0.0,
            1.0,  # Troisème vertex
        ]

        log.debug("creating and binding the vertex buffer (VBO)")
        vertex_buffer = gl.glGenBuffers(1)
        try:
            gl.glBindBuffer(gl.GL_ARRAY_BUFFER, vertex_buffer)

            array_type = gl.GLfloat * len(vertex_data)
            gl.glBufferData(
                gl.GL_ARRAY_BUFFER,
                len(vertex_data) * ctypes.sizeof(ctypes.c_float),
                array_type(*vertex_data),
                gl.GL_STATIC_DRAW,
            )
            if False:
                # note: setting data can be done with numpy array:
                np_vertex_data = np.array(vertex_data, dtype=np.float32)
                gl.glBufferData(
                    gl.GL_ARRAY_BUFFER,
                    ArrayDatatype.arrayByteCount(np_vertex_data),
                    np_vertex_data,
                    gl.GL_STATIC_DRAW,
                )

            log.debug("setting the vertex attributes")
            gl.glVertexAttribPointer(
                0,  # attribute 0.
                2,  # components per vertex attribute
                gl.GL_FLOAT,  # type
                False,  # to be normalized?
                5 * ctypes.sizeof(ctypes.c_float),  # stride
                ctypes.cast(
                    0 * ctypes.sizeof(ctypes.c_float), ctypes.c_void_p
                ),  # array buffer offset
            )
            gl.glEnableVertexAttribArray(0)  # use currently bound VAO

            gl.glVertexAttribPointer(
                1,  # attribute 1
                3,  # components per vertex attribute
                gl.GL_FLOAT,  # type
                False,  # to be normalized?
                5 * ctypes.sizeof(ctypes.c_float),  # stride
                ctypes.cast(
                    2 * ctypes.sizeof(ctypes.c_float), ctypes.c_void_p
                ),  # array buffer offset
            )
            gl.glEnableVertexAttribArray(1)  # use currently bound VAO
            yield
        finally:
            log.debug("cleaning up buffer")
            gl.glDisableVertexAttribArray(0)
            gl.glDisableVertexAttribArray(1)
            gl.glDeleteBuffers(1, [vertex_buffer])


@contextlib.contextmanager
def load_shaders() -> Generator[None, None, None]:
    shaders = {
        gl.GL_VERTEX_SHADER: """\
            #version 330 core

            layout(location = 0) in vec3 iVertexPosition;
            layout(location = 1) in vec3 iVertexColor;
            out vec3 FragColor;
            void main() {
                FragColor = iVertexColor;
                gl_Position = vec4(iVertexPosition, 1.f);
            }
            """,
        gl.GL_FRAGMENT_SHADER: """\
            #version 330 core

            in vec3 FragColor;
            out vec4 oFragColor;
            void main() {
                oFragColor = vec4(FragColor, 1.f);
            }
            """,
    }
    log.debug("creating the shader program")
    program_id = gl.glCreateProgram()
    try:
        shader_ids = []
        for shader_type, shader_src in shaders.items():
            shader_id = gl.glCreateShader(shader_type)
            gl.glShaderSource(shader_id, shader_src)

            log.debug(f"compiling the {shader_type} shader")
            gl.glCompileShader(shader_id)

            # check if compilation was successful
            result = gl.glGetShaderiv(shader_id, gl.GL_COMPILE_STATUS)
            info_log_len = gl.glGetShaderiv(shader_id, gl.GL_INFO_LOG_LENGTH)
            if info_log_len:
                logmsg = gl.glGetShaderInfoLog(shader_id)
                log.error(logmsg)
            if result is False:
                sys.exit(10)

            gl.glAttachShader(program_id, shader_id)
            shader_ids.append(shader_id)

        log.debug("linking shader program")
        gl.glLinkProgram(program_id)

        # check if linking was successful
        result = gl.glGetProgramiv(program_id, gl.GL_LINK_STATUS)
        info_log_len = gl.glGetProgramiv(program_id, gl.GL_INFO_LOG_LENGTH)
        if info_log_len:
            logmsg = gl.glGetProgramInfoLog(program_id)
            log.error(logmsg)
        if result is False:
            sys.exit(11)

        log.debug("installing shader program into rendering state")
        gl.glUseProgram(program_id)
        yield
    finally:
        log.debug("cleaning up shader program")
        for shader_id in shader_ids:
            gl.glDetachShader(program_id, shader_id)
            gl.glDeleteShader(shader_id)
        gl.glUseProgram(0)
        gl.glDeleteProgram(program_id)


def main_loop(window: Any) -> None:
    while glfw.get_key(
        window, glfw.KEY_ESCAPE
    ) != glfw.PRESS and not glfw.window_should_close(window):
        gl.glClear(gl.GL_COLOR_BUFFER_BIT | gl.GL_DEPTH_BUFFER_BIT)
        # Draw the triangle
        gl.glDrawArrays(gl.GL_TRIANGLES, 0, 3)  # Starting from vertex 0
        # 3 vertices total -> 1 triangle
        glfw.swap_buffers(window)
        glfw.poll_events()


@pytest.mark.opengl()
def test_triangle() -> None:
    logging.basicConfig(level=logging.DEBUG)
    with create_main_window() as window:
        with create_vertex_buffer():
            with load_shaders():
                main_loop(window)
