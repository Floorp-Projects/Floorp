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
from taskgraph.util.platforms import platform_family
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

# default worker types keyed by instance-size
LINUX_WORKER_TYPES = {
    'large': 'aws-provisioner-v1/gecko-t-linux-large',
    'xlarge': 'aws-provisioner-v1/gecko-t-linux-xlarge',
    'default': 'aws-provisioner-v1/gecko-t-linux-large',
}

# windows worker types keyed by test-platform and virtualization
WINDOWS_WORKER_TYPES = {
    'windows7-32': {
      'virtual': 'aws-provisioner-v1/gecko-t-win7-32',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win7-32-gpu',
      'hardware': 'releng-hardware/gecko-t-win7-32-hw',
    },
    'windows7-32-pgo': {
      'virtual': 'aws-provisioner-v1/gecko-t-win7-32',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win7-32-gpu',
      'hardware': 'releng-hardware/gecko-t-win7-32-hw',
    },
    'windows7-32-nightly': {
      'virtual': 'aws-provisioner-v1/gecko-t-win7-32',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win7-32-gpu',
      'hardware': 'releng-hardware/gecko-t-win7-32-hw',
    },
    'windows7-32-devedition': {
      'virtual': 'aws-provisioner-v1/gecko-t-win7-32',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win7-32-gpu',
      'hardware': 'releng-hardware/gecko-t-win7-32-hw',
    },
    'windows7-32-stylo-disabled': {
      'virtual': 'aws-provisioner-v1/gecko-t-win7-32',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win7-32-gpu',
      'hardware': 'releng-hardware/gecko-t-win7-32-hw',
    },
    'windows10-64': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows10-64-ccov': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows10-64-pgo': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows10-64-devedition': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows10-64-nightly': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows10-64-stylo-disabled': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows10-64-asan': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    # These values don't really matter since BBB will be executing them
    'windows8-64': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows8-64-pgo': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
    'windows8-64-nightly': {
      'virtual': 'aws-provisioner-v1/gecko-t-win10-64',
      'virtual-with-gpu': 'aws-provisioner-v1/gecko-t-win10-64-gpu',
      'hardware': 'releng-hardware/gecko-t-win10-64-hw',
    },
}

