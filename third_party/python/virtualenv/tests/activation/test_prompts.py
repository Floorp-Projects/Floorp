"""test that prompt behavior is correct in supported shells"""
from __future__ import absolute_import, unicode_literals

import os
import subprocess
import sys
from textwrap import dedent

import pytest

import virtualenv
from virtualenv import IS_DARWIN, IS_WIN

try:
    from pathlib import Path
except ImportError:
    from pathlib2 import Path

VIRTUAL_ENV_DISABLE_PROMPT = "VIRTUAL_ENV_DISABLE_PROMPT"

# This must match the DEST_DIR provided in the ../conftest.py:clean_python fixture
ENV_DEFAULT = "env"

# This can be anything
ENV_CUSTOM = "envy"

# Standard prefix, surround the env name in parentheses and separate by a space
PREFIX_DEFAULT = "({}) ".format(ENV_DEFAULT)

# Arbitrary prefix for the environment that's provided a 'prompt' arg
PREFIX_CUSTOM = "---ENV---"

# Temp script filename template: {shell}.script.(normal|suppress).(default|custom)[extension]
SCRIPT_TEMPLATE = "{}.script.{}.{}{}"

# Temp output filename template: {shell}.out.(normal|suppress).(default|custom)
OUTPUT_TEMPLATE = "{}.out.{}.{}"

# For skipping shells not installed by default if absent on a contributor's system
IS_INSIDE_CI = "CI_RUN" in os.environ


# Py2 doesn't like unicode in the environment
def env_compat(string):
    return string.encode("utf-8") if sys.version_info.major < 3 else string


class ShellInfo(object):
    """Parent class for shell information for prompt testing."""

    # Typo insurance
    __slots__ = []

    # Equality check based on .name, but only if both are not None
    def __eq__(self, other):
        if type(self) != type(other):
            return False
        if self.name is None or other.name is None:
            return False
        return self.name == other.name

    # Helper formatting string
    @property
    def platform_incompat_msg(self):
        return "No sane provision for {} on {{}} yet".format(self.name)

    # Each shell must specify
    name = None
    avail_cmd = None
    execute_cmd = None
    prompt_cmd = None
    activate_script = None

    # Default values defined here
    # 'preamble_cmd' *MUST NOT* emit anything to stdout!
    testscript_extension = ""
    preamble_cmd = ""
    activate_cmd = "source "
    deactivate_cmd = "deactivate"
    clean_env_update = {}

    # Skip check function; must be specified per-shell
    platform_check_skip = None

    # Test assert method for comparing activated prompt to deactivated.
    # Default defined, but can be overridden per-shell. Takes the captured
    # lines of output as the lone argument.
    def overall_prompt_test(self, lines, prefix):
        """Perform all tests on (de)activated prompts.

        From a Python 3 perspective, 'lines' is expected to be *bytes*,
        and 'prefix' is expected to be *str*.

        Happily, this all seems to translate smoothly enough to 2.7.

        """
        # Prompts before activation and after deactivation should be identical.
        assert lines[1] == lines[3], lines

        # The .partition here operates on the environment marker text expected to occur
        # in the prompt. A non-empty 'env_marker' thus tests that the correct marker text
        # has been applied into the prompt string.
        before, env_marker, after = lines[2].partition(prefix.encode("utf-8"))
        assert env_marker != b"", lines

        # Some shells need custom activated-prompt tests, so this is split into
        # its own submethod.
        self.activated_prompt_test(lines, after)

    def activated_prompt_test(self, lines, after):
        """Perform just the check for the deactivated prompt contents in the activated prompt text.

        The default is a strict requirement that the portion of the activated prompt following the environment
        marker must exactly match the non-activated prompt.

        Some shells require weaker tests, due to idiosyncrasies.

        """
        assert after == lines[1], lines


