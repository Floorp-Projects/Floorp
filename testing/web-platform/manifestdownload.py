from __future__ import absolute_import

import argparse
import os
from datetime import datetime, timedelta
import tarfile
import requests
import vcs
from cStringIO import StringIO
import logging

HEADERS = {'User-Agent': "wpt manifest download"}


def get(logger, url, **kwargs):
    logger.debug(url)
    if "headers" not in kwargs:
        kwargs["headers"] = HEADERS
    return requests.get(url, **kwargs)


def abs_path(path):
    return os.path.abspath(os.path.expanduser(path))


def hg_commits(repo_root):
    hg = vcs.Mercurial.get_func(repo_root)
    for item in hg("log", "-fl50", "--template={node}\n", "testing/web-platform/tests",
                   "testing/web-platform/mozilla/tests").splitlines():
        yield item


def git_commits(repo_root):
    git = vcs.Git.get_func(repo_root)
    for item in git("log", "--format=%H", "-n50", "testing/web-platform/tests",
                    "testing/web-platform/mozilla/tests").splitlines():
        yield git("cinnabar", "git2hg", item).strip()


def get_commits(logger, repo_root):
    if vcs.Mercurial.is_hg_repo(repo_root):
        return hg_commits(repo_root)

    elif vcs.Git.is_git_repo(repo_root):
        return git_commits(repo_root)

    logger.warning("No VCS found")
    return []


def should_download(logger, manifest_paths, rebuild_time=timedelta(days=5)):
    # TODO: Improve logic for when to download. Maybe if x revisions behind?
    for manifest_path in manifest_paths:
        if not os.path.exists(manifest_path):
            return True
        mtime = datetime.fromtimestamp(os.path.getmtime(manifest_path))
        if mtime < datetime.now() - rebuild_time:
            return True
        if os.path.getsize(manifest_path) == 0:
            return True

    logger.info("Skipping manifest download because existing file is recent")
    return False


def taskcluster_url(logger, commits):
    artifact_path = '/artifacts/public/manifests.tar.gz'

    cset_url = ('https://hg.mozilla.org/mozilla-central/json-pushes?'
                'changeset={changeset}&version=2&tipsonly=1')

    tc_url = ('https://index.taskcluster.net/v1/task/gecko.v2.mozilla-central.'
              'revision.{changeset}.source.manifest-upload')

    default = ("https://index.taskcluster.net/v1/task/gecko.v2.mozilla-central."
               "latest.source.manifest-upload" +
               artifact_path)

    for revision in commits:
        req = None

        if revision == 40 * "0":
            continue
        try:
            req_headers = HEADERS.copy()
            req_headers.update({'Accept': 'application/json'})
            req = get(logger, cset_url.format(changeset=revision),
                      headers=req_headers)
            req.raise_for_status()
        except requests.exceptions.RequestException:
            if req is not None and req.status_code == 404:
                # The API returns a 404 if it can't find a changeset for the revision.
                continue
            else:
                return default

        result = req.json()

        pushes = result['pushes']
        if not pushes:
            continue
        [cset] = pushes.values()[0]['changesets']

        try:
            req = get(logger, tc_url.format(changeset=cset))
        except requests.exceptions.RequestException:
            return default

        if req.status_code == 200:
            return tc_url.format(changeset=cset) + artifact_path

    logger.info("Can't find a commit-specific manifest so just using the most "
                "recent one")

    return default


def download_manifest(logger, test_paths, commits_func, url_func, force=False):
    manifest_paths = [item["manifest_path"] for item in test_paths.itervalues()]

    if not force and not should_download(logger, manifest_paths):
        return True

    commits = commits_func()

    url = url_func(logger, commits)
    if not url:
        logger.warning("No generated manifest found")
        return False

    logger.info("Downloading manifest from %s" % url)
    try:
        req = get(logger, url)
    except Exception:
        logger.warning("Downloading pregenerated manifest failed")
        return False

    if req.status_code != 200:
        logger.warning("Downloading pregenerated manifest failed; got"
                        "HTTP status %d" % req.status_code)
        return False

    tar = tarfile.open(mode="r:gz", fileobj=StringIO(req.content))
    for paths in test_paths.itervalues():
        try:
            member = tar.getmember(paths["manifest_rel_path"].replace(os.path.sep, "/"))
        except KeyError:
            logger.warning("Failed to find downloaded manifest %s" % paths["manifest_rel_path"])
        else:
            try:
                logger.debug("Unpacking %s to %s" % (member.name, paths["manifest_path"]))
                src = tar.extractfile(member)
                with open(paths["manifest_path"], "w") as dest:
                    dest.write(src.read())
                src.close()
            except IOError:
                import traceback
                logger.warning("Failed to decompress %s:\n%s" % (paths["manifest_rel_path"], traceback.format_exc()))
                return False

        os.utime(paths["manifest_path"], None)

    return True


def download_from_taskcluster(logger, repo_root, test_paths, force=False):
    return download_manifest(logger,
                             test_paths,
                             lambda: get_commits(logger, repo_root),
                             taskcluster_url,
                             force)
