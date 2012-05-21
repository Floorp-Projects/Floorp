# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Adds peptest's dependencies to sys.path then runs the tests
"""
import os
import sys

deps = ['manifestdestiny',
        'mozinfo',
        'mozhttpd',
        'mozlog',
        'mozprofile',
        'mozprocess',
        'mozrunner',
       ]

here = os.path.dirname(__file__)
mozbase = os.path.realpath(os.path.join(here, '..', 'mozbase'))

for dep in deps:
    module = os.path.join(mozbase, dep)
    if module not in sys.path:
        sys.path.insert(0, module)

from peptest import runpeptests
runpeptests.main(sys.argv[1:])
