# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

config = {
    "log_name": "raptor",
    "title": os.uname()[1].lower().split(".")[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install-chrome-android",
        "install-chromium-android",
        "install-chromium-distribution",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/tooltool_cache",
    "download_tooltool": True,
    "hostutils_manifest_path": "testing/config/tooltool-manifests/linux64/hostutils.manifest",
}

# raptor will pick these up in mitmproxy.py, doesn't use the mozharness config
os.environ["TOOLTOOLCACHE"] = config["tooltool_cache"]
os.environ["HOSTUTILS_MANIFEST_PATH"] = config["hostutils_manifest_path"]
