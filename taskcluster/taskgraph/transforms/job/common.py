# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Common support for various job types.  These functions are all named after the
worker implementation they operate on, and take the same three parameters, for
consistency.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.taskcluster import get_artifact_prefix

SECRET_SCOPE = 'secrets:get:project/releng/gecko/{}/level-{}/{}'


def add_cache(job, taskdesc, name, mount_point, skip_untrusted=False):
    """Adds a cache based on the worker's implementation.

    Args:
        job (dict): Task's job description.
        taskdesc (dict): Target task description to modify.
        name (str): Name of the cache.
        mount_point (path): Path on the host to mount the cache.
        skip_untrusted (bool): Whether cache is used in untrusted environments
            (default: False). Only applies to docker-worker.
    """
    if not job['run'].get('use-caches', True):
        return

    worker = job['worker']

    if worker['implementation'] == 'docker-worker':
        taskdesc['worker'].setdefault('caches', []).append({
            'type': 'persistent',
            'name': name,
            'mount-point': mount_point,
            'skip-untrusted': skip_untrusted,
        })

    elif worker['implementation'] == 'generic-worker':
        taskdesc['worker'].setdefault('mounts', []).append({
            'cache-name': name,
            'directory': mount_point,
        })

    else:
        # Caches not implemented
        pass


def docker_worker_add_workspace_cache(config, job, taskdesc, extra=None):
    """Add the workspace cache.

    Args:
        config (TransformConfig): Transform configuration object.
        job (dict): Task's job description.
        taskdesc (dict): Target task description to modify.
        extra (str): Optional context passed in that supports extending the cache
            key name to avoid undesired conflicts with other caches.
    """
    cache_name = '{}-build-{}-{}-workspace'.format(
        config.params['project'],
        taskdesc['attributes']['build_platform'],
        taskdesc['attributes']['build_type'],
    )
    if extra:
        cache_name = '{}-{}'.format(cache_name, extra)

    mount_point = "{workdir}/workspace".format(**job['run'])

    # Don't enable the workspace cache when we can't guarantee its
    # behavior, like on Try.
    add_cache(job, taskdesc, cache_name, mount_point, skip_untrusted=True)


def add_artifacts(config, job, taskdesc, path):
    taskdesc['worker'].setdefault('artifacts', []).append({
        'name': get_artifact_prefix(taskdesc),
        'path': path,
        'type': 'directory',
    })


def docker_worker_add_artifacts(config, job, taskdesc):
    """ Adds an artifact directory to the task """
    add_artifacts(config, job, taskdesc, path='{workdir}/artifacts/'.format(**job['run']))


def generic_worker_add_artifacts(config, job, taskdesc):
    """ Adds an artifact directory to the task """
    # The path is the location on disk; it doesn't necessarily
    # mean the artifacts will be public or private; that is set via the name
    # attribute in add_artifacts.
    add_artifacts(config, job, taskdesc, path=get_artifact_prefix(taskdesc))


