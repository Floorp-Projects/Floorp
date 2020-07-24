# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from pathlib import Path


def main(output, *filenames):
    # Unlike most vendored packages, we don't want "glean_parser" in
    # "build/virtualenv_packages.txt" because it interferes with mach's usage of Glean.
    # This is because Glean ("glean_sdk"):
    # * Has native code (so, can't be vendored)
    # * Also depends on "glean_parser"
    # Since this file has different version constraints than mach on "glean_parser",
    # we want the two different consumers to own their own separate "glean_parser"
    # packages.
    #
    # This is solved by:
    # * Having mach import Glean (and transitively "glean_parser") via the "--user"
    #   package environment. To accomplish this, the vendored "glean_parser" is removed
    #   from "virtualenv_packages.txt".
    # * Having this script import "glean_parser" from the vendored location. This is
    #   done by manually adding it to the pythonpath.

    srcdir = Path(__file__).joinpath('../../../../../')
    glean_parser_path = srcdir.joinpath('third_party/python/glean_parser')
    sys.path.insert(0, str(glean_parser_path.resolve()))
    from glean_parser import lint
    if lint.glinter([Path(x) for x in filenames], {"allow_reserved": False}):
        sys.exit(1)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
