from pathlib import (
    Path,
)

import pytest


def test_viewer_application() -> None:
    app_path = Path()
    width = 1080
    height = 720
    app = ViewerApplication(
        app_path,
        width,
        height,
    )
    assert app.run() == 0


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

    def __init__(self, app_path: Path, width: int, height: int) -> None:
        if width <= 0:
            raise ValueError(f"Invalid width={width}, should be positive")
        if height <= 0:
            raise ValueError(f"Invalid height={height}, should be positive")
        self.app_path = app_path
        self.width = width
        self.height = height

    def run(self) -> int:
        return 0
