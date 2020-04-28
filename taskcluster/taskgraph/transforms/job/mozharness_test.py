# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os

import six
from six import text_type
from voluptuous import Required

from taskgraph.util.taskcluster import get_artifact_url
from taskgraph.transforms.job import (
    configure_taskdesc_for_run,
    run_job_using,
)
from taskgraph.util.schema import Schema
from taskgraph.util.taskcluster import get_artifact_path
from taskgraph.transforms.tests import (
    test_description_schema,
    normpath
)
from taskgraph.transforms.job.common import (
    support_vcs_checkout,
)

VARIANTS = [
    'nightly',
    'shippable',
    'devedition',
    'pgo',
    'asan',
    'stylo',
    'qr',
    'ccov',
]


def get_variant(test_platform):
    for v in VARIANTS:
        if '-{}/'.format(v) in test_platform:
            return v
    return ''


mozharness_test_run_schema = Schema({
    Required('using'): 'mozharness-test',
    Required('test'): test_description_schema,
    # Base work directory used to set up the task.
    Required('workdir'): text_type,
})


def test_packages_url(taskdesc):
    """Account for different platforms that name their test packages differently"""
    artifact_url = get_artifact_url('<build>', get_artifact_path(taskdesc,
                                    'target.test_packages.json'))
    # for android nightly we need to add 'en-US' to the artifact url
    test = taskdesc['run']['test']
    if 'android' in test['test-platform'] and (
            get_variant(test['test-platform']) in ("nightly", 'shippable')):
        head, tail = os.path.split(artifact_url)
        artifact_url = os.path.join(head, 'en-US', tail)
    return artifact_url


@run_job_using('docker-worker', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_docker(config, job, taskdesc):
    run = job['run']
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker'] = job['worker']

    # apply some defaults
    worker['docker-image'] = test['docker-image']
    worker['allow-ptrace'] = True  # required for all tests, for crashreporter
    worker['loopback-video'] = test['loopback-video']
    worker['loopback-audio'] = test['loopback-audio']
    worker['max-run-time'] = test['max-run-time']
    worker['retry-exit-status'] = test['retry-exit-status']
    if 'android-em-7.0-x86' in test['test-platform']:
        worker['privileged'] = True

    artifacts = [
        # (artifact name prefix, in-image path)
        ("public/logs/", "{workdir}/workspace/logs/".format(**run)),
        ("public/test", "{workdir}/artifacts/".format(**run)),
        ("public/test_info/", "{workdir}/workspace/build/blobber_upload_dir/".format(**run)),
    ]

    if 'installer-url' in mozharness:
        installer_url = mozharness['installer-url']
    else:
        installer_url = get_artifact_url('<build>', mozharness['build-artifact-name'])

    mozharness_url = get_artifact_url('<build>',
                                      get_artifact_path(taskdesc, 'mozharness.zip'))

    worker['artifacts'] = [{
        'name': prefix,
        'path': os.path.join('{workdir}/workspace'.format(**run), path),
        'type': 'directory',
    } for (prefix, path) in artifacts]

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
        'MOZHARNESS_SCRIPT': mozharness['script'],
        'MOZILLA_BUILD_URL': {'task-reference': installer_url},
        'NEED_PULSEAUDIO': 'true',
        'NEED_WINDOW_MANAGER': 'true',
        'ENABLE_E10S': text_type(bool(test.get('e10s'))).lower(),
        'WORKING_DIR': '/builds/worker',
    })

    # Legacy linux64 tests rely on compiz.
    if test.get('docker-image', {}).get('in-tree') == 'desktop1604-test':
        env.update({
            'NEED_COMPIZ': 'true'
        })

    # Bug 1602701/1601828 - use compiz on ubuntu1804 due to GTK asynchiness
    # when manipulating windows.
    if test.get('docker-image', {}).get('in-tree') == 'ubuntu1804-test':
        if ('wdspec' in job['run']['test']['suite'] or
            ('marionette' in job['run']['test']['suite'] and 'headless' not in job['label'])):
            env.update({
                'NEED_COMPIZ': 'true'
            })

    if mozharness.get('mochitest-flavor'):
        env['MOCHITEST_FLAVOR'] = mozharness['mochitest-flavor']

    if mozharness['set-moz-node-path']:
        env['MOZ_NODE_PATH'] = '/usr/local/bin/node'

    if 'actions' in mozharness:
        env['MOZHARNESS_ACTIONS'] = ' '.join(mozharness['actions'])

    if config.params.is_try():
        env['TRY_COMMIT_MSG'] = config.params['message']

    # handle some of the mozharness-specific options
    if test['reboot']:
        raise Exception('reboot: {} not supported on generic-worker'.format(test['reboot']))

    # Support vcs checkouts regardless of whether the task runs from
    # source or not in case it is needed on an interactive loaner.
    support_vcs_checkout(config, job, taskdesc)

    # If we have a source checkout, run mozharness from it instead of
    # downloading a zip file with the same content.
    if test['checkout']:
        env['MOZHARNESS_PATH'] = '{workdir}/checkouts/gecko/testing/mozharness'.format(**run)
    else:
        env['MOZHARNESS_URL'] = {'task-reference': mozharness_url}

    extra_config = {
        'installer_url': installer_url,
        'test_packages_url': test_packages_url(taskdesc),
    }
    env['EXTRA_MOZHARNESS_CONFIG'] = {
        'task-reference': six.ensure_text(json.dumps(extra_config))
    }

    command = [
        '{workdir}/bin/test-linux.sh'.format(**run),
    ]
    command.extend(mozharness.get('extra-options', []))

    if test.get('test-manifests'):
        env['MOZHARNESS_TEST_PATHS'] = six.ensure_text(
            json.dumps({test['suite']: test['test-manifests']}))

    # TODO: remove the need for run['chunked']
    elif mozharness.get('chunked') or test['chunks'] > 1:
        command.append('--total-chunk={}'.format(test['chunks']))
        command.append('--this-chunk={}'.format(test['this-chunk']))

    if 'download-symbols' in mozharness:
        download_symbols = mozharness['download-symbols']
        download_symbols = {True: 'true', False: 'false'}.get(download_symbols, download_symbols)
        command.append('--download-symbols=' + download_symbols)

    job['run'] = {
        'workdir': run['workdir'],
        'tooltool-downloads': mozharness['tooltool-downloads'],
        'checkout': test['checkout'],
        'command': command,
        'using': 'run-task',
    }
    configure_taskdesc_for_run(config, job, taskdesc, worker['implementation'])


