from __future__ import absolute_import, unicode_literals

import inspect
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import zipfile

import pypiserver
import pytest
import pytest_localserver.http
import six

import virtualenv

try:
    from pathlib import Path
    from unittest.mock import NonCallableMock, call, patch
except ImportError:
    from mock import NonCallableMock, call, patch
    from pathlib2 import Path


try:
    import venv as std_venv
except ImportError:
    std_venv = None


def test_version():
    """Should have a version string"""
    assert virtualenv.virtualenv_version, "Should have version"


class TestGetInstalledPythons:
    key_local_machine = "key-local-machine"
    key_current_user = "key-current-user"
    key_local_machine_64 = "key-local-machine-64"
    key_current_user_64 = "key-current-user-64"

    @classmethod
    def mock_virtualenv_winreg(cls, monkeypatch, data):
        def enum_key(key, index):
            try:
                return data.get(key, [])[index]
            except IndexError:
                raise WindowsError

        def query_value(key, path):
            installed_version_tags = data.get(key, [])
            suffix = "\\InstallPath"
            if path.endswith(suffix):
                version_tag = path[: -len(suffix)]
                if version_tag in installed_version_tags:
                    return "{}-{}-path".format(key, version_tag)
            raise WindowsError

        mock_winreg = NonCallableMock(
            spec_set=[
                "HKEY_LOCAL_MACHINE",
                "HKEY_CURRENT_USER",
                "KEY_READ",
                "KEY_WOW64_32KEY",
                "KEY_WOW64_64KEY",
                "OpenKey",
                "EnumKey",
                "QueryValue",
                "CloseKey",
            ]
        )
        mock_winreg.HKEY_LOCAL_MACHINE = "HKEY_LOCAL_MACHINE"
        mock_winreg.HKEY_CURRENT_USER = "HKEY_CURRENT_USER"
        mock_winreg.KEY_READ = 0x10
        mock_winreg.KEY_WOW64_32KEY = 0x1
        mock_winreg.KEY_WOW64_64KEY = 0x2
        mock_winreg.OpenKey.side_effect = [
            cls.key_local_machine,
            cls.key_current_user,
            cls.key_local_machine_64,
            cls.key_current_user_64,
        ]
        mock_winreg.EnumKey.side_effect = enum_key
        mock_winreg.QueryValue.side_effect = query_value
        mock_winreg.CloseKey.return_value = None
        monkeypatch.setattr(virtualenv, "winreg", mock_winreg)
        return mock_winreg

    @pytest.mark.skipif(sys.platform == "win32", reason="non-windows specific test")
    def test_on_non_windows(self, monkeypatch):
        assert not virtualenv.IS_WIN
        assert not hasattr(virtualenv, "winreg")
        assert virtualenv.get_installed_pythons() == {}

    @pytest.mark.skipif(sys.platform != "win32", reason="windows specific test")
    def test_on_windows(self, monkeypatch):
        assert virtualenv.IS_WIN
        mock_winreg = self.mock_virtualenv_winreg(
            monkeypatch,
            {
                self.key_local_machine: (
                    "2.4",
                    "2.7",
                    "3.2",
                    "3.4",
                    "3.6-32",  # 32-bit only
                    "3.7-32",  # both 32 & 64-bit with a 64-bit user install
                ),
                self.key_current_user: ("2.5", "2.7", "3.8-32"),
                self.key_local_machine_64: (
                    "2.6",
                    "3.5",  # 64-bit only
                    "3.7",
                    "3.8",  # 64-bit with a 32-bit user install
                ),
                self.key_current_user_64: ("3.7",),
            },
        )
        monkeypatch.setattr(virtualenv, "join", "{}\\{}".format)

        installed_pythons = virtualenv.get_installed_pythons()

        assert installed_pythons == {
            "2": self.key_current_user + "-2.7-path\\python.exe",
            "2-32": self.key_current_user + "-2.7-path\\python.exe",
            "2-64": self.key_local_machine_64 + "-2.6-path\\python.exe",
            "2.4": self.key_local_machine + "-2.4-path\\python.exe",
            "2.4-32": self.key_local_machine + "-2.4-path\\python.exe",
            "2.5": self.key_current_user + "-2.5-path\\python.exe",
            "2.5-32": self.key_current_user + "-2.5-path\\python.exe",
            "2.6": self.key_local_machine_64 + "-2.6-path\\python.exe",
            "2.6-64": self.key_local_machine_64 + "-2.6-path\\python.exe",
            "2.7": self.key_current_user + "-2.7-path\\python.exe",
            "2.7-32": self.key_current_user + "-2.7-path\\python.exe",
            "3": self.key_local_machine_64 + "-3.8-path\\python.exe",
            "3-32": self.key_current_user + "-3.8-32-path\\python.exe",
            "3-64": self.key_local_machine_64 + "-3.8-path\\python.exe",
            "3.2": self.key_local_machine + "-3.2-path\\python.exe",
            "3.2-32": self.key_local_machine + "-3.2-path\\python.exe",
            "3.4": self.key_local_machine + "-3.4-path\\python.exe",
            "3.4-32": self.key_local_machine + "-3.4-path\\python.exe",
            "3.5": self.key_local_machine_64 + "-3.5-path\\python.exe",
            "3.5-64": self.key_local_machine_64 + "-3.5-path\\python.exe",
            "3.6": self.key_local_machine + "-3.6-32-path\\python.exe",
            "3.6-32": self.key_local_machine + "-3.6-32-path\\python.exe",
            "3.7": self.key_current_user_64 + "-3.7-path\\python.exe",
            "3.7-32": self.key_local_machine + "-3.7-32-path\\python.exe",
            "3.7-64": self.key_current_user_64 + "-3.7-path\\python.exe",
            "3.8": self.key_local_machine_64 + "-3.8-path\\python.exe",
            "3.8-32": self.key_current_user + "-3.8-32-path\\python.exe",
            "3.8-64": self.key_local_machine_64 + "-3.8-path\\python.exe",
        }
        assert mock_winreg.mock_calls == [
            call.OpenKey(mock_winreg.HKEY_LOCAL_MACHINE, "Software\\Python\\PythonCore", 0, 0x11),
            call.EnumKey(self.key_local_machine, 0),
            call.QueryValue(self.key_local_machine, "2.4\\InstallPath"),
            call.EnumKey(self.key_local_machine, 1),
            call.QueryValue(self.key_local_machine, "2.7\\InstallPath"),
            call.EnumKey(self.key_local_machine, 2),
            call.QueryValue(self.key_local_machine, "3.2\\InstallPath"),
            call.EnumKey(self.key_local_machine, 3),
            call.QueryValue(self.key_local_machine, "3.4\\InstallPath"),
            call.EnumKey(self.key_local_machine, 4),
            call.QueryValue(self.key_local_machine, "3.6-32\\InstallPath"),
            call.EnumKey(self.key_local_machine, 5),
            call.QueryValue(self.key_local_machine, "3.7-32\\InstallPath"),
            call.EnumKey(self.key_local_machine, 6),
            call.CloseKey(self.key_local_machine),
            call.OpenKey(mock_winreg.HKEY_CURRENT_USER, "Software\\Python\\PythonCore", 0, 0x11),
            call.EnumKey(self.key_current_user, 0),
            call.QueryValue(self.key_current_user, "2.5\\InstallPath"),
            call.EnumKey(self.key_current_user, 1),
            call.QueryValue(self.key_current_user, "2.7\\InstallPath"),
            call.EnumKey(self.key_current_user, 2),
            call.QueryValue(self.key_current_user, "3.8-32\\InstallPath"),
            call.EnumKey(self.key_current_user, 3),
            call.CloseKey(self.key_current_user),
            call.OpenKey(mock_winreg.HKEY_LOCAL_MACHINE, "Software\\Python\\PythonCore", 0, 0x12),
            call.EnumKey(self.key_local_machine_64, 0),
            call.QueryValue(self.key_local_machine_64, "2.6\\InstallPath"),
            call.EnumKey(self.key_local_machine_64, 1),
            call.QueryValue(self.key_local_machine_64, "3.5\\InstallPath"),
            call.EnumKey(self.key_local_machine_64, 2),
            call.QueryValue(self.key_local_machine_64, "3.7\\InstallPath"),
            call.EnumKey(self.key_local_machine_64, 3),
            call.QueryValue(self.key_local_machine_64, "3.8\\InstallPath"),
            call.EnumKey(self.key_local_machine_64, 4),
            call.CloseKey(self.key_local_machine_64),
            call.OpenKey(mock_winreg.HKEY_CURRENT_USER, "Software\\Python\\PythonCore", 0, 0x12),
            call.EnumKey(self.key_current_user_64, 0),
            call.QueryValue(self.key_current_user_64, "3.7\\InstallPath"),
            call.EnumKey(self.key_current_user_64, 1),
            call.CloseKey(self.key_current_user_64),
        ]

    @pytest.mark.skipif(sys.platform != "win32", reason="windows specific test")
    def test_on_windows_with_no_installations(self, monkeypatch):
        assert virtualenv.IS_WIN
        mock_winreg = self.mock_virtualenv_winreg(monkeypatch, {})

        installed_pythons = virtualenv.get_installed_pythons()

        assert installed_pythons == {}
        assert mock_winreg.mock_calls == [
            call.OpenKey(mock_winreg.HKEY_LOCAL_MACHINE, "Software\\Python\\PythonCore", 0, 0x11),
            call.EnumKey(self.key_local_machine, 0),
            call.CloseKey(self.key_local_machine),
            call.OpenKey(mock_winreg.HKEY_CURRENT_USER, "Software\\Python\\PythonCore", 0, 0x11),
            call.EnumKey(self.key_current_user, 0),
            call.CloseKey(self.key_current_user),
            call.OpenKey(mock_winreg.HKEY_LOCAL_MACHINE, "Software\\Python\\PythonCore", 0, 0x12),
            call.EnumKey(self.key_local_machine_64, 0),
            call.CloseKey(self.key_local_machine_64),
            call.OpenKey(mock_winreg.HKEY_CURRENT_USER, "Software\\Python\\PythonCore", 0, 0x12),
            call.EnumKey(self.key_current_user_64, 0),
            call.CloseKey(self.key_current_user_64),
        ]


