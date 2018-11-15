# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import json
import subprocess


def from_gradle():
    process = subprocess.Popen(["./gradlew", "--no-daemon", "--quiet", "printModules"], stdout=subprocess.PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()

    if exit_code is not 0:
        print("Gradle command returned error: {}".format(exit_code))

    modules_line = [line for line in output.splitlines() if line.startswith('modules: ')][0]
    modules_json = modules_line.split(' ', 1)[1]
    gradle_modules = json.loads(modules_json)
    return [{
        'name': module['name'][1:],  # Gradle prefixes all module names with ":", e.g.: ":browser-awesomebar"
        'artifact': "public/build/{}.maven.zip".format(module['name'][1:]),
        'path': "{}/target.maven.zip".format(module['buildPath']),
        'shouldPublish': module['shouldPublish']
    } for module in gradle_modules]


def get_version_from_gradle():
    process = subprocess.Popen(["./gradlew", "--no-daemon", "--quiet", "printVersion"], stdout=subprocess.PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()

    if exit_code is not 0:
        print("Gradle command returned error: {}".format(exit_code))

    return output.split(' ', 1)[1]