# os x worker types keyed by test-platform
MACOSX_WORKER_TYPES = {
    'macosx64': 'releng-hardware/gecko-t-osx-1010',
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
    # the test-name.  This will translate to the `unittest_try_name` or
    # `talos_try_name` attribute.
    Optional('try-name'): basestring,

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

    # relative path (from config.path) to the file task was defined in
    Optional('job-from'): basestring,

    # The `run_on_projects` attribute, defaulting to "all".  This dictates the
    # projects on which this task should be included in the target task set.
    # See the attributes documentation for details.
    #
    # Note that the special case 'built-projects', the default, uses the parent
    # build task's run-on-projects, meaning that tests run only on platforms
    # that are built.
    Optional('run-on-projects', default='built-projects'): optionally_keyed_by(
        'test-platform',
        Any([basestring], 'built-projects')),

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
    Required('e10s', default='true'): optionally_keyed_by(
        'test-platform', 'project',
        Any(bool, 'both')),

    # Whether the task should run with WebRender enabled or not.
    Optional('webrender', default=False): bool,

    # The EC2 instance size to run these tests on.
    Required('instance-size', default='default'): optionally_keyed_by(
        'test-platform',
        Any('default', 'large', 'xlarge')),

    # type of virtualization or hardware required by test.
    Required('virtualization', default='virtual'): optionally_keyed_by(
        'test-platform',
        Any('virtual', 'virtual-with-gpu', 'hardware')),

    # Whether the task requires loopback audio or video (whatever that may mean
    # on the platform)
    Required('loopback-audio', default=False): bool,
    Required('loopback-video', default=False): bool,

    # Whether the test can run using a software GL implementation on Linux
    # using the GL compositor. May not be used with "legacy" sized instances
    # due to poor LLVMPipe performance (bug 1296086).  Defaults to true for
    # unit tests on linux platforms and false otherwise
    Optional('allow-software-gl-layers'): bool,

    # For tasks that will run in docker-worker or docker-engine, this is the
    # name of the docker image or in-tree docker image to run the task in.  If
    # in-tree, then a dependency will be created automatically.  This is
    # generally `desktop-test`, or an image that acts an awful lot like it.
    Required('docker-image', default={'in-tree': 'desktop1604-test'}): optionally_keyed_by(
        'test-platform',
        Any(
            # a raw Docker image path (repo/image:tag)
            basestring,
            # an in-tree generated docker image (from `taskcluster/docker/<name>`)
            {'in-tree': basestring},
            # an indexed docker image
            {'indexed': basestring},
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
    Optional('reboot', default=False):
        Any(False, 'always', 'on-exception', 'on-failure'),

    # What to run
    Required('mozharness'): {
        # the mozharness script used to run this task
        Required('script'): optionally_keyed_by(
            'test-platform',
            basestring),

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

        Required('requires-signed-builds', default=False): optionally_keyed_by(
            'test-platform',
            bool),
    },

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

    # the label of the signing task generating the materials to test.
    # Signed builds are used in xpcshell tests on Windows, for instance.
    Optional('build-signing-label'): basestring,

    # the build's attributes
    'build-attributes': {basestring: object},

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

    Optional('worker-type'): optionally_keyed_by(
        'test-platform',
        Any(basestring, None),
    ),

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
            test.setdefault('e10s', 'true')

        # software-gl-layers is only meaningful on linux unittests, where it defaults to True
        if test['test-platform'].startswith('linux') and test['suite'] != 'talos':
            test.setdefault('allow-software-gl-layers', True)
        else:
            test['allow-software-gl-layers'] = False

        # Enable WebRender by default on the QuantumRender test platform, since
        # the whole point of QuantumRender is to run with WebRender enabled.
        # If other *-qr test platforms are added they should also be checked for
        # here; currently linux64-qr is the only one.
        if test['test-platform'].startswith('linux64-qr'):
            test['webrender'] = True
        else:
            test.setdefault('webrender', False)

        test.setdefault('try-name', test['test-name'])

        test.setdefault('os-groups', [])
        test.setdefault('chunks', 1)
        test.setdefault('run-on-projects', 'built-projects')
        test.setdefault('instance-size', 'default')
        test.setdefault('max-run-time', 3600)
        test.setdefault('reboot', True)
        test['mozharness'].setdefault('extra-options', [])
        yield test


@transforms.add
def setup_talos(config, tests):
    """Add options that are specific to talos jobs (identified by suite=talos)"""
    for test in tests:
        if test['suite'] != 'talos':
            yield test
            continue

        extra_options = test.setdefault('mozharness', {}).setdefault('extra-options', [])
        extra_options.append('--add-option')
        extra_options.append('--webServer,localhost')
        extra_options.append('--use-talos-json')

        # Per https://bugzilla.mozilla.org/show_bug.cgi?id=1357753#c3, branch
        # name is only required for try
        if config.params['project'] == 'try':
            extra_options.append('--branch-name')
            extra_options.append('try')

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
            target = 'target.zip'
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
        'win32-pgo/opt': 'windows7-32/pgo',
        'win64-pgo/opt': 'windows10-64/pgo',
        # The build names for Android platforms have partially evolved over the
        # years and need to be translated.
        'android-api-16/debug': 'android-4-3-armv7-api16/debug',
        'android-api-16/opt': 'android-4-3-armv7-api16/opt',
        'android-x86/opt': 'android-4-2-x86/opt',
        'android-api-16-gradle/opt': 'android-api-16-gradle/opt',
    }
    for test in tests:
        # For most desktop platforms, the above table is not used for "regular"
        # builds, so we'll always pick the test platform here.
        # On macOS though, the regular builds are in the table.  This causes a
        # conflict in `verify_task_graph_symbol` once you add a new test
        # platform based on regular macOS builds, such as for Stylo.
        # Since it's unclear if the regular macOS builds can be removed from
        # the table, workaround the issue for Stylo.
        if '-stylo-disabled' in test['test-platform']:
            test['treeherder-machine-platform'] = test['test-platform']
        else:
            test['treeherder-machine-platform'] = translation.get(
                test['build-platform'], test['test-platform'])
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
                                         'linux32-devedition/opt',
                                         'linux32-stylo-disabled/debug',
                                         'linux32-stylo-disabled/opt',
                                         'linux64/opt',
                                         'linux64-nightly/opt',
                                         'linux64/debug',
                                         'linux64-pgo/opt',
                                         'linux64-devedition/opt',
                                         'linux64-asan/opt',
                                         'linux64-stylo-disabled/debug',
                                         'linux64-stylo-disabled/opt',
                                         'windows7-32/debug',
                                         'windows7-32/opt',
                                         'windows7-32-pgo/opt',
                                         'windows7-32-devedition/opt',
                                         'windows7-32-nightly/opt',
                                         'windows7-32-stylo-disabled/debug',
                                         'windows7-32-stylo-disabled/opt',
                                         'windows10-64/debug',
                                         'windows10-64/opt',
                                         'windows10-64-pgo/opt',
                                         'windows10-64-devedition/opt',
                                         'windows10-64-nightly/opt',
                                         'windows10-64-stylo-disabled/debug',
                                         'windows10-64-stylo-disabled/opt',
                                         'macosx64/opt',
                                         'macosx64/debug',
                                         'macosx64-nightly/opt',
                                         'macosx64-devedition/opt',
                                         'macosx64-stylo-disabled/debug',
                                         'macosx64-stylo-disabled/opt',
                                         'android-4.3-arm7-api-16/opt',
                                         'android-4.3-arm7-api-16/debug',
                                         'android-4.2-x86/opt']:
                test['tier'] = 1
            else:
                test['tier'] = 2

        # Temporarily set windows10-64-ccov/debug tests as tier 3, until we get the tests
        # consistently green.
        if test['test-platform'] == 'windows10-64-ccov/debug':
            test['tier'] = 3

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
        'mozharness.requires-signed-builds',
        'mozharness.script',
        'worker-type',
        'virtualization',
    ]
    for test in tests:
        for field in fields:
            resolve_keyed_by(test, field, item_name=test['test-name'],
                             project=config.params['project'])
        yield test


