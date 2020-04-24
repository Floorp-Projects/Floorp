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

import copy
import logging
from six import text_type

from mozbuild.schedules import INCLUSIVE_COMPONENTS
from moztest.resolve import TEST_SUITES
from voluptuous import (
    Any,
    Optional,
    Required,
    Exclusive,
)

import taskgraph
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import match_run_on_projects, keymatch
from taskgraph.util.keyed_by import evaluate_keyed_by
from taskgraph.util.schema import resolve_keyed_by, OptimizationSchema
from taskgraph.util.templates import merge
from taskgraph.util.treeherder import split_symbol, join_symbol, add_suffix
from taskgraph.util.platforms import platform_family
from taskgraph.util.schema import (
    optionally_keyed_by,
    Schema,
)
from taskgraph.util.chunking import (
    get_chunked_manifests,
    guess_mozinfo_from_task,
)
from taskgraph.util.taskcluster import (
    get_artifact_path,
    get_index_url,
)
from taskgraph.util.perfile import perfile_number_of_chunks


# default worker types keyed by instance-size
LINUX_WORKER_TYPES = {
    'large': 't-linux-large',
    'xlarge': 't-linux-xlarge',
    'default': 't-linux-large',
}

# windows worker types keyed by test-platform and virtualization
WINDOWS_WORKER_TYPES = {
    'windows7-32': {
      'virtual': 't-win7-32',
      'virtual-with-gpu': 't-win7-32-gpu',
      'hardware': 't-win10-64-hw',
    },
    'windows7-32-shippable': {
      'virtual': 't-win7-32',
      'virtual-with-gpu': 't-win7-32-gpu',
      'hardware': 't-win10-64-hw',
    },
    'windows7-32-devedition': {
      'virtual': 't-win7-32',
      'virtual-with-gpu': 't-win7-32-gpu',
      'hardware': 't-win10-64-hw',
    },
    'windows7-32-mingwclang': {
      'virtual': 't-win7-32',
      'virtual-with-gpu': 't-win7-32-gpu',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-aarch64': {
      'virtual': 't-win64-aarch64-laptop',
      'virtual-with-gpu': 't-win64-aarch64-laptop',
      'hardware': 't-win64-aarch64-laptop',
    },
    'windows10-64-ccov': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-devedition': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-shippable': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-asan': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-qr': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-shippable-qr': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-mingwclang': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-hw',
    },
    'windows10-64-ref-hw-2017': {
      'virtual': 't-win10-64',
      'virtual-with-gpu': 't-win10-64-gpu-s',
      'hardware': 't-win10-64-ref-hw',
    },
}

# os x worker types keyed by test-platform
MACOSX_WORKER_TYPES = {
    'macosx1014-64': 't-osx-1014',
    'macosx1014-64-power': 't-osx-1014-power'
}


def runs_on_central(test):
    return match_run_on_projects('mozilla-central', test['run-on-projects'])


def fission_filter(test):
    return (
        runs_on_central(test) and
        test.get('e10s') in (True, 'both') and
        get_mobile_project(test) != 'fennec'
    )


