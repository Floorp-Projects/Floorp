# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transforms construct a task description to run the given test, based on a
test description.  The implementation here is shared among all test kinds, but
contains specific support for how we run tests in Gecko (via mozharness,
invoked in particular ways).

This is a good place to translate a test-description option such as
`single-core: true` to the implementation of that option in a task description
(worker options, mozharness commandline, environment variables, etc.)

The test description should be fully formed by the time it reaches these
transforms, and these transforms should not embody any specific knowledge about
what should run where. this is the wrong place for special-casing platforms,
for example - use `all_tests.py` instead.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.treeherder import split_symbol, join_symbol
from taskgraph.util.schema import (
    validate_schema,
    optionally_keyed_by,
    Schema,
)
from voluptuous import (
    Any,
    Optional,
    Required,
)

import copy
import logging

WORKER_TYPE = {
    # default worker types keyed by instance-size
    'large': 'aws-provisioner-v1/gecko-t-linux-large',
    'xlarge': 'aws-provisioner-v1/gecko-t-linux-xlarge',
    'legacy': 'aws-provisioner-v1/gecko-t-linux-medium',
    'default': 'aws-provisioner-v1/gecko-t-linux-large',
    # windows / os x worker types keyed by test-platform
    'windows7-32-vm': 'aws-provisioner-v1/gecko-t-win7-32',
    'windows7-32': 'aws-provisioner-v1/gecko-t-win7-32-gpu',
    'windows10-64-vm': 'aws-provisioner-v1/gecko-t-win10-64',
    'windows10-64': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
    'windows10-64-asan': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
    'macosx64': 'scl3-puppet/os-x-10-10-gw'
}

logger = logging.getLogger(__name__)

transforms = TransformSequence()

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
    Required('suite'): optionally_keyed_by(
        'test-platform',
        basestring),

    # the name by which this test suite is addressed in try syntax; defaults to
    # the test-name
    Optional('unittest-try-name'): basestring,

    # the name by which this talos test is addressed in try syntax
    Optional('talos-try-name'): basestring,

    # additional tags to mark up this type of test
    Optional('tags'): {basestring: object},

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
    Optional('tier'): optionally_keyed_by(
        'test-platform',
        Any(int, 'default')),

    # number of chunks to create for this task.  This can be keyed by test
    # platform by passing a dictionary in the `by-test-platform` key.  If the
    # test platform is not found, the key 'default' will be tried.
    Required('chunks', default=1): optionally_keyed_by(
        'test-platform',
        int),

    # the time (with unit) after which this task is deleted; default depends on
    # the branch (see below)
    Optional('expires-after'): basestring,

    # Whether to run this task with e10s (desktop-test only).  If false, run
    # without e10s; if true, run with e10s; if 'both', run one task with and
    # one task without e10s.  E10s tasks have "-e10s" appended to the test name
    # and treeherder group.
    Required('e10s', default='both'): optionally_keyed_by(
        'test-platform', 'project',
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
    # due to poor LLVMPipe performance (bug 1296086).  Defaults to true for
    # linux platforms and false otherwise
    Optional('allow-software-gl-layers'): bool,

    # The worker implementation for this test, as dictated by policy and by the
    # test platform.
    Optional('worker-implementation'): Any(
        'docker-worker',
        'native-engine',
        'generic-worker',
        # coming soon:
        'docker-engine',
        'buildbot-bridge',
    ),

    # For tasks that will run in docker-worker or docker-engine, this is the
    # name of the docker image or in-tree docker image to run the task in.  If
    # in-tree, then a dependency will be created automatically.  This is
    # generally `desktop-test`, or an image that acts an awful lot like it.
    Required('docker-image', default={'in-tree': 'desktop-test'}): optionally_keyed_by(
        'test-platform', 'test-platform-phylum',
        Any(
            # a raw Docker image path (repo/image:tag)
            basestring,
            # an in-tree generated docker image (from `taskcluster/docker/<name>`)
            {'in-tree': basestring}
        )
    ),

    # seconds of runtime after which the task will be killed.  Like 'chunks',
    # this can be keyed by test pltaform.
    Required('max-run-time', default=3600): optionally_keyed_by(
        'test-platform',
        int),

    # the exit status code that indicates the task should be retried
    Optional('retry-exit-status'): int,

    # Whether to perform a gecko checkout.
    Required('checkout', default=False): bool,

    # Wheter to perform a machine reboot after test is done
    Optional('reboot', default=True): bool,

    # What to run
    Required('mozharness'): optionally_keyed_by(
        'test-platform', 'test-platform-phylum', {
            # the mozharness script used to run this task
            Required('script'): basestring,

            # the config files required for the task
            Required('config'): optionally_keyed_by(
                'test-platform',
                [basestring]),

            # mochitest flavor for mochitest runs
            Optional('mochitest-flavor'): basestring,

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
            Required('chunked', default=False): optionally_keyed_by(
                'test-platform',
                bool),

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
        }
    ),

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

    # conditional files to determine when these tests should be run
    Optional('when'): Any({
        Optional('files-changed'): [basestring],
    }),

}, required=True)


