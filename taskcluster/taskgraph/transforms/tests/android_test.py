# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transforms are specific to the android-test kind, and apply defaults to
the test descriptions appropriate to that kind.

Both the input to and output from these transforms must conform to
`taskgraph.transforms.tests.test:test_schema`.
"""

from __future__ import absolute_import, print_function, unicode_literals
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_defaults(config, tests):
    for test in tests:
        # all Android test tasks download internal objects from tooltool
        test['mozharness']['tooltool-downloads'] = True
        test['mozharness']['build-artifact-name'] = 'public/build/target.apk'
        test['mozharness']['actions'] = ['get-secrets']
        yield test


@transforms.add
def set_treeherder_machine_platform(config, tests):
    """Set the appropriate task.extra.treeherder.machine.platform"""
    # The build names for these build platforms have partially evolved over the
    # years..  This is temporary until we can clean up the handling of
    # platforms
    translation = {
        'android-api-15/debug': 'android-4-3-armv7-api15/debug',
        'android-api-15/opt': 'android-4-3-armv7-api15/opt',
        'android-x86/opt': 'android-4-2-x86/opt',
    }
    for test in tests:
        build_platform = test['build-platform']
        test['treeherder-machine-platform'] = translation.get(build_platform, build_platform)
        yield test


@transforms.add
def set_e10s_attr(config, tests):
    """Set the e10s attribute to false"""
    for test in tests:
        test.setdefault('attributes', {})['e10s'] = False
        yield test
