# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os

config = {
    "stage_platform": "macosx64-fuzzing-asan",
    "publish_nightly_en_US_routes": False,
    "platform_supports_post_upload_to_latest": False,
    #### 64 bit build specific #####
    "env": {
        "MOZBUILD_STATE_PATH": os.path.join(os.getcwd(), ".mozbuild"),
        "DISPLAY": ":2",
        "HG_SHARE_BASE_DIR": "/builds/hg-shared",
        "MOZ_OBJDIR": "%(abs_obj_dir)s",
        "TINDERBOX_OUTPUT": "1",
        "TOOLTOOL_CACHE": "/builds/tooltool_cache",
        "TOOLTOOL_HOME": "/builds",
        "MOZ_CRASHREPORTER_NO_REPORT": "1",
        "LC_ALL": "C",
        "ASAN_OPTIONS": "detect_leaks=0",
        ## 64 bit specific
        "PATH": "/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin",
    },
}
