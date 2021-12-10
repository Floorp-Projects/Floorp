# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import shutil
import subprocess
import sys
from pathlib import Path

import mozunit
from buildconfig import topsrcdir
from mach.requirements import MachEnvRequirements


def _resolve_command_site_names():
    virtualenv_names = []
    for child in (Path(topsrcdir) / "build").iterdir():
        if not child.name.endswith("_virtualenv_packages.txt"):
            continue

        if child.name == "mach_virtualenv_packages.txt":
            continue

        virtualenv_names.append(child.name[: -len("_virtualenv_packages.txt")])
    return virtualenv_names


def _requirement_definition_to_pip_format(virtualenv_name, cache, is_mach_or_build_env):
    """Convert from parsed requirements object to pip-consumable format"""
    path = Path(topsrcdir) / "build" / f"{virtualenv_name}_virtualenv_packages.txt"
    requirements = MachEnvRequirements.from_requirements_definition(
        topsrcdir, False, is_mach_or_build_env, path
    )

    lines = []
    for pypi in (
        requirements.pypi_requirements + requirements.pypi_optional_requirements
    ):
        lines.append(str(pypi.requirement))

    for vendored in requirements.vendored_requirements:
        lines.append(str(cache.package_for_vendor_dir(Path(vendored.path))))

    return "\n".join(lines)


class PackageCache:
    def __init__(self, storage_dir: Path):
        self._cache = {}
        self._storage_dir = storage_dir

    def package_for_vendor_dir(self, vendor_path: Path):
        if vendor_path in self._cache:
            return self._cache[vendor_path]

        if not any((p for p in vendor_path.iterdir() if p.name.endswith(".dist-info"))):
            # This vendored package is not a wheel. It may be a source package (with
            # a setup.py), or just some Python code that was manually copied into the
            # tree. If it's a source package, the setup.py file may be up a few levels
            # from the referenced Python module path.
            package_dir = vendor_path
            while True:
                if (package_dir / "setup.py").exists():
                    break
                elif package_dir.parent == package_dir:
                    raise Exception(
                        f'Package "{vendor_path}" is not a wheel and does not have a '
                        'setup.py file. Perhaps it should be "pth:" instead of '
                        '"vendored:"?'
                    )
                package_dir = package_dir.parent

            self._cache[vendor_path] = package_dir
            return package_dir

        # Pip requires that wheels have a version number in their name, even if
        # it ignores it. We should parse out the version and put it in here
        # so that failure debugging is easier, but that's non-trivial work.
        # So, this "0" satisfies pip's naming requirement while being relatively
        # obvious that it's a placeholder.
        output_path = self._storage_dir / f"{vendor_path.name}-0-py3-none-any"
        shutil.make_archive(str(output_path), "zip", vendor_path)

        whl_path = output_path.parent / (output_path.name + ".whl")
        (output_path.parent / (output_path.name + ".zip")).rename(whl_path)
        self._cache[vendor_path] = whl_path

        return whl_path


def test_sites_compatible(tmpdir: str):
    command_site_names = _resolve_command_site_names()
    work_dir = Path(tmpdir)
    cache = PackageCache(work_dir)
    mach_requirements = _requirement_definition_to_pip_format("mach", cache, True)

    # Create virtualenv to try to install all dependencies into.
    subprocess.check_call(
        [
            sys.executable,
            str(
                Path(topsrcdir)
                / "third_party"
                / "python"
                / "virtualenv"
                / "virtualenv.py"
            ),
            "--no-download",
            str(work_dir / "env"),
        ]
    )

    for name in command_site_names:
        print(f'Checking compatibility of "{name}" site')
        command_requirements = _requirement_definition_to_pip_format(
            name, cache, name == "build"
        )
        with open(work_dir / "requirements.txt", "w") as requirements_txt:
            requirements_txt.write(mach_requirements)
            requirements_txt.write("\n")
            requirements_txt.write(command_requirements)

        # Attempt to install combined set of dependencies (global Mach + current
        # command)
        subprocess.check_call(
            [
                str(work_dir / "env" / "bin" / "pip"),
                "install",
                "-r",
                str(work_dir / "requirements.txt"),
            ],
            cwd=topsrcdir,
        )


if __name__ == "__main__":
    mozunit.main()