TEST_VARIANTS = {
    'fission': {
        'description': "{description} with fission enabled",
        'filterfn': fission_filter,
        'suffix': 'fis',
        'replace': {
            'e10s': True,
        },
        'merge': {
            # Ensures the default state is to not run anywhere.
            'fission-run-on-projects': [],
            'mozharness': {
                'extra-options': ['--setpref=fission.autostart=true',
                                  '--setpref=dom.serviceWorkers.parent_intercept=true',
                                  '--setpref=browser.tabs.documentchannel=true'],
            },
        },
    },
    'socketprocess': {
        'description': "{description} with socket process enabled",
        'suffix': 'spi',
        'merge': {
            'mozharness': {
                'extra-options': [
                    '--setpref=media.peerconnection.mtransport_process=true',
                    '--setpref=network.process.enabled=true',
                ],
            }
        }
    }
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
    'description': text_type,

    # test suite category and name
    Optional('suite'): Any(
        text_type,
        {Optional('category'): text_type, Optional('name'): text_type},
    ),

    # base work directory used to set up the task.
    Optional('workdir'): optionally_keyed_by(
        'test-platform',
        Any(text_type, 'default')),

    # the name by which this test suite is addressed in try syntax; defaults to
    # the test-name.  This will translate to the `unittest_try_name` or
    # `talos_try_name` attribute.
    Optional('try-name'): text_type,

    # additional tags to mark up this type of test
    Optional('tags'): {text_type: object},

    # the symbol, or group(symbol), under which this task should appear in
    # treeherder.
    'treeherder-symbol': text_type,

    # the value to place in task.extra.treeherder.machine.platform; ideally
    # this is the same as build-platform, and that is the default, but in
    # practice it's not always a match.
    Optional('treeherder-machine-platform'): text_type,

    # attributes to appear in the resulting task (later transforms will add the
    # common attributes)
    Optional('attributes'): {text_type: object},

    # relative path (from config.path) to the file task was defined in
    Optional('job-from'): text_type,

    # The `run_on_projects` attribute, defaulting to "all".  This dictates the
    # projects on which this task should be included in the target task set.
    # See the attributes documentation for details.
    #
    # Note that the special case 'built-projects', the default, uses the parent
    # build task's run-on-projects, meaning that tests run only on platforms
    # that are built.
    Optional('run-on-projects'): optionally_keyed_by(
        'test-platform',
        Any([text_type], 'built-projects')),

    # Same as `run-on-projects` except it only applies to Fission tasks. Fission
    # tasks will ignore `run_on_projects` and non-Fission tasks will ignore
    # `fission-run-on-projects`.
    Optional('fission-run-on-projects'): optionally_keyed_by(
        'test-platform',
        Any([text_type], 'built-projects')),

    # the sheriffing tier for this task (default: set based on test platform)
    Optional('tier'): optionally_keyed_by(
        'test-platform',
        Any(int, 'default')),

    # Same as `tier` except it only applies to Fission tasks. Fission tasks
    # will ignore `tier` and non-Fission tasks will ignore `fission-tier`.
    Optional('fission-tier'): optionally_keyed_by(
        'test-platform',
        Any(int, 'default')),

    # number of chunks to create for this task.  This can be keyed by test
    # platform by passing a dictionary in the `by-test-platform` key.  If the
    # test platform is not found, the key 'default' will be tried.
    Required('chunks'): optionally_keyed_by(
        'test-platform',
        int),

    # the time (with unit) after which this task is deleted; default depends on
    # the branch (see below)
    Optional('expires-after'): text_type,

    # The different configurations that should be run against this task, defined
    # in the TEST_VARIANTS object.
    Optional('variants'): optionally_keyed_by(
        'test-platform', 'project',
        Any(list(TEST_VARIANTS))),

    # Whether to run this task with e10s.  If false, run
    # without e10s; if true, run with e10s; if 'both', run one task with and
    # one task without e10s.  E10s tasks have "-e10s" appended to the test name
    # and treeherder group.
    Required('e10s'): optionally_keyed_by(
        'test-platform', 'project',
        Any(bool, 'both')),

    # Whether the task should run with WebRender enabled or not.
    Optional('webrender'): bool,

    # The EC2 instance size to run these tests on.
    Required('instance-size'): optionally_keyed_by(
        'test-platform',
        Any('default', 'large', 'xlarge')),

    # type of virtualization or hardware required by test.
    Required('virtualization'): optionally_keyed_by(
        'test-platform',
        Any('virtual', 'virtual-with-gpu', 'hardware')),

    # Whether the task requires loopback audio or video (whatever that may mean
    # on the platform)
    Required('loopback-audio'): bool,
    Required('loopback-video'): bool,

    # Whether the test can run using a software GL implementation on Linux
    # using the GL compositor. May not be used with "legacy" sized instances
    # due to poor LLVMPipe performance (bug 1296086).  Defaults to true for
    # unit tests on linux platforms and false otherwise
    Optional('allow-software-gl-layers'): bool,

    # For tasks that will run in docker-worker, this is the
    # name of the docker image or in-tree docker image to run the task in.  If
    # in-tree, then a dependency will be created automatically.  This is
    # generally `desktop-test`, or an image that acts an awful lot like it.
    Required('docker-image'): optionally_keyed_by(
        'test-platform',
        Any(
            # a raw Docker image path (repo/image:tag)
            text_type,
            # an in-tree generated docker image (from `taskcluster/docker/<name>`)
            {'in-tree': text_type},
            # an indexed docker image
            {'indexed': text_type},
        )
    ),

    # seconds of runtime after which the task will be killed.  Like 'chunks',
    # this can be keyed by test pltaform.
    Required('max-run-time'): optionally_keyed_by(
        'test-platform',
        int),

    # the exit status code that indicates the task should be retried
    Optional('retry-exit-status'): [int],

    # Whether to perform a gecko checkout.
    Required('checkout'): bool,

    # Wheter to perform a machine reboot after test is done
    Optional('reboot'):
        Any(False, 'always', 'on-exception', 'on-failure'),

    # What to run
    Required('mozharness'): {
        # the mozharness script used to run this task
        Required('script'): optionally_keyed_by(
            'test-platform',
            text_type),

        # the config files required for the task
        Required('config'): optionally_keyed_by(
            'test-platform',
            [text_type]),

        # mochitest flavor for mochitest runs
        Optional('mochitest-flavor'): text_type,

        # any additional actions to pass to the mozharness command
        Optional('actions'): [text_type],

        # additional command-line options for mozharness, beyond those
        # automatically added
        Required('extra-options'): optionally_keyed_by(
            'test-platform',
            [text_type]),

        # the artifact name (including path) to test on the build task; this is
        # generally set in a per-kind transformation
        Optional('build-artifact-name'): text_type,
        Optional('installer-url'): text_type,

        # If not false, tooltool downloads will be enabled via relengAPIProxy
        # for either just public files, or all files.  Not supported on Windows
        Required('tooltool-downloads'): Any(
            False,
            'public',
            'internal',
        ),

        # Add --blob-upload-branch=<project> mozharness parameter
        Optional('include-blob-upload-branch'): bool,

        # The setting for --download-symbols (if omitted, the option will not
        # be passed to mozharness)
        Optional('download-symbols'): Any(True, 'ondemand'),

        # If set, then MOZ_NODE_PATH=/usr/local/bin/node is included in the
        # environment.  This is more than just a helpful path setting -- it
        # causes xpcshell tests to start additional servers, and runs
        # additional tests.
        Required('set-moz-node-path'): bool,

        # If true, include chunking information in the command even if the number
        # of chunks is 1
        Required('chunked'): optionally_keyed_by(
            'test-platform',
            bool),

        Required('requires-signed-builds'): optionally_keyed_by(
            'test-platform',
            bool),
    },

    # The set of test manifests to run.
    Optional('test-manifests'): [text_type],

    # The current chunk (if chunking is enabled).
    Optional('this-chunk'): int,

    # os user groups for test task workers; required scopes, will be
    # added automatically
    Optional('os-groups'): optionally_keyed_by(
        'test-platform',
        [text_type]),

    Optional('run-as-administrator'): optionally_keyed_by(
        'test-platform',
        bool),

    # -- values supplied by the task-generation infrastructure

    # the platform of the build this task is testing
    'build-platform': text_type,

    # the label of the build task generating the materials to test
    'build-label': text_type,

    # the label of the signing task generating the materials to test.
    # Signed builds are used in xpcshell tests on Windows, for instance.
    Optional('build-signing-label'): text_type,

    # the build's attributes
    'build-attributes': {text_type: object},

    # the platform on which the tests will run
    'test-platform': text_type,

    # limit the test-platforms (as defined in test-platforms.yml)
    # that the test will run on
    Optional('limit-platforms'): optionally_keyed_by(
        'app',
        [text_type]
    ),

    # the name of the test (the key in tests.yml)
    'test-name': text_type,

    # the product name, defaults to firefox
    Optional('product'): text_type,

    # conditional files to determine when these tests should be run
    Exclusive(Optional('when'), 'optimization'): {
        Optional('files-changed'): [text_type],
    },

    # Optimization to perform on this task during the optimization phase.
    # Optimizations are defined in taskcluster/taskgraph/optimize.py.
    Exclusive(Optional('optimization'), 'optimization'): OptimizationSchema,

    # The SCHEDULES component for this task; this defaults to the suite
    # (not including the flavor) but can be overridden here.
    Exclusive(Optional('schedules-component'), 'optimization'): text_type,

    Optional('worker-type'): optionally_keyed_by(
        'test-platform',
        Any(text_type, None),
    ),

    Optional(
        'require-signed-extensions',
        description="Whether the build being tested requires extensions be signed.",
    ): optionally_keyed_by('release-type', 'test-platform', bool),

    # The target name, specifying the build artifact to be tested.
    # If None or not specified, a transform sets the target based on OS:
    # target.dmg (Mac), target.apk (Android), target.tar.bz2 (Linux),
    # or target.zip (Windows).
    Optional('target'): optionally_keyed_by(
        'test-platform',
        Any(text_type, None, {'index': text_type, 'name': text_type}),
    ),

    # A list of artifacts to install from 'fetch' tasks.
    Optional('fetches'): {
        text_type: optionally_keyed_by('test-platform', [text_type])
    },
}, required=True)


