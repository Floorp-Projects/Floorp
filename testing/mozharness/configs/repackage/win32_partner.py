# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

platform = "win32"

config = {
    "repack_id": os.environ.get("REPACK_ID"),
    "run_configure": False,
    "env": {
        "PATH": "%(abs_input_dir)s/upx/bin:%(PATH)s",
    },
}
