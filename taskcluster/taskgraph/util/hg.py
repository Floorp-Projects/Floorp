# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import requests
import subprocess

PUSHLOG_TMPL = '{}/json-pushes?version=2&changeset={}&tipsonly=1&full=1'


def find_hg_revision_pushlog_id(parameters, graph_config, revision):
    """Given the parameters for this action and a revision, find the
    pushlog_id of the revision."""
    repo_param = '{}head_repository'.format(graph_config['project-repo-param-prefix'])
    pushlog_url = PUSHLOG_TMPL.format(parameters[repo_param], revision)
    r = requests.get(pushlog_url)
    r.raise_for_status()
    pushes = r.json()['pushes'].keys()
    if len(pushes) != 1:
        raise RuntimeError(
            "Unable to find a single pushlog_id for {} revision {}: {}".format(
                parameters['head_repository'], revision, pushes
            )
        )
    return pushes[0]


def get_hg_revision_branch(root, revision):
    """Given the parameters for a revision, find the hg_branch (aka
    relbranch) of the revision."""
    return subprocess.check_output(['hg', 'identify', '--branch', '--rev', revision], cwd=root)


def calculate_head_rev(root):
    # we assume that run-task has correctly checked out the revision indicated by
    # GECKO_HEAD_REF, so all that remains is to see what the current revision is.
    # Mercurial refers to that as `.`.
    return subprocess.check_output(['hg', 'log', '-r', '.', '-T', '{node}'], cwd=root)
