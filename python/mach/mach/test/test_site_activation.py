# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import ast
import functools
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from subprocess import CompletedProcess
from typing import List

import buildconfig
import mozunit
import pkg_resources
import pytest

from mach.site import MozSiteMetadata, PythonVirtualenv, activate_virtualenv


class ActivationContext:
    def __init__(
        self,
        topsrcdir: Path,
        work_dir: Path,
        original_python_path: str,
        stdlib_paths: List[Path],
        system_paths: List[Path],
        required_mach_sys_paths: List[Path],
        mach_requirement_paths: List[Path],
        command_requirement_path: Path,
    ):
        self.topsrcdir = topsrcdir
        self.work_dir = work_dir
        self.original_python_path = original_python_path
        self.stdlib_paths = stdlib_paths
        self.system_paths = system_paths
        self.required_moz_init_sys_paths = required_mach_sys_paths
        self.mach_requirement_paths = mach_requirement_paths
        self.command_requirement_path = command_requirement_path

    def virtualenv(self, name: str) -> PythonVirtualenv:
        base_path = self.work_dir

        if name == "mach":
            base_path = base_path / "_virtualenvs"
        return PythonVirtualenv(str(base_path / name))


def test_new_package_appears_in_pkg_resources():
    try:
        # "carrot" was chosen as the package to use because:
        # * It has to be a package that doesn't exist in-scope at the start (so,
        #   all vendored modules included in the test virtualenv aren't usage).
        # * It must be on our internal PyPI mirror.
        # Of the options, "carrot" is a small install that fits these requirements.
        pkg_resources.get_distribution("carrot")
        assert False, "Expected to not find 'carrot' as the initial state of the test"
    except pkg_resources.DistributionNotFound:
        pass

    with tempfile.TemporaryDirectory() as venv_dir:
        subprocess.check_call(
            [
                sys.executable,
                "-m",
                "venv",
                venv_dir,
            ]
        )

        venv = PythonVirtualenv(venv_dir)
        venv.pip_install(["carrot==0.10.7"])

        initial_metadata = MozSiteMetadata.from_runtime()
        try:
            metadata = MozSiteMetadata(None, None, None, None, venv.prefix)
            with metadata.update_current_site(venv.python_path):
                activate_virtualenv(venv)

            assert pkg_resources.get_distribution("carrot").version == "0.10.7"
        finally:
            MozSiteMetadata.current = initial_metadata


def test_sys_path_source_none_build(context):
    original, mach, command = _run_activation_script_for_paths(context, "none", "build")
    _assert_original_python_sys_path(context, original)

    assert not os.path.exists(context.virtualenv("mach").prefix)
    assert mach == [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
    ]

    expected_command_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        context.command_requirement_path,
    ]
    assert command == expected_command_paths

    command_venv = _sys_path_of_virtualenv(context.virtualenv("build"))
    assert command_venv == [Path(""), *expected_command_paths]


def test_sys_path_source_none_other(context):
    original, mach, command = _run_activation_script_for_paths(context, "none", "other")
    _assert_original_python_sys_path(context, original)

    assert not os.path.exists(context.virtualenv("mach").prefix)
    assert mach == [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
    ]

    command_virtualenv = PythonVirtualenv(str(context.work_dir / "other"))
    expected_command_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        context.command_requirement_path,
        *(Path(p) for p in command_virtualenv.site_packages_dirs()),
    ]
    assert command == expected_command_paths

    command_venv = _sys_path_of_virtualenv(context.virtualenv("other"))
    assert command_venv == [
        Path(""),
        *expected_command_paths,
    ]


def test_sys_path_source_venv_build(context):
    original, mach, command = _run_activation_script_for_paths(context, "pip", "build")
    _assert_original_python_sys_path(context, original)

    mach_virtualenv = context.virtualenv("mach")
    expected_mach_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *(Path(p) for p in mach_virtualenv.site_packages_dirs()),
    ]
    assert mach == expected_mach_paths

    command_virtualenv = context.virtualenv("build")
    expected_command_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *(Path(p) for p in mach_virtualenv.site_packages_dirs()),
        context.command_requirement_path,
        *(Path(p) for p in command_virtualenv.site_packages_dirs()),
    ]
    assert command == expected_command_paths

    mach_venv = _sys_path_of_virtualenv(mach_virtualenv)
    assert mach_venv == [
        Path(""),
        *expected_mach_paths,
    ]

    command_venv = _sys_path_of_virtualenv(command_virtualenv)
    assert command_venv == [
        Path(""),
        *expected_command_paths,
    ]


