# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import subprocess


def from_gradle():
    process = subprocess.Popen(["./gradlew", "--no-daemon", "--quiet", "printModules"], stdout=subprocess.PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()

    if exit_code is not 0:
        print "Gradle command returned error:", exit_code

    gradle_modules = json.loads(output)
    return [{
        'name': module['name'][1:],  # Gradle prefixes all module names with ":", e.g.: ":browser-awesomebar"
        'artifact': "public/build/{}.maven.zip".format(module['name'][1:]),
        'path': "{}/target.maven.zip".format(module['buildPath'])
    } for module in gradle_modules]
