# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

config = {
    "default_actions": [
        "clobber",
        "build",
    ],
    "stage_platform": "macosx64-searchfox-debug",
    "debug_build": True,
    #### 64 bit build specific #####
    "env": {
        "MOZBUILD_STATE_PATH": os.path.join(os.getcwd(), ".mozbuild"),
        "HG_SHARE_BASE_DIR": "/builds/hg-shared",
        "MOZ_OBJDIR": "%(abs_obj_dir)s",
        "TINDERBOX_OUTPUT": "1",
        "TOOLTOOL_CACHE": "/builds/tooltool_cache",
        "TOOLTOOL_HOME": "/builds",
        "MOZ_CRASHREPORTER_NO_REPORT": "1",
        "LC_ALL": "C",
        "XPCOM_DEBUG_BREAK": "stack-and-abort",
        # 64 bit specific
        "PATH": "/tools/python/bin:/opt/local/bin:/usr/bin:"
        "/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin",
    },
    "mozconfig_variant": "debug-searchfox",
}