class BashInfo(ShellInfo):
    name = "bash"
    avail_cmd = "bash -c 'echo foo'"
    execute_cmd = "bash"
    prompt_cmd = 'echo "$PS1"'
    activate_script = "activate"

    def platform_check_skip(self):
        if IS_WIN:
            return self.platform_incompat_msg.format(sys.platform)


class FishInfo(ShellInfo):
    name = "fish"
    avail_cmd = "fish -c 'echo foo'"
    execute_cmd = "fish"
    prompt_cmd = "fish_prompt; echo ' '"
    activate_script = "activate.fish"

    # Azure Devops doesn't set a terminal type, which breaks fish's colorization
    # machinery in a way that spuriously fouls the activation script.
    clean_env_update = {"TERM": "linux"}

    def platform_check_skip(self):
        if IS_WIN:
            return self.platform_incompat_msg.format(sys.platform)

    def activated_prompt_test(self, lines, after):
        """Require a looser match here, due to interposed ANSI color codes.

        This construction allows coping with the messiness of fish's ANSI codes for colorizing.
        It's not as rigorous as I would like---it doesn't ensure no space is inserted between
        a custom env prompt (argument to --prompt) and the base prompt---but it does provide assurance as
        to the key pieces of content that should be present.

        """
        assert lines[1] in after, lines


class CshInfo(ShellInfo):
    name = "csh"
    avail_cmd = "csh -c 'echo foo'"
    execute_cmd = "csh"
    prompt_cmd = r"set | grep -E 'prompt\s' | sed -E 's/^prompt\s+(.*)$/\1/'"
    activate_script = "activate.csh"

    # csh defaults to an unset 'prompt' in non-interactive shells
    preamble_cmd = "set prompt=%"

    def platform_check_skip(self):
        if IS_WIN:
            return self.platform_incompat_msg.format(sys.platform)

    def activated_prompt_test(self, lines, after):
        """Test with special handling on MacOS, which does funny things to stdout under (t)csh."""
        if IS_DARWIN:
            # Looser assert for (t)csh on MacOS, which prepends extra text to
            # what gets sent to stdout
            assert lines[1].endswith(after), lines
        else:
            # Otherwise, use the rigorous default
            # Full 2-arg form for super() used for 2.7 compat
            super(CshInfo, self).activated_prompt_test(lines, after)


class XonshInfo(ShellInfo):
    name = "xonsh"
    avail_cmd = "xonsh -c 'echo foo'"
    execute_cmd = "xonsh"
    prompt_cmd = "print(__xonsh__.shell.prompt)"
    activate_script = "activate.xsh"

    # Sets consistent initial state
    preamble_cmd = (
        "$VIRTUAL_ENV = ''; $PROMPT = '{env_name}$ '; "
        "$PROMPT_FIELDS['env_prefix'] = '('; $PROMPT_FIELDS['env_postfix'] = ') '"
    )

    @staticmethod
    def platform_check_skip():
        if IS_WIN:
            return "Provisioning xonsh on windows is unreliable"

        if sys.version_info < (3, 5):
            return "xonsh requires Python 3.5 at least"


class CmdInfo(ShellInfo):
    name = "cmd"
    avail_cmd = "echo foo"
    execute_cmd = ""
    prompt_cmd = "echo %PROMPT%"
    activate_script = "activate.bat"

    testscript_extension = ".bat"
    preamble_cmd = "@echo off & set PROMPT=$P$G"  # For consistent initial state
    activate_cmd = "call "
    deactivate_cmd = "call deactivate"

    def platform_check_skip(self):
        if not IS_WIN:
            return self.platform_incompat_msg.format(sys.platform)


class PoshInfo(ShellInfo):
    name = "powershell"
    avail_cmd = "powershell 'echo foo'"
    execute_cmd = "powershell -File "
    prompt_cmd = "prompt"
    activate_script = "activate.ps1"

    testscript_extension = ".ps1"
    activate_cmd = ". "

    def platform_check_skip(self):
        if not IS_WIN:
            return self.platform_incompat_msg.format(sys.platform)


