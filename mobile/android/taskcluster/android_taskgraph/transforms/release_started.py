# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add github links for release notifications
"""
from datetime import datetime
import os
import re

from mozilla_version.mobile import MobileVersion
from taskgraph.util.vcs import get_repository
from taskgraph.util.taskcluster import find_task_id, get_task_definition
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


CHANGESET_URL_TEMPLATE = "{repo}/compare/{from_version}...{to_version}"
TAG_PREFIX = "focus-v"
# XXX is there a better index we can use to get from buildid to revision?
GECKO_INDEX = "gecko.v2.{repo}.pushdate.{build_date}.{build_timestamp}.mobile-l10n.android-geckoview-fat-aar-opt.multi"
GECKO_KT = "android-components/plugins/dependencies/src/main/java/Gecko.kt"
BUILDID_RE = re.compile(r'version = "[0-9]*\.[0-9]*\.([0-9]*)"', re.MULTILINE)
CHANNEL_RE = re.compile(
    r"val channel = GeckoChannel.(NIGHTLY|BETA|RELEASE)", re.MULTILINE
)
HG_CHANGESET_URL_TEMPLATE = (
    "{repo}/{logtype}?rev={to_version}+%25+{from_version}&revcount=1000"
)

REPO_FROM_CHANNEL = {
    "NIGHTLY": "mozilla-central",
    "BETA": "mozilla-beta",
    "RELEASE": "mozilla-release",
}


def get_gecko_revision(channel, buildid):
    repo = REPO_FROM_CHANNEL[channel]
    build_date = datetime.strptime(buildid, "%Y%m%d%H%M%S").strftime("%Y.%m.%d")
    index_path = GECKO_INDEX.format(
        repo=repo, build_date=build_date, build_timestamp=buildid
    )
    task_id = find_task_id(index_path)
    task_definition = get_task_definition(task_id)
    rev = task_definition["payload"]["env"]["GECKO_HEAD_REV"]
    return rev


def get_gecko_channel_and_buildid(repo, commit):
    gecko_kt = repo.run("show", f"{commit}:{GECKO_KT}")
    buildid_match = BUILDID_RE.search(gecko_kt)
    channel_match = CHANNEL_RE.search(gecko_kt)
    if not buildid_match or not channel_match:
        return
    return channel_match[1], buildid_match[1]


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
    try:
        previous_gecko_revision = get_gecko_revision(
            *get_gecko_channel_and_buildid(repo, f"{TAG_PREFIX}{previous_version}")
        )
        gecko_channel, gecko_buildid = get_gecko_channel_and_buildid(
            repo, current_revision
        )
        assert gecko_channel in ("BETA", "RELEASE")
        current_gecko_revision = get_gecko_revision(gecko_channel, gecko_buildid)
        gecko_url = (
            f"https://hg.mozilla.org/releases/{REPO_FROM_CHANNEL[gecko_channel]}"
        )
        gecko_changelog_url = HG_CHANGESET_URL_TEMPLATE.format(
            repo=gecko_url,
            logtype="log",
            from_version=previous_gecko_revision,
            to_version=current_gecko_revision,
        )
        gecko_changelog = f"[Full Gecko mercurial changelog]({gecko_changelog_url})"
    except Exception:
        gecko_changelog = ""

    for task in tasks:
        task["notifications"][
            "message"
        ] = """\
Commit: [{revision}]({repo}/commit/{revision})
Task group: [{task_group_id}]({root_url}/tasks/groups/{task_group_id})

Comparing git changes from {from_version} to {to_version}:
[Full Git changelog]({compare_url})
{gecko_changelog}
""".format(
            repo=repo_url,
            revision=current_revision,
            # TASK_ID isn't set when e.g. using taskgraph locally
            task_group_id=os.getenv("TASK_ID", "<decision>"),
            root_url=os.getenv("TASKCLUSTER_ROOT_URL"),
            from_version=previous_version,
            to_version=version,
            compare_url=compare_url,
            gecko_changelog=gecko_changelog,
        )
        yield task