@transforms.add
def handle_keyed_by_mozharness(config, tests):
    """Resolve a mozharness field if it is keyed by something"""
    fields = [
        'mozharness',
        'mozharness.chunked',
        'mozharness.config',
        'mozharness.extra-options',
        'mozharness.requires-signed-builds',
        'mozharness.script',
    ]
    for test in tests:
        for field in fields:
            resolve_keyed_by(test, field, item_name=test['test-name'])
        yield test


@transforms.add
def set_defaults(config, tests):
    for test in tests:
        build_platform = test['build-platform']
        if build_platform.startswith('android'):
            # all Android test tasks download internal objects from tooltool
            test['mozharness']['tooltool-downloads'] = 'internal'
            test['mozharness']['actions'] = ['get-secrets']

            # loopback-video is always true for Android, but false for other
            # platform phyla
            test['loopback-video'] = True
        test['mozharness']['set-moz-node-path'] = True

        # software-gl-layers is only meaningful on linux unittests, where it defaults to True
        if test['test-platform'].startswith('linux') and test['suite'] not in ['talos', 'raptor']:
            test.setdefault('allow-software-gl-layers', True)
        else:
            test['allow-software-gl-layers'] = False

        # Enable WebRender by default on the QuantumRender test platforms, since
        # the whole point of QuantumRender is to run with WebRender enabled.
        # This currently matches linux64-qr and windows10-64-qr; both of these
        # have /opt and /debug variants.
        if "-qr/" in test['test-platform']:
            test['webrender'] = True
        else:
            test.setdefault('webrender', False)

        test.setdefault('e10s', True)
        test.setdefault('try-name', test['test-name'])
        test.setdefault('os-groups', [])
        test.setdefault('run-as-administrator', False)
        test.setdefault('chunks', 1)
        test.setdefault('run-on-projects', 'built-projects')
        test.setdefault('instance-size', 'default')
        test.setdefault('max-run-time', 3600)
        test.setdefault('reboot', False)
        test.setdefault('virtualization', 'virtual')
        test.setdefault('loopback-audio', False)
        test.setdefault('loopback-video', False)
        test.setdefault('limit-platforms', [])
        # Bug 1602863 - temporarily in place while ubuntu1604 and ubuntu1804
        # both exist in the CI.
        if ('linux1804' in test['test-platform']):
            test.setdefault('docker-image', {'in-tree': 'ubuntu1804-test'})
        else:
            test.setdefault('docker-image', {'in-tree': 'desktop1604-test'})
        test.setdefault('checkout', False)
        test.setdefault('require-signed-extensions', False)
        test.setdefault('variants', [])

        test['mozharness'].setdefault('extra-options', [])
        test['mozharness'].setdefault('requires-signed-builds', False)
        test['mozharness'].setdefault('tooltool-downloads', 'public')
        test['mozharness'].setdefault('set-moz-node-path', False)
        test['mozharness'].setdefault('chunked', False)
        yield test


@transforms.add
def resolve_keys(config, tests):
    for test in tests:
        resolve_keyed_by(
            test, 'require-signed-extensions',
            item_name=test['test-name'],
            **{
                'release-type': config.params['release_type'],
            }
        )
        yield test


@transforms.add
def setup_raptor(config, tests):
    """Add options that are specific to raptor jobs (identified by suite=raptor)"""
    from taskgraph.transforms.raptor import transforms as raptor_transforms

    for test in tests:
        if test['suite'] != 'raptor':
            yield test
            continue

        for t in raptor_transforms(config, [test]):
            yield t


@transforms.add
def limit_platforms(config, tests):
    for test in tests:
        if not test['limit-platforms']:
            yield test
            continue

        limited_platforms = {key: key for key in test['limit-platforms']}
        if keymatch(limited_platforms, test['test-platform']):
            yield test


transforms.add_validate(test_description_schema)


@transforms.add
def handle_suite_category(config, tests):
    for test in tests:
        test.setdefault('suite', {})

        if isinstance(test['suite'], text_type):
            test['suite'] = {'name': test['suite']}

        suite = test['suite'].setdefault('name', test['test-name'])
        category = test['suite'].setdefault('category', suite)

        test.setdefault('attributes', {})
        test['attributes']['unittest_suite'] = suite
        test['attributes']['unittest_category'] = category

        script = test['mozharness']['script']
        category_arg = None
        if suite.startswith('test-verify') or suite.startswith('test-coverage'):
            pass
        elif script in ('android_emulator_unittest.py', 'android_hardware_unittest.py'):
            category_arg = '--test-suite'
        elif script == 'desktop_unittest.py':
            category_arg = '--{}-suite'.format(category)

        if category_arg:
            test['mozharness'].setdefault('extra-options', [])
            extra = test['mozharness']['extra-options']
            if not any(arg.startswith(category_arg) for arg in extra):
                extra.append('{}={}'.format(category_arg, suite))

        # From here on out we only use the suite name.
        test['suite'] = suite
        yield test


@transforms.add
def setup_talos(config, tests):
    """Add options that are specific to talos jobs (identified by suite=talos)"""
    for test in tests:
        if test['suite'] != 'talos':
            yield test
            continue

        extra_options = test.setdefault('mozharness', {}).setdefault('extra-options', [])
        extra_options.append('--use-talos-json')

        # win7 needs to test skip
        if test['build-platform'].startswith('win32'):
            extra_options.append('--add-option')
            extra_options.append('--setpref,gfx.direct2d.disabled=true')

        yield test


