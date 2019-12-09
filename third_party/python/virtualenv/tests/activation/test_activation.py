from __future__ import absolute_import, unicode_literals

import os
import pipes
import re
import subprocess
import sys
from os.path import dirname, join, normcase, realpath

import pytest
import six

import virtualenv

IS_INSIDE_CI = "CI_RUN" in os.environ


def need_executable(name, check_cmd):
    """skip running this locally if executable not found, unless we're inside the CI"""

    def wrapper(fn):
        fn = getattr(pytest.mark, name)(fn)
        if not IS_INSIDE_CI:
            # locally we disable, so that contributors don't need to have everything setup
            # noinspection PyBroadException
            try:
                fn.version = subprocess.check_output(check_cmd, env=get_env())
            except Exception as exception:
                return pytest.mark.skip(reason="{} is not available due {}".format(name, exception))(fn)
        return fn

    return wrapper


def requires(on):
    def wrapper(fn):
        return need_executable(on.cmd.replace(".exe", ""), on.check)(fn)

    return wrapper


def norm_path(path):
    # python may return Windows short paths, normalize
    path = realpath(path)
    if virtualenv.IS_WIN:
        from ctypes import create_unicode_buffer, windll

        buffer_cont = create_unicode_buffer(256)
        get_long_path_name = windll.kernel32.GetLongPathNameW
        get_long_path_name(six.text_type(path), buffer_cont, 256)  # noqa: F821
        result = buffer_cont.value
    else:
        result = path
    return normcase(result)


class Activation(object):
    cmd = ""
    extension = "test"
    invoke_script = []
    command_separator = os.linesep
    activate_cmd = "source"
    activate_script = ""
    check_has_exe = []
    check = []
    env = {}
    also_test_error_if_not_sourced = False

    def __init__(self, activation_env, tmp_path):
        self.home_dir = activation_env[0]
        self.bin_dir = activation_env[1]
        self.path = tmp_path

    def quote(self, s):
        return pipes.quote(s)

    def python_cmd(self, cmd):
        return "{} -c {}".format(self.quote(virtualenv.EXPECTED_EXE), self.quote(cmd))

    def python_script(self, script):
        return "{} {}".format(self.quote(virtualenv.EXPECTED_EXE), self.quote(script))

    def print_python_exe(self):
        return self.python_cmd("import sys; print(sys.executable)")

    def print_os_env_var(self, var):
        val = '"{}"'.format(var)
        return self.python_cmd("import os; print(os.environ.get({}, None))".format(val))

    def __call__(self, monkeypatch):
        absolute_activate_script = norm_path(join(self.bin_dir, self.activate_script))

        commands = [
            self.print_python_exe(),
            self.print_os_env_var("VIRTUAL_ENV"),
            self.activate_call(absolute_activate_script),
            self.print_python_exe(),
            self.print_os_env_var("VIRTUAL_ENV"),
            # pydoc loads documentation from the virtualenv site packages
            "pydoc -w pydoc_test",
            "deactivate",
            self.print_python_exe(),
            self.print_os_env_var("VIRTUAL_ENV"),
            "",  # just finish with an empty new line
        ]
        script = self.command_separator.join(commands)
        test_script = self.path / "script.{}".format(self.extension)
        test_script.write_text(script)
        assert test_script.exists()

        monkeypatch.chdir(str(self.path))
        invoke_shell = self.invoke_script + [str(test_script)]

        monkeypatch.delenv(str("VIRTUAL_ENV"), raising=False)

        # in case the tool is provided by the dev environment (e.g. xonosh)
        env = get_env()
        env.update(self.env)

        try:
            raw = subprocess.check_output(invoke_shell, universal_newlines=True, stderr=subprocess.STDOUT, env=env)
        except subprocess.CalledProcessError as exception:
            assert not exception.returncode, exception.output
        out = re.sub(r"pydev debugger: process \d+ is connecting\n\n", "", raw, re.M).strip().split("\n")

        # pre-activation
        assert out[0], raw
        assert out[1] == "None", raw

        # post-activation
        exe = "{}.exe".format(virtualenv.EXPECTED_EXE) if virtualenv.IS_WIN else virtualenv.EXPECTED_EXE
        assert norm_path(out[2]) == norm_path(join(self.bin_dir, exe)), raw
        assert norm_path(out[3]) == norm_path(str(self.home_dir)).replace("\\\\", "\\"), raw

        assert out[4] == "wrote pydoc_test.html"
        content = self.path / "pydoc_test.html"
        assert content.exists(), raw

        # post deactivation, same as before
        assert out[-2] == out[0], raw
        assert out[-1] == "None", raw

        if self.also_test_error_if_not_sourced:
            invoke_shell = self.invoke_script + [absolute_activate_script]

            with pytest.raises(subprocess.CalledProcessError) as c:
                subprocess.check_output(invoke_shell, stderr=subprocess.STDOUT, env=env)
            assert c.value.returncode, c

    def activate_call(self, script):
        return "{} {}".format(pipes.quote(self.activate_cmd), pipes.quote(script)).strip()