SHELL_INFO_LIST = [BashInfo(), FishInfo(), CshInfo(), XonshInfo(), CmdInfo(), PoshInfo()]


@pytest.fixture(scope="module")
def posh_execute_enabled(tmp_path_factory):
    """Return check value for whether Powershell script execution is enabled.

    Posh may be available interactively, but the security settings may not allow
    execution of script files.

    # Enable with:  PS> Set-ExecutionPolicy -scope currentuser -ExecutionPolicy Bypass -Force;
    # Disable with: PS> Set-ExecutionPolicy -scope currentuser -ExecutionPolicy Restricted -Force;

    """
    if not IS_WIN:
        return False

    test_ps1 = tmp_path_factory.mktemp("posh_test") / "test.ps1"
    with open(str(test_ps1), "w") as f:
        f.write("echo 'foo bar baz'\n")

    out = subprocess.check_output(["powershell", "-File", "{}".format(str(test_ps1))], shell=True)
    return b"foo bar baz" in out


@pytest.fixture(scope="module")
def shell_avail(posh_execute_enabled):
    """Generate mapping of ShellInfo.name strings to bools of shell availability."""
    retvals = {si.name: subprocess.call(si.avail_cmd, shell=True) for si in SHELL_INFO_LIST}
    avails = {si.name: retvals[si.name] == 0 for si in SHELL_INFO_LIST}

    # Extra check for whether powershell scripts are enabled
    avails[PoshInfo().name] = avails[PoshInfo().name] and posh_execute_enabled

    return avails


@pytest.fixture(scope="module")
def custom_prompt_root(tmp_path_factory):
    """Provide Path to root with default and custom venvs created."""
    root = tmp_path_factory.mktemp("custom_prompt")
    virtualenv.create_environment(
        str(root / ENV_CUSTOM), prompt=PREFIX_CUSTOM, no_setuptools=True, no_pip=True, no_wheel=True
    )

    _, _, _, bin_dir = virtualenv.path_locations(str(root / ENV_DEFAULT))

    bin_dir_name = os.path.split(bin_dir)[-1]

    return root, bin_dir_name


@pytest.fixture(scope="module")
def clean_python_root(clean_python):
    root = Path(clean_python[0]).resolve().parent
    bin_dir_name = os.path.split(clean_python[1])[-1]

    return root, bin_dir_name


@pytest.fixture(scope="module")
def get_work_root(clean_python_root, custom_prompt_root):
    def pick_root(env):
        if env == ENV_DEFAULT:
            return clean_python_root
        elif env == ENV_CUSTOM:
            return custom_prompt_root
        else:
            raise ValueError("Invalid test virtualenv")

    return pick_root


@pytest.fixture(scope="function")
def clean_env():
    """Provide a fresh copy of the shell environment.

    VIRTUAL_ENV_DISABLE_PROMPT is always removed, if present, because
    the prompt tests assume it to be unset.

    """
    clean_env = os.environ.copy()
    clean_env.pop(env_compat(VIRTUAL_ENV_DISABLE_PROMPT), None)
    return clean_env