@transforms.add
def setup_browsertime_flag(config, tests):
    """Optionally add `--browsertime` flag to Raptor pageload tests."""

    browsertime_flag = config.params['try_task_config'].get('browsertime', False)

    for test in tests:
        if not browsertime_flag or test['suite'] != 'raptor':
            yield test
            continue

        if test['treeherder-symbol'].startswith('Rap'):
            # The Rap group is subdivided as Rap{-fenix,-refbrow,-fennec}(...),
            # so `taskgraph.util.treeherder.replace_group` isn't appropriate.
            test['treeherder-symbol'] = test['treeherder-symbol'].replace('Rap', 'Btime', 1)

        extra_options = test.setdefault('mozharness', {}).setdefault('extra-options', [])
        extra_options.append('--browsertime')

        yield test


@transforms.add
def handle_artifact_prefix(config, tests):
    """Handle translating `artifact_prefix` appropriately"""
    for test in tests:
        if test['build-attributes'].get('artifact_prefix'):
            test.setdefault("attributes", {}).setdefault(
                'artifact_prefix', test['build-attributes']['artifact_prefix']
            )
        yield test


@transforms.add
def set_target(config, tests):
    for test in tests:
        build_platform = test['build-platform']
        target = None
        if 'target' in test:
            resolve_keyed_by(test, 'target', item_name=test['test-name'])
            target = test['target']
        if not target:
            if build_platform.startswith('macosx'):
                target = 'target.dmg'
            elif build_platform.startswith('android'):
                target = 'target.apk'
            elif build_platform.startswith('win'):
                target = 'target.zip'
            else:
                target = 'target.tar.bz2'

        if isinstance(target, dict):
            # TODO Remove hardcoded mobile artifact prefix
            index_url = get_index_url(target['index'])
            installer_url = '{}/artifacts/public/{}'.format(index_url, target['name'])
            test['mozharness']['installer-url'] = installer_url
        else:
            test['mozharness']['build-artifact-name'] = get_artifact_path(test, target)

        yield test


@transforms.add
def set_treeherder_machine_platform(config, tests):
    """Set the appropriate task.extra.treeherder.machine.platform"""
    translation = {
        # Linux64 build platforms for asan and pgo are specified differently to
        # treeherder.
        'linux64-pgo/opt': 'linux64/pgo',
        'macosx1014-64/debug': 'osx-10-14/debug',
        'macosx1014-64/opt': 'osx-10-14/opt',
        'macosx1014-64-shippable/opt': 'osx-10-14-shippable/opt',
        'win64-asan/opt': 'windows10-64/asan',
        'win64-aarch64/opt': 'windows10-aarch64/opt',
        'win32-pgo/opt': 'windows7-32/pgo',
        'win64-pgo/opt': 'windows10-64/pgo',
    }
    for test in tests:
        # For most desktop platforms, the above table is not used for "regular"
        # builds, so we'll always pick the test platform here.
        # On macOS though, the regular builds are in the table.  This causes a
        # conflict in `verify_task_graph_symbol` once you add a new test
        # platform based on regular macOS builds, such as for QR.
        # Since it's unclear if the regular macOS builds can be removed from
        # the table, workaround the issue for QR.
        if 'android' in test['test-platform'] and 'pgo/opt' in test['test-platform']:
            platform_new = test['test-platform'].replace('-pgo/opt', '/pgo')
            test['treeherder-machine-platform'] = platform_new
        elif 'android-em-7.0-x86_64-qr' in test['test-platform']:
            opt = test['test-platform'].split('/')[1]
            test['treeherder-machine-platform'] = 'android-em-7-0-x86_64-qr/'+opt
        elif '-qr' in test['test-platform']:
            test['treeherder-machine-platform'] = test['test-platform']
        elif 'android-hw' in test['test-platform']:
            test['treeherder-machine-platform'] = test['test-platform']
        elif 'android-em-7.0-x86_64' in test['test-platform']:
            opt = test['test-platform'].split('/')[1]
            test['treeherder-machine-platform'] = 'android-em-7-0-x86_64/'+opt
        elif 'android-em-7.0-x86' in test['test-platform']:
            opt = test['test-platform'].split('/')[1]
            test['treeherder-machine-platform'] = 'android-em-7-0-x86/'+opt
        # Bug 1602863 - must separately define linux64/asan and linux1804-64/asan
        # otherwise causes an exception during taskgraph generation about
        # duplicate treeherder platform/symbol.
        elif 'linux64-asan/opt' in test['test-platform']:
            test['treeherder-machine-platform'] = 'linux64/asan'
        elif 'linux1804-asan/opt' in test['test-platform']:
            test['treeherder-machine-platform'] = 'linux1804-64/asan'
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

        if 'fission-tier' in test:
            resolve_keyed_by(test, 'fission-tier', item_name=test['test-name'])

        # only override if not set for the test
        if 'tier' not in test or test['tier'] == 'default':
            if test['test-platform'] in [
                'linux64/opt',
                'linux64-nightly/opt',
                'linux64/debug',
                'linux64-pgo/opt',
                'linux64-shippable/opt',
                'linux64-devedition/opt',
                'linux64-asan/opt',
                'linux64-qr/opt',
                'linux64-qr/debug',
                'linux64-pgo-qr/opt',
                'linux64-shippable-qr/opt',
                'linux1804-64/opt',
                'linux1804-64/debug',
                'linux1804-64-shippable/opt',
                'linux1804-64-qr/opt',
                'linux1804-64-qr/debug',
                'linux1804-64-shippable-qr/opt',
                'linux1804-64-asan/opt',
                'linux1804-64-tsan/opt',
                'windows7-32/debug',
                'windows7-32/opt',
                'windows7-32-pgo/opt',
                'windows7-32-devedition/opt',
                'windows7-32-nightly/opt',
                'windows7-32-shippable/opt',
                'windows10-aarch64/opt',
                'windows10-64/debug',
                'windows10-64/opt',
                'windows10-64-pgo/opt',
                'windows10-64-shippable/opt',
                'windows10-64-devedition/opt',
                'windows10-64-nightly/opt',
                'windows10-64-asan/opt',
                'windows10-64-qr/opt',
                'windows10-64-qr/debug',
                'windows10-64-pgo-qr/opt',
                'windows10-64-shippable-qr/opt',
                'macosx1014-64/opt',
                'macosx1014-64/debug',
                'macosx1014-64-nightly/opt',
                'macosx1014-64-shippable/opt',
                'macosx1014-64-devedition/opt',
                'macosx1014-64-qr/opt',
                'macosx1014-64-shippable-qr/opt',
                'macosx1014-64-qr/debug',
                'android-em-7.0-x86_64/opt',
                'android-em-7.0-x86_64/debug',
                'android-em-7.0-x86/opt',
                'android-em-7.0-x86_64-qr/opt',
                'android-em-7.0-x86_64-qr/debug'
            ]:
                test['tier'] = 1
            else:
                test['tier'] = 2

        yield test


