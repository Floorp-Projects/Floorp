# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import subprocess


def from_gradle():
    process = subprocess.Popen(["./gradlew", "--no-daemon", "printModules"], stdout=subprocess.PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()

    if exit_code is not 0:
        print "Gradle command returned error:", exit_code

    tuples = re.findall('module: name=(.*) buildPath=(.*)', output, re.M)
    return map(lambda (name, build_path): {
        'name': name[1:],
        'artifact': 'public/build/' + name[1:].replace('-', '.', 1) + '.maven.zip',
        'path': build_path + '/target.maven.zip'
    }, tuples)