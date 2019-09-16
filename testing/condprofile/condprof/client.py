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

from condprof import check_install  # NOQA
from condprof import progress
from condprof.util import check_exists, download_file, TASK_CLUSTER, get_logger
from condprof.changelog import Changelog


ROOT_URL = "https://index.taskcluster.net"
INDEX_PATH = "gecko.v2.try.latest.firefox.condprof-%(platform)s"
PUBLIC_DIR = "artifacts/public/condprof"
TC_LINK = ROOT_URL + "/v1/task/" + INDEX_PATH + "/" + PUBLIC_DIR + "/"
ARTIFACT_NAME = "profile-%(platform)s-%(scenario)s-%(customization)s.tgz"
CHANGELOG_LINK = (
    ROOT_URL + "/v1/task/" + INDEX_PATH + "/" + PUBLIC_DIR + "/changelog.json"
)
DIRECT_LINK = "https://taskcluster-artifacts.net/%(task_id)s/0/public/condprof/"


class ProfileNotFoundError(Exception):
    pass


def get_profile(target_dir, platform, scenario, customization="default", task_id=None):
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
    }
    filename = ARTIFACT_NAME % params
    if task_id is None:
        url = TC_LINK % params + filename
    else:
        url = DIRECT_LINK % params + filename

    download_dir = tempfile.mkdtemp()
    downloaded_archive = os.path.join(download_dir, filename)
    get_logger().msg("Getting %s" % url)
    exists, __ = check_exists(url)
    if exists != 200:
        raise ProfileNotFoundError(exists)

    archive = download_file(url, target=downloaded_archive)
    try:
        with tarfile.open(archive, "r:gz") as tar:
            get_logger().msg("Extracting the tarball content in %s" % target_dir)
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
        raise ProfileNotFoundError(str(e))
    finally:
        shutil.rmtree(download_dir)
    get_logger().msg("Success, we have a profile to work with")
    return target_dir


def read_changelog(platform):
    params = {"platform": platform}
    changelog_url = CHANGELOG_LINK % params
    get_logger().msg("Getting %s" % changelog_url)
    exists, __ = check_exists(changelog_url)
    if exists != 200:
        raise ProfileNotFoundError(exists)
    download_dir = tempfile.mkdtemp()
    downloaded_changelog = os.path.join(download_dir, "changelog.json")
    download_file(changelog_url, target=downloaded_changelog)
    return Changelog(download_dir)


def main():
    # XXX demo. download an older version of a profile, given a task id
    # plat = get_current_platform()
    older_change = read_changelog("win64").history()[0]
    task_id = older_change["TASK_ID"]
    target_dir = tempfile.mkdtemp()
    filename = get_profile(target_dir, "win64", "cold", "default", task_id)
    print("Profile downloaded and extracted at %s" % filename)


if __name__ == "__main__":
    main()
