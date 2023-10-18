"""Utility functions for Raptor"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import bz2
import gzip
import os
import signal
import socket
import sys
import time
from subprocess import PIPE, Popen

from redo import retriable, retry
from six.moves.urllib.request import urlretrieve

try:
    import zstandard
except ImportError:
    zstandard = None
try:
    import lzma
except ImportError:
    lzma = None

from mozlog import get_proxy_logger

from mozproxy import mozbase_dir, mozharness_dir

LOG = get_proxy_logger(component="mozproxy")

# running locally via mach
TOOLTOOL_PATHS = [
    os.path.join(mozharness_dir, "external_tools", "tooltool.py"),
    os.path.join(
        mozbase_dir,
        "..",
        "..",
        "python",
        "mozbuild",
        "mozbuild",
        "action",
        "tooltool.py",
    ),
]

if "MOZ_UPLOAD_DIR" in os.environ:
    TOOLTOOL_PATHS.append(
        os.path.join(
            os.environ["MOZ_UPLOAD_DIR"],
            "..",
            "..",
            "mozharness",
            "external_tools",
            "tooltool.py",
        )
    )


def transform_platform(str_to_transform, config_platform, config_processor=None):
    """Transform platform name i.e. 'mitmproxy-rel-bin-{platform}.manifest'

    transforms to 'mitmproxy-rel-bin-osx.manifest'.
    Also transform '{x64}' if needed for 64 bit / win 10
    """
    if "{platform}" not in str_to_transform and "{x64}" not in str_to_transform:
        return str_to_transform

    if "win" in config_platform:
        platform_id = "win"
    elif config_platform == "mac":
        platform_id = "osx"
    else:
        platform_id = "linux64"

    if "{platform}" in str_to_transform:
        str_to_transform = str_to_transform.replace("{platform}", platform_id)

    if "{x64}" in str_to_transform and config_processor is not None:
        if "x86_64" in config_processor:
            str_to_transform = str_to_transform.replace("{x64}", "_x64")
        else:
            str_to_transform = str_to_transform.replace("{x64}", "")

    return str_to_transform


@retriable(sleeptime=2)
def tooltool_download(manifest, run_local, raptor_dir):
    """Download a file from tooltool using the provided tooltool manifest"""

    tooltool_path = None

    for path in TOOLTOOL_PATHS:
        if os.path.exists(path):
            tooltool_path = path
            break
    if tooltool_path is None:
        raise Exception("Could not find tooltool path!")

    if run_local:
        command = [sys.executable, tooltool_path, "fetch", "-o", "-m", manifest]
    else:
        # Attempt to determine the tooltool cache path:
        #  - TOOLTOOLCACHE is used by Raptor tests
        #  - TOOLTOOL_CACHE is automatically set for taskcluster jobs
        #  - fallback to a hardcoded path
        _cache = next(
            x
            for x in (
                os.environ.get("TOOLTOOLCACHE"),
                os.environ.get("TOOLTOOL_CACHE"),
                "/builds/tooltool_cache",
            )
            if x is not None
        )

        command = [
            sys.executable,
            tooltool_path,
            "fetch",
            "-o",
            "-m",
            manifest,
            "-c",
            _cache,
        ]

    try:
        proc = Popen(command, cwd=raptor_dir, text=True)
        if proc.wait() != 0:
            raise Exception("Command failed")
    except Exception as e:
        LOG.critical(
            "Error while downloading {} from tooltool:{}".format(manifest, str(e))
        )
        if proc.poll() is None:
            proc.kill(signal.SIGTERM)
        raise


def archive_type(path):
    filename, extension = os.path.splitext(path)
    filename, extension2 = os.path.splitext(filename)
    if extension2 != "":
        extension = extension2
    if extension == ".tar":
        return "tar"
    elif extension == ".zip":
        return "zip"
    return None


def extract_archive(path, dest_dir, typ):
    """Extract an archive to a destination directory."""

    # Resolve paths to absolute variants.
    path = os.path.abspath(path)
    dest_dir = os.path.abspath(dest_dir)
    suffix = os.path.splitext(path)[-1]

    # We pipe input to the decompressor program so that we can apply
    # custom decompressors that the program may not know about.
    if typ == "tar":
        if suffix == ".bz2":
            ifh = bz2.open(str(path), "rb")
        elif suffix == ".gz":
            ifh = gzip.open(str(path), "rb")
        elif suffix == ".xz":
            if not lzma:
                raise ValueError("lzma Python package not available")
            ifh = lzma.open(str(path), "rb")
        elif suffix == ".zst":
            if not zstandard:
                raise ValueError("zstandard Python package not available")
            dctx = zstandard.ZstdDecompressor()
            ifh = dctx.stream_reader(path.open("rb"))
        elif suffix == ".tar":
            ifh = path.open("rb")
        else:
            raise ValueError("unknown archive format for tar file: %s" % path)
        args = ["tar", "xf", "-"]
        pipe_stdin = True
    elif typ == "zip":
        # unzip from stdin has wonky behavior. We don't use a pipe for it.
        ifh = open(os.devnull, "rb")
        args = ["unzip", "-o", str(path)]
        pipe_stdin = False
    else:
        raise ValueError("unknown archive format: %s" % path)

    LOG.info("Extracting %s to %s using %r" % (path, dest_dir, args))
    t0 = time.time()
    with ifh:
        p = Popen(args, cwd=str(dest_dir), bufsize=0, stdin=PIPE)
        while True:
            if not pipe_stdin:
                break
            chunk = ifh.read(131072)
            if not chunk:
                break
            p.stdin.write(chunk)
        # make sure we wait for the command to finish
        p.communicate()

    if p.returncode:
        raise Exception("%r exited %d" % (args, p.returncode))
    LOG.info("%s extracted in %.3fs" % (path, time.time() - t0))


def download_file_from_url(url, local_dest, extract=False):
    """Receive a file in a URL and download it, i.e. for the hostutils tooltool manifest
    the url received would be formatted like this:
      config/tooltool-manifests/linux64/hostutils.manifest"""
    if os.path.exists(local_dest):
        LOG.info("file already exists at: %s" % local_dest)
        if not extract:
            return True
    else:
        LOG.info("downloading: %s to %s" % (url, local_dest))
        try:
            retry(urlretrieve, args=(url, local_dest), attempts=3, sleeptime=5)
        except Exception:
            LOG.error("Failed to download file: %s" % local_dest, exc_info=True)
            if os.path.exists(local_dest):
                # delete partial downloaded file
                os.remove(local_dest)
            return False

    if not extract:
        return os.path.exists(local_dest)

    typ = archive_type(local_dest)
    if typ is None:
        LOG.info("Not able to determine archive type for: %s" % local_dest)
        return False

    extract_archive(local_dest, os.path.dirname(local_dest), typ)
    return True


def get_available_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("", 0))
    s.listen(1)
    port = s.getsockname()[1]
    s.close()
    return port
