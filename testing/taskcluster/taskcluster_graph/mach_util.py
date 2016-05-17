# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

from collections import defaultdict
import os
import json
import copy
import re
import sys
import time
from collections import namedtuple

ROOT = os.path.dirname(os.path.realpath(__file__))
GECKO = os.path.realpath(os.path.join(ROOT, '..', '..', '..'))

def merge_dicts(*dicts):
    merged_dict = {}
    for dictionary in dicts:
        merged_dict.update(dictionary)
    return merged_dict

def gaia_info():
    '''
    Fetch details from in tree gaia.json (which links this version of
    gecko->gaia) and construct the usual base/head/ref/rev pairing...
    '''
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

def configure_dependent_task(task_path, parameters, taskid, templates, build_treeherder_config):
    """
    Configure a build dependent task. This is shared between post-build and test tasks.

    :param task_path: location to the task yaml
    :param parameters: parameters to load the template
    :param taskid: taskid of the dependent task
    :param templates: reference to the template builder
    :param build_treeherder_config: parent treeherder config
    :return: the configured task
    """
    task = templates.load(task_path, parameters)
    task['taskId'] = taskid

    if 'requires' not in task:
        task['requires'] = []

    task['requires'].append(parameters['build_slugid'])

    if 'treeherder' not in task['task']['extra']:
        task['task']['extra']['treeherder'] = {}

    # Copy over any treeherder configuration from the build so
    # tests show up under the same platform...
    treeherder_config = task['task']['extra']['treeherder']

    treeherder_config['collection'] = \
        build_treeherder_config.get('collection', {})

    treeherder_config['build'] = \
        build_treeherder_config.get('build', {})

    treeherder_config['machine'] = \
        build_treeherder_config.get('machine', {})

    if 'routes' not in task['task']:
        task['task']['routes'] = []

    if 'scopes' not in task['task']:
        task['task']['scopes'] = []

    return task

def set_interactive_task(task, interactive):
    r"""Make the task interactive.

    :param task: task definition.
    :param interactive: True if the task should be interactive.
    """
    if not interactive:
        return

    payload = task["task"]["payload"]
    if "features" not in payload:
        payload["features"] = {}
    payload["features"]["interactive"] = True

def remove_caches_from_task(task):
    r"""Remove all caches but tc-vcs from the task.

    :param task: task definition.
    """
    whitelist = [
        re.compile("^level-[123]-.*-tc-vcs(-public-sources)?$"),
        re.compile("^tooltool-cache$"),
    ]
    try:
        caches = task["task"]["payload"]["cache"]
        for cache in caches.keys():
            if not any(pat.match(cache) for pat in whitelist):
                caches.pop(cache)
    except KeyError:
        pass

def query_vcs_info(repository, revision):
    """Query the pushdate and pushid of a repository/revision.
    This is intended to be used on hg.mozilla.org/mozilla-central and
    similar. It may or may not work for other hg repositories.
    """
    if not repository or not revision:
        sys.stderr.write('cannot query vcs info because vcs info not provided\n')
        return None

    VCSInfo = namedtuple('VCSInfo', ['pushid', 'pushdate', 'changesets'])

    try:
        import requests
        url = '%s/json-automationrelevance/%s' % (repository.rstrip('/'),
                                                  revision)
        sys.stderr.write("Querying version control for metadata: %s\n" % url)
        contents = requests.get(url).json()

        changesets = []
        for c in contents['changesets']:
            changesets.append({k: c[k] for k in ('desc', 'files', 'node')})

        pushid = contents['changesets'][-1]['pushid']
        pushdate = contents['changesets'][-1]['pushdate'][0]

        return VCSInfo(pushid, pushdate, changesets)

    except Exception:
        sys.stderr.write(
            "Error querying VCS info for '%s' revision '%s'\n" % (
                repository, revision,
            )
        )
        return None


