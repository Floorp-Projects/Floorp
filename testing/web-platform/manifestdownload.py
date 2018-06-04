from __future__ import absolute_import

import argparse
import json
import io
import os
from datetime import datetime, timedelta
import gzip
from vcs import Mercurial
import requests

from six.moves.urllib.request import urlopen

def abs_path(path):
    return os.path.abspath(os.path.expanduser(path))


def hg_commits(repo_root):
    hg = Mercurial.get_func(repo_root)
    return [item for item in hg("log", "-fl50", "--template={node}\n",
            "testing/web-platform/tests/", "testing/web-platform/mozilla/tests").split("\n")
            if item]


def should_download(logger, manifest_path, rebuild_time=timedelta(days=5)):
    # TODO: Improve logic for when to download. Maybe if x revisions behind?
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
        req = requests.get(cset_url.format(changeset=revision),
                           headers={'Accept': 'application/json'})

        req.raise_for_status()

        result = req.json()
        [cset] = result['pushes'].values()[0]['changesets']
        resp = requests.get(tc_url.format(changeset=cset))

        if req.status_code == 200:
            return tc_url.format(changeset=cset)

    logger.info("Can't find a commit-specific manifest so just using the most"
                "recent one")

    return ("https://index.taskcluster.net/v1/task/gecko.v2.mozilla-central."
            "latest.source.manifest-upload")


def download_manifest(logger, wpt_dir, commits_func, url_func, force=False):
    if not force and not should_download(logger, wpt_dir):
        return False

    commits = commits_func()
    url = url_func(logger, commits) + "/artifacts/public/"

    man_url= url + "manifest.json.gz"
    moz_man_url= url + "moz_manifest.json.gz"

    return ( _download(logger, os.path.join(wpt_dir, "meta", "MANIFEST.json"), man_url) and
             _download(logger, os.path.join(wpt_dir,"mozilla", "meta", "MANIFEST.json"), moz_man_url))


def _download(logger, manifest_path, url):
    if not url:
        logger.warning("No generated manifest found")
        return False

    logger.info("Downloading manifest from %s" % url)
    try:
        resp = urlopen(url)
    except Exception:
        logger.warning("Downloading pregenerated manifest failed")
        return False

    if resp.code != 200:
        logger.warning("Downloading pregenerated manifest failed; got"
                        "HTTP status %d" % resp.code)
        return False

    gzf = gzip.GzipFile(fileobj=io.BytesIO(resp.read()))

    try:
        decompressed = gzf.read()
    except IOError:
        logger.warning("Failed to decompress downloaded file")
        return False

    try:
        with open(manifest_path, 'wb') as f:
            f.write(decompressed)
    except Exception as e:
        logger.warning("Failed to write manifest at %s" % manifest_path)
        return False

    logger.info("Manifest at %s downloaded" % manifest_path)
    return True


def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--path", type=abs_path, help="Path to manifest file.")
    parser.add_argument(
        "--force", action="store_true",
        help="Always download, even if the existing manifest is recent")
    return parser


def download_from_taskcluster(logger, wpt_dir, repo_root, force=False):
    return download_manifest(logger, wpt_dir, lambda: hg_commits(repo_root),
                             taskcluster_url, force)


def run(logger, wpt_dir, repo_root, force=False):
    success = download_from_taskcluster(logger, wpt_dir, repo_root, force)
    return 0 if success else 1
