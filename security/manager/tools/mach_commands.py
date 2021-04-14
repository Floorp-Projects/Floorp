# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from mach.util import UserError
from mozbuild.base import MachCommandBase
from mozpack.files import FileFinder
from mozpack.path import basedir


from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


here = os.path.abspath(os.path.dirname(__file__))
sys.path.append(here)


def run_module_main_on(module, input_filename):
    """Run the given module (pycert or pykey) on the given
    file."""
    # By convention, the specification files have names of the form
    # "name.ext.*spec", where "ext" is some extension, and the "*" in
    # "*spec" identifies what kind of specification it represents
    # (certspec or keyspec). Taking off the ".*spec" part results in the
    # desired filename for this file.
    output_filename = os.path.splitext(input_filename)[0]
    with open(output_filename, mode="w", encoding="utf-8", newline="\n") as output:
        module.main(output, input_filename)


def is_certspec_file(filename):
    """Returns True if the given filename is a certificate
    specification file (.certspec) and False otherwise."""
    return filename.endswith(".certspec")


def is_keyspec_file(filename):
    """Returns True if the given filename is a key specification
    file (.keyspec) and False otherwise."""
    return filename.endswith(".keyspec")


def is_specification_file(filename):
    """Returns True if the given filename is a specification
    file supported by this script, and False otherewise."""
    return is_certspec_file(filename) or is_keyspec_file(filename)


def is_excluded_directory(directory, exclusions):
    """Returns True if the given directory is in or is a
    subdirectory of a directory in the list of exclusions and
    False otherwise."""

    for exclusion in exclusions:
        if directory.startswith(exclusion):
            return True
    return False


@CommandProvider
class MachCommands(MachCommandBase):
    @Command(
        "generate-test-certs",
        category="devenv",
        description="Generate test certificates and keys from specifications.",
    )
    @CommandArgument(
        "specifications",
        nargs="*",
        help="Specification files for test certs. If omitted, all certs are regenerated.",
    )
    def generate_test_certs(self, specifications):
        """Generate test certificates and keys from specifications."""

        self.activate_virtualenv()
        import pycert
        import pykey

        if not specifications:
            specifications = self.find_all_specifications()

        for specification in specifications:
            if is_certspec_file(specification):
                module = pycert
            elif is_keyspec_file(specification):
                module = pykey
            else:
                raise UserError(
                    "'{}' is not a .certspec or .keyspec file".format(specification)
                )
            run_module_main_on(module, os.path.abspath(specification))
        return 0

    def find_all_specifications(self):
        """Searches the source tree for all specification files
        and returns them as a list."""
        specifications = []
        inclusions = [
            "netwerk/test/unit",
            "security/manager/ssl/tests",
            "services/settings/test/unit/test_remote_settings_signatures",
            "testing/xpcshell/moz-http2",
        ]
        exclusions = ["security/manager/ssl/tests/unit/test_signed_apps"]
        finder = FileFinder(self.topsrcdir)
        for inclusion_path in inclusions:
            for f, _ in finder.find(inclusion_path):
                if basedir(f, exclusions):
                    continue
                if is_specification_file(f):
                    specifications.append(os.path.join(self.topsrcdir, f))
        return specifications