@transforms.add
def handle_suite_category(config, tests):
    for test in tests:
        if '/' in test['suite']:
            suite, flavor = test['suite'].split('/', 1)
        else:
            suite = flavor = test['suite']

        test.setdefault('attributes', {})
        test['attributes']['unittest_suite'] = suite
        test['attributes']['unittest_flavor'] = flavor

        script = test['mozharness']['script']
        category_arg = None
        if suite == 'test-verification':
            pass
        elif script == 'android_emulator_unittest.py':
            category_arg = '--test-suite'
        elif script == 'desktop_unittest.py':
            category_arg = '--{}-suite'.format(suite)

        if category_arg:
            test['mozharness'].setdefault('extra-options', [])
            extra = test['mozharness']['extra-options']
            if not any(arg.startswith(category_arg) for arg in extra):
                extra.append('{}={}'.format(category_arg, flavor))

        yield test


@transforms.add
def enable_code_coverage(config, tests):
    """Enable code coverage for the linux64-ccov/opt & linux64-jsdcov/opt & win64-ccov/debug
    build-platforms"""
    for test in tests:
        if 'ccov' in test['build-platform'] and not test['test-name'].startswith('test-verify'):
            test['mozharness'].setdefault('extra-options', []).append('--code-coverage')
            test['when'] = {}
            test['instance-size'] = 'xlarge'
            # Ensure we don't run on inbound/autoland/beta, but if the test is try only, ignore it
            if 'mozilla-central' in test['run-on-projects'] or \
                    test['run-on-projects'] == 'built-projects':
                test['run-on-projects'] = ['mozilla-central', 'try']

            if 'talos' in test['test-name']:
                test['max-run-time'] = 7200
                if 'linux' in test['build-platform']:
                    test['docker-image'] = {"in-tree": "desktop1604-test"}
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--cycles,1')
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--tppagecycles,1')
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--no-upload-results')
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--tptimeout,15000')
        elif test['build-platform'] == 'linux64-jsdcov/opt':
            # Ensure we don't run on inbound/autoland/beta, but if the test is try only, ignore it
            if 'mozilla-central' in test['run-on-projects'] or \
                    test['run-on-projects'] == 'built-projects':
                test['run-on-projects'] = ['mozilla-central', 'try']
            test['mozharness'].setdefault('extra-options', []).append('--jsd-code-coverage')
        yield test


