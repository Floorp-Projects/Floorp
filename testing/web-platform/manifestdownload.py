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
    return False


def should_download(logger, manifest_paths, rebuild_time=timedelta(days=5)):
    # TODO: Improve logic for when to download. Maybe if x revisions behind?
    for manifest_path in manifest_paths:
        if not os.path.exists(manifest_path):
            return True
        mtime = datetime.fromtimestamp(os.path.getmtime(manifest_path))
        if mtime < datetime.now() - rebuild_time:
            return True

    logger.info("Skipping manifest download because existing file is recent")
    return False


def taskcluster_url(logger, commits):
    cset_url = ('https://hg.mozilla.org/mozilla-central/json-pushes?'
                'changeset={changeset}&version=2&tipsonly=1')

    tc_url = ('https://index.taskcluster.net/v1/task/gecko.v2.mozilla-central.'
              'revision.{changeset}.source.manifest-upload')

    for revision in commits:
        if revision == 40 * "0":
            continue
        try:
            req_headers = HEADERS.copy()
            req_headers.update({'Accept': 'application/json'})
            req = requests.get(cset_url.format(changeset=revision),
                               headers=req_headers)
            req.raise_for_status()
        except requests.exceptions.RequestException:
            if req.status_code == 404:
                # The API returns a 404 if it can't find a changeset for the revision.
                continue
            else:
                return False

        result = req.json()

        pushes = result['pushes']
        if not pushes:
            continue
        [cset] = pushes.values()[0]['changesets']

        try:
            req = requests.get(tc_url.format(changeset=cset),
                               headers=HEADERS)
        except requests.exceptions.RequestException:
            return False

        if req.status_code == 200:
            return tc_url.format(changeset=cset)

    logger.info("Can't find a commit-specific manifest so just using the most"
                "recent one")

    return ("https://index.taskcluster.net/v1/task/gecko.v2.mozilla-central."
            "latest.source.manifest-upload")


def download_manifest(logger, wpt_dir, commits_func, url_func, force=False):
    manifest_path = os.path.join(wpt_dir, "meta", "MANIFEST.json")
    mozilla_manifest_path = os.path.join(wpt_dir, "mozilla", "meta", "MANIFEST.json")

    if not force and not should_download(logger, [manifest_path, mozilla_manifest_path]):
        return True

    commits = commits_func()
    if not commits:
        return False

    url = url_func(logger, commits)
    if not url:
        logger.warning("No generated manifest found")
        return False
    url+= "/artifacts/public/manifests.tar.gz"

    logger.info("Downloading manifest from %s" % url)
    try:
        req = requests.get(url, headers=HEADERS)
    except Exception:
        logger.warning("Downloading pregenerated manifest failed")
        return False

    if req.status_code != 200:
        logger.warning("Downloading pregenerated manifest failed; got"
                        "HTTP status %d" % req.status_code)
        return False

    tar = tarfile.open(mode="r:gz", fileobj=StringIO(req.content))
    try:
        tar.extractall(path=wpt_dir)
    except IOError:
        logger.warning("Failed to decompress downloaded file")
        return False

    os.utime(manifest_path, None)
    os.utime(mozilla_manifest_path, None)

    logger.info("Manifest downloaded")
    return True


def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--path", type=abs_path, help="Path to manifest file.")
    parser.add_argument(
        "--force", action="store_true",
        help="Always download, even if the existing manifest is recent")
    parser.add_argument(
        "--no-manifest-update", action="store_false", dest="manifest_update",
        default=True, help="Don't update the downloaded manifest")
    return parser


def download_from_taskcluster(logger, wpt_dir, repo_root, force=False):
    return download_manifest(logger, wpt_dir, lambda: get_commits(logger, repo_root),
                             taskcluster_url, force)


def generate_config(path):
    """Generate the local wptrunner.ini file to use locally"""
    import ConfigParser
    here = os.path.split(os.path.abspath(__file__))[0]
    config_path = os.path.join(here, 'wptrunner.ini')
    path = os.path.join(path, 'wptrunner.local.ini')

    if os.path.exists(path):
        return True

    parser = ConfigParser.SafeConfigParser()
    success = parser.read(config_path)
    assert config_path in success, success

    parser.set('manifest:upstream', 'tests', os.path.join(here, 'tests'))
    parser.set('manifest:mozilla', 'tests', os.path.join(here, 'mozilla', 'tests'))
    parser.set('paths', 'prefs', os.path.join(os.getcwd(), 'testing', 'profiles'))

    with open(path, 'wb') as config_file:
        parser.write(config_file)
    return True


def update_manifest(logger, config_dir, manifest_update=True):
    if manifest_update:
        logger.info("Updating manifests")
        import manifestupdate
        here = os.path.split(os.path.abspath(__file__))[0]
        return manifestupdate.update(logger, here, config_dir=config_dir) is 0
    else:
        logger.info("Skipping manifest update")
        return True

def check_dirs(logger, success, wpt_dir):
    if success:
        return
    else:
        logger.info("Could not download manifests.")
        logger.info("Generating from scratch instead.")
        try:
            os.mkdir(os.path.join(wpt_dir, "meta"))
        except OSError:
            pass
        try:
            os.makedirs(os.path.join(wpt_dir, "mozilla", "meta"))
        except OSError:
            pass


def run(wpt_dir, repo_root, logger=None, force=False, manifest_update=True):
    if not logger:
        logger = logging.getLogger(__name__)
        handler = logging.FileHandler(os.devnull)
        logger.addHandler(handler)

    success = download_from_taskcluster(logger, wpt_dir, repo_root, force)
    check_dirs(logger, success, wpt_dir)
    generate_config(wpt_dir)
    success |= update_manifest(logger, wpt_dir, manifest_update)
    return 0 if success else 1