@patch("distutils.spawn.find_executable")
@patch("virtualenv.is_executable", return_value=True)
@patch("virtualenv.get_installed_pythons")
@patch("os.path.exists", return_value=True)
@patch("os.path.abspath")
def test_resolve_interpreter_with_installed_python(
    mock_abspath, mock_exists, mock_get_installed_pythons, mock_is_executable, mock_find_executable
):
    test_tag = "foo"
    test_path = "/path/to/foo/python.exe"
    test_abs_path = "some-abs-path"
    test_found_path = "some-found-path"
    mock_get_installed_pythons.return_value = {test_tag: test_path, test_tag + "2": test_path + "2"}
    mock_abspath.return_value = test_abs_path
    mock_find_executable.return_value = test_found_path

    exe = virtualenv.resolve_interpreter("foo")

    assert exe == test_found_path, "installed python should be accessible by key"

    mock_get_installed_pythons.assert_called_once_with()
    mock_abspath.assert_called_once_with(test_path)
    mock_find_executable.assert_called_once_with(test_path)
    mock_exists.assert_called_once_with(test_found_path)
    mock_is_executable.assert_called_once_with(test_found_path)


@patch("virtualenv.is_executable", return_value=True)
@patch("virtualenv.get_installed_pythons", return_value={"foo": "bar"})
@patch("os.path.exists", return_value=True)
def test_resolve_interpreter_with_absolute_path(mock_exists, mock_get_installed_pythons, mock_is_executable):
    """Should return absolute path if given and exists"""
    test_abs_path = os.path.abspath("/usr/bin/python53")

    exe = virtualenv.resolve_interpreter(test_abs_path)

    assert exe == test_abs_path, "Absolute path should return as is"

    mock_exists.assert_called_with(test_abs_path)
    mock_is_executable.assert_called_with(test_abs_path)


