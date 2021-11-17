# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
from pathlib import Path
import subprocess

from packaging.requirements import Requirement


THUNDERBIRD_PYPI_ERROR = """
Thunderbird requirements definitions cannot include PyPI packages.
""".strip()


class EnvironmentPackageValidationResult:
    def __init__(self):
        self._package_discrepancies = []
        self.has_all_packages = True

    def add_discrepancy(self, requirement, found):
        self._package_discrepancies.append((requirement, found))
        self.has_all_packages = False

    def report(self):
        lines = []
        for requirement, found in self._package_discrepancies:
            if found:
                error = f'Installed with unexpected version "{found}"'
            else:
                error = "Not installed"
            lines.append(f"{requirement}: {error}")
        return "\n".join(lines)


class PthSpecifier:
    def __init__(self, path):
        self.path = path


class PypiSpecifier:
    def __init__(self, requirement):
        self.requirement = requirement


class PypiOptionalSpecifier(PypiSpecifier):
    def __init__(self, repercussion, requirement):
        super().__init__(requirement)
        self.repercussion = repercussion


class MachEnvRequirements:
    """Requirements associated with a "virtualenv_packages.txt" definition

    Represents the dependencies of a site. The source files consist
    of colon-delimited fields. The first field
    specifies the action. The remaining fields are arguments to that
    action. The following actions are supported:

    pth -- Adds the path given as argument to "mach.pth" under
        the virtualenv's site packages directory.

    pypi -- Fetch the package, plus dependencies, from PyPI.

    pypi-optional -- Attempt to install the package and dependencies from PyPI.
        Continue using the site, even if the package could not be installed.

    packages.txt -- Denotes that the specified path is a child manifest. It
        will be read and processed as if its contents were concatenated
        into the manifest being read.

    thunderbird-packages.txt -- Denotes a Thunderbird child manifest.
        Thunderbird child manifests are only activated when working on Thunderbird,
        and they can cannot have "pypi" or "pypi-optional" entries.
    """

    def __init__(self):
        self.requirements_paths = []
        self.pth_requirements = []
        self.pypi_requirements = []
        self.pypi_optional_requirements = []
        self.vendored_requirements = []

    def pths_as_absolute(self, topsrcdir):
        return sorted(
            [
                os.path.normcase(os.path.join(topsrcdir, pth.path))
                for pth in (self.pth_requirements + self.vendored_requirements)
            ]
        )

    def validate_environment_packages(self, pip_command):
        result = EnvironmentPackageValidationResult()
        if not self.pypi_requirements and not self.pypi_optional_requirements:
            return result

        pip_json = subprocess.check_output(
            pip_command + ["list", "--format", "json"], universal_newlines=True
        )

        installed_packages = json.loads(pip_json)
        installed_packages = {
            package["name"]: package["version"] for package in installed_packages
        }
        for pkg in self.pypi_requirements:
            installed_version = installed_packages.get(pkg.requirement.name)
            if not installed_version or not pkg.requirement.specifier.contains(
                installed_version
            ):
                result.add_discrepancy(pkg.requirement, installed_version)

        for pkg in self.pypi_optional_requirements:
            installed_version = installed_packages.get(pkg.requirement.name)
            if installed_version and not pkg.requirement.specifier.contains(
                installed_version
            ):
                result.add_discrepancy(pkg.requirement, installed_version)

        return result

    @classmethod
    def from_requirements_definition(
        cls,
        topsrcdir,
        is_thunderbird,
        is_mach_or_build_site,
        requirements_definition,
    ):
        requirements = cls()
        _parse_mach_env_requirements(
            requirements,
            requirements_definition,
            topsrcdir,
            is_thunderbird,
            is_mach_or_build_site,
        )
        return requirements


def _parse_mach_env_requirements(
    requirements_output,
    root_requirements_path,
    topsrcdir,
    is_thunderbird,
    is_mach_or_build_site,
):
    topsrcdir = Path(topsrcdir)

    def _parse_requirements_line(
        current_requirements_path, line, line_number, is_thunderbird_packages_txt
    ):
        line = line.strip()
        if not line or line.startswith("#"):
            return

        action, params = line.rstrip().split(":", maxsplit=1)
        if action == "pth":
            path = topsrcdir / params
            if not path.exists():
                # In sparse checkouts, not all paths will be populated.
                return

            for child in path.iterdir():
                if child.name.endswith(".dist-info"):
                    raise Exception(
                        f'The "pth:" pointing to "{path}" has a ".dist-info" file.\n'
                        f'Perhaps "{current_requirements_path}:{line_number}" '
                        'should change to start with "vendored:" instead of "pth:".'
                    )
                if child.name == "PKG-INFO":
                    raise Exception(
                        f'The "pth:" pointing to "{path}" has a "PKG-INFO" file.\n'
                        f'Perhaps "{current_requirements_path}:{line_number}" '
                        'should change to start with "vendored:" instead of "pth:".'
                    )

            requirements_output.pth_requirements.append(PthSpecifier(params))
        elif action == "vendored":
            requirements_output.vendored_requirements.append(PthSpecifier(params))
        elif action == "packages.txt":
            _parse_requirements_definition_file(
                os.path.join(topsrcdir, params),
                is_thunderbird_packages_txt,
            )
        elif action == "pypi":
            if is_thunderbird_packages_txt:
                raise Exception(THUNDERBIRD_PYPI_ERROR)

            requirements_output.pypi_requirements.append(
                PypiSpecifier(_parse_package_specifier(params, is_mach_or_build_site))
            )
        elif action == "pypi-optional":
            if is_thunderbird_packages_txt:
                raise Exception(THUNDERBIRD_PYPI_ERROR)

            if len(params.split(":", maxsplit=1)) != 2:
                raise Exception(
                    "Expected pypi-optional package to have a repercussion "
                    'description in the format "package:fallback explanation", '
                    'found "{}"'.format(params)
                )
            raw_requirement, repercussion = params.split(":")
            requirements_output.pypi_optional_requirements.append(
                PypiOptionalSpecifier(
                    repercussion,
                    _parse_package_specifier(raw_requirement, is_mach_or_build_site),
                )
            )
        elif action == "thunderbird-packages.txt":
            if is_thunderbird:
                _parse_requirements_definition_file(
                    os.path.join(topsrcdir, params), is_thunderbird_packages_txt=True
                )
        else:
            raise Exception("Unknown requirements definition action: %s" % action)

    def _parse_requirements_definition_file(
        requirements_path, is_thunderbird_packages_txt
    ):
        """Parse requirements file into list of requirements"""
        assert os.path.isfile(requirements_path)
        requirements_output.requirements_paths.append(requirements_path)

        with open(requirements_path, "r") as requirements_file:
            lines = [line for line in requirements_file]

        for number, line in enumerate(lines, start=1):
            _parse_requirements_line(
                requirements_path, line, number, is_thunderbird_packages_txt
            )

    _parse_requirements_definition_file(root_requirements_path, False)


def _parse_package_specifier(raw_requirement, is_mach_or_build_site):
    requirement = Requirement(raw_requirement)

    if not is_mach_or_build_site and [
        s for s in requirement.specifier if s.operator != "=="
    ]:
        raise Exception(
            'All sites except for "mach" and "build" must pin pypi package '
            f'versions in the format "package==version", found "{raw_requirement}"'
        )
    return requirement
