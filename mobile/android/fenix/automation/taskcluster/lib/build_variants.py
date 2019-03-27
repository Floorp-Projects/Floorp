# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import json
import subprocess


def from_gradle():
    process = subprocess.Popen([
        "./gradlew", "--no-daemon", "--quiet", "printBuildVariants"
    ], stdout=subprocess.PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()

    if exit_code != 0:
        print("Gradle command returned error: {}".format(exit_code))

    variants_line = [line for line in output.split('\n') if line.startswith('variants: ')][0]
    variants_json = variants_line.split(' ', 1)[1]
    variants = json.loads(variants_json)

    return variants
