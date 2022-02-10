"""
CLI utilities for developer environment. Just run `python .devenv` from root directory to execute.
"""
import typer

from gltf_viewer.utils.logging import (
    LogLevel,
    get_logger,
    init_logging,
)


init_logging(LogLevel.DEBUG, "gltf_viewer", "devenv")

logger = get_logger("devenv")

app = typer.Typer(
    no_args_is_help=True,
    invoke_without_command=True,
)

sub_app_1 = typer.Typer(help="Commands related to sub_app_1 application")
app.add_typer(sub_app_1, name="sub_app_1")


@sub_app_1.command()
def sub_app_1_command() -> None:
    logger.info("Running sub_app_1_command")


if __name__ == "__main__":
    app()