def test_sys_path_source_venv_other(context):
    original, mach, command = _run_activation_script_for_paths(context, "pip", "other")
    _assert_original_python_sys_path(context, original)

    mach_virtualenv = context.virtualenv("mach")
    expected_mach_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *(Path(p) for p in mach_virtualenv.site_packages_dirs()),
    ]
    assert mach == expected_mach_paths

    command_virtualenv = context.virtualenv("other")
    expected_command_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *(Path(p) for p in mach_virtualenv.site_packages_dirs()),
        context.command_requirement_path,
        *(Path(p) for p in command_virtualenv.site_packages_dirs()),
    ]
    assert command == expected_command_paths

    mach_venv = _sys_path_of_virtualenv(mach_virtualenv)
    assert mach_venv == [
        Path(""),
        *expected_mach_paths,
    ]

    command_venv = _sys_path_of_virtualenv(command_virtualenv)
    assert command_venv == [
        Path(""),
        *expected_command_paths,
    ]


def test_sys_path_source_system_build(context):
    original, mach, command = _run_activation_script_for_paths(
        context, "system", "build"
    )
    _assert_original_python_sys_path(context, original)

    assert not os.path.exists(context.virtualenv("mach").prefix)
    expected_mach_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *context.system_paths,
    ]
    assert mach == expected_mach_paths

    command_virtualenv = context.virtualenv("build")
    expected_command_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *context.system_paths,
        context.command_requirement_path,
    ]
    assert command == expected_command_paths

    command_venv = _sys_path_of_virtualenv(command_virtualenv)
    assert command_venv == [
        Path(""),
        *expected_command_paths,
    ]


def test_sys_path_source_system_other(context):
    result = _run_activation_script(
        context,
        "system",
        "other",
        context.original_python_path,
        stderr=subprocess.PIPE,
    )
    assert result.returncode != 0
    assert (
        'Cannot use MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE="system" for any sites '
        "other than" in result.stderr
    )


def test_sys_path_source_venvsystem_build(context):
    venv_system_python = _create_venv_system_python(
        context.work_dir, context.original_python_path
    )
    venv_system_site_packages_dirs = [
        Path(p) for p in venv_system_python.site_packages_dirs()
    ]
    original, mach, command = _run_activation_script_for_paths(
        context, "system", "build", venv_system_python.python_path
    )

    assert original == [
        Path(__file__).parent,
        *context.required_moz_init_sys_paths,
        *context.stdlib_paths,
        *venv_system_site_packages_dirs,
    ]

    assert not os.path.exists(context.virtualenv("mach").prefix)
    expected_mach_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *venv_system_site_packages_dirs,
    ]
    assert mach == expected_mach_paths

    command_virtualenv = context.virtualenv("build")
    expected_command_paths = [
        *context.stdlib_paths,
        *context.mach_requirement_paths,
        *venv_system_site_packages_dirs,
        context.command_requirement_path,
    ]
    assert command == expected_command_paths

    command_venv = _sys_path_of_virtualenv(command_virtualenv)
    assert command_venv == [
        Path(""),
        *expected_command_paths,
    ]


def test_sys_path_source_venvsystem_other(context):
    venv_system_python = _create_venv_system_python(
        context.work_dir, context.original_python_path
    )
    result = _run_activation_script(
        context,
        "system",
        "other",
        venv_system_python.python_path,
        stderr=subprocess.PIPE,
    )
    assert result.returncode != 0
    assert (
        'Cannot use MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE="system" for any sites '
        "other than" in result.stderr
    )


@pytest.fixture(name="context")
def _activation_context():
    original_python_path, stdlib_paths, system_paths = _original_python()
    topsrcdir = Path(buildconfig.topsrcdir)
    required_mach_sys_paths = [
        topsrcdir / "python" / "mach",
        topsrcdir / "third_party" / "python" / "packaging",
        topsrcdir / "third_party" / "python" / "pip",
    ]

    with tempfile.TemporaryDirectory() as work_dir:
        # Get "resolved" version of path to ease comparison against "site"-added sys.path
        # entries, as "site" calculates the realpath of provided locations.
        work_dir = Path(work_dir).resolve()
        mach_requirement_paths = [
            *required_mach_sys_paths,
            work_dir / "mach_site_path",
        ]
        command_requirement_path = work_dir / "command_site_path"
        (work_dir / "mach_site_path").touch()
        command_requirement_path.touch()
        yield ActivationContext(
            topsrcdir,
            work_dir,
            original_python_path,
            stdlib_paths,
            system_paths,
            required_mach_sys_paths,
            mach_requirement_paths,
            command_requirement_path,
        )


