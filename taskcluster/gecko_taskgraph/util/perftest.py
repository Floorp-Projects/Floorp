# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def is_external_browser(label):
    if any(
        external_browser in label
        for external_browser in (
            "safari",
            "safari-tp",
            "chrome",
            "custom-car",
            "chrome-m",
            "cstm-car-m",
        )
    ):
        return True
    return False
