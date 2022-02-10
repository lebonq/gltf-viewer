from dataclasses import (
    dataclass,
)
from pathlib import (
    Path,
)

import typer

from gltf_viewer import (
    __version__,
)
from gltf_viewer.utils.cli import (
    ENVVAR_PREFIX,
    LogLevelOption,
    VersionOption,
)
from gltf_viewer.utils.logging import (
    LogLevel,
    get_logger,
)

app = typer.Typer(
    invoke_without_command=True,
    no_args_is_help=True,
)

logger = get_logger(__name__)


@app.callback()
def cli_callback(
    ctx: typer.Context,
    log_level: str = LogLevelOption(),
    version: bool = VersionOption(__version__),
    example_config_path: Path = typer.Option(
        default=Path(),
        envvar=f"{ENVVAR_PREFIX}_EXAMPLE_CONFIG_PATH",
        file_okay=False,
        dir_okay=True,
    ),
) -> None:
    ctx.obj = Config(
        log_level=LogLevel(log_level), example_config_path=example_config_path
    )
    logger.debug(f"Using example_config_path {ctx.obj.example_config_path.absolute()}")


@dataclass
class Config:
    log_level: LogLevel
    example_config_path: Path


def get_config(ctx: typer.Context) -> Config:
    return ctx.obj


@app.command()
def about() -> None:
    typer.echo(f"glTF Viewer CLI version {__version__}")


example_app = typer.Typer(help="The example app")
app.add_typer(example_app, name="example")


@example_app.command()
def foo(
    ctx: typer.Context,
) -> None:
    config = get_config(ctx)
    logger.debug(f"Running foo command with {config.example_config_path}")


if __name__ == "__main__":
    app()