@patch("virtualenv.get_installed_pythons", return_value={"foo": "bar"})
@patch("os.path.exists", return_value=False)
def test_resolve_interpreter_with_nonexistent_interpreter(mock_exists, mock_get_installed_pythons):
    """Should SystemExit with an nonexistent python interpreter path"""
    with pytest.raises(SystemExit):
        virtualenv.resolve_interpreter("/usr/bin/python53")

    mock_exists.assert_called_with("/usr/bin/python53")


@patch("virtualenv.is_executable", return_value=False)
@patch("os.path.exists", return_value=True)
def test_resolve_interpreter_with_invalid_interpreter(mock_exists, mock_is_executable):
    """Should exit when with absolute path if not exists"""
    invalid = os.path.abspath("/usr/bin/pyt_hon53")

    with pytest.raises(SystemExit):
        virtualenv.resolve_interpreter(invalid)

    mock_exists.assert_called_with(invalid)
    mock_is_executable.assert_called_with(invalid)


def test_activate_after_future_statements():
    """Should insert activation line after last future statement"""
    script = [
        "#!/usr/bin/env python",
        "from __future__ import with_statement",
        "from __future__ import print_function",
        'print("Hello, world!")',
    ]
    out = virtualenv.relative_script(script)
    assert out == [
        "#!/usr/bin/env python",
        "from __future__ import with_statement",
        "from __future__ import print_function",
        "",
        "import os; "
        "activate_this=os.path.join(os.path.dirname(os.path.realpath(__file__)), 'activate_this.py'); "
        "exec(compile(open(activate_this).read(), activate_this, 'exec'), { '__file__': activate_this}); "
        "del os, activate_this",
        "",
        'print("Hello, world!")',
    ], out