@transforms.add
def set_expires_after(config, tests):
    """Try jobs expire after 2 weeks; everything else lasts 1 year.  This helps
    keep storage costs low."""
    for test in tests:
        if 'expires-after' not in test:
            if config.params.is_try():
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
        'variants',
        'e10s',
        'suite',
        'run-on-projects',
        'fission-run-on-projects',
        'os-groups',
        'run-as-administrator',
        'workdir',
        'worker-type',
        'virtualization',
        'fetches.fetch',
        'fetches.toolchain',
        'target',
    ]
    for test in tests:
        for field in fields:
            resolve_keyed_by(test, field, item_name=test['test-name'],
                             project=config.params['project'])
        yield test


@transforms.add
def setup_browsertime(config, tests):
    """Configure browsertime dependencies for Raptor pageload tests that have
`--browsertime` extra option."""

    for test in tests:
        # We need to make non-trivial changes to various fetches, and our
        # `by-test-platform` may not be "compatible" with existing
        # `by-test-platform` filters.  Therefore we do everything after
        # `handle_keyed_by` so that existing fields have been resolved down to
        # simple lists.  But we use the `by-test-platform` machinery to express
        # filters so that when the time comes to move browsertime into YAML
        # files, the transition is straight-forward.
        extra_options = test.get('mozharness', {}).get('extra-options', [])

        if test['suite'] != 'raptor' or '--browsertime' not in extra_options:
            yield test
            continue

        # This is appropriate as the browsertime task variants mature.
        test['tier'] = max(test['tier'], 2)

        ts = {
            'by-test-platform': {
                'android.*': ['browsertime', 'linux64-geckodriver', 'linux64-node'],
                'linux.*': ['browsertime', 'linux64-geckodriver', 'linux64-node'],
                'macosx.*': ['browsertime', 'macosx64-geckodriver', 'macosx64-node'],
                'windows.*aarch64.*': ['browsertime', 'win32-geckodriver', 'win32-node'],
                'windows.*-32.*': ['browsertime', 'win32-geckodriver', 'win32-node'],
                'windows.*-64.*': ['browsertime', 'win64-geckodriver', 'win64-node'],
            },
        }

        test.setdefault('fetches', {}).setdefault('toolchain', []).extend(
            evaluate_keyed_by(ts, 'fetches.toolchain', test))

        fs = {
            'by-test-platform': {
                'android.*': ['linux64-ffmpeg-4.1.4'],
                'linux.*': ['linux64-ffmpeg-4.1.4'],
                'macosx.*': ['mac64-ffmpeg-4.1.1'],
                'windows.*aarch64.*': ['win64-ffmpeg-4.1.1'],
                'windows.*-32.*': ['win64-ffmpeg-4.1.1'],
                'windows.*-64.*': ['win64-ffmpeg-4.1.1'],
            },
        }

        cd_fetches = {
            'android.*': [
                'linux64-chromedriver-80',
                'linux64-chromedriver-81'
            ],
            'linux.*': [
                'linux64-chromedriver-79',
                'linux64-chromedriver-80',
                'linux64-chromedriver-81'
            ],
            'macosx.*': [
                'mac64-chromedriver-79',
                'mac64-chromedriver-80',
                'mac64-chromedriver-81'
            ],
            'windows.*aarch64.*': [
                'win32-chromedriver-79',
                'win32-chromedriver-80',
                'win32-chromedriver-81'
            ],
            'windows.*-32.*': [
                'win32-chromedriver-79',
                'win32-chromedriver-80',
                'win32-chromedriver-81'
            ],
            'windows.*-64.*': [
                'win32-chromedriver-79',
                'win32-chromedriver-80',
                'win32-chromedriver-81'
            ],
        }

        if '--app=chrome' in extra_options \
           or '--app=chromium' in extra_options \
           or '--app=chrome-m' in extra_options:
            # Only add the chromedriver fetches when chrome/chromium is running
            for platform in cd_fetches:
                fs['by-test-platform'][platform].extend(cd_fetches[platform])

        # Disable the Raptor install step
        if '--app=chrome-m' in extra_options:
            extra_options.append('--noinstall')

        test.setdefault('fetches', {}).setdefault('fetch', []).extend(
            evaluate_keyed_by(fs, 'fetches.fetch', test))

        extra_options.extend(('--browsertime-browsertimejs',
                              '$MOZ_FETCHES_DIR/browsertime/node_modules/browsertime/bin/browsertime.js'))  # noqa: E501

        eos = {
            'by-test-platform': {
                'windows.*':
                ['--browsertime-node',
                 '$MOZ_FETCHES_DIR/node/node.exe',
                 '--browsertime-geckodriver',
                 '$MOZ_FETCHES_DIR/geckodriver.exe',
                 '--browsertime-chromedriver',
                 '$MOZ_FETCHES_DIR/{}chromedriver.exe',
                 '--browsertime-ffmpeg',
                 '$MOZ_FETCHES_DIR/ffmpeg-4.1.1-win64-static/bin/ffmpeg.exe',
                 ],
                'macosx.*':
                ['--browsertime-node',
                 '$MOZ_FETCHES_DIR/node/bin/node',
                 '--browsertime-geckodriver',
                 '$MOZ_FETCHES_DIR/geckodriver',
                 '--browsertime-chromedriver',
                 '$MOZ_FETCHES_DIR/{}chromedriver',
                 '--browsertime-ffmpeg',
                 '$MOZ_FETCHES_DIR/ffmpeg-4.1.1-macos64-static/bin/ffmpeg',
                 ],
                'default':
                ['--browsertime-node',
                 '$MOZ_FETCHES_DIR/node/bin/node',
                 '--browsertime-geckodriver',
                 '$MOZ_FETCHES_DIR/geckodriver',
                 '--browsertime-chromedriver',
                 '$MOZ_FETCHES_DIR/{}chromedriver',
                 '--browsertime-ffmpeg',
                 '$MOZ_FETCHES_DIR/ffmpeg-4.1.4-i686-static/ffmpeg',
                 ],
            }
        }

        extra_options.extend(evaluate_keyed_by(eos, 'mozharness.extra-options', test))

        yield test


