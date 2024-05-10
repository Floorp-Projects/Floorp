# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib
import re
import subprocess
from datetime import datetime, timedelta

import requests
from mozperftest.system.android_startup import (
    BASE_URL_DICT,
    DATETIME_FORMAT,
    KEY_ARCHITECTURE,
    KEY_COMMIT,
    KEY_DATETIME,
    KEY_NAME,
    KEY_PRODUCT,
    MOZILLA_PRODUCTS,
)
from mozversioncontrol import get_repository_object

HTTP_200_OKAY = 200
MONO_REPO_MIGRATION_DAY = datetime(2024, 3, 18)


def before_iterations(kw):
    product = kw["AndroidStartUp_product"]
    architecture = "arm64-v8a"
    if product == "geckoview_example":
        architecture = "aarch64"
    if get_repository_object("").name == "git":
        commit_info = subprocess.getoutput("git log --max-count 1")
    else:
        commit_info = subprocess.getoutput("hg log -l 1")
    commit_date = re.search(r"[Dd]ate:\s+([:\s\w]+)\s+", str(commit_info)).group(1)
    download_date = datetime.strptime(commit_date, "%a %b %d %H:%M:%S %Y") - timedelta(
        days=1
    )
    pre_mono_repo = ""
    if download_date < MONO_REPO_MIGRATION_DAY and product != "geckoview_example":
        pre_mono_repo = "-pre-mono-repo"
    download_date = download_date.strftime(DATETIME_FORMAT)
    if product in MOZILLA_PRODUCTS:
        nightly_url = BASE_URL_DICT[product + pre_mono_repo].format(
            date=download_date, architecture=architecture
        )
        filename = f"{product}_nightly_{architecture}.apk"
        print("Fetching {}...".format(filename), end="", flush=True)
        download_apk_as_date(nightly_url, download_date, filename)
        print(f"Downloaded {product} for date: {download_date}")
    else:
        filename = product + ".apk"
    kw["apk_metadata"] = {
        KEY_NAME: filename,
        KEY_DATETIME: download_date,
        KEY_COMMIT: "",
        KEY_ARCHITECTURE: architecture,
        KEY_PRODUCT: product,
    }


def download_apk_as_date(nightly_url, download_date_string, filename):
    apk = requests.get(nightly_url)
    if apk.status_code != HTTP_200_OKAY:
        raise Exception(
            f"Something went wrong downloading the apk check to make sure you have entered"
            f" a date that is valid and that the apk for the date you have requested("
            f"{download_date_string}) is available and that the URL({nightly_url}) is also "
            f"valid"
        )
    pathlib.Path(filename).write_bytes(apk.content)