def test_cop_update_defaults_with_store_false():
    """store_false options need reverted logic"""

    class MyConfigOptionParser(virtualenv.ConfigOptionParser):
        def __init__(self, *args, **kwargs):
            self.config = virtualenv.ConfigParser.RawConfigParser()
            self.files = []
            optparse.OptionParser.__init__(self, *args, **kwargs)

        def get_environ_vars(self, prefix="VIRTUALENV_"):
            yield ("no_site_packages", "1")

    cop = MyConfigOptionParser()
    cop.add_option(
        "--no-site-packages",
        dest="system_site_packages",
        action="store_false",
        help="Don't give access to the global site-packages dir to the " "virtual environment (default)",
    )

    defaults = {}
    cop.update_defaults(defaults)
    assert defaults == {"system_site_packages": 0}


def test_install_python_bin():
    """Should create the right python executables and links"""
    tmp_virtualenv = tempfile.mkdtemp()
    try:
        home_dir, lib_dir, inc_dir, bin_dir = virtualenv.path_locations(tmp_virtualenv)
        virtualenv.install_python(home_dir, lib_dir, inc_dir, bin_dir, False, False)

        if virtualenv.IS_WIN:
            required_executables = ["python.exe", "pythonw.exe"]
        else:
            py_exe_no_version = "python"
            py_exe_version_major = "python%s" % sys.version_info[0]
            py_exe_version_major_minor = "python{}.{}".format(sys.version_info[0], sys.version_info[1])
            required_executables = [py_exe_no_version, py_exe_version_major, py_exe_version_major_minor]

        for pth in required_executables:
            assert os.path.exists(os.path.join(bin_dir, pth)), "%s should exist in bin_dir" % pth
        root_inc_dir = os.path.join(home_dir, "include")
        assert not os.path.islink(root_inc_dir)
    finally:
        shutil.rmtree(tmp_virtualenv)


@pytest.mark.skipif("platform.python_implementation() == 'PyPy'")
def test_always_copy_option():
    """Should be no symlinks in directory tree"""
    tmp_virtualenv = tempfile.mkdtemp()
    ve_path = os.path.join(tmp_virtualenv, "venv")
    try:
        virtualenv.create_environment(ve_path, symlink=False)

        for root, dirs, files in os.walk(tmp_virtualenv):
            for f in files + dirs:
                full_name = os.path.join(root, f)
                assert not os.path.islink(full_name), "%s should not be a" " symlink (to %s)" % (
                    full_name,
                    os.readlink(full_name),
                )
    finally:
        shutil.rmtree(tmp_virtualenv)