@transforms.add
def validate(config, tests):
    for test in tests:
        yield validate_schema(test_description_schema, test,
                              "In test {!r}:".format(test['test-name']))


@transforms.add
def handle_keyed_by_mozharness(config, tests):
    """Resolve a mozharness field if it is keyed by something"""
    for test in tests:
        resolve_keyed_by(test, 'mozharness', item_name=test['test-name'])
        yield test


@transforms.add
def set_defaults(config, tests):
    for test in tests:
        build_platform = test['build-platform']
        if build_platform.startswith('android'):
            # all Android test tasks download internal objects from tooltool
            test['mozharness']['tooltool-downloads'] = True
            test['mozharness']['actions'] = ['get-secrets']
            # Android doesn't do e10s
            test['e10s'] = False
            # loopback-video is always true for Android, but false for other
            # platform phyla
            test['loopback-video'] = True
        else:
            # all non-android tests want to run the bits that require node
            test['mozharness']['set-moz-node-path'] = True
            test.setdefault('e10s', 'both')

        # software-gl-layers is only meaningful on linux, where it defaults to True
        if test['test-platform'].startswith('linux'):
            test.setdefault('allow-software-gl-layers', True)
        else:
            test['allow-software-gl-layers'] = False

        test.setdefault('os-groups', [])
        test.setdefault('chunks', 1)
        test.setdefault('run-on-projects', ['all'])
        test.setdefault('instance-size', 'default')
        test.setdefault('max-run-time', 3600)
        test.setdefault('reboot', True)
        test['mozharness'].setdefault('extra-options', [])
        yield test


@transforms.add
def set_target(config, tests):
    for test in tests:
        build_platform = test['build-platform']
        if build_platform.startswith('macosx'):
            target = 'target.dmg'
        elif build_platform.startswith('android'):
            if 'geckoview' in test['test-name']:
                target = 'geckoview_example.apk'
            else:
                target = 'target.apk'
        elif build_platform.startswith('win'):
            target = 'firefox-{}.en-US.{}.zip'.format(
                get_firefox_version(),
                build_platform.split('/')[0]
            )
        else:
            target = 'target.tar.bz2'
        test['mozharness']['build-artifact-name'] = 'public/build/' + target
        yield test


