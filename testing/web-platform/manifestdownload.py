from __future__ import absolute_import

import os
from datetime import datetime, timedelta
import tarfile
import requests
import six
import mozversioncontrol

try:
    from cStringIO import StringIO as BytesIO
except ImportError:
    from io import BytesIO

HEADERS = {'User-Agent': "wpt manifest download"}


def get(logger, url, **kwargs):
    logger.debug(url)
    if "headers" not in kwargs:
        kwargs["headers"] = HEADERS
    return requests.get(url, **kwargs)


def abs_path(path):
    return os.path.abspath(os.path.expanduser(path))


def get_commits(logger, repo_root):
    try:
        repo = mozversioncontrol.get_repository_object(repo_root)
    except mozversioncontrol.InvalidRepoPath:
        logger.warning("No VCS found for path %s" % repo_root)
        return []

    # The base_ref doesn't actually return a ref, sadly
    base_rev = repo.base_ref
    if repo.name == "git":
        logger.debug("Found git repo")
        logger.debug("Base rev is %s" % base_rev)
        if not repo.has_git_cinnabar:
            logger.error("git cinnabar not found")
            return []
        changeset_iter = (repo._run("cinnabar", "git2hg", rev).strip() for rev in
                          repo._run("log",
                                    "--format=%H",
                                    "-n50",
                                    base_rev,
                                    "testing/web-platform/tests",
                                    "testing/web-platform/mozilla/tests").splitlines())
    else:
        logger.debug("Found hg repo")
        logger.debug("Base rev is %s" % base_rev)
        changeset_iter = repo._run("log",
                                   "-fl50",
                                   "--template={node}\n",
                                   "-r", base_rev,
                                   "testing/web-platform/tests",
                                   "testing/web-platform/mozilla/tests").splitlines()
    return changeset_iter


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

    repos = {"mozilla-central": "mozilla-central",
             "integration/autoland": "autoland"}
    cset_url = ('https://hg.mozilla.org/{repo}/json-pushes?'
                'changeset={changeset}&version=2&tipsonly=1')

    tc_url = ('https://firefox-ci-tc.services.mozilla.com/api/index/v1/'
              'task/gecko.v2.{name}.'
              'revision.{changeset}.source.manifest-upload')

    default = ("https://firefox-ci-tc.services.mozilla.com/api/index/v1/"
               "task/gecko.v2.mozilla-central.latest.source.manifest-upload" +
               artifact_path)

    for revision in commits:
        req = None

        if revision == 40 * "0":
            continue

        for repo_path, index_name in six.iteritems(repos):
            try:
                req_headers = HEADERS.copy()
                req_headers.update({'Accept': 'application/json'})
                req = get(logger, cset_url.format(changeset=revision, repo=repo_path),
                          headers=req_headers)
                req.raise_for_status()
            except requests.exceptions.RequestException:
                if req is not None and req.status_code == 404:
                    # The API returns a 404 if it can't find a changeset for the revision.
                    logger.debug("%s not found in %s" % (revision, repo_path))
                    continue
                else:
                    return default

            result = req.json()

            pushes = result['pushes']
            if not pushes:
                logger.debug("Error reading response; 'pushes' key not found")
                continue
            [cset] = pushes.values()[0]['changesets']

            tc_index_url = tc_url.format(changeset=cset, name=index_name)
            try:
                req = get(logger, tc_index_url)
            except requests.exceptions.RequestException:
                return default

            if req.status_code == 200:
                return tc_index_url + artifact_path

    logger.info("Can't find a commit-specific manifest so just using the most "
                "recent one")

    return default


def download_manifest(logger, test_paths, commits_func, url_func, force=False):
    manifest_paths = [item["manifest_path"] for item in six.itervalues(test_paths)]

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

    tar = tarfile.open(mode="r:gz", fileobj=BytesIO(req.content))
    for paths in six.itervalues(test_paths):
        try:
            member = tar.getmember(paths["manifest_rel_path"].replace(os.path.sep, "/"))
        except KeyError:
            logger.warning("Failed to find downloaded manifest %s" % paths["manifest_rel_path"])
        else:
            try:
                logger.debug("Unpacking %s to %s" % (member.name, paths["manifest_path"]))
                src = tar.extractfile(member)
                with open(paths["manifest_path"], "wb") as dest:
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