@transforms.add
def handle_run_on_projects(config, tests):
    """Handle translating `built-projects` appropriately"""
    for test in tests:
        if test['run-on-projects'] == 'built-projects':
            test['run-on-projects'] = test['build-attributes'].get('run_on_projects', ['all'])
        yield test


@transforms.add
def split_e10s(config, tests):
    for test in tests:
        e10s = test['e10s']

        test['e10s'] = False
        test['attributes']['e10s'] = False

        if e10s == 'both':
            yield copy.deepcopy(test)
            e10s = True
        if e10s:
            test['test-name'] += '-e10s'
            test['try-name'] += '-e10s'
            test['e10s'] = True
            test['attributes']['e10s'] = True
            group, symbol = split_symbol(test['treeherder-symbol'])
            if group != '?':
                group += '-e10s'
            test['treeherder-symbol'] = join_symbol(group, symbol)
            if test['suite'] == 'talos':
                for i, option in enumerate(test['mozharness']['extra-options']):
                    if option.startswith('--suite='):
                        test['mozharness']['extra-options'][i] += '-e10s'
            else:
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

        # HACK: Bug 1373578 appears to pass with more chunks, non-e10s only though
        if test['test-platform'] == 'windows7-32/debug' and test['test-name'] == 'reftest':
            test['chunks'] = 32

        if (test['test-platform'] == 'windows7-32/opt' or
            test['test-platform'] == 'windows7-32-pgo/opt') and \
                test['test-name'] in ['reftest-e10s', 'reftest-no-accel-e10s', 'reftest-gpu-e10s']:
            test['chunks'] = 32

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
            # This should be set always once bug 1296086 is resolved.
            test['mozharness'].setdefault('extra-options', [])\
                              .append("--allow-software-gl-layers")

        yield test


@transforms.add
def enable_webrender(config, tests):
    """
    Handle the "webrender" property by passing a flag to mozharness if it is
    enabled.
    """
    for test in tests:
        if test.get('webrender'):
            test['mozharness'].setdefault('extra-options', [])\
                              .append("--enable-webrender")

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
    profile = None
    if config.params['try_mode'] == 'try_option_syntax':
        profile = config.params['try_options']['profile']
    for test in tests:
        if profile and test['suite'] == 'talos':
            test['mozharness']['extra-options'].append('--geckoProfile')
        yield test


@transforms.add
def set_tag(config, tests):
    """Set test for a specific tag."""
    tag = None
    if config.params['try_mode'] == 'try_option_syntax':
        tag = config.params['try_options']['tag']
    for test in tests:
        if tag:
            test['mozharness']['extra-options'].extend(['--tag', tag])
        yield test


@transforms.add
def set_test_type(config, tests):
    for test in tests:
        for test_type in ['mochitest', 'reftest']:
            if test_type in test['suite'] and 'web-platform' not in test['suite']:
                test.setdefault('tags', {})['test-type'] = test_type
        yield test


@transforms.add
def disable_stylo(config, tests):
    """
    Disable Stylo for all jobs on `-stylo-disabled` platforms.
    """
    for test in tests:
        if '-stylo-disabled' not in test['test-platform']:
            yield test
            continue

        test['mozharness'].setdefault('extra-options', []).append('--disable-stylo')

        yield test


@transforms.add
def single_stylo_traversal_tests(config, tests):
    """Enable single traversal for all tests on the sequential Stylo platform."""

    for test in tests:
        if not test['test-platform'].startswith('linux64-stylo-sequential/'):
            yield test
            continue

        # Bug 1356122 - Run Stylo tests in sequential mode
        test['mozharness'].setdefault('extra-options', [])\
                          .append('--single-stylo-traversal')
        yield test


