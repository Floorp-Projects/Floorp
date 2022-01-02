# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Support for optimizing tasks based on the set of files that have changed.
"""

import logging
import os

from mozpack.path import match as mozpackmatch, join as join_path
from mozversioncontrol import get_repository_object, InvalidRepoPath
from subprocess import CalledProcessError
from mozbuild.util import memoize

from gecko_taskgraph import GECKO
from gecko_taskgraph.util.hg import get_json_automationrelevance

logger = logging.getLogger(__name__)


@memoize
def get_changed_files(repository, revision):
    """
    Get the set of files changed in the push headed by the given revision.
    Responses are cached, so multiple calls with the same arguments are OK.
    """
    contents = get_json_automationrelevance(repository, revision)
    try:
        changesets = contents["changesets"]
    except KeyError:
        # We shouldn't hit this error in CI.
        if os.environ.get("MOZ_AUTOMATION"):
            raise

        # We're likely on an unpublished commit, grab changed files from
        # version control.
        return get_locally_changed_files(GECKO)

    logger.debug("{} commits influencing task scheduling:".format(len(changesets)))
    changed_files = set()
    for c in changesets:
        desc = ""  # Support empty desc
        if c["desc"]:
            desc = c["desc"].splitlines()[0].encode("ascii", "ignore")
        logger.debug(" {cset} {desc}".format(cset=c["node"][0:12], desc=desc))
        changed_files |= set(c["files"])

    return changed_files


def check(params, file_patterns):
    """Determine whether any of the files changed in the indicated push to
    https://hg.mozilla.org match any of the given file patterns."""
    repository = params.get("head_repository")
    revision = params.get("head_rev")
    if not repository or not revision:
        logger.warning(
            "Missing `head_repository` or `head_rev` parameters; "
            "assuming all files have changed"
        )
        return True

    changed_files = get_changed_files(repository, revision)

    if "comm_head_repository" in params:
        repository = params.get("comm_head_repository")
        revision = params.get("comm_head_rev")
        if not revision:
            logger.warning(
                "Missing `comm_head_rev` parameters; " "assuming all files have changed"
            )
            return True

        changed_files |= {
            join_path("comm", file) for file in get_changed_files(repository, revision)
        }

    for pattern in file_patterns:
        for path in changed_files:
            if mozpackmatch(path, pattern):
                return True

    return False


@memoize
def get_locally_changed_files(repo):
    try:
        vcs = get_repository_object(repo)
        return set(vcs.get_outgoing_files("AM"))
    except (InvalidRepoPath, CalledProcessError):
        return set()
