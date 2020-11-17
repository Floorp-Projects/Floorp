# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

platform = "linux32"

config = {
    "locale": os.environ.get("LOCALE"),
    # ToolTool
    "tooltool_cache": os.environ.get("TOOLTOOL_CACHE"),
    "run_configure": False,
}
