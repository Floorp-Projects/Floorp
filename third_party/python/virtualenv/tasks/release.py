# -*- coding: utf-8 -*-
"""Handles creating a release PR"""
from pathlib import Path
from subprocess import check_call
from typing import Tuple

from git import Commit, Head, Remote, Repo, TagReference
from packaging.version import Version

ROOT_SRC_DIR = Path(__file__).resolve().parents[1]


def main(version_str: str) -> None:
    version = Version(version_str)
    repo = Repo(str(ROOT_SRC_DIR))

    if repo.is_dirty():
        raise RuntimeError("Current repository is dirty. Please commit any changes and try again.")
    upstream, release_branch = create_release_branch(repo, version)
    release_commit = release_changelog(repo, version)
    tag = tag_release_commit(release_commit, repo, version)
    print("push release commit")
    repo.git.push(upstream.name, release_branch)
    print("push release tag")
    repo.git.push(upstream.name, tag)
    print("All done! ✨ 🍰 ✨")


def create_release_branch(repo: Repo, version: Version) -> Tuple[Remote, Head]:
    print("create release branch from upstream master")
    upstream = get_upstream(repo)
    upstream.fetch()
    branch_name = f"release-{version}"
    release_branch = repo.create_head(branch_name, upstream.refs.master, force=True)
    upstream.push(refspec=f"{branch_name}:{branch_name}", force=True)
    release_branch.set_tracking_branch(repo.refs[f"{upstream.name}/{branch_name}"])
    release_branch.checkout()
    return upstream, release_branch


def get_upstream(repo: Repo) -> Remote:
    upstream_remote = "pypa/virtualenv.git"
    urls = set()
    for remote in repo.remotes:
        for url in remote.urls:
            if url.endswith(upstream_remote):
                return remote
            urls.add(url)
    raise RuntimeError(f"could not find {upstream_remote} remote, has {urls}")


def release_changelog(repo: Repo, version: Version) -> Commit:
    print("generate release commit")
    check_call(["towncrier", "--yes", "--version", version.public], cwd=str(ROOT_SRC_DIR))
    release_commit = repo.index.commit(f"release {version}")
    return release_commit


def tag_release_commit(release_commit, repo, version) -> TagReference:
    print("tag release commit")
    existing_tags = [x.name for x in repo.tags]
    if version in existing_tags:
        print("delete existing tag {}".format(version))
        repo.delete_tag(version)
    print("create tag {}".format(version))
    tag = repo.create_tag(version, ref=release_commit, force=True)
    return tag


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(prog="release")
    parser.add_argument("--version", required=True)
    options = parser.parse_args()
    main(options.version)
