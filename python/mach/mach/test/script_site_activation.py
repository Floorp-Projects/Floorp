# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used by "test_site_activation.py" to verify how site activations
# affect the sys.path.
# The sys.path is printed in three stages:
# 1. Once at the beginning
# 2. Once after Mach site activation
# 3. Once after the command site activation
# The output of this script should be an ast-parsable list with three nested lists: one
# for each sys.path state.
# Note that virtualenv-creation output may need to be filtered out - it can be done by
# only ast-parsing the last line of text outputted by this script.

import os
import sys
from unittest.mock import patch

from mach.requirements import MachEnvRequirements, PthSpecifier
from mach.site import CommandSiteManager, MachSiteManager


def main():
    # Should be set by calling test
    topsrcdir = os.environ["TOPSRCDIR"]
    command_site = os.environ["COMMAND_SITE"]
    mach_site_requirements = os.environ["MACH_SITE_PTH_REQUIREMENTS"]
    command_site_requirements = os.environ["COMMAND_SITE_PTH_REQUIREMENTS"]
    work_dir = os.environ["WORK_DIR"]

    def resolve_requirements(topsrcdir, site_name):
        req = MachEnvRequirements()
        if site_name == "mach":
            req.pth_requirements = [
                PthSpecifier(path) for path in mach_site_requirements.split(os.pathsep)
            ]
        else:
            req.pth_requirements = [PthSpecifier(command_site_requirements)]
        return req

    with patch("mach.site.resolve_requirements", resolve_requirements):
        initial_sys_path = sys.path.copy()

        mach_site = MachSiteManager.from_environment(
            topsrcdir,
            lambda: work_dir,
        )
        mach_site.activate()
        mach_sys_path = sys.path.copy()

        command_site = CommandSiteManager.from_environment(
            topsrcdir, lambda: work_dir, command_site, work_dir
        )
        command_site.activate()
        command_sys_path = sys.path.copy()
    print(
        [
            initial_sys_path,
            mach_sys_path,
            command_sys_path,
        ]
    )


if __name__ == "__main__":
    main()
