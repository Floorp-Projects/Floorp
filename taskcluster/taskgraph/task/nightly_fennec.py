# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Temporary placeholder for nightly builds
"""

from __future__ import absolute_import, print_function, unicode_literals

import logging
import json
import os
import time
from collections import namedtuple

from . import base
from slugid import nice as slugid
from taskgraph.util.templates import Templates
from taskgraph.util.docker import docker_image


logger = logging.getLogger(__name__)
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'
TASKID_PLACEHOLDER = 'TaskLabel=={}'


def mklabel():
    return TASKID_PLACEHOLDER.format(slugid())


def gaia_info():
    '''Fetch details from in tree gaia.json (which links this version of
    gecko->gaia) and construct the usual base/head/ref/rev pairing...'''
    gaia = json.load(open(os.path.join(GECKO, 'b2g', 'config', 'gaia.json')))

    if gaia['git'] is None or \
       gaia['git']['remote'] == '' or \
       gaia['git']['git_revision'] == '' or \
       gaia['git']['branch'] == '':

        # Just use the hg params...
        return {
            'gaia_base_repository': 'https://hg.mozilla.org/{}'.format(gaia['repo_path']),
            'gaia_head_repository': 'https://hg.mozilla.org/{}'.format(gaia['repo_path']),
            'gaia_ref': gaia['revision'],
            'gaia_rev': gaia['revision']
        }

    else:
        # Use git
        return {
            'gaia_base_repository': gaia['git']['remote'],
            'gaia_head_repository': gaia['git']['remote'],
            'gaia_rev': gaia['git']['git_revision'],
            'gaia_ref': gaia['git']['branch'],
        }


def decorate_task_json_routes(task, json_routes, parameters):
    """Decorate the given task with routes.json routes.

    :param dict task: task definition.
    :param json_routes: the list of routes to use from routes.json
    :param parameters: dictionary of parameters to use in route templates
    """
    routes = task.get('routes', [])
    for route in json_routes:
        routes.append(route.format(**parameters))

    task['routes'] = routes


def query_vcs_info(repository, revision):
    """Query the pushdate and pushid of a repository/revision.

    This is intended to be used on hg.mozilla.org/mozilla-central and
    similar. It may or may not work for other hg repositories.
    """
    if not repository or not revision:
        logger.warning('cannot query vcs info because vcs info not provided')
        return None

    VCSInfo = namedtuple('VCSInfo', ['pushid', 'pushdate', 'changesets'])

    try:
        import requests
        url = '%s/json-automationrelevance/%s' % (repository.rstrip('/'),
                                                  revision)
        logger.debug("Querying version control for metadata: %s", url)
        contents = requests.get(url).json()

        changesets = []
        for c in contents['changesets']:
            changesets.append({k: c[k] for k in ('desc', 'files', 'node')})

        pushid = contents['changesets'][-1]['pushid']
        pushdate = contents['changesets'][-1]['pushdate'][0]

        return VCSInfo(pushid, pushdate, changesets)

    except Exception:
        logger.exception("Error querying VCS info for '%s' revision '%s'",
                         repository, revision)
        return None


class NightlyFennecTask(base.Task):

    def __init__(self, *args, **kwargs):
        try:
            self.task_dict = kwargs.pop('task_dict')
        except KeyError:
            pass
        super(NightlyFennecTask, self).__init__(*args, **kwargs)

    @classmethod
    def load_tasks(cls, kind, path, config, params, loaded_tasks):
        root = os.path.abspath(os.path.join(path, config[
            'nightly_fennec_path']))

        project = params['project']

        # Set up the parameters, including the time of the build
        push_epoch = int(time.time())
        vcs_info = query_vcs_info(params['head_repository'], params['head_rev'])
        changed_files = set()
        if vcs_info:
            push_epoch = vcs_info.pushdate

            logger.debug(
                '{} commits influencing task scheduling:'.format(len(vcs_info.changesets)))
            for c in vcs_info.changesets:
                logger.debug("{cset} {desc}".format(
                    cset=c['node'][0:12],
                    desc=c['desc'].splitlines()[0].encode('ascii', 'ignore')))
                changed_files |= set(c['files'])

        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime(push_epoch))

        # Template parameters used when expanding the graph
        parameters = dict(gaia_info().items() + {
            'index': 'index',
            'project': project,
            'pushlog_id': params.get('pushlog_id', 0),
            'base_repository': params['base_repository'] or params['head_repository'],
            'docker_image': docker_image,
            'head_repository': params['head_repository'],
            'head_ref': params['head_ref'] or params['head_rev'],
            'head_rev': params['head_rev'],
            'pushdate': pushdate,
            'pushtime': pushdate[8:],
            'year': pushdate[0:4],
            'month': pushdate[4:6],
            'day': pushdate[6:8],
            'rank': push_epoch,
            'owner': params['owner'],
            'level': params['level'],
            'build_slugid': mklabel(),
            'source': '{repo}file/{rev}/taskcluster/ci/nightly-fennec'.format(
                repo=params['head_repository'], rev=params['head_rev']),
            'build_name': 'android',
            'build_type': 'opt',
            'build_product': 'mobile'
        }.items())

        routes_file = os.path.join(root, 'routes.json')
        with open(routes_file) as f:
            contents = json.load(f)
            json_routes = contents['routes']

        tasks = []
        templates = Templates(root)

        task = templates.load('android-api-15-nightly-build.yml', parameters)
        decorate_task_json_routes(task['task'], json_routes, parameters)

        attributes = {'kind': 'nightly-fennec'}
        tasks.append(cls(kind, 'build-nightly-fennec',
                     task=task['task'], attributes=attributes,
                         task_dict=task))

        # Convert to a dictionary of tasks.  The process above has invented a
        # taskId for each task, and we use those as the *labels* for the tasks;
        # taskgraph will later assign them new taskIds.
        return tasks

    def get_dependencies(self, taskgraph):
        deps = [(label, label) for label in self.task_dict.get('requires', [])]

        # add a dependency on an image task, if needed
        if 'docker-image' in self.task_dict:
            deps.append(('build-docker-image-{docker-image}'.format(**self.task_dict),
                         'docker-image'))

        return deps

    def optimize(self, params):
        return False, None
