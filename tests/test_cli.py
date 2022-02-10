from pathlib import (
    Path,
)

from typer.testing import (
    CliRunner,
)

from gltf_viewer import (
    __version__,
)
from gltf_viewer.__main__ import (
    app,
)
from gltf_viewer.utils.logging import (
    Logger,
)

runner = CliRunner()


def test_about() -> None:
    result = runner.invoke(app, ["about"])
    assert result.exit_code == 0
    assert __version__ in result.stdout
    assert "glTF Viewer" in result.stdout


def test_version() -> None:
    result = runner.invoke(app, ["--version"])
    assert result.exit_code == 0
    assert __version__ == result.stdout.strip()


def test_fixtures(
    global_datadir: Path,
    datadir: Path,
    original_datadir: Path,
    shared_datadir: Path,
    original_shared_datadir: Path,
    logger: Logger,
    session_id: str,
) -> None:
    logger.info(f"Session id: {session_id}")
    logger.info(
        f"global_datadir={global_datadir}\n"
        f"datadir={datadir}\n"
        f"original_datadir={original_datadir}\n"
        f"shared_datadir={shared_datadir}\n"
        f"original_shared_datadir={original_shared_datadir}\n"
    )