def get_env():
    env = os.environ.copy()
    env[str("PATH")] = os.pathsep.join([dirname(sys.executable)] + env.get(str("PATH"), str("")).split(os.pathsep))
    return env


class BashActivation(Activation):
    cmd = "bash.exe" if virtualenv.IS_WIN else "bash"
    invoke_script = [cmd]
    extension = "sh"
    activate_script = "activate"
    check = [cmd, "--version"]
    also_test_error_if_not_sourced = True


@pytest.mark.skipif(sys.platform == "win32", reason="no sane way to provision bash on Windows yet")
@requires(BashActivation)
def test_bash(clean_python, monkeypatch, tmp_path):
    BashActivation(clean_python, tmp_path)(monkeypatch)


class CshActivation(Activation):
    cmd = "csh.exe" if virtualenv.IS_WIN else "csh"
    invoke_script = [cmd]
    extension = "csh"
    activate_script = "activate.csh"
    check = [cmd, "--version"]


@pytest.mark.skipif(sys.platform == "win32", reason="no sane way to provision csh on Windows yet")
@requires(CshActivation)
def test_csh(clean_python, monkeypatch, tmp_path):
    CshActivation(clean_python, tmp_path)(monkeypatch)


class FishActivation(Activation):
    cmd = "fish.exe" if virtualenv.IS_WIN else "fish"
    invoke_script = [cmd]
    extension = "fish"
    activate_script = "activate.fish"
    check = [cmd, "--version"]


@pytest.mark.skipif(sys.platform == "win32", reason="no sane way to provision fish on Windows yet")
@requires(FishActivation)
def test_fish(clean_python, monkeypatch, tmp_path):
    FishActivation(clean_python, tmp_path)(monkeypatch)


class PowershellActivation(Activation):
    cmd = "powershell.exe" if virtualenv.IS_WIN else "pwsh"
    extension = "ps1"
    invoke_script = [cmd, "-File"]
    activate_script = "activate.ps1"
    activate_cmd = "."
    check = [cmd, "-c", "$PSVersionTable"]

    def quote(self, s):
        """powershell double double quote needed for quotes within single quotes"""
        return pipes.quote(s).replace('"', '""')


@requires(PowershellActivation)
def test_powershell(clean_python, monkeypatch, tmp_path):
    PowershellActivation(clean_python, tmp_path)(monkeypatch)


class XonoshActivation(Activation):
    cmd = "xonsh"
    extension = "xsh"
    invoke_script = [sys.executable, "-m", "xonsh"]
    activate_script = "activate.xsh"
    check = [sys.executable, "-m", "xonsh", "--version"]
    env = {"XONSH_DEBUG": "1", "XONSH_SHOW_TRACEBACK": "True"}

    def activate_call(self, script):
        return "{} {}".format(self.activate_cmd, repr(script)).strip()


@pytest.mark.skipif(sys.version_info < (3, 5), reason="xonosh requires Python 3.5 at least")
@requires(XonoshActivation)
def test_xonosh(clean_python, monkeypatch, tmp_path):
    XonoshActivation(clean_python, tmp_path)(monkeypatch)
