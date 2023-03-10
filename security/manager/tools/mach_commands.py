# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mach.decorators import Command, CommandArgument
from mach.util import UserError
from mozpack.files import FileFinder
from mozpack.path import basedir


def run_module_main_on(module, input_filename, output_is_binary):
    """Run the given module (pycert or pykey) on the given
    file."""
    # By convention, the specification files have names of the form
    # "name.ext.*spec", where "ext" is some extension, and the "*" in
    # "*spec" identifies what kind of specification it represents
    # (certspec or keyspec). Taking off the ".*spec" part results in the
    # desired filename for this file.
    output_filename = os.path.splitext(input_filename)[0]
    mode = "w"
    encoding = "utf-8"
    newline = "\n"
    if output_is_binary:
        mode = "wb"
        encoding = None
        newline = None
    with open(output_filename, mode=mode, encoding=encoding, newline=newline) as output:
        module.main(output, input_filename)


def is_certspec_file(filename):
    """Returns True if the given filename is a certificate
    specification file (.certspec) and False otherwise."""
    return filename.endswith(".certspec")


def is_keyspec_file(filename):
    """Returns True if the given filename is a key specification
    file (.keyspec) and False otherwise."""
    return filename.endswith(".keyspec")


def is_pkcs12spec_file(filename):
    """Returns True if the given filename is a pkcs12
    specification file (.pkcs12spec) and False otherwise."""
    return filename.endswith(".pkcs12spec")


def is_specification_file(filename):
    """Returns True if the given filename is a specification
    file supported by this script, and False otherewise."""
    return (
        is_certspec_file(filename)
        or is_keyspec_file(filename)
        or is_pkcs12spec_file(filename)
    )


def is_excluded_directory(directory, exclusions):
    """Returns True if the given directory is in or is a
    subdirectory of a directory in the list of exclusions and
    False otherwise."""

    for exclusion in exclusions:
        if directory.startswith(exclusion):
            return True
    return False


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
def generate_test_certs(command_context, specifications):
    """Generate test certificates and keys from specifications."""
    import pycert
    import pykey
    import pypkcs12

    if not specifications:
        specifications = find_all_specifications(command_context)

    for specification in specifications:
        output_is_binary = False
        if is_certspec_file(specification):
            module = pycert
        elif is_keyspec_file(specification):
            module = pykey
        elif is_pkcs12spec_file(specification):
            module = pypkcs12
            output_is_binary = True
        else:
            raise UserError(
                "'{}' is not a .certspec, .keyspec, or .pkcs12spec file".format(
                    specification
                )
            )
        run_module_main_on(module, os.path.abspath(specification), output_is_binary)
    return 0


def find_all_specifications(command_context):
    """Searches the source tree for all specification files
    and returns them as a list."""
    specifications = []
    inclusions = [
        "netwerk/test/unit",
        "security/manager/ssl/tests",
        "services/settings/test/unit/test_remote_settings_signatures",
        "testing/xpcshell/moz-http2",
        "toolkit/mozapps/extensions/test/xpcshell/data/productaddons",
    ]
    exclusions = ["security/manager/ssl/tests/unit/test_signed_apps"]
    finder = FileFinder(command_context.topsrcdir)
    for inclusion_path in inclusions:
        for f, _ in finder.find(inclusion_path):
            if basedir(f, exclusions):
                continue
            if is_specification_file(f):
                specifications.append(os.path.join(command_context.topsrcdir, f))
    return specifications
