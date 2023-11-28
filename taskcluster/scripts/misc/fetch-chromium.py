#!/usr/bin/python3 -u
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script downloads the latest chromium build (or a manually
defined version) for a given platform. It then uploads the build,
with the revision of the build stored in a REVISION file.
"""

import argparse
import errno
import os
import shutil
import subprocess
import tempfile

import requests
from redo import retriable

LAST_CHANGE_URL = (
    # formatted with platform
    "https://www.googleapis.com/download/storage/v1/b/"
    "chromium-browser-snapshots/o/{}%2FLAST_CHANGE?alt=media"
)

CHROMIUM_BASE_URL = (
    # formatted with (platform/revision/archive)
    "https://www.googleapis.com/download/storage/v1/b/"
    "chromium-browser-snapshots/o/{}%2F{}%2F{}?alt=media"
)


CHROMIUM_INFO = {
    "linux": {
        "platform": "Linux_x64",
        "chromium": "chrome-linux.zip",
        "result": "chromium-linux.tar.bz2",
        "chromedriver": "chromedriver_linux64.zip",
    },
    "win32": {
        "platform": "Win",
        "chromium": "chrome-win.zip",
        "result": "chromium-win32.tar.bz2",
        "chromedriver": "chromedriver_win32.zip",
    },
    "win64": {
        "platform": "Win",
        "chromium": "chrome-win.zip",
        "result": "chromium-win64.tar.bz2",
        "chromedriver": "chromedriver_win32.zip",
    },
    "mac": {
        "platform": "Mac",
        "chromium": "chrome-mac.zip",
        "result": "chromium-mac.tar.bz2",
        "chromedriver": "chromedriver_mac64.zip",
    },
    "mac-arm": {
        "platform": "Mac_Arm",
        "chromium": "chrome-mac.zip",
        "result": "chromium-mac-arm.tar.bz2",
        "chromedriver": "chromedriver_mac64.zip",
    },
}


def log(msg):
    print("build-chromium: %s" % msg)


@retriable(attempts=7, sleeptime=5, sleepscale=2)
def fetch_file(url, filepath):
    """Download a file from the given url to a given file."""
    size = 4096
    r = requests.get(url, stream=True)
    r.raise_for_status()

    with open(filepath, "wb") as fd:
        for chunk in r.iter_content(size):
            fd.write(chunk)


def unzip(zippath, target):
    """Unzips an archive to the target location."""
    log("Unpacking archive at: %s to: %s" % (zippath, target))
    unzip_command = ["unzip", "-q", "-o", zippath, "-d", target]
    subprocess.check_call(unzip_command)


@retriable(attempts=7, sleeptime=5, sleepscale=2)
def fetch_chromium_revision(platform):
    """Get the revision of the latest chromium build."""
    chromium_platform = CHROMIUM_INFO[platform]["platform"]
    revision_url = LAST_CHANGE_URL.format(chromium_platform)

    log("Getting revision number for latest %s chromium build..." % chromium_platform)

    # Expecting a file with a single number indicating the latest
    # chromium build with a chromedriver that we can download
    r = requests.get(revision_url, timeout=30)
    r.raise_for_status()

    chromium_revision = r.content.decode("utf-8")
    return chromium_revision.strip()


def fetch_chromium_build(platform, revision, zippath):
    """Download a chromium build for a given revision, or the latest."""
    if not revision:
        revision = fetch_chromium_revision(platform)

    download_platform = CHROMIUM_INFO[platform]["platform"]
    download_url = CHROMIUM_BASE_URL.format(
        download_platform, revision, CHROMIUM_INFO[platform]["chromium"]
    )

    log("Downloading %s chromium build revision %s..." % (download_platform, revision))
    log(download_url)
    fetch_file(download_url, zippath)
    return revision


def fetch_chromedriver(platform, revision, chromium_dir):
    """Get the chromedriver for the given revision and repackage it."""
    download_url = CHROMIUM_BASE_URL.format(
        CHROMIUM_INFO[platform]["platform"],
        revision,
        CHROMIUM_INFO[platform]["chromedriver"],
    )

    tmpzip = os.path.join(tempfile.mkdtemp(), "cd-tmp.zip")
    log("Downloading chromedriver from %s" % download_url)
    fetch_file(download_url, tmpzip)

    tmppath = tempfile.mkdtemp()
    unzip(tmpzip, tmppath)

    # Find the chromedriver then copy it to the chromium directory
    cd_path = None
    for dirpath, _, filenames in os.walk(tmppath):
        for filename in filenames:
            if filename == "chromedriver" or filename == "chromedriver.exe":
                cd_path = os.path.join(dirpath, filename)
                break
        if cd_path is not None:
            break
    if cd_path is None:
        raise Exception("Could not find chromedriver binary in %s" % tmppath)
    log("Copying chromedriver from: %s to: %s" % (cd_path, chromium_dir))
    shutil.copy(cd_path, chromium_dir)


def build_chromium_archive(platform, revision=None):
    """
    Download and store a chromium build for a given platform.

    Retrieves either the latest version, or uses a pre-defined version if
    the `--revision` option is given a revision.
    """
    upload_dir = os.environ.get("UPLOAD_DIR")
    if upload_dir:
        # Create the upload directory if it doesn't exist.
        try:
            log("Creating upload directory in %s..." % os.path.abspath(upload_dir))
            os.makedirs(upload_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

    # Make a temporary location for the file
    tmppath = tempfile.mkdtemp()
    tmpzip = os.path.join(tmppath, "tmp-chromium.zip")

    revision = fetch_chromium_build(platform, revision, tmpzip)

    # Unpack archive in `tmpzip` to store the revision number and
    # the chromedriver
    unzip(tmpzip, tmppath)

    dirs = [
        d
        for d in os.listdir(tmppath)
        if os.path.isdir(os.path.join(tmppath, d)) and d.startswith("chrome-")
    ]

    if len(dirs) > 1:
        raise Exception(
            "Too many directories starting with `chrome-` after extracting."
        )
    elif len(dirs) == 0:
        raise Exception(
            "Could not find any directories after extraction of chromium zip."
        )

    chromium_dir = os.path.join(tmppath, dirs[0])
    revision_file = os.path.join(chromium_dir, ".REVISION")
    with open(revision_file, "w+") as f:
        f.write(str(revision))

    # Get and store the chromedriver
    fetch_chromedriver(platform, revision, chromium_dir)

    tar_file = CHROMIUM_INFO[platform]["result"]
    tar_command = ["tar", "cjf", tar_file, "-C", tmppath, dirs[0]]
    log("Added revision to %s file." % revision_file)

    log("Tarring with the command: %s" % str(tar_command))
    subprocess.check_call(tar_command)

    upload_dir = os.environ.get("UPLOAD_DIR")
    if upload_dir:
        # Move the tarball to the output directory for upload.
        log("Moving %s to the upload directory..." % tar_file)
        shutil.copy(tar_file, os.path.join(upload_dir, tar_file))

    shutil.rmtree(tmppath)


def parse_args():
    """Read command line arguments and return options."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--platform", help="Platform version of chromium to build.", required=True
    )
    parser.add_argument(
        "--revision",
        help="Revision of chromium to build to get. "
        "(Defaults to the newest chromium build).",
        default=None,
    )

    return parser.parse_args()


if __name__ == "__main__":
    args = vars(parse_args())
    build_chromium_archive(**args)
