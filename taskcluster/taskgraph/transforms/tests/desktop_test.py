# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transforms are specific to the desktop-test kind, and apply defaults to
the test descriptions appropriate to that kind.

Both the input to and output from these transforms must conform to
`taskgraph.transforms.tests.test:test_schema`.
"""

from __future__ import absolute_import, print_function, unicode_literals
from taskgraph.transforms.base import TransformSequence, get_keyed_by
from taskgraph.util.treeherder import split_symbol, join_symbol

import copy

transforms = TransformSequence()


@transforms.add
def set_defaults(config, tests):
    for test in tests:
        test['mozharness']['build-artifact-name'] = 'public/build/target.tar.bz2'
        # all desktop tests want to run the bits that require node
        test['mozharness']['set-moz-node-path'] = True
        yield test


@transforms.add
def set_treeherder_machine_platform(config, tests):
    """Set the appropriate task.extra.treeherder.machine.platform"""
    # Linux64 build platforms for asan and pgo are specified differently to
    # treeherder.  This is temporary until we can clean up the handling of
    # platforms
    translation = {
        'linux64-asan/opt': 'linux64/asan',
        'linux64-pgo/opt': 'linux64/pgo',
    }
    for test in tests:
        build_platform = test['build-platform']
        test['treeherder-machine-platform'] = translation.get(build_platform, build_platform)
        yield test


@transforms.add
def set_asan_docker_image(config, tests):
    """Set the appropriate task.extra.treeherder.docker-image"""
    # Linux64-asan has many leaks with running mochitest-media jobs
    # on Ubuntu 16.04, please remove this when bug 1289209 is resolved
    for test in tests:
        if test['suite'] == 'mochitest/mochitest-media' and \
           test['build-platform'] == 'linux64-asan/opt':
            test['docker-image'] = {"in-tree": "desktop-test"}
        yield test


@transforms.add
def split_e10s(config, tests):
    for test in tests:
        e10s = get_keyed_by(item=test, field='e10s',
                            item_name=test['test-name'])
        test.setdefault('attributes', {})
        test['e10s'] = False
        test['attributes']['e10s'] = False

        if e10s == 'both':
            yield test
            test = copy.deepcopy(test)
            e10s = True
        if e10s:
            test['test-name'] += '-e10s'
            test['e10s'] = True
            test['attributes']['e10s'] = True
            group, symbol = split_symbol(test['treeherder-symbol'])
            if group != '?':
                group += '-e10s'
            test['treeherder-symbol'] = join_symbol(group, symbol)
            test['mozharness'].setdefault('extra-options', []).append('--e10s')
        yield test


@transforms.add
def allow_software_gl_layers(config, tests):
    for test in tests:
        allow = get_keyed_by(item=test, field='allow-software-gl-layers',
                             item_name=test['test-name'])
        if allow:
            assert test['instance-size'] != 'legacy',\
                   'Software GL layers on a legacy instance is disallowed (bug 1296086).'

            # This should be set always once bug 1296086 is resolved.
            test['mozharness'].setdefault('extra-options', [])\
                              .append("--allow-software-gl-layers")

        yield test
