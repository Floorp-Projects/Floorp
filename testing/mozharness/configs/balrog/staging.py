# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "balrog_servers": [
        {
            "balrog_api_root": "https://balrog-admin.stage.mozaws.net/api",
            "ignore_failures": False,
            "balrog_usernames": {
                "firefox": "balrog-stage-ffxbld",
                "thunderbird": "balrog-stage-tbirdbld",
                "mobile": "balrog-stage-ffxbld",
                "Fennec": "balrog-stage-ffxbld",
            },
        }
    ]
}
