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

from mozprofile.prefs import Preferences

from condprof import check_install  # NOQA
from condprof import progress
from condprof.util import (
    download_file,
    TASK_CLUSTER,
    logger,
    check_exists,
    ArchiveNotFound,
)
from condprof.changelog import Changelog


TC_SERVICE = "https://firefox-ci-tc.services.mozilla.com"
ROOT_URL = TC_SERVICE + "/api/index"
INDEX_PATH = "gecko.v2.%(repo)s.latest.firefox.condprof-%(platform)s"
PUBLIC_DIR = "artifacts/public/condprof"
TC_LINK = ROOT_URL + "/v1/task/" + INDEX_PATH + "/" + PUBLIC_DIR + "/"
ARTIFACT_NAME = "profile-%(platform)s-%(scenario)s-%(customization)s.tgz"
CHANGELOG_LINK = (
    ROOT_URL + "/v1/task/" + INDEX_PATH + "/" + PUBLIC_DIR + "/changelog.json"
)
ARTIFACTS_SERVICE = "https://taskcluster-artifacts.net"
DIRECT_LINK = ARTIFACTS_SERVICE + "/%(task_id)s/0/public/condprof/"
CONDPROF_CACHE = "~/.condprof-cache"
RETRIES = 3
RETRY_PAUSE = 45


class ServiceUnreachableError(Exception):
    pass


class ProfileNotFoundError(Exception):
    pass


class RetriesError(Exception):
    pass


def _check_service(url):
    """Sanity check to see if we can reach the service root url."""

    def _check():
        exists, _ = check_exists(url, all_types=True)
        if not exists:
            raise ServiceUnreachableError(url)

    try:
        return _retries(_check)
    except RetriesError:
        raise ServiceUnreachableError(url)


def _check_profile(profile_dir):
    """Checks for prefs we need to remove or set."""
    to_remove = ("gfx.blacklist.", "marionette.")

    def _keep_pref(name, value):
        for item in to_remove:
            if not name.startswith(item):
                continue
            logger.info("Removing pref %s: %s" % (name, value))
            return False
        return True

    def _clean_pref_file(name):
        js_file = os.path.join(profile_dir, name)
        prefs = Preferences.read_prefs(js_file)
        cleaned_prefs = dict([pref for pref in prefs if _keep_pref(*pref)])
        if name == "prefs.js":
            # When we start Firefox, forces startupScanScopes to SCOPE_PROFILE (1)
            # otherwise, side loading will be deactivated and the
            # Raptor web extension won't be able to run.
            cleaned_prefs["extensions.startupScanScopes"] = 1

            # adding a marker so we know it's a conditioned profile
            cleaned_prefs["profile.conditioned"] = True

        with open(js_file, "w") as f:
            Preferences.write(f, cleaned_prefs)

    _clean_pref_file("prefs.js")
    _clean_pref_file("user.js")


def _retries(callable, onerror=None):
    retries = 0
    pause = RETRY_PAUSE

    while retries < RETRIES:
        try:
            return callable()
        except Exception as e:
            if onerror is not None:
                onerror(e)
            logger.info("Failed, retrying")
            retries += 1
            time.sleep(pause)
            pause *= 1.5

    # If we reach that point, it means all attempts failed
    logger.error("All attempt failed")
    raise RetriesError()


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
        _check_service(TC_SERVICE)
    else:
        url = DIRECT_LINK % params + filename
        _check_service(ARTIFACTS_SERVICE)

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

    def _get_profile():
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

        _check_profile(target_dir)
        logger.info("Success, we have a profile to work with")
        return target_dir

    def onerror(error):
        logger.info("Failed to get the profile.")
        if os.path.exists(downloaded_archive):
            try:
                os.remove(downloaded_archive)
            except Exception:
                logger.error("Could not remove the file")

    try:
        return _retries(_get_profile, onerror)
    except RetriesError:
        raise ProfileNotFoundError(url)


def read_changelog(platform, repo="mozilla-central"):
    params = {"platform": platform, "repo": repo}
    changelog_url = CHANGELOG_LINK % params
    logger.info("Getting %s" % changelog_url)
    download_dir = tempfile.mkdtemp()
    downloaded_changelog = os.path.join(download_dir, "changelog.json")

    def _get_changelog():
        try:
            download_file(changelog_url, target=downloaded_changelog)
        except ArchiveNotFound:
            shutil.rmtree(download_dir)
            raise ProfileNotFoundError(changelog_url)
        return Changelog(download_dir)

    try:
        return _retries(_get_changelog)
    except Exception:
        raise ProfileNotFoundError(changelog_url)
