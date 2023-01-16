# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib
from datetime import datetime

import requests
from mozperftest.system.android_startup import (
    BASE_URL_DICT,
    DATETIME_FORMAT,
    KEY_ARCHITECTURE,
    KEY_COMMIT,
    KEY_DATETIME,
    KEY_NAME,
    KEY_PRODUCT,
    PROD_FOCUS,
)


def before_iterations(kw):
    product = kw["AndroidStartUp_product"]
    download_date = datetime.today()
    architecture = "arm64-v8a"

    if product == PROD_FOCUS:
        if download_date >= datetime(2022, 12, 15):
            product += "-v3"
        elif download_date >= datetime(2021, 11, 5):
            product += "-v2"
    # The above sections with v2, v3 are occurring because of change in task cluster indicies
    # This is not expected to occur regularly, the new indicies contain both focus and fenix
    # android components

    nightly_url = BASE_URL_DICT[product + "-latest"].format(
        date=download_date.strftime(DATETIME_FORMAT), architecture=architecture
    )
    filename = f"{product}_nightly_{architecture}.apk"
    print("Fetching {}...".format(filename), end="", flush=True)
    download_apk_as_date(nightly_url, download_date, filename)

    kw["apk_metadata"] = {
        KEY_NAME: filename,
        KEY_DATETIME: download_date,
        KEY_COMMIT: "",
        KEY_ARCHITECTURE: architecture,
        KEY_PRODUCT: product,
    }


def download_apk_as_date(nightly_url, download_date_string, filename):
    apk = requests.get(nightly_url)
    if apk.status_code != 200:
        raise Exception(
            f"Something went wrong downloading the apk check to make sure you have entered"
            f" a date that is valid and that the apk for the date you have requested("
            f"{download_date_string}) is available and that the URL({nightly_url}) is also "
            f"valid"
        )
    pathlib.Path(filename).write_bytes(apk.content)