def get_mobile_project(test):
    """Returns the mobile project of the specified task or None."""

    if not test['build-platform'].startswith('android'):
        return

    mobile_projects = (
        'fenix',
        'fennec',
        'geckoview',
        'refbrow',
        'chrome-m'
    )

    for name in mobile_projects:
        if name in test['test-name']:
            return name

    target = test.get('target')
    if target:
        if isinstance(target, dict):
            target = target['name']

        for name in mobile_projects:
            if name in target:
                return name

    return 'fennec'


@transforms.add
def disable_fennec_e10s(config, tests):
    for test in tests:
        if get_mobile_project(test) == 'fennec':
            # Fennec is non-e10s
            test['e10s'] = False
        yield test


@transforms.add
def enable_code_coverage(config, tests):
    """Enable code coverage for the ccov build-platforms"""
    for test in tests:
        if 'ccov' in test['build-platform']:
            # Do not run tests on fuzzing builds
            if 'fuzzing' in test['build-platform']:
                test['run-on-projects'] = []
                continue

            # Skip this transform for android code coverage builds.
            if 'android' in test['build-platform']:
                test.setdefault('fetches', {}).setdefault('toolchain', []).append('linux64-grcov')
                test['mozharness'].setdefault('extra-options', []).append('--java-code-coverage')
                yield test
                continue
            test['mozharness'].setdefault('extra-options', []).append('--code-coverage')
            test['instance-size'] = 'xlarge'

            # Temporarily disable Mac tests on mozilla-central
            if 'mac' in test['build-platform']:
                test['run-on-projects'] = ['try']

            # Ensure we always run on the projects defined by the build, unless the test
            # is try only or shouldn't run at all.
            if test['run-on-projects'] not in [[], ['try']]:
                test['run-on-projects'] = 'built-projects'

            # Ensure we don't optimize test suites out.
            # We always want to run all test suites for coverage purposes.
            test.pop('schedules-component', None)
            test.pop('when', None)
            test['optimization'] = None

            # Add a toolchain and a fetch task for the grcov binary.
            if any(p in test['build-platform'] for p in ('linux', 'osx', 'win')):
                test.setdefault('fetches', {})
                test['fetches'].setdefault('fetch', [])
                test['fetches'].setdefault('toolchain', [])

            if 'linux' in test['build-platform']:
                test['fetches']['toolchain'].append('linux64-grcov')
            elif 'osx' in test['build-platform']:
                test['fetches']['fetch'].append('grcov-osx-x86_64')
            elif 'win' in test['build-platform']:
                test['fetches']['toolchain'].append('win64-grcov')

            if 'talos' in test['test-name']:
                test['max-run-time'] = 7200
                if 'linux' in test['build-platform']:
                    test['docker-image'] = {"in-tree": "ubuntu1804-test"}
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--cycles,1')
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--tppagecycles,1')
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--no-upload-results')
                test['mozharness']['extra-options'].append('--add-option')
                test['mozharness']['extra-options'].append('--tptimeout,15000')
            if 'raptor' in test['test-name']:
                test['max-run-time'] = 1800
                if 'linux' in test['build-platform']:
                    test['docker-image'] = {"in-tree": "desktop1604-test"}
        yield test


@transforms.add
def handle_run_on_projects(config, tests):
    """Handle translating `built-projects` appropriately"""
    for test in tests:
        if test['run-on-projects'] == 'built-projects':
            test['run-on-projects'] = test['build-attributes'].get('run_on_projects', ['all'])
        yield test


@transforms.add
def split_variants(config, tests):
    for test in tests:
        variants = test.pop('variants', [])

        yield copy.deepcopy(test)

        for name in variants:
            testv = copy.deepcopy(test)
            variant = TEST_VARIANTS[name]

            if 'filterfn' in variant and not variant['filterfn'](testv):
                continue

            testv['attributes']['unittest_variant'] = name
            testv['description'] = variant['description'].format(**testv)

            suffix = '-' + variant['suffix']
            testv['test-name'] += suffix
            testv['try-name'] += suffix

            group, symbol = split_symbol(testv['treeherder-symbol'])
            if group != '?':
                group += suffix
            else:
                symbol += suffix
            testv['treeherder-symbol'] = join_symbol(group, symbol)

            testv.update(variant.get('replace', {}))

            if test['suite'] == 'raptor':
                testv['tier'] = max(testv['tier'], 2)

            yield merge(testv, variant.get('merge', {}))


@transforms.add
def handle_fission_attributes(config, tests):
    """Handle run_on_projects for fission tasks."""
    for test in tests:
        for attr in ('run-on-projects', 'tier'):
            fission_attr = test.pop('fission-{}'.format(attr), None)

            if test['attributes'].get('unittest_variant') != 'fission' or fission_attr is None:
                continue

            test[attr] = fission_attr

        yield test


@transforms.add
def ensure_spi_disabled_on_all_but_spi(config, tests):
    for test in tests:
        variant = test['attributes'].get('unittest_variant', '')
        has_setpref = ('gtest' not in test['suite'] and
                       'cppunit' not in test['suite'] and
                       'jittest' not in test['suite'] and
                       'junit' not in test['suite'] and
                       'raptor' not in test['suite'])

        if has_setpref and variant != 'socketprocess':
            test['mozharness']['extra-options'].append(
                    '--setpref=media.peerconnection.mtransport_process=false')
            test['mozharness']['extra-options'].append(
                    '--setpref=network.process.enabled=false')

        yield test