@pytest.mark.parametrize("shell_info", SHELL_INFO_LIST, ids=[i.name for i in SHELL_INFO_LIST])
@pytest.mark.parametrize("env", [ENV_DEFAULT, ENV_CUSTOM], ids=["default", "custom"])
@pytest.mark.parametrize(("value", "disable"), [("", False), ("0", True), ("1", True)])
def test_suppressed_prompt(shell_info, shell_avail, env, value, disable, get_work_root, clean_env):
    """Confirm non-empty VIRTUAL_ENV_DISABLE_PROMPT suppresses prompt changes on activate."""
    skip_test = shell_info.platform_check_skip()
    if skip_test:
        pytest.skip(skip_test)

    if not IS_INSIDE_CI and not shell_avail[shell_info.name]:
        pytest.skip(
            "Shell '{}' not provisioned{}".format(
                shell_info.name, " - is Powershell script execution disabled?" if shell_info == PoshInfo() else ""
            )
        )

    script_name = SCRIPT_TEMPLATE.format(shell_info.name, "suppress", env, shell_info.testscript_extension)
    output_name = OUTPUT_TEMPLATE.format(shell_info.name, "suppress", env)

    clean_env.update({env_compat(VIRTUAL_ENV_DISABLE_PROMPT): env_compat(value)})

    work_root = get_work_root(env)

    # The extra "{prompt}" here copes with some oddity of xonsh in certain emulated terminal
    # contexts: xonsh can dump stuff into the first line of the recorded script output,
    # so we have to include a dummy line of output that can get munged w/o consequence.
    with open(str(work_root[0] / script_name), "w") as f:
        f.write(
            dedent(
                """\
        {preamble}
        {prompt}
        {prompt}
        {act_cmd}{env}/{bindir}/{act_script}
        {prompt}
    """.format(
                    env=env,
                    act_cmd=shell_info.activate_cmd,
                    preamble=shell_info.preamble_cmd,
                    prompt=shell_info.prompt_cmd,
                    act_script=shell_info.activate_script,
                    bindir=work_root[1],
                )
            )
        )

    command = "{} {} > {}".format(shell_info.execute_cmd, script_name, output_name)

    assert 0 == subprocess.call(command, cwd=str(work_root[0]), shell=True, env=clean_env)

    with open(str(work_root[0] / output_name), "rb") as f:
        text = f.read()
        lines = text.split(b"\n")

    # Is the prompt suppressed based on the env var value?
    assert (lines[1] == lines[2]) == disable, text


@pytest.mark.parametrize("shell_info", SHELL_INFO_LIST, ids=[i.name for i in SHELL_INFO_LIST])
@pytest.mark.parametrize(["env", "prefix"], [(ENV_DEFAULT, PREFIX_DEFAULT), (ENV_CUSTOM, PREFIX_CUSTOM)])
def test_activated_prompt(shell_info, shell_avail, env, prefix, get_work_root, clean_env):
    """Confirm prompt modification behavior with and without --prompt specified."""
    skip_test = shell_info.platform_check_skip()
    if skip_test:
        pytest.skip(skip_test)

    if not IS_INSIDE_CI and not shell_avail[shell_info.name]:
        pytest.skip(
            "Shell '{}' not provisioned".format(shell_info.name)
            + (" - is Powershell script execution disabled?" if shell_info == PoshInfo() else "")
        )

    for k, v in shell_info.clean_env_update.items():
        clean_env.update({env_compat(k): env_compat(v)})

    script_name = SCRIPT_TEMPLATE.format(shell_info.name, "normal", env, shell_info.testscript_extension)
    output_name = OUTPUT_TEMPLATE.format(shell_info.name, "normal", env)

    work_root = get_work_root(env)

    # The extra "{prompt}" here copes with some oddity of xonsh in certain emulated terminal
    # contexts: xonsh can dump stuff into the first line of the recorded script output,
    # so we have to include a dummy line of output that can get munged w/o consequence.
    with open(str(work_root[0] / script_name), "w") as f:
        f.write(
            dedent(
                """\
        {preamble}
        {prompt}
        {prompt}
        {act_cmd}{env}/{bindir}/{act_script}
        {prompt}
        {deactivate}
        {prompt}
        """.format(
                    env=env,
                    act_cmd=shell_info.activate_cmd,
                    deactivate=shell_info.deactivate_cmd,
                    preamble=shell_info.preamble_cmd,
                    prompt=shell_info.prompt_cmd,
                    act_script=shell_info.activate_script,
                    bindir=work_root[1],
                )
            )
        )

    command = "{} {} > {}".format(shell_info.execute_cmd, script_name, output_name)

    assert 0 == subprocess.call(command, cwd=str(work_root[0]), shell=True, env=clean_env)

    with open(str(work_root[0] / output_name), "rb") as f:
        lines = f.read().split(b"\n")

    shell_info.overall_prompt_test(lines, prefix)
