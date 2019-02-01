# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import requests
import subprocess
from redo import retry

PUSHLOG_TMPL = '{}/json-pushes?version=2&changeset={}&tipsonly=1&full=1'


def find_hg_revision_push_info(repository, revision):
    """Given the parameters for this action and a revision, find the
    pushlog_id of the revision."""
    pushlog_url = PUSHLOG_TMPL.format(repository, revision)

    def query_pushlog(url):
        r = requests.get(pushlog_url, timeout=60)
        r.raise_for_status()
        return r
    r = retry(
        query_pushlog, args=(pushlog_url,),
        attempts=5, sleeptime=10,
    )
    pushes = r.json()['pushes']
    if len(pushes) != 1:
        raise RuntimeError(
            "Unable to find a single pushlog_id for {} revision {}: {}".format(
                repository, revision, pushes
            )
        )
    pushid = pushes.keys()[0]
    return {'pushdate': pushes[pushid]['date'], 'pushid': pushid}


def get_hg_revision_branch(root, revision):
    """Given the parameters for a revision, find the hg_branch (aka
    relbranch) of the revision."""
    return subprocess.check_output([
        'hg', 'identify',
        '-T', '{branch}',
        '--rev', revision,
    ], cwd=root)


# For these functions, we assume that run-task has correctly checked out the
# revision indicated by GECKO_HEAD_REF, so all that remains is to see what the
# current revision is.  Mercurial refers to that as `.`.
def get_hg_commit_message(root):
    return subprocess.check_output(['hg', 'log', '-r', '.', '-T', '{desc}'], cwd=root)


def calculate_head_rev(root):
    return subprocess.check_output(['hg', 'log', '-r', '.', '-T', '{node}'], cwd=root)
