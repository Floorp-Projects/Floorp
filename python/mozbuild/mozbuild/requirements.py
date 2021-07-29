# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os


class PthSpecifier:
    def __init__(self, path):
        self.path = path


class PypiSpecifier:
    def __init__(self, package_name, version, full_specifier):
        self.package_name = package_name
        self.version = version
        self.full_specifier = full_specifier


class PypiOptionalSpecifier:
    def __init__(self, repercussion, package_name, version, full_specifier):
        self.repercussion = repercussion
        self.package_name = package_name
        self.version = version
        self.full_specifier = full_specifier


class MachEnvRequirements:
    """Requirements associated with a "virtualenv_packages.txt" definition

    Represents the dependencies of a virtualenv. The source files consist
    of colon-delimited fields. The first field
    specifies the action. The remaining fields are arguments to that
    action. The following actions are supported:

    pth -- Adds the path given as argument to "mach.pth" under
        the virtualenv site packages directory.

    pypi -- Fetch the package, plus dependencies, from PyPI.

    packages.txt -- Denotes that the specified path is a child manifest. It
        will be read and processed as if its contents were concatenated
        into the manifest being read.

    thunderbird -- This denotes the action as to only occur for Thunderbird
        checkouts. The initial "thunderbird" field is stripped, then the
        remaining line is processed like normal. e.g.
        "thunderbird:pth:python/foo"
    """

    def __init__(self):
        self.requirements_paths = []
        self.pth_requirements = []
        self.pypi_requirements = []
        self.pypi_optional_requirements = []

    @classmethod
    def from_requirements_definition(
        cls, topsrcdir, is_thunderbird, requirements_definition
    ):
        requirements = cls()
        _parse_mach_env_requirements(
            requirements, requirements_definition, topsrcdir, is_thunderbird
        )
        return requirements


def _parse_mach_env_requirements(
    requirements_output, root_requirements_path, topsrcdir, is_thunderbird
):
    def _parse_requirements_line(line):
        line = line.strip()
        if line.startswith("#"):
            return

        action, params = line.rstrip().split(":", maxsplit=1)
        if action == "pth":
            requirements_output.pth_requirements.append(PthSpecifier(params))
        elif action == "pypi":
            package_name, version = _parse_package_specifier(params)
            requirements_output.pypi_requirements.append(
                PypiSpecifier(package_name, version, params)
            )
        elif action == "pypi-optional":
            if len(params.split(":", maxsplit=1)) != 2:
                raise Exception(
                    "Expected pypi-optional package to have a repercussion"
                    'description in the format "package:fallback explanation", '
                    'found "{}"'.format(params)
                )
            package, repercussion = params.split(":")
            package_name, version = _parse_package_specifier(package)
            requirements_output.pypi_optional_requirements.append(
                PypiOptionalSpecifier(repercussion, package_name, version, package)
            )
        elif action == "packages.txt":
            nested_definition_path = os.path.join(topsrcdir, params)
            assert os.path.isfile(nested_definition_path)
            _parse_requirements_definition_file(nested_definition_path)
        elif action == "thunderbird":
            if is_thunderbird:
                _parse_requirements_line(params)
        else:
            raise Exception("Unknown requirements definition action: %s" % action)

    def _parse_requirements_definition_file(requirements_path):
        """Parse requirements file into list of requirements"""
        requirements_output.requirements_paths.append(requirements_path)

        with open(requirements_path, "r") as requirements_file:
            lines = [line for line in requirements_file]

        for line in lines:
            _parse_requirements_line(line)

    _parse_requirements_definition_file(root_requirements_path)


def _parse_package_specifier(specifier):
    if len(specifier.split("==")) != 2:
        raise Exception(
            "Expected pypi package version to be pinned in the "
            'format "package==version", found "{}"'.format(specifier)
        )
    return specifier.split("==")