@pytest.mark.skipif(not hasattr(os, "symlink"), reason="requires working symlink implementation")
def test_relative_symlink(tmpdir):
    """ Test if a virtualenv works correctly if it was created via a symlink and this symlink is removed """

    tmpdir = str(tmpdir)
    ve_path = os.path.join(tmpdir, "venv")
    os.mkdir(ve_path)

    workdir = os.path.join(tmpdir, "work")
    os.mkdir(workdir)

    ve_path_linked = os.path.join(workdir, "venv")
    os.symlink(ve_path, ve_path_linked)

    lib64 = os.path.join(ve_path, "lib64")

    virtualenv.create_environment(ve_path_linked, symlink=True)
    if not os.path.lexists(lib64):
        # no lib 64 on this platform
        return

    assert os.path.exists(lib64)

    shutil.rmtree(workdir)

    assert os.path.exists(lib64)


@pytest.mark.skipif(not hasattr(os, "symlink"), reason="requires working symlink implementation")
def test_copyfile_from_symlink(tmp_path):
    """Test that copyfile works correctly when the source is a symlink with a
    relative target, and a symlink to a symlink. (This can occur when creating
    an environment if Python was installed using stow or homebrew.)"""

    # Set up src/link2 -> ../src/link1 -> file.
    # We will copy to a different directory, so misinterpreting either symlink
    # will be detected.
    src_dir = tmp_path / "src"
    src_dir.mkdir()
    with open(str(src_dir / "file"), "w") as f:
        f.write("contents")
    os.symlink("file", str(src_dir / "link1"))
    os.symlink(str(Path("..") / "src" / "link1"), str(src_dir / "link2"))

    # Check that copyfile works on link2.
    # This may produce a symlink or a regular file depending on the platform --
    # which doesn't matter as long as it has the right contents.
    copy_path = tmp_path / "copy"
    virtualenv.copyfile(str(src_dir / "link2"), str(copy_path))
    with open(str(copy_path), "r") as f:
        assert f.read() == "contents"

    shutil.rmtree(str(src_dir))
    os.remove(str(copy_path))


def test_missing_certifi_pem(tmp_path):
    """Make sure that we can still create virtual environment if pip is
    patched to not use certifi's cacert.pem and the file is removed.
    This can happen if pip is packaged by Linux distributions."""
    proj_dir = Path(__file__).parent.parent
    support_original = proj_dir / "virtualenv_support"
    pip_wheel = sorted(support_original.glob("pip*whl"))[0]
    whl_name = pip_wheel.name

    wheeldir = tmp_path / "wheels"
    wheeldir.mkdir()
    tmpcert = tmp_path / "tmpcert.pem"
    cacert = "pip/_vendor/certifi/cacert.pem"
    certifi = "pip/_vendor/certifi/core.py"
    oldpath = b"os.path.join(f, 'cacert.pem')"
    newpath = "r'{}'".format(tmpcert).encode()
    removed = False
    replaced = False

    with zipfile.ZipFile(str(pip_wheel), "r") as whlin:
        with zipfile.ZipFile(str(wheeldir / whl_name), "w") as whlout:
            for item in whlin.infolist():
                buff = whlin.read(item.filename)
                if item.filename == cacert:
                    tmpcert.write_bytes(buff)
                    removed = True
                    continue
                if item.filename == certifi:
                    nbuff = buff.replace(oldpath, newpath)
                    assert nbuff != buff
                    buff = nbuff
                    replaced = True
                whlout.writestr(item, buff)

    assert removed and replaced

    venvdir = tmp_path / "venv"
    search_dirs = [str(wheeldir), str(support_original)]
    virtualenv.create_environment(str(venvdir), search_dirs=search_dirs)


def test_create_environment_from_dir_with_spaces(tmpdir):
    """Should work with wheel sources read from a dir with spaces."""
    ve_path = str(tmpdir / "venv")
    spaced_support_dir = str(tmpdir / "support with spaces")
    from virtualenv_support import __file__ as support_dir

    support_dir = os.path.dirname(os.path.abspath(support_dir))
    shutil.copytree(support_dir, spaced_support_dir)
    virtualenv.create_environment(ve_path, search_dirs=[spaced_support_dir])


def test_create_environment_in_dir_with_spaces(tmpdir):
    """Should work with environment path containing spaces."""
    ve_path = str(tmpdir / "venv with spaces")
    virtualenv.create_environment(ve_path)


