# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""

Support for running jobs via mozharness.  Ideally, most stuff gets run this
way, and certainly anything using mozharness should use this approach.

"""

from __future__ import absolute_import, print_function, unicode_literals
import json
import slugid

from textwrap import dedent

from taskgraph.util.schema import Schema
from voluptuous import Required, Optional, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_workspace_cache,
    docker_worker_add_gecko_vcs_env_vars,
    docker_worker_setup_secrets,
    docker_worker_add_public_artifacts,
    docker_worker_add_tooltool,
    generic_worker_add_public_artifacts,
    support_vcs_checkout,
)

mozharness_run_schema = Schema({
    Required('using'): 'mozharness',

    # the mozharness script used to run this task, relative to the testing/
    # directory and using forward slashes even on Windows
    Required('script'): basestring,

    # Additional paths to look for mozharness configs in. These should be
    # relative to the base of the source checkout
    Optional('config-paths'): [basestring],

    # the config files required for the task, relative to
    # testing/mozharness/configs or one of the paths specified in
    # `config-paths` and using forward slashes even on Windows
    Required('config'): [basestring],

    # any additional actions to pass to the mozharness command
    Optional('actions'): [basestring],

    # any additional options (without leading --) to be passed to mozharness
    Optional('options'): [basestring],

    # --custom-build-variant-cfg value
    Optional('custom-build-variant-cfg'): basestring,

    # Extra configuration options to pass to mozharness.
    Optional('extra-config'): dict,

    # Extra metadata to use toward the workspace caching.
    # Only supported on docker-worker
    Optional('extra-workspace-cache-key'): basestring,

    # If not false, tooltool downloads will be enabled via relengAPIProxy
    # for either just public files, or all files.  Not supported on Windows
    Required('tooltool-downloads'): Any(
        False,
        'public',
        'internal',
    ),

    # The set of secret names to which the task has access; these are prefixed
    # with `project/releng/gecko/{treeherder.kind}/level-{level}/`.  Setting
    # this will enable any worker features required and set the task's scopes
    # appropriately.  `true` here means ['*'], all secrets.  Not supported on
    # Windows
    Required('secrets'): Any(bool, [basestring]),

    # If true, taskcluster proxy will be enabled; note that it may also be enabled
    # automatically e.g., for secrets support.  Not supported on Windows.
    Required('taskcluster-proxy'): bool,

    # If true, the build scripts will start Xvfb.  Not supported on Windows.
    Required('need-xvfb'): bool,

    # If false, indicate that builds should skip producing artifacts.  Not
    # supported on Windows.
    Required('keep-artifacts'): bool,

    # If specified, use the in-tree job script specified.
    Optional('job-script'): basestring,

    Required('requires-signed-builds'): bool,

    # If false, don't set MOZ_SIMPLE_PACKAGE_NAME
    # Only disableable on windows
    Required('use-simple-package'): bool,

    # If false don't pass --branch or --skip-buildbot-actions to mozharness script
    # Only disableable on windows
    Required('use-magic-mh-args'): bool,

    # if true, perform a checkout of a comm-central based branch inside the
    # gecko checkout
    Required('comm-checkout'): bool,
})


mozharness_defaults = {
    'tooltool-downloads': False,
    'secrets': False,
    'taskcluster-proxy': False,
    'need-xvfb': False,
    'keep-artifacts': True,
    'requires-signed-builds': False,
    'use-simple-package': True,
    'use-magic-mh-args': True,
    'comm-checkout': False,
}


@run_job_using("docker-worker", "mozharness", schema=mozharness_run_schema,
               defaults=mozharness_defaults)
def mozharness_on_docker_worker_setup(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']
    worker['implementation'] = job['worker']['implementation']

    if not run['use-simple-package']:
        raise NotImplementedError("Simple packaging cannot be disabled via"
                                  "'use-simple-package' on docker-workers")
    if not run['use-magic-mh-args']:
        raise NotImplementedError("Cannot disabled mh magic arg passing via"
                                  "'use-magic-mh-args' on docker-workers")

    # Running via mozharness assumes an image that contains build.sh:
    # by default, desktop-build, but it could be another image (like
    # android-build) that "inherits" from desktop-build.
    taskdesc['worker'].setdefault('docker-image', {'in-tree': 'desktop-build'})

    worker['taskcluster-proxy'] = run.get('taskcluster-proxy')

    docker_worker_add_public_artifacts(config, job, taskdesc)
    docker_worker_add_workspace_cache(config, job, taskdesc,
                                      extra=run.get('extra-workspace-cache-key'))
    support_vcs_checkout(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_CONFIG': ' '.join(run['config']),
        'MOZHARNESS_SCRIPT': run['script'],
        'MH_BRANCH': config.params['project'],
        'MH_BUILD_POOL': 'taskcluster',
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'MOZ_AUTOMATION': '1',
    })

    if 'actions' in run:
        env['MOZHARNESS_ACTIONS'] = ' '.join(run['actions'])

    if 'options' in run:
        env['MOZHARNESS_OPTIONS'] = ' '.join(run['options'])

    if 'config-paths' in run:
        env['MOZHARNESS_CONFIG_PATHS'] = ' '.join(run['config-paths'])

    if 'custom-build-variant-cfg' in run:
        env['MH_CUSTOM_BUILD_VARIANT_CFG'] = run['custom-build-variant-cfg']

    if 'extra-config' in run:
        env['EXTRA_MOZHARNESS_CONFIG'] = json.dumps(run['extra-config'])

    if 'job-script' in run:
        env['JOB_SCRIPT'] = run['job-script']

    if 'try' in config.params['project']:
        env['TRY_COMMIT_MSG'] = config.params['message']

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

    if run['tooltool-downloads']:
        internal = run['tooltool-downloads'] == 'internal'
        docker_worker_add_tooltool(config, job, taskdesc, internal=internal)

    # Retry if mozharness returns TBPL_RETRY
    worker['retry-exit-status'] = 4

    docker_worker_setup_secrets(config, job, taskdesc)

    command = [
        '/builds/worker/bin/run-task',
        '--vcs-checkout', '/builds/worker/workspace/build/src',
        '--tools-checkout', '/builds/worker/workspace/build/tools',
    ]
    if run['comm-checkout']:
        command.append('--comm-checkout=/builds/worker/workspace/build/src/comm')

    command += [
        '--',
        '/builds/worker/workspace/build/src/{}'.format(
            run.get('job-script', 'taskcluster/scripts/builder/build-linux.sh')
        ),
    ]

    worker['command'] = command


@run_job_using("generic-worker", "mozharness", schema=mozharness_run_schema,
               defaults=mozharness_defaults)
def mozharness_on_generic_worker(config, job, taskdesc):
    assert job['worker']['os'] == 'windows', 'only supports windows right now'

    run = job['run']

    # fail if invalid run options are included
    invalid = []
    for prop in ['tooltool-downloads',
                 'secrets', 'taskcluster-proxy', 'need-xvfb']:
        if prop in run and run[prop]:
            invalid.append(prop)
    if not run.get('keep-artifacts', True):
        invalid.append('keep-artifacts')
    if invalid:
        raise Exception("Jobs run using mozharness on Windows do not support properties " +
                        ', '.join(invalid))

    worker = taskdesc['worker']

    generic_worker_add_public_artifacts(config, job, taskdesc)

    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'MOZ_AUTOMATION': '1',
    })
    if run['use-simple-package']:
        env.update({'MOZ_SIMPLE_PACKAGE_NAME': 'target'})

    if 'extra-config' in run:
        env['EXTRA_MOZHARNESS_CONFIG'] = json.dumps(run['extra-config'])

    # The windows generic worker uses batch files to pass environment variables
    # to commands.  Setting a variable to empty in a batch file unsets, so if
    # there is no `TRY_COMMIT_MESSAGE`, pass a space instead, so that
    # mozharness doesn't try to find the commit message on its own.
    if 'try' in config.params['project']:
        env['TRY_COMMIT_MSG'] = config.params['message'] or 'no commit message'

    if not job['attributes']['build_platform'].startswith('win'):
        raise Exception(
            "Task generation for mozharness build jobs currently only supported on Windows"
        )

    mh_command = [r'c:\mozilla-build\python\python.exe']
    mh_command.append('\\'.join([r'.\build\src\testing', run['script'].replace('/', '\\')]))

    if 'config-paths' in run:
        for path in run['config-paths']:
            mh_command.append(r'--extra-config-path '
                              r'.\build\src\{}'.format(path.replace('/', '\\')))

    for cfg in run['config']:
        mh_command.append('--config ' + cfg.replace('/', '\\'))
    if run['use-magic-mh-args']:
        mh_command.append('--branch ' + config.params['project'])
        mh_command.append(r'--skip-buildbot-actions')
    mh_command.append(r'--work-dir %cd:Z:=z:%\build')
    for action in run.get('actions', []):
        assert ' ' not in action
        mh_command.append('--' + action)

    for option in run.get('options', []):
        assert ' ' not in option
        mh_command.append('--' + option)
    if run.get('custom-build-variant-cfg'):
        mh_command.append('--custom-build-variant')
        mh_command.append(run['custom-build-variant-cfg'])

    def checkout_repo(base_repo, head_repo, head_rev, path):
        hg_command = ['"c:\\Program Files\\Mercurial\\hg.exe"']
        hg_command.append('robustcheckout')
        hg_command.extend(['--sharebase', 'y:\\hg-shared'])
        hg_command.append('--purge')
        hg_command.extend(['--upstream', base_repo])
        hg_command.extend(['--revision', head_rev])
        hg_command.append(head_repo)
        hg_command.append(path)

        logging_command = [
            b":: TinderboxPrint:<a href={source_repo}/rev/{revision} "
            b"title='Built from {repo_name} revision {revision}'>{revision}</a>\n".format(
                revision=head_rev,
                source_repo=head_repo,
                repo_name=head_repo.split('/')[-1],
            )]

        return [
            ' '.join(hg_command),
            ' '.join(logging_command),
        ]

    hg_commands = checkout_repo(
        base_repo=env['GECKO_BASE_REPOSITORY'],
        head_repo=env['GECKO_HEAD_REPOSITORY'],
        head_rev=env['GECKO_HEAD_REV'],
        path='.\\build\\src')

    if run['comm-checkout']:
        hg_commands.extend(
            checkout_repo(
                base_repo=env['COMM_BASE_REPOSITORY'],
                head_repo=env['COMM_HEAD_REPOSITORY'],
                head_rev=env['COMM_HEAD_REV'],
                path='.\\build\\src\\comm')
        )

    worker['command'] = []
    if taskdesc.get('needs-sccache'):
        worker['command'].extend([
            # Make the comment part of the first command, as it will help users to
            # understand what is going on, and why these steps are implemented.
            dedent('''\
            :: sccache currently uses the full compiler commandline as input to the
            :: cache hash key, so create a symlink to the task dir and build from
            :: the symlink dir to get consistent paths.
            if exist z:\\build rmdir z:\\build'''),
            r'mklink /d z:\build %cd%',
            # Grant delete permission on the link to everyone.
            r'icacls z:\build /grant *S-1-1-0:D /L',
            r'cd /d z:\build',
        ])

    worker['command'].extend(hg_commands)
    worker['command'].extend([
        ' '.join(mh_command)
    ])


@run_job_using('buildbot-bridge', 'mozharness', schema=mozharness_run_schema,
               defaults=mozharness_defaults)
def mozharness_on_buildbot_bridge(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker']
    branch = config.params['project']
    product = run.get('index', {}).get('product', 'firefox')

    worker.pop('env', None)

    if 'devedition' in job['attributes']['build_platform']:
        buildername = 'OS X 10.7 {} devedition build'.format(branch)
    else:
        buildername = 'OS X 10.7 {} build'.format(branch)

    worker.update({
        'buildername': buildername,
        'sourcestamp': {
            'branch': branch,
            'repository': config.params['head_repository'],
            'revision': config.params['head_rev'],
        },
        'properties': {
            'product': product,
            'who': config.params['owner'],
            'upload_to_task_id': slugid.nice(),
        }
    })
