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
def set_tier(config, tests):
    for test in tests:
        if 'tier' not in test:
            # Bug 1282850: android debug is tier 1, but opt is tier 2
            if test['test-platform'] == 'android-4.3-arm7-api-15/debug':
                test['tier'] = 1
            else:
                test['tier'] = 2
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
    }
    for test in tests:
        build_platform = test['build-platform']
        test['treeherder-machine-platform'] = translation.get(build_platform, build_platform)
        yield test


@transforms.add
def set_chunk_args(config, tests):
    # Android tests do not take the --this-chunk/--total-chunk args like linux
    # tests, preferring to define a --test-suite argument for each chunk.
    # Where debug and opt have different chunk counts, there are *different*
    # test-suite definitions for the debug and opt runs.
    #
    # Within the mozharness scripts, there is a translation *back* to
    # --this-chunk/--total-chunk.
    #
    # TODO: remove the need for this with some changes to the mozharness script
    # to take --total-chunk/this-chunk

    for test in tests:
        test['mozharness']['chunking-args'] = 'test-suite-suffix'

        # if the chunks are an integer, then they do not differ between
        # platforms, so the suffix is always "-<CHUNK>"
        if isinstance(test['chunks'], int):
            test['mozharness']['chunk-suffix'] = "-<CHUNK>"
        else:
            # otherwise, by convention, the debug version has "-debug" in the
            # suite name and the opt version does not
            if test['test-platform'].endswith('debug'):
                test['mozharness']['chunk-suffix'] = '-debug-<CHUNK>'
            else:
                test['mozharness']['chunk-suffix'] = '-<CHUNK>'
        yield test
