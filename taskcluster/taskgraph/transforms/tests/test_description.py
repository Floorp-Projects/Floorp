# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This file defines the schema for tests -- the things in `tests.yml`.  It should
be run both before and after the kind-specific transforms, to ensure that the
transforms do not generate invalid tests.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import validate_schema, optionally_keyed_by
from voluptuous import (
    Any,
    Optional,
    Required,
    Schema,
)


# Schema for a test description
#
# *****WARNING*****
#
# This is a great place for baffling cruft to accumulate, and that makes
# everyone move more slowly.  Be considerate of your fellow hackers!
# See the warnings in taskcluster/docs/how-tos.rst
#
# *****WARNING*****
test_description_schema = Schema({
    # description of the suite, for the task metadata
    'description': basestring,

    # test suite name, or <suite>/<flavor>
    Required('suite'): optionally_keyed_by('test-platform', basestring),

    # the name by which this test suite is addressed in try syntax; defaults to
    # the test-name
    Optional('unittest-try-name'): basestring,

    # the name by which this talos test is addressed in try syntax
    Optional('talos-try-name'): basestring,

    # the symbol, or group(symbol), under which this task should appear in
    # treeherder.
    'treeherder-symbol': basestring,

    # the value to place in task.extra.treeherder.machine.platform; ideally
    # this is the same as build-platform, and that is the default, but in
    # practice it's not always a match.
    Optional('treeherder-machine-platform'): basestring,

    # attributes to appear in the resulting task (later transforms will add the
    # common attributes)
    Optional('attributes'): {basestring: object},

    # The `run_on_projects` attribute, defaulting to "all".  This dictates the
    # projects on which this task should be included in the target task set.
    # See the attributes documentation for details.
    Optional('run-on-projects', default=['all']): optionally_keyed_by(
        'test-platform',
        [basestring]),

    # the sheriffing tier for this task (default: set based on test platform)
    Optional('tier'): Any(
        int,
        {'by-test-platform': {basestring: int}},
    ),

    # number of chunks to create for this task.  This can be keyed by test
    # platform by passing a dictionary in the `by-test-platform` key.  If the
    # test platform is not found, the key 'default' will be tried.
    Required('chunks', default=1): optionally_keyed_by('test-platform', int),

    # the time (with unit) after which this task is deleted; default depends on
    # the branch (see below)
    Optional('expires-after'): basestring,

    # Whether to run this task with e10s (desktop-test only).  If false, run
    # without e10s; if true, run with e10s; if 'both', run one task with and
    # one task without e10s.  E10s tasks have "-e10s" appended to the test name
    # and treeherder group.
    Required('e10s', default='both'): optionally_keyed_by(
        'test-platform',
        Any(bool, 'both')),

    # The EC2 instance size to run these tests on.
    Required('instance-size', default='default'): optionally_keyed_by(
        'test-platform',
        Any('default', 'large', 'xlarge', 'legacy')),

    # Whether the task requires loopback audio or video (whatever that may mean
    # on the platform)
    Required('loopback-audio', default=False): bool,
    Required('loopback-video', default=False): bool,

    # Whether the test can run using a software GL implementation on Linux
    # using the GL compositor. May not be used with "legacy" sized instances
    # due to poor LLVMPipe performance (bug 1296086).
    Optional('allow-software-gl-layers', default=True): bool,

    # The worker implementation for this test, as dictated by policy and by the
    # test platform.
    Optional('worker-implementation'): Any(
        'docker-worker',
        'macosx-engine',
        'generic-worker',
        # coming soon:
        'docker-engine',
        'buildbot-bridge',
    ),

    # For tasks that will run in docker-worker or docker-engine, this is the
    # name of the docker image or in-tree docker image to run the task in.  If
    # in-tree, then a dependency will be created automatically.  This is
    # generally `desktop-test`, or an image that acts an awful lot like it.
    Required('docker-image', default={'in-tree': 'desktop-test'}): Any(
        # a raw Docker image path (repo/image:tag)
        basestring,
        # an in-tree generated docker image (from `taskcluster/docker/<name>`)
        {'in-tree': basestring}
    ),

    # seconds of runtime after which the task will be killed.  Like 'chunks',
    # this can be keyed by test pltaform.
    Required('max-run-time', default=3600): optionally_keyed_by('test-platform', int),

    # the exit status code that indicates the task should be retried
    Optional('retry-exit-status'): int,

    # Whether to perform a gecko checkout.
    Required('checkout', default=False): bool,

    # What to run
    Required('mozharness'): Any({
        # the mozharness script used to run this task
        Required('script'): basestring,

        # the config files required for the task
        Required('config'): optionally_keyed_by('test-platform', [basestring]),

        # any additional actions to pass to the mozharness command
        Optional('actions'): [basestring],

        # additional command-line options for mozharness, beyond those
        # automatically added
        Required('extra-options', default=[]): optionally_keyed_by(
            'test-platform',
            [basestring]),

        # the artifact name (including path) to test on the build task; this is
        # generally set in a per-kind transformation
        Optional('build-artifact-name'): basestring,

        # If true, tooltool downloads will be enabled via relengAPIProxy.
        Required('tooltool-downloads', default=False): bool,

        # This mozharness script also runs in Buildbot and tries to read a
        # buildbot config file, so tell it not to do so in TaskCluster
        Required('no-read-buildbot-config', default=False): bool,

        # Add --blob-upload-branch=<project> mozharness parameter
        Optional('include-blob-upload-branch'): bool,

        # The setting for --download-symbols (if omitted, the option will not
        # be passed to mozharness)
        Optional('download-symbols'): Any(True, 'ondemand'),

        # If set, then MOZ_NODE_PATH=/usr/local/bin/node is included in the
        # environment.  This is more than just a helpful path setting -- it
        # causes xpcshell tests to start additional servers, and runs
        # additional tests.
        Required('set-moz-node-path', default=False): bool,

        # If true, include chunking information in the command even if the number
        # of chunks is 1
        Required('chunked', default=False): bool,

        # The chunking argument format to use
        Required('chunking-args', default='this-chunk'): Any(
            # Use the usual --this-chunk/--total-chunk arguments
            'this-chunk',
            # Use --test-suite=<suite>-<chunk-suffix>; see chunk-suffix, below
            'test-suite-suffix',
        ),

        # the string to append to the `--test-suite` arugment when
        # chunking-args = test-suite-suffix; "<CHUNK>" in this string will
        # be replaced with the chunk number.
        Optional('chunk-suffix'): basestring,
    }),

    # The current chunk; this is filled in by `all_kinds.py`
    Optional('this-chunk'): int,

    # os user groups for test task workers; required scopes, will be
    # added automatically
    Optional('os-groups', default=[]): optionally_keyed_by(
        'test-platform',
        [basestring]),

    # -- values supplied by the task-generation infrastructure

    # the platform of the build this task is testing
    'build-platform': basestring,

    # the label of the build task generating the materials to test
    'build-label': basestring,

    # the platform on which the tests will run
    'test-platform': basestring,

    # the name of the test (the key in tests.yml)
    'test-name': basestring,

    # the product name, defaults to firefox
    Optional('product'): basestring,

}, required=True)


# TODO: can we have validate and validate_full for before and after?
def validate(config, tests):
    for test in tests:
        yield validate_schema(test_description_schema, test,
                              "In test {!r}:".format(test['test-name']))