@transforms.add
def set_treeherder_machine_platform(config, tests):
    """Set the appropriate task.extra.treeherder.machine.platform"""
    translation = {
        # Linux64 build platforms for asan and pgo are specified differently to
        # treeherder.
        'linux64-asan/opt': 'linux64/asan',
        'linux64-pgo/opt': 'linux64/pgo',
        'macosx64/debug': 'osx-10-10/debug',
        'macosx64/opt': 'osx-10-10/opt',
        'win64-asan/opt': 'windows10-64/asan',
        # The build names for Android platforms have partially evolved over the
        # years and need to be translated.
        'android-api-15/debug': 'android-4-3-armv7-api15/debug',
        'android-api-15/opt': 'android-4-3-armv7-api15/opt',
        'android-x86/opt': 'android-4-2-x86/opt',
        'android-api-15-gradle/opt': 'android-api-15-gradle/opt',
    }
    for test in tests:
        test['treeherder-machine-platform'] = translation.get(
            test['build-platform'], test['test-platform'])
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
def set_worker_implementation(config, tests):
    """Set the worker implementation based on the test platform."""
    for test in tests:
        if test['test-platform'].startswith('macosx'):
            # see if '-g' appears in try syntax
            if config.config['args'].generic_worker:
                test['worker-implementation'] = 'generic-worker'
            # see if '-w' appears in try syntax
            elif config.config['args'].taskcluster_worker:
                test['worker-implementation'] = 'native-engine'
            else:
                test['worker-implementation'] = 'buildbot-bridge'
        elif test.get('suite', '') == 'talos':
            test['worker-implementation'] = 'buildbot-bridge'
        elif test['test-platform'].startswith('win'):
            test['worker-implementation'] = 'generic-worker'
        else:
            test['worker-implementation'] = 'docker-worker'
        yield test


@transforms.add
def set_tier(config, tests):
    """Set the tier based on policy for all test descriptions that do not
    specify a tier otherwise."""
    for test in tests:
        if 'tier' in test:
            resolve_keyed_by(test, 'tier', item_name=test['test-name'])

        # only override if not set for the test
        if 'tier' not in test or test['tier'] == 'default':
            if test['test-platform'] in ['linux32/opt',
                                         'linux32/debug',
                                         'linux32-nightly/opt',
                                         'linux64/opt',
                                         'linux64-nightly/opt',
                                         'linux64/debug',
                                         'linux64-pgo/opt',
                                         'linux64-asan/opt',
                                         'android-4.3-arm7-api-15/opt',
                                         'android-4.3-arm7-api-15/debug',
                                         'android-4.2-x86/opt']:
                test['tier'] = 1
            elif test['worker-implementation'] == 'native-engine':
                test['tier'] = 3
            else:
                test['tier'] = 2
        yield test


@transforms.add
def set_expires_after(config, tests):
    """Try jobs expire after 2 weeks; everything else lasts 1 year.  This helps
    keep storage costs low."""
    for test in tests:
        if 'expires-after' not in test:
            if config.params['project'] == 'try':
                test['expires-after'] = "14 days"
            else:
                test['expires-after'] = "1 year"
        yield test


@transforms.add
def set_download_symbols(config, tests):
    """In general, we download symbols immediately for debug builds, but only
    on demand for everything else. ASAN builds shouldn't download
    symbols since they don't product symbol zips see bug 1283879"""
    for test in tests:
        if test['test-platform'].split('/')[-1] == 'debug':
            test['mozharness']['download-symbols'] = True
        elif test['build-platform'] == 'linux64-asan/opt' or \
                test['build-platform'] == 'windows10-64-asan/opt':
            if 'download-symbols' in test['mozharness']:
                del test['mozharness']['download-symbols']
        else:
            test['mozharness']['download-symbols'] = 'ondemand'
        yield test


@transforms.add
def handle_keyed_by(config, tests):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        'instance-size',
        'docker-image',
        'max-run-time',
        'chunks',
        'e10s',
        'suite',
        'run-on-projects',
        'os-groups',
        'mozharness.chunked',
        'mozharness.config',
        'mozharness.extra-options',
    ]
    for test in tests:
        for field in fields:
            resolve_keyed_by(test, field, item_name=test['test-name'],
                             project=config.params['project'])
        yield test