@run_job_using('generic-worker', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_generic_worker(config, job, taskdesc):
    run = job['run']
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker'] = job['worker']

    bitbar_script = 'test-linux.sh'

    is_macosx = worker['os'] == 'macosx'
    is_windows = worker['os'] == 'windows'
    is_linux = worker['os'] == 'linux' or worker['os'] == 'linux-bitbar'
    is_bitbar = worker['os'] == 'linux-bitbar'
    assert is_macosx or is_windows or is_linux

    artifacts = [
        {
            'name': 'public/logs',
            'path': 'logs',
            'type': 'directory'
        },
    ]

    # jittest doesn't have blob_upload_dir
    if test['test-name'] != 'jittest':
        artifacts.append({
            'name': 'public/test_info',
            'path': 'build/blobber_upload_dir',
            'type': 'directory'
        })

    if is_bitbar:
        artifacts = [
            {
                'name': 'public/test/',
                'path': 'artifacts/public',
                'type': 'directory'
            },
            {
                'name': 'public/logs/',
                'path': 'workspace/logs',
                'type': 'directory'
            },
            {
                'name': 'public/test_info/',
                'path': 'workspace/build/blobber_upload_dir',
                'type': 'directory'
            },
        ]

    if 'installer-url' in mozharness:
        installer_url = mozharness['installer-url']
    else:
        upstream_task = '<build-signing>' if mozharness['requires-signed-builds'] else '<build>'
        installer_url = get_artifact_url(upstream_task, mozharness['build-artifact-name'])

    worker['os-groups'] = test['os-groups']

    # run-as-administrator is a feature for workers with UAC enabled and as such should not be
    # included in tasks on workers that have UAC disabled. Currently UAC is only enabled on
    # gecko Windows 10 workers, however this may be subject to change. Worker type
    # environment definitions can be found in https://github.com/mozilla-releng/OpenCloudConfig
    # See https://docs.microsoft.com/en-us/windows/desktop/secauthz/user-account-control
    # for more information about UAC.
    if test.get('run-as-administrator', False):
        if job['worker-type'].startswith('t-win10-64'):
            worker['run-as-administrator'] = True
        else:
            raise Exception('run-as-administrator not supported on {}'.format(job['worker-type']))

    if test['reboot']:
        raise Exception('reboot: {} not supported on generic-worker'.format(test['reboot']))

    worker['max-run-time'] = test['max-run-time']
    worker['retry-exit-status'] = test['retry-exit-status']
    worker['artifacts'] = artifacts

    env = worker.setdefault('env', {})
    env['GECKO_HEAD_REPOSITORY'] = config.params['head_repository']
    env['GECKO_HEAD_REV'] = config.params['head_rev']

    # this list will get cleaned up / reduced / removed in bug 1354088
    if is_macosx:
        env.update({
            'LC_ALL': 'en_US.UTF-8',
            'LANG': 'en_US.UTF-8',
            'MOZ_NODE_PATH': '/usr/local/bin/node',
            'PATH': '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin',
            'SHELL': '/bin/bash',
        })
    elif is_bitbar:
        env.update({
            'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
            'MOZHARNESS_SCRIPT': mozharness['script'],
            'MOZHARNESS_URL': {'artifact-reference': '<build/public/build/mozharness.zip>'},
            'MOZILLA_BUILD_URL': {'task-reference': installer_url},
            "MOZ_NO_REMOTE": '1',
            "NEED_XVFB": "false",
            "XPCOM_DEBUG_BREAK": 'warn',
            "NO_FAIL_ON_TEST_ERRORS": '1',
            "MOZ_HIDE_RESULTS_TABLE": '1',
            "MOZ_NODE_PATH": "/usr/local/bin/node",
            'TASKCLUSTER_WORKER_TYPE': job['worker-type'],
        })

    extra_config = {
        'installer_url': installer_url,
        'test_packages_url': test_packages_url(taskdesc),
    }
    env['EXTRA_MOZHARNESS_CONFIG'] = {
        'task-reference': six.ensure_text(json.dumps(extra_config))
    }

    if is_windows:
        mh_command = [
            'c:\\mozilla-build\\python\\python.exe',
            '-u',
            'mozharness\\scripts\\' + normpath(mozharness['script'])
        ]
    elif is_bitbar:
        mh_command = [
            'bash',
            "./{}".format(bitbar_script)
        ]
    elif is_macosx and 'macosx1014-64' in test['test-platform']:
        mh_command = [
            '/usr/local/bin/python2',
            '-u',
            'mozharness/scripts/' + mozharness['script']
        ]
    else:
        # is_linux or is_macosx
        mh_command = [
            # Using /usr/bin/python2.7 rather than python2.7 because
            # /usr/local/bin/python2.7 is broken on the mac workers.
            # See bug #1547903.
            '/usr/bin/python2.7',
            '-u',
            'mozharness/scripts/' + mozharness['script']
        ]

    for mh_config in mozharness['config']:
        cfg_path = 'mozharness/configs/' + mh_config
        if is_windows:
            cfg_path = normpath(cfg_path)
        mh_command.extend(['--cfg', cfg_path])
    mh_command.extend(mozharness.get('extra-options', []))
    if mozharness.get('download-symbols'):
        if isinstance(mozharness['download-symbols'], text_type):
            mh_command.extend(['--download-symbols', mozharness['download-symbols']])
        else:
            mh_command.extend(['--download-symbols', 'true'])
    if mozharness.get('include-blob-upload-branch'):
        mh_command.append('--blob-upload-branch=' + config.params['project'])

    if test.get('test-manifests'):
        env['MOZHARNESS_TEST_PATHS'] = six.ensure_text(
            json.dumps({test['suite']: test['test-manifests']}))

    # TODO: remove the need for run['chunked']
    elif mozharness.get('chunked') or test['chunks'] > 1:
        mh_command.append('--total-chunk={}'.format(test['chunks']))
        mh_command.append('--this-chunk={}'.format(test['this-chunk']))

    if config.params.is_try():
        env['TRY_COMMIT_MSG'] = config.params['message']

    worker['mounts'] = [{
        'directory': '.',
        'content': {
            'artifact': get_artifact_path(taskdesc, 'mozharness.zip'),
            'task-id': {
                'task-reference': '<build>'
            }
        },
        'format': 'zip'
    }]
    if is_bitbar:
        a_url = config.params.file_url(
            'taskcluster/scripts/tester/{}'.format(bitbar_script),
        )
        worker['mounts'] = [{
            'file': bitbar_script,
            'content': {
                'url': a_url,
            },
        }]

    job['run'] = {
        'workdir': run['workdir'],
        'tooltool-downloads': mozharness['tooltool-downloads'],
        'checkout': test['checkout'],
        'command': mh_command,
        'using': 'run-task',
    }
    if is_bitbar:
        job['run']['run-as-root'] = True
    configure_taskdesc_for_run(config, job, taskdesc, worker['implementation'])


@run_job_using('script-engine-autophone', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_script_engine_autophone(config, job, taskdesc):
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker']
    is_talos = test['suite'] == 'talos' or test['suite'] == 'raptor'
    if worker['os'] != 'linux':
        raise Exception('os: {} not supported on script-engine-autophone'.format(worker['os']))

    if 'installer-url' in mozharness:
        installer_url = mozharness['installer-url']
    else:
        installer_url = get_artifact_url('<build>', mozharness['build-artifact-name'])
    mozharness_url = get_artifact_url('<build>',
                                      'public/build/mozharness.zip')

    artifacts = [
        # (artifact name prefix, in-image path)
        ("public/test/", "/builds/worker/artifacts"),
        ("public/logs/", "/builds/worker/workspace/build/logs"),
        ("public/test_info/", "/builds/worker/workspace/build/blobber_upload_dir"),
    ]

    worker['artifacts'] = [{
        'name': prefix,
        'path': path,
        'type': 'directory',
    } for (prefix, path) in artifacts]

    if test['reboot']:
        worker['reboot'] = test['reboot']

    worker['env'] = env = {
        'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
        'GECKO_HEAD_REV': config.params['head_rev'],
        'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
        'MOZHARNESS_SCRIPT': mozharness['script'],
        'MOZHARNESS_URL': {'task-reference': mozharness_url},
        'MOZILLA_BUILD_URL': {'task-reference': installer_url},
        "MOZ_NO_REMOTE": '1',
        "XPCOM_DEBUG_BREAK": 'warn',
        "NO_FAIL_ON_TEST_ERRORS": '1',
        "MOZ_HIDE_RESULTS_TABLE": '1',
        "MOZ_NODE_PATH": "/usr/local/bin/node",
        'WORKING_DIR': '/builds/worker',
        'WORKSPACE': '/builds/worker/workspace',
        'TASKCLUSTER_WORKER_TYPE': job['worker-type'],
    }

    # for fetch tasks on mobile
    if 'env' in job['worker'] and 'MOZ_FETCHES' in job['worker']['env']:
        env['MOZ_FETCHES'] = job['worker']['env']['MOZ_FETCHES']
        env['MOZ_FETCHES_DIR'] = job['worker']['env']['MOZ_FETCHES_DIR']

    # talos tests don't need Xvfb
    if is_talos:
        env['NEED_XVFB'] = 'false'

    extra_config = {
        'installer_url': installer_url,
        'test_packages_url': test_packages_url(taskdesc),
    }
    env['EXTRA_MOZHARNESS_CONFIG'] = {
        'task-reference': six.ensure_text(json.dumps(extra_config))
    }

    script = 'test-linux.sh'
    worker['context'] = config.params.file_url(
        'taskcluster/scripts/tester/{}'.format(script),
    )

    command = worker['command'] = ["./{}".format(script)]
    if mozharness.get('include-blob-upload-branch'):
        command.append('--blob-upload-branch=' + config.params['project'])
    command.extend(mozharness.get('extra-options', []))

    if test.get('test-manifests'):
        env['MOZHARNESS_TEST_PATHS'] = six.ensure_text(
            json.dumps({test['suite']: test['test-manifests']}))

    # TODO: remove the need for run['chunked']
    elif mozharness.get('chunked') or test['chunks'] > 1:
        command.append('--total-chunk={}'.format(test['chunks']))
        command.append('--this-chunk={}'.format(test['this-chunk']))

    if 'download-symbols' in mozharness:
        download_symbols = mozharness['download-symbols']
        download_symbols = {True: 'true', False: 'false'}.get(download_symbols, download_symbols)
        command.append('--download-symbols=' + download_symbols)