@functools.lru_cache(maxsize=None)
def _original_python():
    current_site = MozSiteMetadata.from_runtime()
    stdlib_paths, system_paths = current_site.original_python.sys_path()
    stdlib_paths = [Path(path) for path in _filter_pydev_from_paths(stdlib_paths)]
    system_paths = [Path(path) for path in system_paths]
    return current_site.original_python.python_path, stdlib_paths, system_paths


def _run_activation_script(
    context: ActivationContext,
    source: str,
    site_name: str,
    invoking_python: str,
    **kwargs
) -> CompletedProcess:
    return subprocess.run(
        [
            invoking_python,
            str(Path(__file__).parent / "script_site_activation.py"),
        ],
        stdout=subprocess.PIPE,
        universal_newlines=True,
        env={
            "TOPSRCDIR": str(context.topsrcdir),
            "COMMAND_SITE": site_name,
            "PYTHONPATH": os.pathsep.join(
                str(p) for p in context.required_moz_init_sys_paths
            ),
            "MACH_SITE_PTH_REQUIREMENTS": os.pathsep.join(
                str(p) for p in context.mach_requirement_paths
            ),
            "COMMAND_SITE_PTH_REQUIREMENTS": str(context.command_requirement_path),
            "MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE": source,
            "WORK_DIR": str(context.work_dir),
            # These two variables are needed on Windows so that Python initializes
            # properly and adds the "user site packages" to the sys.path like normal.
            "SYSTEMROOT": os.environ.get("SYSTEMROOT", ""),
            "APPDATA": os.environ.get("APPDATA", ""),
        },
        **kwargs,
    )


def _run_activation_script_for_paths(
    context: ActivationContext, source: str, site_name: str, invoking_python: str = None
) -> List[List[Path]]:
    """Return the states of the sys.path when activating Mach-managed sites

    Three sys.path states are returned:
    * The initial sys.path, equivalent to "path_to_python -c "import sys; print(sys.path)"
    * The sys.path after activating the Mach site
    * The sys.path after activating the command site
    """

    output = _run_activation_script(
        context,
        source,
        site_name,
        invoking_python or context.original_python_path,
        check=True,
    ).stdout
    # Filter to the last line, which will have our nested list that we want to
    # parse. This will avoid unrelated output, such as from virtualenv creation
    output = output.splitlines()[-1]
    return [
        [Path(path) for path in _filter_pydev_from_paths(paths)]
        for paths in ast.literal_eval(output)
    ]


def _assert_original_python_sys_path(context: ActivationContext, original: List[Path]):
    # Assert that initial sys.path (prior to any activations) matches expectations.
    assert original == [
        Path(__file__).parent,
        *context.required_moz_init_sys_paths,
        *context.stdlib_paths,
        *context.system_paths,
    ]


def _sys_path_of_virtualenv(virtualenv: PythonVirtualenv) -> List[Path]:
    output = subprocess.run(
        [virtualenv.python_path, "-c", "import sys; print(sys.path)"],
        stdout=subprocess.PIPE,
        universal_newlines=True,
        env={
            # Needed for python to initialize properly
            "SYSTEMROOT": os.environ.get("SYSTEMROOT", ""),
        },
        check=True,
    ).stdout
    return [Path(path) for path in _filter_pydev_from_paths(ast.literal_eval(output))]


def _filter_pydev_from_paths(paths: List[str]) -> List[str]:
    # Filter out injected "pydev" debugging tool if running within a JetBrains
    # debugging context.
    return [path for path in paths if "pydev" not in path and "JetBrains" not in path]


def _create_venv_system_python(
    work_dir: Path, invoking_python: str
) -> PythonVirtualenv:
    virtualenv = PythonVirtualenv(str(work_dir / "system_python"))
    subprocess.run(
        [
            invoking_python,
            "-m",
            "venv",
            virtualenv.prefix,
            "--without-pip",
        ],
        check=True,
    )
    return virtualenv


if __name__ == "__main__":
    mozunit.main()
