# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

config = {
    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    "default_actions": [
        "clobber",
        "build",
    ],
    "vcs_share_base": "/builds/hg-shared",
    #########################################################################
    #########################################################################
    ###### 64 bit specific ######
    "platform": "linux64",
    "stage_platform": "linux64-searchfox-opt",
    "env": {
        "MOZBUILD_STATE_PATH": os.path.join(os.getcwd(), ".mozbuild"),
        "DISPLAY": ":2",
        "HG_SHARE_BASE_DIR": "/builds/hg-shared",
        "MOZ_OBJDIR": "%(abs_obj_dir)s",
        "TINDERBOX_OUTPUT": "1",
        "TOOLTOOL_CACHE": "/builds/worker/tooltool-cache",
        "TOOLTOOL_HOME": "/builds",
        "MOZ_CRASHREPORTER_NO_REPORT": "1",
        "LC_ALL": "C",
        ## 64 bit specific
        "PATH": "/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin",
        ##
    },
    # This doesn't actually inherit from anything.
    "mozconfig_platform": "linux64",
    "mozconfig_variant": "debug-searchfox-clang",
}
