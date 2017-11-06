# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Check APK file size for limit

from os import path, listdir, stat
from sys import exit

SIZE_LIMIT = 5242880
PATH = path.join(path.dirname(path.abspath(__file__)), '../../app/build/outputs/apk/')

files = []
try:
    files = [f for f in listdir(PATH) if path.isfile(path.join(PATH, f)) and f.endswith('.apk') and "release" in f]
except OSError as e:
    if e.errno == 2:
        print("Directory is missing, build apk first!")
        exit(1)
    print("Unknown error: {err}".format(err=str(e)))
    exit(2)

for apk_file in files:
    file_size = stat(path.join(PATH, apk_file)).st_size
    if file_size > SIZE_LIMIT:
        print(" * [TOOBIG] {filename} ({filesize} > {sizelimit})".format(
            filename=apk_file, filesize=file_size, sizelimit=SIZE_LIMIT
        ))
        exit(27)
    print(" * [OKAY] {filename} ({filesize} <= {sizelimit})".format(
            filename=apk_file, filesize=file_size, sizelimit=SIZE_LIMIT
        ))