@transforms.add
def set_worker_type(config, tests):
    """Set the worker type based on the test platform."""
    for test in tests:
        # during the taskcluster migration, this is a bit tortured, but it
        # will get simpler eventually!
        test_platform = test['test-platform']
        try_options = config.params['try_options'] if config.params['try_options'] else {}
        if test.get('worker-type'):
            # This test already has its worker type defined, so just use that (yields below)
            pass
        elif test_platform.startswith('macosx'):
            test['worker-type'] = MACOSX_WORKER_TYPES['macosx64']
        elif test_platform.startswith('win'):
            win_worker_type_platform = WINDOWS_WORKER_TYPES[
                test_platform.split('/')[0]
            ]
            if test.get('suite', '') == 'talos' and 'ccov' not in test['build-platform']:
                if try_options.get('taskcluster_worker'):
                    test['worker-type'] = win_worker_type_platform['hardware']
                elif test['virtualization'] == 'virtual':
                    test['worker-type'] = win_worker_type_platform[test['virtualization']]
                else:
                    test['worker-type'] = 'buildbot-bridge/buildbot-bridge'
            else:
                test['worker-type'] = win_worker_type_platform[test['virtualization']]
        elif test_platform.startswith('linux') or test_platform.startswith('android'):
            if test.get('suite', '') == 'talos' and test['build-platform'] != 'linux64-ccov/opt':
                if try_options.get('taskcluster_worker'):
                    test['worker-type'] = 'releng-hardware/gecko-t-linux-talos'
                else:
                    test['worker-type'] = 'buildbot-bridge/buildbot-bridge'
            else:
                test['worker-type'] = LINUX_WORKER_TYPES[test['instance-size']]
        else:
            raise Exception("unknown test_platform {}".format(test_platform))

        yield test


@transforms.add
def skip_win10_hardware(config, tests):
    """Windows 10 hardware isn't ready yet, don't even bother scheduling
    unless we're on try"""
    for test in tests:
        if 'releng-hardware/gecko-t-win10-64-hw' not in test['worker-type']:
            yield test
        if config.params == 'try':
            yield test
        # Silently drop the test on the floor if its win10 hardware and we're not try


@transforms.add
def make_job_description(config, tests):
    """Convert *test* descriptions to *job* descriptions (input to
    taskgraph.transforms.job)"""

    for test in tests:
        label = '{}-{}-{}'.format(config.kind, test['test-platform'], test['test-name'])
        if test['chunks'] > 1:
            label += '-{}'.format(test['this-chunk'])

        build_label = test['build-label']

        try_name = test['try-name']
        if test['suite'] == 'talos':
            attr_try_name = 'talos_try_name'
        else:
            attr_try_name = 'unittest_try_name'

        attr_build_platform, attr_build_type = test['build-platform'].split('/', 1)

        attributes = test.get('attributes', {})
        attributes.update({
            'build_platform': attr_build_platform,
            'build_type': attr_build_type,
            'test_platform': test['test-platform'],
            'test_chunk': str(test['this-chunk']),
            attr_try_name: try_name,
        })

        jobdesc = {}
        name = '{}-{}'.format(test['test-platform'], test['test-name'])
        jobdesc['name'] = name
        jobdesc['label'] = label
        jobdesc['description'] = test['description']
        jobdesc['attributes'] = attributes
        jobdesc['dependencies'] = {'build': build_label}

        if test['mozharness']['requires-signed-builds'] is True:
            jobdesc['dependencies']['build-signing'] = test['build-signing-label']

        jobdesc['expires-after'] = test['expires-after']
        jobdesc['routes'] = []
        jobdesc['run-on-projects'] = test['run-on-projects']
        jobdesc['scopes'] = []
        jobdesc['tags'] = test.get('tags', {})
        jobdesc['extra'] = {
            'chunks': {
                'current': test['this-chunk'],
                'total': test['chunks'],
            },
            'suite': {
                'name': attributes['unittest_suite'],
                'flavor': attributes['unittest_flavor'],
            },
        }
        jobdesc['treeherder'] = {
            'symbol': test['treeherder-symbol'],
            'kind': 'test',
            'tier': test['tier'],
            'platform': test.get('treeherder-machine-platform', test['build-platform']),
        }

        if test.get('when'):
            jobdesc['when'] = test['when']
        else:
            schedules = [attributes['unittest_suite'], platform_family(test['build-platform'])]
            if config.params['project'] != 'try':
                # for non-try branches, include SETA
                jobdesc['optimization'] = {'skip-unless-schedules-or-seta': schedules}
            else:
                # otherwise just use skip-unless-schedules
                jobdesc['optimization'] = {'skip-unless-schedules': schedules}

        run = jobdesc['run'] = {}
        run['using'] = 'mozharness-test'
        run['test'] = test

        jobdesc['worker-type'] = test.pop('worker-type')

        yield jobdesc


def normpath(path):
    return path.replace('/', '\\')


def get_firefox_version():
    with open('browser/config/version.txt', 'r') as f:
        return f.readline().strip()
