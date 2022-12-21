# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from pathlib import Path

from packaging.requirements import Requirement

THUNDERBIRD_PYPI_ERROR = """
Thunderbird requirements definitions cannot include PyPI packages.
""".strip()


class PthSpecifier:
    def __init__(self, path: str):
        self.path = path


class PypiSpecifier:
    def __init__(self, requirement):
        self.requirement = requirement


class PypiOptionalSpecifier(PypiSpecifier):
    def __init__(self, repercussion, requirement):
        super().__init__(requirement)
        self.repercussion = repercussion


class MachEnvRequirements:
    """Requirements associated with a "site dependency manifest", as
    defined in "python/sites/".

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

    def pths_as_absolute(self, topsrcdir: str):
        return [
            os.path.normcase(Path(topsrcdir) / pth.path)
            for pth in (self.pth_requirements + self.vendored_requirements)
        ]

    @classmethod
    def from_requirements_definition(
        cls,
        topsrcdir: str,
        is_thunderbird,
        only_strict_requirements,
        requirements_definition,
    ):
        requirements = cls()
        _parse_mach_env_requirements(
            requirements,
            Path(requirements_definition),
            Path(topsrcdir),
            is_thunderbird,
            only_strict_requirements,
        )
        return requirements


def _parse_mach_env_requirements(
    requirements_output,
    root_requirements_path: Path,
    topsrcdir: Path,
    is_thunderbird,
    only_strict_requirements,
):
    def _parse_requirements_line(
        current_requirements_path: Path, line, line_number, is_thunderbird_packages_txt
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

            requirements_output.pth_requirements.append(PthSpecifier(params))
        elif action == "vendored":
            requirements_output.vendored_requirements.append(PthSpecifier(params))
        elif action == "packages.txt":
            _parse_requirements_definition_file(
                topsrcdir / params,
                is_thunderbird_packages_txt,
            )
        elif action == "pypi":
            if is_thunderbird_packages_txt:
                raise Exception(THUNDERBIRD_PYPI_ERROR)

            requirements_output.pypi_requirements.append(
                PypiSpecifier(
                    _parse_package_specifier(params, only_strict_requirements)
                )
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
                    _parse_package_specifier(raw_requirement, only_strict_requirements),
                )
            )
        elif action == "thunderbird-packages.txt":
            if is_thunderbird:
                _parse_requirements_definition_file(
                    topsrcdir / params, is_thunderbird_packages_txt=True
                )
        else:
            raise Exception("Unknown requirements definition action: %s" % action)

    def _parse_requirements_definition_file(
        requirements_path: Path, is_thunderbird_packages_txt
    ):
        """Parse requirements file into list of requirements"""
        if not requirements_path.is_file():
            raise Exception(f'Missing requirements file at "{requirements_path}"')

        requirements_output.requirements_paths.append(str(requirements_path))

        with open(requirements_path, "r") as requirements_file:
            lines = [line for line in requirements_file]

        for number, line in enumerate(lines, start=1):
            _parse_requirements_line(
                requirements_path, line, number, is_thunderbird_packages_txt
            )

    _parse_requirements_definition_file(root_requirements_path, False)


class UnexpectedFlexibleRequirementException(Exception):
    def __init__(self, raw_requirement):
        self.raw_requirement = raw_requirement


def _parse_package_specifier(raw_requirement, only_strict_requirements):
    requirement = Requirement(raw_requirement)

    if only_strict_requirements and [
        s for s in requirement.specifier if s.operator != "=="
    ]:
        raise UnexpectedFlexibleRequirementException(raw_requirement)
    return requirement
