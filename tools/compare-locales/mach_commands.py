# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import logging
import os
import tempfile
from pathlib import Path

from appdirs import user_config_dir
from hglib.error import CommandError
from mach.base import FailedCommandError
from mach.decorators import Command, CommandArgument
from mozrelease.scriptworker_canary import get_secret
from redo import retry


@Command(
    "compare-locales",
    category="build",
    description="Run source checks on a localization.",
)
@CommandArgument(
    "config_paths",
    metavar="l10n.toml",
    nargs="+",
    help="TOML or INI file for the project",
)
@CommandArgument(
    "l10n_base_dir",
    metavar="l10n-base-dir",
    help="Parent directory of localizations",
)
@CommandArgument(
    "locales",
    nargs="*",
    metavar="locale-code",
    help="Locale code and top-level directory of each localization",
)
@CommandArgument(
    "-q",
    "--quiet",
    action="count",
    default=0,
    help="""Show less data.
Specified once, don't show obsolete entities. Specified twice, also hide
missing entities. Specify thrice to exclude warnings and four times to
just show stats""",
)
@CommandArgument("-m", "--merge", help="""Use this directory to stage merged files""")
@CommandArgument(
    "--validate", action="store_true", help="Run compare-locales against reference"
)
@CommandArgument(
    "--json",
    help="""Serialize to JSON. Value is the name of
the output file, pass "-" to serialize to stdout and hide the default output.
""",
)
@CommandArgument(
    "-D",
    action="append",
    metavar="var=value",
    default=[],
    dest="defines",
    help="Overwrite variables in TOML files",
)
@CommandArgument(
    "--full", action="store_true", help="Compare projects that are disabled"
)
@CommandArgument(
    "--return-zero", action="store_true", help="Return 0 regardless of l10n status"
)
def compare(command_context, **kwargs):
    """Run compare-locales."""
    from compare_locales.commands import CompareLocales

    class ErrorHelper(object):
        """Dummy ArgumentParser to marshall compare-locales
        commandline errors to mach exceptions.
        """

        def error(self, msg):
            raise FailedCommandError(msg)

        def exit(self, message=None, status=0):
            raise FailedCommandError(message, exit_code=status)

    cmd = CompareLocales()
    cmd.parser = ErrorHelper()
    return cmd.handle(**kwargs)


# https://stackoverflow.com/a/14117511
def _positive_int(value):
    value = int(value)
    if value <= 0:
        raise argparse.ArgumentTypeError(f"{value} must be a positive integer.")
    return value


class RetryError(Exception):
    ...


VCT_PATH = Path(".").resolve() / "vct"
VCT_URL = "https://hg.mozilla.org/hgcustom/version-control-tools/"
FXTREE_PATH = VCT_PATH / "hgext" / "firefoxtree"
HGRC_PATH = Path(user_config_dir("hg")).joinpath("hgrc")


@Command(
    "l10n-cross-channel",
    category="misc",
    description="Create cross-channel content.",
)
@CommandArgument(
    "--strings-path",
    "-s",
    metavar="en-US",
    type=Path,
    default=Path("en-US"),
    help="Path to mercurial repository for gecko-strings-quarantine",
)
@CommandArgument(
    "--outgoing-path",
    "-o",
    type=Path,
    help="create an outgoing() patch if there are changes",
)
@CommandArgument(
    "--attempts",
    type=_positive_int,
    default=1,
    help="Number of times to try (for automation)",
)
@CommandArgument(
    "--ssh-secret",
    action="store",
    help="Taskcluster secret to use to push (for automation)",
)
@CommandArgument(
    "actions",
    choices=("prep", "create", "push", "clean"),
    nargs="+",
    # This help block will be poorly formatted until we fix bug 1714239
    help="""
    "prep": clone repos and pull heads.
    "create": create the en-US strings commit an optionally create an
              outgoing() patch.
    "push": push the en-US strings to the quarantine repo.
    "clean": clean up any sub-repos.
    """,
)
def cross_channel(
    command_context,
    strings_path,
    outgoing_path,
    actions,
    attempts,
    ssh_secret,
    **kwargs,
):
    """Run l10n cross-channel content generation."""
    # This can be any path, as long as the name of the directory is en-US.
    # Not entirely sure where this is a requirement; perhaps in l10n
    # string manipulation logic?
    if strings_path.name != "en-US":
        raise FailedCommandError("strings_path needs to be named `en-US`")
    command_context.activate_virtualenv()
    # XXX pin python requirements
    command_context.virtualenv_manager.install_pip_requirements(
        Path(os.path.dirname(__file__)) / "requirements.in"
    )
    strings_path = strings_path.resolve()  # abspath
    if outgoing_path:
        outgoing_path = outgoing_path.resolve()  # abspath
    get_config = kwargs.get("get_config", None)
    try:
        with tempfile.TemporaryDirectory() as ssh_key_dir:
            retry(
                _do_create_content,
                attempts=attempts,
                retry_exceptions=(RetryError,),
                args=(
                    command_context,
                    strings_path,
                    outgoing_path,
                    ssh_secret,
                    Path(ssh_key_dir),
                    actions,
                    get_config,
                ),
            )
    except RetryError as exc:
        raise FailedCommandError(exc) from exc


