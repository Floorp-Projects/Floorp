# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

here = os.path.realpath(__file__)
__TESTS_DIR = os.path.join(os.path.dirname(os.path.dirname(here)), "tests")


def remove_develop_files(starting_dir=__TESTS_DIR):
    for file_name in os.listdir(starting_dir):
        file_path = os.path.join(starting_dir, file_name)

        if file_name.endswith(".develop") and os.path.isfile(file_path):
            os.remove(file_path)
        elif os.path.isdir(file_path):
            remove_develop_files(file_path)


def patched_build_manifest(config, manifestName):
    # for unit testing, we aren't downloading the tp5 pageset; therefore
    # manifest file doesn't exist; create a dummy one so we can continue
    if not os.path.exists(manifestName):
        try:
            if not os.path.exists(os.path.dirname(manifestName)):
                os.makedirs(os.path.dirname(manifestName))
            f = open(manifestName, "w")
            f.close()
        except Exception as e:
            raise (e)

    # read manifest lines
    with open(manifestName, "r") as fHandle:
        manifestLines = fHandle.readlines()

    # write modified manifest lines
    with open(manifestName + ".develop", "w") as newHandle:
        for line in manifestLines:
            newline = line.replace("localhost", config["webserver"])
            newline = newline.replace("page_load_test", "tests")
            newHandle.write(newline)

    newManifestName = manifestName + ".develop"

    # return new manifest
    return newManifestName