@transforms.add
def split_e10s(config, tests):
    for test in tests:
        e10s = test['e10s']

        if e10s:
            test_copy = copy.deepcopy(test)
            test_copy['test-name'] += '-e10s'
            test_copy['e10s'] = True
            test_copy['attributes']['e10s'] = True
            yield test_copy

        if not e10s or e10s == 'both':
            test['test-name'] += '-1proc'
            test['try-name'] += '-1proc'
            test['e10s'] = False
            test['attributes']['e10s'] = False
            group, symbol = split_symbol(test['treeherder-symbol'])
            if group != '?':
                group += '-1proc'
            test['treeherder-symbol'] = join_symbol(group, symbol)
            test['mozharness']['extra-options'].append('--disable-e10s')
            yield test


CHUNK_SUITES_BLACKLIST = (
    'awsy',
    'cppunittest',
    'crashtest',
    'firefox-ui-functional-local',
    'firefox-ui-functional-remote',
    'geckoview-junit',
    'gtest',
    'jittest',
    'jsreftest',
    'marionette',
    'mochitest-browser-chrome-screenshots',
    'mochitest-browser-chrome-thunderbird',
    'mochitest-valgrind-plain',
    'mochitest-webgl1-core',
    'mochitest-webgl1-ext',
    'mochitest-webgl2-core',
    'mochitest-webgl2-ext',
    'raptor',
    'reftest',
    'reftest-qr',
    'reftest-gpu',
    'reftest-no-accel',
    'talos',
    'telemetry-tests-client',
    'test-coverage',
    'test-coverage-wpt',
    'test-verify',
    'test-verify-gpu',
    'test-verify-wpt',
    'web-platform-tests',
    'web-platform-tests-crashtest',
    'web-platform-tests-reftest',
    'web-platform-tests-reftest-backlog',
    'web-platform-tests-wdspec',
)
"""These suites will be chunked at test runtime rather than here in the taskgraph."""


@transforms.add
def split_chunks(config, tests):
    """Based on the 'chunks' key, split tests up into chunks by duplicating
    them and assigning 'this-chunk' appropriately and updating the treeherder
    symbol."""

    for test in tests:
        if test['suite'].startswith('test-verify') or \
           test['suite'].startswith('test-coverage'):
            env = config.params.get('try_task_config', {}) or {}
            env = env.get('templates', {}).get('env', {})
            test['chunks'] = perfile_number_of_chunks(config.params.is_try(),
                                                      env.get('MOZHARNESS_TEST_PATHS', ''),
                                                      config.params.get('head_repository', ''),
                                                      config.params.get('head_rev', ''),
                                                      test['test-name'])

            # limit the number of chunks we run for test-verify mode because
            # test-verify is comprehensive and takes a lot of time, if we have
            # >30 tests changed, this is probably an import of external tests,
            # or a patch renaming/moving files in bulk
            maximum_number_verify_chunks = 3
            if test['chunks'] > maximum_number_verify_chunks:
                test['chunks'] = maximum_number_verify_chunks

        chunked_manifests = None
        if not taskgraph.fast and test['suite'] not in CHUNK_SUITES_BLACKLIST:
            suite_definition = TEST_SUITES[test['suite']]
            mozinfo = guess_mozinfo_from_task(test)
            chunked_manifests = get_chunked_manifests(
                suite_definition['build_flavor'],
                suite_definition.get('kwargs', {}).get('subsuite', 'undefined'),
                test['chunks'],
                frozenset(mozinfo.items()),
            )

        for i in range(test['chunks']):
            this_chunk = i + 1

            # copy the test and update with the chunk number
            chunked = copy.deepcopy(test)
            chunked['this-chunk'] = this_chunk

            if chunked_manifests is not None:
                manifests = sorted(chunked_manifests[i])
                if not manifests:
                    print(chunked_manifests)
                    raise Exception(
                        'Chunking algorithm yielded no manifests for chunk {} of {} on {}'.format(
                            this_chunk, test['test-name'], test['test-platform']))
                    chunked['test-manifests'] = manifests

            if test['chunks'] > 1:
                # add the chunk number to the TH symbol
                chunked['treeherder-symbol'] = add_suffix(
                    chunked['treeherder-symbol'], this_chunk)

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
            extra_options = test['mozharness'].setdefault('extra-options', [])
            extra_options.append("--enable-webrender")
            # We only want to 'setpref' on tests that have a profile
            if not test['attributes']['unittest_category'] in ['cppunittest', 'gtest', 'raptor']:
                extra_options.append("--setpref=layers.d3d11.enable-blacklist=false")

        yield test


@transforms.add
def set_schedules_for_webrender_android(config, tests):
    """android-hw has limited resources, we need webrender on phones"""
    for test in tests:
        if test['suite'] in ['crashtest', 'reftest'] and \
           test['test-platform'].startswith('android-hw'):
            test['schedules-component'] = 'android-hw-gfx'
        yield test


@transforms.add
def set_retry_exit_status(config, tests):
    """Set the retry exit status to TBPL_RETRY, the value returned by mozharness
       scripts to indicate a transient failure that should be retried."""
    for test in tests:
        test['retry-exit-status'] = [4]
        yield test


@transforms.add
def set_profile(config, tests):
    """Set profiling mode for tests."""
    profile = config.params['try_task_config'].get('gecko-profile', False)

    for test in tests:
        if profile and test['suite'] in ['talos', 'raptor']:
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
    types = ['mochitest', 'reftest', 'talos', 'raptor', 'geckoview-junit', 'gtest']
    for test in tests:
        for test_type in types:
            if test_type in test['suite'] and 'web-platform' not in test['suite']:
                test.setdefault('tags', {})['test-type'] = test_type
        yield test