@transforms.add
def enable_code_coverage(config, tests):
    """Enable code coverage for the linux64-ccov/opt & linux64-jsdcov/opt build-platforms"""
    for test in tests:
        if test['build-platform'] == 'linux64-ccov/opt':
            test['mozharness'].setdefault('extra-options', []).append('--code-coverage')
            test['instance-size'] = 'xlarge'
            test['e10s'] = False
            test['run-on-projects'] = []
        elif test['build-platform'] == 'linux64-jsdcov/opt':
            test['run-on-projects'] = []
        yield test


@transforms.add
def split_e10s(config, tests):
    for test in tests:
        e10s = test['e10s']

        test.setdefault('attributes', {})
        test['e10s'] = False
        test['attributes']['e10s'] = False

        if e10s == 'both':
            yield copy.deepcopy(test)
            e10s = True
        if e10s:
            test['test-name'] += '-e10s'
            test['e10s'] = True
            test['attributes']['e10s'] = True
            group, symbol = split_symbol(test['treeherder-symbol'])
            if group != '?':
                group += '-e10s'
            test['treeherder-symbol'] = join_symbol(group, symbol)
            test['mozharness']['extra-options'].append('--e10s')
        yield test


@transforms.add
def split_chunks(config, tests):
    """Based on the 'chunks' key, split tests up into chunks by duplicating
    them and assigning 'this-chunk' appropriately and updating the treeherder
    symbol."""
    for test in tests:
        if test['chunks'] == 1:
            test['this-chunk'] = 1
            yield test
            continue

        for this_chunk in range(1, test['chunks'] + 1):
            # copy the test and update with the chunk number
            chunked = copy.deepcopy(test)
            chunked['this-chunk'] = this_chunk

            # add the chunk number to the TH symbol
            group, symbol = split_symbol(chunked['treeherder-symbol'])
            symbol += str(this_chunk)
            chunked['treeherder-symbol'] = join_symbol(group, symbol)

            yield chunked


@transforms.add
def allow_software_gl_layers(config, tests):
    """
    Handle the "allow-software-gl-layers" property for platforms where it
    applies.
    """
    for test in tests:
        if test.get('allow-software-gl-layers'):
            assert test['instance-size'] != 'legacy',\
                'Software GL layers on a legacy instance is disallowed (bug 1296086).'

            # This should be set always once bug 1296086 is resolved.
            test['mozharness'].setdefault('extra-options', [])\
                              .append("--allow-software-gl-layers")

        yield test


@transforms.add
def set_retry_exit_status(config, tests):
    """Set the retry exit status to TBPL_RETRY, the value returned by mozharness
       scripts to indicate a transient failure that should be retried."""
    for test in tests:
        test['retry-exit-status'] = 4
        yield test


@transforms.add
def set_profile(config, tests):
    """Set profiling mode for tests."""
    for test in tests:
        if config.config['args'].profile and test['suite'] == 'talos':
            test['mozharness']['extra-options'].append('--geckoProfile')
        yield test


@transforms.add
def set_tag(config, tests):
    """Set test for a specific tag."""
    for test in tests:
        tag = config.config['args'].tag
        if tag:
            test['mozharness']['extra-options'].extend(['--tag', tag])
        yield test


@transforms.add
def remove_linux_pgo_try_talos(config, tests):
    """linux64-pgo talos tests don't run on try."""
    def predicate(test):
        return not(
            test['test-platform'] == 'linux64-pgo/opt'
            and (test['suite'] == 'talos' or test['suite'] == 'awsy')
            and config.params['project'] == 'try'
        )
    for test in filter(predicate, tests):
        yield test


@transforms.add
def set_test_type(config, tests):
    for test in tests:
        for test_type in ['mochitest', 'reftest']:
            if test_type in test['suite'] and 'web-platform' not in test['suite']:
                test.setdefault('tags', {})['test-type'] = test_type
        yield test


@transforms.add
def parallel_stylo_tests(config, tests):
    """Ensure that any stylo tests running with e10s enabled also test
    parallel traversal in the style system."""

    for test in tests:
        if not test['test-platform'].startswith('linux64-stylo/'):
            yield test
            continue

        e10s = test['e10s']
        # We should have already handled 'both' in an earlier transform.
        assert e10s != 'both'
        if not e10s:
            yield test
            continue

        test['mozharness'].setdefault('extra-options', [])\
                          .append('--parallel-stylo-traversal')
        yield test


