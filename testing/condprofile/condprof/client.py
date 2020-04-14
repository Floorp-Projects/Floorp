# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This module needs to stay Python 2 and 3 compatible
#
from __future__ import absolute_import
import os
import tarfile
import functools
import tempfile
import shutil
import time

from condprof import check_install  # NOQA
from condprof import progress
from condprof.util import download_file, TASK_CLUSTER, logger, ArchiveNotFound
from condprof.changelog import Changelog


ROOT_URL = "https://firefox-ci-tc.services.mozilla.com/api/index"
INDEX_PATH = "gecko.v2.%(repo)s.latest.firefox.condprof-%(platform)s"
PUBLIC_DIR = "artifacts/public/condprof"
TC_LINK = ROOT_URL + "/v1/task/" + INDEX_PATH + "/" + PUBLIC_DIR + "/"
ARTIFACT_NAME = "profile-%(platform)s-%(scenario)s-%(customization)s.tgz"
CHANGELOG_LINK = (
    ROOT_URL + "/v1/task/" + INDEX_PATH + "/" + PUBLIC_DIR + "/changelog.json"
)
DIRECT_LINK = "https://taskcluster-artifacts.net/%(task_id)s/0/public/condprof/"
CONDPROF_CACHE = "~/.condprof-cache"
RETRIES = 3
RETRY_PAUSE = 30


class ProfileNotFoundError(Exception):
    pass


def get_profile(
    target_dir,
    platform,
    scenario,
    customization="default",
    task_id=None,
    download_cache=True,
    repo="mozilla-central",
):
    """Extract a conditioned profile in the target directory.

    If task_id is provided, will grab the profile from that task. when not
    provided (default) will grab the latest profile.
    """
    # XXX assert values
    params = {
        "platform": platform,
        "scenario": scenario,
        "customization": customization,
        "task_id": task_id,
        "repo": repo,
    }
    logger.info("Getting conditioned profile with arguments: %s" % params)
    filename = ARTIFACT_NAME % params
    if task_id is None:
        url = TC_LINK % params + filename
    else:
        url = DIRECT_LINK % params + filename

    logger.info("preparing download dir")
    if not download_cache:
        download_dir = tempfile.mkdtemp()
    else:
        # using a cache dir in the user home dir
        download_dir = os.path.expanduser(CONDPROF_CACHE)
        if not os.path.exists(download_dir):
            os.makedirs(download_dir)

    downloaded_archive = os.path.join(download_dir, filename)
    logger.info("Downloaded archive path: %s" % downloaded_archive)
    retries = 0

    while retries < RETRIES:
        try:
            logger.info("Getting %s" % url)
            try:
                archive = download_file(url, target=downloaded_archive)
            except ArchiveNotFound:
                raise ProfileNotFoundError(url)

            try:
                with tarfile.open(archive, "r:gz") as tar:
                    logger.info("Extracting the tarball content in %s" % target_dir)
                    size = len(list(tar))
                    with progress.Bar(expected_size=size) as bar:

                        def _extract(self, *args, **kw):
                            if not TASK_CLUSTER:
                                bar.show(bar.last_progress + 1)
                            return self.old(*args, **kw)

                        tar.old = tar.extract
                        tar.extract = functools.partial(_extract, tar)
                        tar.extractall(target_dir)
            except (OSError, tarfile.ReadError) as e:
                logger.info("Failed to extract the tarball")
                if download_cache and os.path.exists(archive):
                    logger.info("Removing cached file to attempt a new download")
                    os.remove(archive)
                raise ProfileNotFoundError(str(e))
            finally:
                if not download_cache:
                    shutil.rmtree(download_dir)
            logger.info("Success, we have a profile to work with")
            return target_dir
        except Exception:
            logger.info("Failed to get the profile.")
            retries += 1
            if os.path.exists(downloaded_archive):
                try:
                    os.remove(downloaded_archive)
                except Exception:
                    logger.error("Could not remove the file")
            time.sleep(RETRY_PAUSE)

    # If we reach that point, it means all attempts failed
    logger.error("All attempt failed")
    raise ProfileNotFoundError(url)


def read_changelog(platform, repo="mozilla-central"):
    params = {"platform": platform, "repo": repo}
    changelog_url = CHANGELOG_LINK % params
    logger.info("Getting %s" % changelog_url)
    download_dir = tempfile.mkdtemp()
    downloaded_changelog = os.path.join(download_dir, "changelog.json")
    try:
        download_file(changelog_url, target=downloaded_changelog)
    except ArchiveNotFound:
        shutil.rmtree(download_dir)
        raise ProfileNotFoundError(changelog_url)
    return Changelog(download_dir)