def test_create_environment_with_local_https_pypi(tmpdir):
    """Create virtual environment using local PyPI listening https with
    certificate signed with custom certificate authority
    """
    test_dir = Path(__file__).parent
    ssl_dir = test_dir / "ssl"
    proj_dir = test_dir.parent
    support_dir = proj_dir / "virtualenv_support"
    local_pypi_app = pypiserver.app(root=str(support_dir))
    local_pypi = pytest_localserver.http.WSGIServer(
        host="localhost",
        port=0,
        application=local_pypi_app,
        ssl_context=(str(ssl_dir / "server.crt"), str(ssl_dir / "server.key")),
    )
    local_pypi.start()
    local_pypi_url = "https://localhost:{}/".format(local_pypi.server_address[1])
    venvdir = tmpdir / "venv"
    pip_log = tmpdir / "pip.log"
    env_addition = {
        "PIP_CERT": str(ssl_dir / "rootCA.pem"),
        "PIP_INDEX_URL": local_pypi_url,
        "PIP_LOG": str(pip_log),
        "PIP_RETRIES": "0",
    }
    if six.PY2:
        env_addition = {key.encode("utf-8"): value.encode("utf-8") for key, value in env_addition.items()}
    env_backup = {}
    for key, value in env_addition.items():
        if key in os.environ:
            env_backup[key] = os.environ[key]
        os.environ[key] = value
    try:
        virtualenv.create_environment(str(venvdir), download=True)
        with pip_log.open("rb") as f:
            assert b"SSLError" not in f.read()
    finally:
        local_pypi.stop()
        for key in env_addition.keys():
            os.environ.pop(key)
            if key in env_backup:
                os.environ[key] = env_backup[key]


def check_pypy_pre_import():
    import sys

    # These modules(module_name, optional) are taken from PyPy's site.py:
    # https://bitbucket.org/pypy/pypy/src/d0187cf2f1b70ec4b60f10673ff081bdd91e9a17/lib-python/2.7/site.py#lines-532:539
    modules = [
        ("encodings", False),
        ("exceptions", True),  # "exceptions" module does not exist in Python3
        ("zipimport", True),
    ]

    for module, optional in modules:
        if not optional or module in sys.builtin_module_names:
            assert module in sys.modules, "missing {!r} in sys.modules".format(module)