def support_vcs_checkout(config, job, taskdesc, sparse=False):
    """Update a job/task with parameters to enable a VCS checkout.

    This can only be used with ``run-task`` tasks, as the cache name is
    reserved for ``run-task`` tasks.
    """
    worker = job['worker']
    is_mac = worker['os'] == 'macosx'
    is_win = worker['os'] == 'windows'
    is_linux = worker['os'] == 'linux'
    assert is_mac or is_win or is_linux

    if is_win:
        checkoutdir = './build'
        geckodir = '{}/src'.format(checkoutdir)
        hgstore = 'y:/hg-shared'
    elif is_mac:
        checkoutdir = './checkouts'
        geckodir = '{}/gecko'.format(checkoutdir)
        hgstore = '{}/hg-shared'.format(checkoutdir)
    else:
        checkoutdir = '{workdir}/checkouts'.format(**job['run'])
        geckodir = '{}/gecko'.format(checkoutdir)
        hgstore = '{}/hg-store'.format(checkoutdir)

    cache_name = 'checkouts'

    # Sparse checkouts need their own cache because they can interfere
    # with clients that aren't sparse aware.
    if sparse:
        cache_name += '-sparse'

    add_cache(job, taskdesc, cache_name, checkoutdir)

    taskdesc['worker'].setdefault('env', {}).update({
        'GECKO_BASE_REPOSITORY': config.params['base_repository'],
        'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
        'GECKO_HEAD_REV': config.params['head_rev'],
        'GECKO_PATH': geckodir,
        'HG_STORE_PATH': hgstore,
    })

    if 'comm_base_repository' in config.params:
        taskdesc['worker']['env'].update({
            'COMM_BASE_REPOSITORY': config.params['comm_base_repository'],
            'COMM_HEAD_REPOSITORY': config.params['comm_head_repository'],
            'COMM_HEAD_REV': config.params['comm_head_rev'],
        })
    elif job['run'].get('comm-checkout', False):
        raise Exception("Can't checkout from comm-* repository if not given a repository.")

    # Give task access to hgfingerprint secret so it can pin the certificate
    # for hg.mozilla.org.
    taskdesc['scopes'].append('secrets:get:project/taskcluster/gecko/hgfingerprint')
    taskdesc['scopes'].append('secrets:get:project/taskcluster/gecko/hgmointernal')

    # only some worker platforms have taskcluster-proxy enabled
    if job['worker']['implementation'] in ('docker-worker',):
        taskdesc['worker']['taskcluster-proxy'] = True


def generic_worker_hg_commands(base_repo, head_repo, head_rev, path,
                               sparse_profile=None):
    """Obtain commands needed to obtain a Mercurial checkout on generic-worker.

    Returns two command strings. One performs the checkout. Another logs.
    """
    args = [
        r'"c:\Program Files\Mercurial\hg.exe"',
        'robustcheckout',
        '--sharebase', r'y:\hg-shared',
        '--purge',
        '--upstream', base_repo,
        '--revision', head_rev,
    ]

    if sparse_profile:
        args.extend(['--config', 'extensions.sparse='])
        args.extend(['--sparseprofile', sparse_profile])

    args.extend([
        head_repo,
        path,
    ])

    logging_args = [
        b":: TinderboxPrint:<a href={source_repo}/rev/{revision} "
        b"title='Built from {repo_name} revision {revision}'>{revision}</a>"
        b"\n".format(
            revision=head_rev,
            source_repo=head_repo,
            repo_name=head_repo.split('/')[-1]),
    ]

    return [' '.join(args), ' '.join(logging_args)]


def setup_secrets(config, job, taskdesc):
    """Set up access to secrets via taskcluster-proxy.  The value of
    run['secrets'] should be a boolean or a list of secret names that
    can be accessed."""
    if not job['run'].get('secrets'):
        return

    taskdesc['worker']['taskcluster-proxy'] = True
    secrets = job['run']['secrets']
    if secrets is True:
        secrets = ['*']
    for sec in secrets:
        taskdesc['scopes'].append(SECRET_SCOPE.format(
            job['treeherder']['kind'], config.params['level'], sec))


def docker_worker_add_tooltool(config, job, taskdesc, internal=False):
    """Give the task access to tooltool.

    Enables the tooltool cache. Adds releng proxy. Configures scopes.

    By default, only public tooltool access will be granted. Access to internal
    tooltool can be enabled via ``internal=True``.

    This can only be used with ``run-task`` tasks, as the cache name is
    reserved for use with ``run-task``.
    """

    assert job['worker']['implementation'] in ('docker-worker',)

    level = config.params['level']
    add_cache(job, taskdesc, 'tooltool-cache'.format(level),
              '{workdir}/tooltool-cache'.format(**job['run']))

    taskdesc['worker'].setdefault('env', {}).update({
        'TOOLTOOL_CACHE': '{workdir}/tooltool-cache'.format(**job['run']),
    })

    taskdesc['worker']['relengapi-proxy'] = True
    taskdesc['scopes'].extend([
        'docker-worker:relengapi-proxy:tooltool.download.public',
    ])

    if internal:
        taskdesc['scopes'].extend([
            'docker-worker:relengapi-proxy:tooltool.download.internal',
        ])
