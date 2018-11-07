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
    return map(lambda module: {
        'name': module['name'][1:],
        'artifact': 'public/build/' + module['name'][1:].replace('-', '.', 1) + '.maven.zip',
        'path': module['buildPath'] + '/target.maven.zip'
    }, gradle_modules)