@pytest.mark.skipif("platform.python_implementation() != 'PyPy'")
def test_pypy_pre_import(tmp_path):
    """For PyPy, some built-in modules should be pre-imported because
    some programs expect them to be in sys.modules on startup.
    """
    check_code = inspect.getsource(check_pypy_pre_import)
    check_code = textwrap.dedent(check_code[check_code.index("\n") + 1 :])
    if six.PY2:
        check_code = check_code.decode()

    check_prog = tmp_path / "check-pre-import.py"
    check_prog.write_text(check_code)

    ve_path = str(tmp_path / "venv")
    virtualenv.create_environment(ve_path)

    bin_dir = virtualenv.path_locations(ve_path)[-1]

    try:
        cmd = [
            os.path.join(bin_dir, "{}{}".format(virtualenv.EXPECTED_EXE, ".exe" if virtualenv.IS_WIN else "")),
            str(check_prog),
        ]
        subprocess.check_output(cmd, universal_newlines=True, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as exception:
        assert not exception.returncode, exception.output


@pytest.mark.skipif(not hasattr(os, "symlink"), reason="requires working symlink implementation")
def test_create_environment_with_exec_prefix_pointing_to_prefix(tmpdir):
    """Create virtual environment for Python with ``sys.exec_prefix`` pointing
    to ``sys.prefix`` or ``sys.base_prefix`` or ``sys.real_prefix`` under a
    different name
    """
    venvdir = str(tmpdir / "venv")
    python_dir = tmpdir / "python"
    python_dir.mkdir()
    path_key = str("PATH")
    old_path = os.environ[path_key]
    if hasattr(sys, "real_prefix"):
        os.environ[path_key] = os.pathsep.join(
            p for p in os.environ[path_key].split(os.pathsep) if not p.startswith(sys.prefix)
        )
    python = virtualenv.resolve_interpreter(os.path.basename(sys.executable))
    try:
        subprocess.check_call([sys.executable, "-m", "virtualenv", "-p", python, venvdir])
        home_dir, lib_dir, inc_dir, bin_dir = virtualenv.path_locations(venvdir)
        assert not os.path.islink(os.path.join(lib_dir, "distutils"))
    finally:
        os.environ[path_key] = old_path


@pytest.mark.skipif(not hasattr(sys, "real_prefix"), reason="requires running from inside virtualenv")
def test_create_environment_from_virtual_environment(tmpdir):
    """Create virtual environment using Python from another virtual environment
    """
    venvdir = str(tmpdir / "venv")
    home_dir, lib_dir, inc_dir, bin_dir = virtualenv.path_locations(venvdir)
    virtualenv.create_environment(venvdir)
    assert not os.path.islink(os.path.join(lib_dir, "distutils"))


@pytest.mark.skipif(std_venv is None, reason="needs standard venv module")
def test_create_environment_from_venv(tmpdir):
    std_venv_dir = str(tmpdir / "stdvenv")
    ve_venv_dir = str(tmpdir / "vevenv")
    home_dir, lib_dir, inc_dir, bin_dir = virtualenv.path_locations(ve_venv_dir)
    builder = std_venv.EnvBuilder()
    ctx = builder.ensure_directories(std_venv_dir)
    builder.create_configuration(ctx)
    builder.setup_python(ctx)
    builder.setup_scripts(ctx)
    subprocess.check_call([ctx.env_exe, virtualenv.__file__, "--no-setuptools", "--no-pip", "--no-wheel", ve_venv_dir])
    ve_exe = os.path.join(bin_dir, "python")
    out = subprocess.check_output([ve_exe, "-c", "import sys; print(sys.real_prefix)"], universal_newlines=True)
    # Test against real_prefix if present - we might be running the test from a virtualenv (e.g. tox).
    assert out.strip() == getattr(sys, "real_prefix", sys.prefix)


@pytest.mark.skipif(std_venv is None, reason="needs standard venv module")
def test_create_environment_from_venv_no_pip(tmpdir):
    std_venv_dir = str(tmpdir / "stdvenv")
    ve_venv_dir = str(tmpdir / "vevenv")
    home_dir, lib_dir, inc_dir, bin_dir = virtualenv.path_locations(ve_venv_dir)
    builder = std_venv.EnvBuilder()
    ctx = builder.ensure_directories(std_venv_dir)
    builder.create_configuration(ctx)
    builder.setup_python(ctx)
    builder.setup_scripts(ctx)
    subprocess.check_call([ctx.env_exe, virtualenv.__file__, "--no-pip", ve_venv_dir])
    ve_exe = os.path.join(bin_dir, "python")
    out = subprocess.check_output([ve_exe, "-c", "import sys; print(sys.real_prefix)"], universal_newlines=True)
    # Test against real_prefix if present - we might be running the test from a virtualenv (e.g. tox).
    assert out.strip() == getattr(sys, "real_prefix", sys.prefix)


def test_create_environment_with_old_pip(tmpdir):
    old = Path(__file__).parent / "old-wheels"
    old_pip = old / "pip-9.0.1-py2.py3-none-any.whl"
    old_setuptools = old / "setuptools-30.4.0-py2.py3-none-any.whl"
    support_dir = str(tmpdir / "virtualenv_support")
    os.makedirs(support_dir)
    for old_dep in [old_pip, old_setuptools]:
        shutil.copy(str(old_dep), support_dir)
    venvdir = str(tmpdir / "venv")
    virtualenv.create_environment(venvdir, search_dirs=[support_dir], no_wheel=True)


def test_license_builtin(clean_python):
    _, bin_dir, _ = clean_python
    proc = subprocess.Popen(
        (os.path.join(bin_dir, "python"), "-c", "license()"), stdin=subprocess.PIPE, stdout=subprocess.PIPE
    )
    out_b, _ = proc.communicate(b"q\n")
    out = out_b.decode()
    assert not proc.returncode
    assert "Ian Bicking and Contributors" not in out
