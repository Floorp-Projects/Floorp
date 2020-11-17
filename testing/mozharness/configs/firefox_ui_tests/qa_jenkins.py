# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Default configuration as used by Mozmill CI (Jenkins)


config = {
    # Tests run in mozmill-ci do not use RelEng infra
    "developer_mode": True,
    # mozcrash support
    "download_symbols": "ondemand",
}
