# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path

import hglib
from compare_locales import merge
from mozpack import path as mozpath

from . import projectconfig, source


def get_default_config(topsrcdir, strings_path):
    assert isinstance(topsrcdir, Path)
    assert isinstance(strings_path, Path)
    return {
        "strings": {
            "path": strings_path,
            "url": "https://hg.mozilla.org/l10n/gecko-strings-quarantine/",
            "heads": {"default": "default"},
            "update_on_pull": True,
            "push_url": "ssh://hg.mozilla.org/l10n/gecko-strings-quarantine/",
        },
        "source": {
            "mozilla-unified": {
                "path": topsrcdir,
                "url": "https://hg.mozilla.org/mozilla-unified/",
                "heads": {
                    # This list of repositories is ordered, starting with the
                    # one with the most recent content (central) to the oldest
                    # (ESR). In case two ESR versions are supported, the oldest
                    # ESR goes last (e.g. esr78 goes after esr91).
                    "central": "mozilla-central",
                    "beta": "releases/mozilla-beta",
                    "release": "releases/mozilla-release",
                    "esr115": "releases/mozilla-esr115",
                },
                "config_files": [
                    "browser/locales/l10n.toml",
                    "mobile/android/locales/l10n.toml",
                ],
            },
        },
    }


@dataclass
class TargetRevs:
    target: bytes = None
    revs: list = field(default_factory=list)


@dataclass
class CommitRev:
    repo: str
    rev: bytes

    @property
    def message(self):
        return (
            f"X-Channel-Repo: {self.repo}\n"
            f'X-Channel-Revision: {self.rev.decode("ascii")}'
        )


class CrossChannelCreator:
    def __init__(self, config):
        self.config = config
        self.strings_path = config["strings"]["path"]
        self.message = (
            f"cross-channel content for {datetime.utcnow().strftime('%Y-%m-%d %H:%M')}"
        )

    def create_content(self):
        self.prune_target()
        revs = []
        for repo_name, repo_config in self.config["source"].items():
            with hglib.open(repo_config["path"]) as repo:
                revs.extend(self.create_for_repo(repo, repo_name, repo_config))
        self.commit(revs)
        return 0

    def prune_target(self):
        for leaf in self.config["strings"]["path"].iterdir():
            if leaf.name == ".hg":
                continue
            shutil.rmtree(leaf)

    def create_for_repo(self, repo, repo_name, repo_config):
        print(f"Processing {repo_name} in {repo_config['path']}")
        source_target_revs = defaultdict(TargetRevs)
        revs_for_commit = []
        parse_kwargs = {
            "env": {"l10n_base": str(self.strings_path.parent)},
            "ignore_missing_includes": True,
        }
        for head, head_name in repo_config["heads"].items():
            print(f"Gathering files for {head}")
            rev = repo.log(revrange=head)[0].node
            revs_for_commit.append(CommitRev(head_name, rev))
            p = source.HgTOMLParser(repo, rev)
            project_configs = []
            for config_file in repo_config["config_files"]:
                project_configs.append(p.parse(config_file, **parse_kwargs))
                project_configs[-1].set_locales(["en-US"], deep=True)
            hgfiles = source.HGFiles(repo, rev, project_configs)
            for targetpath, refpath, _, _ in hgfiles:
                source_target_revs[refpath].revs.append(rev)
                source_target_revs[refpath].target = targetpath
        root = repo.root()
        print(f"Writing {repo_name} content to target")
        for refpath, targetrevs in source_target_revs.items():
            local_ref = mozpath.relpath(refpath, root)
            content = self.get_content(local_ref, repo, targetrevs.revs)
            target_dir = mozpath.dirname(targetrevs.target)
            if not os.path.isdir(target_dir):
                os.makedirs(target_dir)
            with open(targetrevs.target, "wb") as fh:
                fh.write(content)
        return revs_for_commit

    def commit(self, revs):
        message = self.message + "\n\n"
        if "TASK_ID" in os.environ:
            message += f"X-Task-ID: {os.environ['TASK_ID']}\n\n"
        message += "\n".join(rev.message for rev in revs)
        with hglib.open(self.strings_path) as repo:
            repo.commit(message=message, addremove=True)

    def get_content(self, local_ref, repo, revs):
        if local_ref.endswith(b".toml"):
            return self.get_config_content(local_ref, repo, revs)
        if len(revs) < 2:
            return repo.cat([b"path:" + local_ref], rev=revs[0])
        contents = [repo.cat([b"path:" + local_ref], rev=rev) for rev in revs]
        try:
            return merge.merge_channels(local_ref.decode("utf-8"), contents)
        except merge.MergeNotSupportedError:
            return contents[0]

    def get_config_content(self, local_ref, repo, revs):
        # We don't support merging toml files
        content = repo.cat([b"path:" + local_ref], rev=revs[0])
        return projectconfig.process_config(content)
