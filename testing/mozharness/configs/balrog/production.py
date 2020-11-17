# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "balrog_servers": [
        {
            "balrog_api_root": "https://aus4-admin.mozilla.org/api",
            "ignore_failures": False,
            "url_replacements": [
                (
                    "http://archive.mozilla.org/pub",
                    "http://download.cdn.mozilla.net/pub",
                ),
            ],
            "balrog_usernames": {
                "firefox": "balrog-ffxbld",
                "thunderbird": "balrog-tbirdbld",
                "mobile": "balrog-ffxbld",
                "Fennec": "balrog-ffxbld",
            },
        },
        # Bug 1261346 - temporarily disable staging balrog submissions
        # {
        #     'balrog_api_root': 'https://aus4-admin-dev.allizom.org/api',
        #     'ignore_failures': True,
        #     'balrog_usernames': {
        #         'firefox': 'stage-ffxbld',
        #         'thunderbird': 'stage-tbirdbld',
        #         'mobile': 'stage-ffxbld',
        #         'Fennec': 'stage-ffxbld',
        #     }
        # }
    ]
}
