# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""

Support for running jobs via mozharness.  Ideally, most stuff gets run this
way, and certainly anything using mozharness should use this approach.

"""

from __future__ import absolute_import, print_function, unicode_literals

import time
from voluptuous import Schema, Required, Optional, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_workspace_cache,
    docker_worker_add_tc_vcs_cache,
    docker_worker_add_gecko_vcs_env_vars,
    docker_worker_setup_secrets,
    docker_worker_add_public_artifacts
)

COALESCE_KEY = 'builds.{project}.{name}'

mozharness_run_schema = Schema({
    Required('using'): 'mozharness',

    # the mozharness script used to run this task, relative to the testing/
    # directory and using forward slashes even on Windows
    Required('script'): basestring,

    # the config files required for the task, relative to
    # testing/mozharness/configs and using forward slashes even on Windows
    Required('config'): [basestring],

    # any additional actions to pass to the mozharness command; not supported
    # on Windows
    Optional('actions'): [basestring],

    # any additional options (without leading --) to be passed to mozharness;
    # not supported on Windows
    Optional('options'): [basestring],

    # --custom-build-variant-cfg value (not supported on Windows)
    Optional('custom-build-variant-cfg'): basestring,

    # If not false, tooltool downloads will be enabled via relengAPIProxy
    # for either just public files, or all files.  Not supported on Windows
    Required('tooltool-downloads', default=False): Any(
        False,
        'public',
        'internal',
    ),

    # The set of secret names to which the task has access; these are prefixed
    # with `project/releng/gecko/{treeherder.kind}/level-{level}/`.  Setting
    # this will enable any worker features required and set the task's scopes
    # appropriately.  `true` here means ['*'], all secrets.  Not supported on
    # Windows
    Required('secrets', default=False): Any(bool, [basestring]),

    # If true, taskcluster proxy will be enabled; note that it may also be enabled
    # automatically e.g., for secrets support.  Not supported on Windows.
    Required('taskcluster-proxy', default=False): bool,

    # If true, the build scripts will start Xvfb.  Not supported on Windows.
    Required('need-xvfb', default=False): bool,

    # If false, indicate that builds should skip producing artifacts.  Not
    # supported on Windows.
    Required('keep-artifacts', default=True): bool,
})


@run_job_using("docker-worker", "mozharness", schema=mozharness_run_schema)
def mozharness_on_docker_worker_setup(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']
    worker['implementation'] = job['worker']['implementation']

    # running via mozharness assumes desktop-build (which contains build.sh)
    taskdesc['worker']['docker-image'] = {"in-tree": "desktop-build"}

    worker['relengapi-proxy'] = False  # but maybe enabled for tooltool below
    worker['taskcluster-proxy'] = run.get('taskcluster-proxy')

    docker_worker_add_public_artifacts(config, job, taskdesc)
    docker_worker_add_tc_vcs_cache(config, job, taskdesc)
    docker_worker_add_workspace_cache(config, job, taskdesc)
    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_CONFIG': ' '.join(run['config']),
        'MOZHARNESS_SCRIPT': run['script'],
        'MH_BRANCH': config.params['project'],
        'MH_BUILD_POOL': 'taskcluster',
        'MOZ_BUILD_DATE': time.strftime("%Y%m%d%H%M%S", time.gmtime(config.params['pushdate'])),
        'MOZ_SCM_LEVEL': config.params['level'],
    })

    if 'actions' in run:
        env['MOZHARNESS_ACTIONS'] = ' '.join(run['actions'])

    if 'options' in run:
        env['MOZHARNESS_OPTIONS'] = ' '.join(run['options'])

    if 'custom-build-variant-cfg' in run:
        env['MH_CUSTOM_BUILD_VARIANT_CFG'] = run['custom-build-variant-cfg']

    # if we're not keeping artifacts, set some env variables to empty values
    # that will cause the build process to skip copying the results to the
    # artifacts directory.  This will have no effect for operations that are
    # not builds.
    if not run['keep-artifacts']:
        env['DIST_TARGET_UPLOADS'] = ''
        env['DIST_UPLOADS'] = ''

    # Xvfb
    if run['need-xvfb']:
        env['NEED_XVFB'] = 'true'

    # tooltool downloads
    if run['tooltool-downloads']:
        worker['relengapi-proxy'] = True
        worker['caches'].append({
            'type': 'persistent',
            'name': 'tooltool-cache',
            'mount-point': '/home/worker/tooltool-cache',
        })
        taskdesc['scopes'].extend([
            'docker-worker:relengapi-proxy:tooltool.download.public',
        ])
        if run['tooltool-downloads'] == 'internal':
            taskdesc['scopes'].append(
                'docker-worker:relengapi-proxy:tooltool.download.internal')
        env['TOOLTOOL_CACHE'] = '/home/worker/tooltool-cache'
        env['TOOLTOOL_REPO'] = 'https://github.com/mozilla/build-tooltool'
        env['TOOLTOOL_REV'] = 'master'

    docker_worker_setup_secrets(config, job, taskdesc)

    worker['command'] = ["/bin/bash", "bin/build.sh"]


# We use the generic worker to run tasks on Windows
@run_job_using("generic-worker", "mozharness", schema=mozharness_run_schema)
def mozharness_on_windows(config, job, taskdesc):
    run = job['run']

    # fail if invalid run options are included
    invalid = []
    for prop in ['actions', 'custom-build-variant-cfg',
                 'tooltool-downloads', 'secrets', 'taskcluster-proxy',
                 'need-xvfb']:
        if prop in run and run[prop]:
            invalid.append(prop)
    if not run.get('keep-artifacts', True):
        invalid.append('keep-artifacts')
    if invalid:
        raise Exception("Jobs run using mozharness on Windows do not support properties " +
                        ', '.join(invalid))

    worker = taskdesc['worker']

    worker['artifacts'] = [{
        'path': r'public\build',
        'type': 'directory',
    }]

    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': time.strftime("%Y%m%d%H%M%S", time.gmtime(config.params['pushdate'])),
        'MOZ_SCM_LEVEL': config.params['level'],
        'TOOLTOOL_REPO': 'https://github.com/mozilla/build-tooltool',
        'TOOLTOOL_REV': 'master',
    })

    mh_command = [r'c:\mozilla-build\python\python.exe']
    mh_command.append('\\'.join([r'.\build\src\testing', run['script'].replace('/', '\\')]))
    for cfg in run['config']:
        mh_command.append('--config ' + cfg.replace('/', '\\'))
    mh_command.append('--branch ' + config.params['project'])
    mh_command.append(r'--skip-buildbot-actions --work-dir %cd:Z:=z:%\build')
    for option in run.get('options', []):
        mh_command.append('--' + option)
    hg = r'c:\Program Files\Mercurial\hg.exe'
    worker['command'] = [
        r'mkdir .\build\src',
        r'"{}" share c:\builds\hg-shared\mozilla-central .\build\src'.format(hg),
        r'"{}" pull -u -R .\build\src --rev %GECKO_HEAD_REV% %GECKO_HEAD_REPOSITORY%'.format(hg),
        ' '.join(mh_command),
    ]
