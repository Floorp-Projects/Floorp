# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add github links for release notifications
"""
import os

from mozilla_version.mobile import MobileVersion
from taskgraph.util.vcs import get_repository
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


CHANGESET_URL_TEMPLATE = "{repo}/compare/{from_version}...{to_version}"
TAG_PREFIX = "focus-v"


def get_previous_tag_version(current_version, tags):
    prefixlen = len(TAG_PREFIX)
    versions = [MobileVersion.parse(t[prefixlen:]) for t in tags]
    return max(v for v in versions if v < current_version)


@transforms.add
def add_github_link(config, tasks):
    version = MobileVersion.parse(config.params["version"])
    repo = get_repository(".")
    git_tags = repo.run("tag", "-l", TAG_PREFIX + "*").splitlines()
    previous_version = get_previous_tag_version(version, git_tags)
    current_revision = config.params["head_rev"]
    repo_url = config.params["base_repository"]
    compare_url = CHANGESET_URL_TEMPLATE.format(
        repo=repo_url,
        from_version=f"{TAG_PREFIX}{previous_version}",
        to_version=current_revision,
    )
    for task in tasks:
        task["notifications"][
            "message"
        ] = """\
Commit: [{revision}]({repo}/commit/{revision})
Task group: [{task_group_id}]({root_url}/tasks/groups/{task_group_id})

Comparing git changes from {from_version} to {to_version}:
{compare_url}
""".format(
            repo=repo_url,
            revision=current_revision,
            # TASK_ID isn't set when e.g. using taskgraph locally
            task_group_id=os.getenv("TASK_ID", "<decision>"),
            root_url=os.getenv("TASKCLUSTER_ROOT_URL"),
            from_version=previous_version,
            to_version=version,
            compare_url=compare_url,
        )
        yield task