def _do_create_content(
    command_context,
    strings_path,
    outgoing_path,
    ssh_secret,
    ssh_key_dir,
    actions,
    get_config,
):
    from mozxchannel import CrossChannelCreator, get_default_config

    get_config = get_config or get_default_config

    config = get_config(Path(command_context.topsrcdir), strings_path)
    ccc = CrossChannelCreator(config)
    status = 0
    changes = False
    ssh_key_secret = None
    ssh_key_file = None

    if "prep" in actions:
        if ssh_secret:
            if not os.environ.get("MOZ_AUTOMATION"):
                raise CommandError(
                    "I don't know how to fetch the ssh secret outside of automation!"
                )
            ssh_key_secret = get_secret(ssh_secret)
            ssh_key_file = ssh_key_dir.joinpath("id_rsa")
            ssh_key_file.write_text(ssh_key_secret["ssh_privkey"])
            ssh_key_file.chmod(0o600)
        # Set up firefoxtree for comm per bug 1659691 comment 22
        if os.environ.get("MOZ_AUTOMATION") and not HGRC_PATH.exists():
            _clone_hg_repo(command_context, VCT_URL, VCT_PATH)
            hgrc_content = [
                "[extensions]",
                f"firefoxtree = {FXTREE_PATH}",
                "",
                "[ui]",
                "username = trybld",
            ]
            if ssh_key_file:
                hgrc_content.extend(
                    [
                        f"ssh = ssh -i {ssh_key_file} -l {ssh_key_secret['user']}",
                    ]
                )
            HGRC_PATH.write_text("\n".join(hgrc_content))
        if strings_path.exists() and _check_outgoing(command_context, strings_path):
            _strip_outgoing(command_context, strings_path)
        # Clone strings + source repos, pull heads
        for repo_config in (config["strings"], *config["source"].values()):
            if not repo_config["path"].exists():
                _clone_hg_repo(
                    command_context, repo_config["url"], str(repo_config["path"])
                )
            for head in repo_config["heads"].keys():
                command = ["hg", "--cwd", str(repo_config["path"]), "pull"]
                command.append(head)
                status = _retry_run_process(
                    command_context, command, ensure_exit_code=False
                )
                if status not in (0, 255):  # 255 on pull with no changes
                    raise RetryError(f"Failure on pull: status {status}!")
                if repo_config.get("update_on_pull"):
                    command = [
                        "hg",
                        "--cwd",
                        str(repo_config["path"]),
                        "up",
                        "-C",
                        "-r",
                        head,
                    ]
                    status = _retry_run_process(
                        command_context, command, ensure_exit_code=False
                    )
                    if status not in (0, 255):  # 255 on pull with no changes
                        raise RetryError(f"Failure on update: status {status}!")
            _check_hg_repo(
                command_context,
                repo_config["path"],
                heads=repo_config.get("heads", {}).keys(),
            )
    else:
        _check_hg_repo(command_context, strings_path)
        for repo_config in config.get("source", {}).values():
            _check_hg_repo(
                command_context,
                repo_config["path"],
                heads=repo_config.get("heads", {}).keys(),
            )
        if _check_outgoing(command_context, strings_path):
            raise RetryError(f"check: Outgoing changes in {strings_path}!")

    if "create" in actions:
        try:
            status = ccc.create_content()
            changes = True
            _create_outgoing_patch(command_context, outgoing_path, strings_path)
        except CommandError as exc:
            if exc.ret != 1:
                raise RetryError(exc) from exc
            command_context.log(logging.INFO, "create", {}, "No new strings.")

    if "push" in actions:
        if changes:
            _retry_run_process(
                command_context,
                [
                    "hg",
                    "--cwd",
                    str(strings_path),
                    "push",
                    "-r",
                    ".",
                    config["strings"]["push_url"],
                ],
                line_handler=print,
            )
        else:
            command_context.log(logging.INFO, "push", {}, "Skipping empty push.")

    if "clean" in actions:
        for repo_config in config.get("source", {}).values():
            if repo_config.get("post-clobber", False):
                _nuke_hg_repo(command_context, str(repo_config["path"]))

    return status


def _check_outgoing(command_context, strings_path):
    status = _retry_run_process(
        command_context,
        ["hg", "--cwd", str(strings_path), "out", "-r", "."],
        ensure_exit_code=False,
    )
    if status == 0:
        return True
    if status == 1:
        return False
    raise RetryError(f"Outgoing check in {strings_path} returned unexpected {status}!")


def _strip_outgoing(command_context, strings_path):
    _retry_run_process(
        command_context,
        [
            "hg",
            "--config",
            "extensions.strip=",
            "--cwd",
            str(strings_path),
            "strip",
            "--no-backup",
            "outgoing()",
        ],
    )


def _create_outgoing_patch(command_context, path, strings_path):
    if not path:
        return
    if not path.parent.exists():
        os.makedirs(path.parent)
    with open(path, "w") as fh:

        def writeln(line):
            fh.write(f"{line}\n")

        _retry_run_process(
            command_context,
            [
                "hg",
                "--cwd",
                str(strings_path),
                "log",
                "--patch",
                "--verbose",
                "-r",
                "outgoing()",
            ],
            line_handler=writeln,
        )


def _retry_run_process(command_context, *args, error_msg=None, **kwargs):
    try:
        return command_context.run_process(*args, **kwargs)
    except Exception as exc:
        raise RetryError(error_msg or str(exc)) from exc


def _check_hg_repo(command_context, path, heads=None):
    if not (path.is_dir() and (path / ".hg").is_dir()):
        raise RetryError(f"{path} is not a Mercurial repository")
    if heads:
        for head in heads:
            _retry_run_process(
                command_context,
                ["hg", "--cwd", str(path), "log", "-r", head],
                error_msg=f"check: {path} has no head {head}!",
            )


def _clone_hg_repo(command_context, url, path):
    _retry_run_process(command_context, ["hg", "clone", url, str(path)])


def _nuke_hg_repo(command_context, path):
    _retry_run_process(command_context, ["rm", "-rf", str(path)])