@transforms.add
def set_worker_type(config, tests):
    """Set the worker type based on the test platform."""
    for test in tests:
        # during the taskcluster migration, this is a bit tortured, but it
        # will get simpler eventually!
        test_platform = test['test-platform']
        if test.get('worker-type'):
            # This test already has its worker type defined, so just use that (yields below)
            pass
        elif test_platform.startswith('macosx1014-64'):
            if '--power-test' in test['mozharness']['extra-options']:
                test['worker-type'] = MACOSX_WORKER_TYPES['macosx1014-64-power']
            else:
                test['worker-type'] = MACOSX_WORKER_TYPES['macosx1014-64']
        elif test_platform.startswith('win'):
            # figure out what platform the job needs to run on
            if test['virtualization'] == 'hardware':
                # some jobs like talos and reftest run on real h/w - those are all win10
                if test_platform.startswith('windows10-64-ref-hw-2017'):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES['windows10-64-ref-hw-2017']
                elif test_platform.startswith('windows10-aarch64'):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES['windows10-aarch64']
                else:
                    win_worker_type_platform = WINDOWS_WORKER_TYPES['windows10-64']
            else:
                # the other jobs run on a vm which may or may not be a win10 vm
                win_worker_type_platform = WINDOWS_WORKER_TYPES[
                    test_platform.split('/')[0]
                ]
            # now we have the right platform set the worker type accordingly
            test['worker-type'] = win_worker_type_platform[test['virtualization']]
        elif test_platform.startswith('android-hw-g5'):
            if test['suite'] != 'raptor':
                test['worker-type'] = 't-bitbar-gw-unit-g5'
            else:
                test['worker-type'] = 't-bitbar-gw-perf-g5'
        elif test_platform.startswith('android-hw-p2'):
            if test['suite'] != 'raptor':
                test['worker-type'] = 't-bitbar-gw-unit-p2'
            else:
                test['worker-type'] = 't-bitbar-gw-perf-p2'
        elif test_platform.startswith('android-em-7.0-x86'):
            test['worker-type'] = 'terraform-packet/gecko-t-linux'
        elif test_platform.startswith('linux') or test_platform.startswith('android'):
            if test.get('suite', '') in ['talos', 'raptor'] and \
                 not test['build-platform'].startswith('linux64-ccov'):
                test['worker-type'] = 't-linux-talos'
            else:
                test['worker-type'] = LINUX_WORKER_TYPES[test['instance-size']]
        else:
            raise Exception("unknown test_platform {}".format(test_platform))

        yield test


@transforms.add
def make_job_description(config, tests):
    """Convert *test* descriptions to *job* descriptions (input to
    taskgraph.transforms.job)"""

    for test in tests:
        mobile = get_mobile_project(test)
        if mobile and (mobile not in test['test-name']):
            label = '{}-{}-{}-{}'.format(config.kind, test['test-platform'], mobile,
                                         test['test-name'])
        else:
            label = '{}-{}-{}'.format(config.kind, test['test-platform'], test['test-name'])
        if test['chunks'] > 1:
            label += '-{}'.format(test['this-chunk'])

        build_label = test['build-label']

        try_name = test['try-name']
        if test['suite'] == 'talos':
            attr_try_name = 'talos_try_name'
        elif test['suite'] == 'raptor':
            attr_try_name = 'raptor_try_name'
        else:
            attr_try_name = 'unittest_try_name'

        attr_build_platform, attr_build_type = test['build-platform'].split('/', 1)

        attributes = test.get('attributes', {})
        attributes.update({
            'build_platform': attr_build_platform,
            'build_type': attr_build_type,
            'test_platform': test['test-platform'],
            'test_chunk': str(test['this-chunk']),
            'test_manifests': test.get('test-manifests'),
            attr_try_name: try_name,
        })

        jobdesc = {}
        name = '{}-{}'.format(test['test-platform'], test['test-name'])
        jobdesc['name'] = name
        jobdesc['label'] = label
        jobdesc['description'] = test['description']
        jobdesc['attributes'] = attributes
        jobdesc['dependencies'] = {'build': build_label}
        jobdesc['job-from'] = test['job-from']

        if test.get('fetches'):
            jobdesc['fetches'] = test['fetches']

        if test['mozharness']['requires-signed-builds'] is True:
            jobdesc['dependencies']['build-signing'] = test['build-signing-label']

        jobdesc['expires-after'] = test['expires-after']
        jobdesc['routes'] = []
        jobdesc['run-on-projects'] = sorted(test['run-on-projects'])
        jobdesc['scopes'] = []
        jobdesc['tags'] = test.get('tags', {})
        jobdesc['extra'] = {
            'chunks': {
                'current': test['this-chunk'],
                'total': test['chunks'],
            },
            'suite': attributes['unittest_suite'],
        }
        jobdesc['treeherder'] = {
            'symbol': test['treeherder-symbol'],
            'kind': 'test',
            'tier': test['tier'],
            'platform': test.get('treeherder-machine-platform', test['build-platform']),
        }

        category = test.get('schedules-component', attributes['unittest_category'])
        if category in INCLUSIVE_COMPONENTS:
            # if this is an "inclusive" test, then all files which might
            # cause it to run are annotated with SCHEDULES in moz.build,
            # so do not include the platform or any other components here
            schedules = [category]
        else:
            schedules = [attributes['unittest_category'], platform_family(test['build-platform'])]
            component = test.get('schedules-component')
            if component:
                schedules.append(component)

        if test.get('when'):
            # This may still be used by comm-central.
            jobdesc['when'] = test['when']
        elif 'optimization' in test:
            jobdesc['optimization'] = test['optimization']
        # Pushes generated by `mach try auto` should use the non-try optimizations.
        elif config.params.is_try() and config.params['try_mode'] != 'try_auto':
            jobdesc['optimization'] = {'test-try': schedules}
        elif category in INCLUSIVE_COMPONENTS:
            jobdesc['optimization'] = {'test-inclusive': schedules}
        else:
            jobdesc['optimization'] = {'test': schedules}

        run = jobdesc['run'] = {}
        run['using'] = 'mozharness-test'
        run['test'] = test

        if 'workdir' in test:
            run['workdir'] = test.pop('workdir')

        jobdesc['worker-type'] = test.pop('worker-type')
        if test.get('fetches'):
            jobdesc['fetches'] = test.pop('fetches')

        yield jobdesc


def normpath(path):
    return path.replace('/', '\\')


def get_firefox_version():
    with open('browser/config/version.txt', 'r') as f:
        return f.readline().strip()