@transforms.add
def make_job_description(config, tests):
    """Convert *test* descriptions to *job* descriptions (input to
    taskgraph.transforms.job)"""

    for test in tests:
        label = '{}-{}-{}'.format(config.kind, test['test-platform'], test['test-name'])
        if test['chunks'] > 1:
            label += '-{}'.format(test['this-chunk'])

        build_label = test['build-label']

        if 'talos-try-name' in test:
            try_name = test['talos-try-name']
            attr_try_name = 'talos_try_name'
        else:
            try_name = test.get('unittest-try-name', test['test-name'])
            attr_try_name = 'unittest_try_name'

        attr_build_platform, attr_build_type = test['build-platform'].split('/', 1)

        suite = test['suite']
        if '/' in suite:
            suite, flavor = suite.split('/', 1)
        else:
            flavor = suite

        attributes = test.get('attributes', {})
        attributes.update({
            'build_platform': attr_build_platform,
            'build_type': attr_build_type,
            # only keep the first portion of the test platform
            'test_platform': test['test-platform'].split('/')[0],
            'test_chunk': str(test['this-chunk']),
            'unittest_suite': suite,
            'unittest_flavor': flavor,
            attr_try_name: try_name,
        })

        jobdesc = {}
        name = '{}-{}'.format(test['test-platform'], test['test-name'])
        jobdesc['name'] = name
        jobdesc['label'] = label
        jobdesc['description'] = test['description']
        jobdesc['when'] = test.get('when', {})
        jobdesc['attributes'] = attributes
        jobdesc['dependencies'] = {'build': build_label}
        jobdesc['expires-after'] = test['expires-after']
        jobdesc['routes'] = []
        jobdesc['run-on-projects'] = test.get('run-on-projects', ['all'])
        jobdesc['scopes'] = []
        jobdesc['tags'] = test.get('tags', {})
        jobdesc['optimizations'] = [['seta']]  # always run SETA for tests
        jobdesc['extra'] = {
            'chunks': {
                'current': test['this-chunk'],
                'total': test['chunks'],
            },
            'suite': {
                'name': suite,
                'flavor': flavor,
            },
        }
        jobdesc['treeherder'] = {
            'symbol': test['treeherder-symbol'],
            'kind': 'test',
            'tier': test['tier'],
            'platform': test.get('treeherder-machine-platform', test['build-platform']),
        }
        run = jobdesc['run'] = {}
        run['using'] = 'mozharness-test'
        run['test'] = test
        worker = jobdesc['worker'] = {}
        implementation = worker['implementation'] = test['worker-implementation']

        if implementation == 'buildbot-bridge':
            jobdesc['worker-type'] = 'buildbot-bridge/buildbot-bridge'
        elif implementation == 'native-engine':
            jobdesc['worker-type'] = 'tc-worker-provisioner/gecko-t-osx-10-10'
        elif implementation == 'generic-worker':
            test_platform = test['test-platform'].split('/')[0]
            jobdesc['worker-type'] = WORKER_TYPE[test_platform]
        elif implementation == 'docker-worker' or implementation == 'docker-engine':
            jobdesc['worker-type'] = WORKER_TYPE[test['instance-size']]
            worker = jobdesc['worker']
            worker['docker-image'] = test['docker-image']
            worker['allow-ptrace'] = True  # required for all tests, for crashreporter
            worker['loopback-video'] = test['loopback-video']
            worker['loopback-audio'] = test['loopback-audio']
            worker['max-run-time'] = test['max-run-time']
            worker['retry-exit-status'] = test['retry-exit-status']
        yield jobdesc


def normpath(path):
    return path.replace('/', '\\')


def get_firefox_version():
    with open('browser/config/version.txt', 'r') as f:
        return f.readline().strip()
