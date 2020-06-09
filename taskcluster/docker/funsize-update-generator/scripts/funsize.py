#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function

import argparse
import asyncio
import configparser
import json
import logging
import os
import shutil
import tempfile
import time
from distutils.util import strtobool
from pathlib import Path

import aiohttp
from mardor.reader import MarReader
from mardor.signing import get_keysize
from scriptworker.utils import retry_async, get_hash

log = logging.getLogger(__name__)


ROOT_URL = os.environ.get(
    "TASKCLUSTER_ROOT_URL", "https://firefox-ci-tc.services.mozilla.com"
)
QUEUE_PREFIX = f"{ROOT_URL}/api/queue/"
ALLOWED_URL_PREFIXES = (
    "http://download.cdn.mozilla.net/pub/mozilla.org/firefox/nightly/",
    "http://download.cdn.mozilla.net/pub/firefox/nightly/",
    "https://mozilla-nightly-updates.s3.amazonaws.com",
    "http://ftp.mozilla.org/",
    "http://download.mozilla.org/",
    "https://archive.mozilla.org/",
    "http://archive.mozilla.org/",
    QUEUE_PREFIX,
)
STAGING_URL_PREFIXES = (
    "http://ftp.stage.mozaws.net/",
    "https://ftp.stage.mozaws.net/",
)

BCJ_OPTIONS = {
    "x86": ["--x86"],
    "x86_64": ["--x86"],
    "aarch64": [],
}


def verify_signature(mar, cert):
    log.info("Checking %s signature", mar)
    with open(mar, "rb") as mar_fh:
        m = MarReader(mar_fh)
        if not m.verify(verify_key=cert):
            raise ValueError(
                "MAR Signature invalid: %s (%s) against %s", mar, m.signature_type, cert
            )


def process_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts-dir", required=True)
    parser.add_argument(
        "--signing-cert", type=argparse.FileType("rb"), required=True
    )
    parser.add_argument("--task-definition", required=True, type=argparse.FileType("r"))
    parser.add_argument(
        "--allow-staging-prefixes",
        action="store_true",
        default=strtobool(os.environ.get("FUNSIZE_ALLOW_STAGING_PREFIXES", "false")),
        help="Allow files from staging buckets.",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        dest="log_level",
        action="store_const",
        const=logging.INFO,
        default=logging.DEBUG,
    )
    parser.add_argument(
        "--arch",
        type=str,
        required=True,
        choices=BCJ_OPTIONS.keys(),
        help="The archtecture you are building.",
    )
    return parser.parse_args()


def validate_mar_channel_id(mar, channel_ids):
    log.info("Checking %s for MAR_CHANNEL_ID %s", mar, channel_ids)
    # We may get a string with a list representation, or a single entry string.
    channel_ids = set(channel_ids.split(","))

    product_info = MarReader(open(mar, "rb")).productinfo
    if not isinstance(product_info, tuple):
        raise ValueError(
            "Malformed product information in mar: {}".format(product_info)
        )

    found_channel_ids = set(product_info[1].split(","))

    if not found_channel_ids.issubset(channel_ids):
        raise ValueError(
            "MAR_CHANNEL_ID mismatch, {} not in {}".format(product_info[1], channel_ids)
        )

    log.info("%s channel %s in %s", mar, product_info[1], channel_ids)


async def retry_download(*args, **kwargs):  # noqa: E999
    """Retry download() calls."""
    await retry_async(
        download,
        retry_exceptions=(aiohttp.ClientError, asyncio.TimeoutError),
        args=args,
        kwargs=kwargs,
    )


def verify_allowed_url(mar, allowed_url_prefixes):
    if not any(mar.startswith(prefix) for prefix in allowed_url_prefixes):
        raise ValueError(
            "{mar} is not in allowed URL prefixes: {p}".format(
                mar=mar, p=allowed_url_prefixes
            )
        )


async def download(url, dest, mode=None):  # noqa: E999
    log.info("Downloading %s to %s", url, dest)
    chunk_size = 4096
    bytes_downloaded = 0
    async with aiohttp.ClientSession(raise_for_status=True) as session:
        start = time.time()
        async with session.get(url, timeout=120) as resp:
            # Additional early logging for download timeouts.
            log.debug("Fetching from url %s", resp.url)
            for history in resp.history:
                log.debug("Redirection history: %s", history.url)
            if "Content-Length" in resp.headers:
                log.debug(
                    "Content-Length expected for %s: %s",
                    url,
                    resp.headers["Content-Length"],
                )
            log_interval = chunk_size * 1024
            with open(dest, "wb") as fd:
                while True:
                    chunk = await resp.content.read(chunk_size)
                    if not chunk:
                        break
                    fd.write(chunk)
                    bytes_downloaded += len(chunk)
                    log_interval -= len(chunk)
                    if log_interval <= 0:
                        log.debug("Bytes downloaded for %s: %d", url, bytes_downloaded)
                        log_interval = chunk_size * 1024
            end = time.time()
            log.info(
                "Downloaded %s, %s bytes in %s seconds",
                url,
                bytes_downloaded,
                int(end - start),
            )
            if mode:
                log.info("chmod %o %s", mode, dest)
                os.chmod(dest, mode)


async def download_buildsystem_bits(partials_config, downloads, tools_dir):
    """Download external tools needed to make partials."""

    # We're making the assumption that the "to" mar is the same for all,
    # as that's the way this task is currently used.
    to_url = extract_download_urls(partials_config, mar_type="to").pop()

    repo = get_option(
        downloads[to_url]["extracted_path"],
        filename="platform.ini",
        section="Build",
        option="SourceRepository",
    )
    revision = get_option(
        downloads[to_url]["extracted_path"],
        filename="platform.ini",
        section="Build",
        option="SourceStamp",
    )

    urls = {
        "make_incremental_update.sh": f"{repo}/raw-file/{revision}/tools/"
        "update-packaging/make_incremental_update.sh",
        "common.sh": f"{repo}/raw-file/{revision}/tools/update-packaging/common.sh",
        "mar": "https://archive.mozilla.org/pub/mozilla.org/firefox/nightly/"
        "latest-mozilla-central/mar-tools/linux64/mar",
        "mbsdiff": "https://archive.mozilla.org/pub/mozilla.org/firefox/nightly/"
        "latest-mozilla-central/mar-tools/linux64/mbsdiff",
    }
    for filename, url in urls.items():
        filename = tools_dir / filename
        await retry_download(url, dest=filename, mode=0o755)


def find_file(directory, filename):
    log.debug("Searching for %s in %s", filename, directory)
    return next(Path(directory).rglob(filename))


def get_option(directory, filename, section, option):
    log.info("Extracting [%s]: %s from %s/**/%s", section, option, directory, filename)
    f = find_file(directory, filename)
    config = configparser.ConfigParser()
    config.read(f)
    rv = config.get(section, option)
    log.info("Found %s", rv)
    return rv


def extract_download_urls(partials_config, mar_type):
    """Extract a set of urls to download from the task configuration.

    mar_type should be one of "from", "to"
    """
    return {definition[f"{mar_type}_mar"] for definition in partials_config}


async def download_and_verify_mars(
    partials_config, allowed_url_prefixes, signing_cert
):
    """Download, check signature, channel ID and unpack MAR files."""
    # Separate these categories so we can opt to perform checks on only 'to' downloads.
    from_urls = extract_download_urls(partials_config, mar_type="from")
    to_urls = extract_download_urls(partials_config, mar_type="to")
    tasks = list()
    downloads = dict()
    for url in from_urls.union(to_urls):
        verify_allowed_url(url, allowed_url_prefixes)
        downloads[url] = {
            "download_path": Path(tempfile.mkdtemp()) / Path(url).name,
        }
        tasks.append(retry_download(url, downloads[url]["download_path"]))

    await asyncio.gather(*tasks)

    for url in downloads:
        # Verify signature, but not from an artifact as we don't
        # depend on the signing task
        if not os.getenv("MOZ_DISABLE_MAR_CERT_VERIFICATION") and not url.startswith(
            QUEUE_PREFIX
        ):
            verify_signature(downloads[url]["download_path"], signing_cert)

        # Only validate the target channel ID, as we update from beta->release
        if url in to_urls:
            validate_mar_channel_id(
                downloads[url]["download_path"], os.environ["MAR_CHANNEL_ID"]
            )

        downloads[url]["extracted_path"] = tempfile.mkdtemp()
        with open(downloads[url]["download_path"], "rb") as mar_fh:
            log.info(
                "Unpacking %s into %s",
                downloads[url]["download_path"],
                downloads[url]["extracted_path"],
            )
            m = MarReader(mar_fh)
            m.extract(downloads[url]["extracted_path"])

    return downloads


async def run_command(cmd, cwd="/", env=None, label=None, silent=False):
    log.info("Running: %s", cmd)
    if not env:
        env = dict()
    process = await asyncio.create_subprocess_shell(
        cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=cwd,
        env=env,
    )
    if label:
        label = "{}: ".format(label)
    else:
        label = ""

    async def read_output(stream, label, printcmd):
        while True:
            line = await stream.readline()
            if line == b"":
                break
            printcmd("%s%s", label, line.decode("utf-8").rstrip())

    if silent:
        await process.wait()
    else:
        await asyncio.gather(
            read_output(process.stdout, label, log.info),
            read_output(process.stderr, label, log.warn),
        )
        await process.wait()


async def generate_partial(from_dir, to_dir, dest_mar, mar_data, tools_dir, arch):
    log.info("Generating partial %s", dest_mar)
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    env["MAR"] = tools_dir / "mar"
    env["MBSDIFF"] = tools_dir / "mbsdiff"
    if arch:
        env["BCJ_OPTIONS"] = " ".join(BCJ_OPTIONS[arch])
    env["MOZ_PRODUCT_VERSION"] = mar_data["version"]
    env["MAR_CHANNEL_ID"] = mar_data["MAR_CHANNEL_ID"]
    env["BRANCH"] = mar_data["branch"]
    if "MAR_OLD_FORMAT" in env:
        del env["MAR_OLD_FORMAT"]

    make_incremental_update = tools_dir / "make_incremental_update.sh"
    cmd = f"{make_incremental_update} {dest_mar} {from_dir} {to_dir}"

    await run_command(cmd, cwd=dest_mar.parent, env=env, label=dest_mar.name)
    validate_mar_channel_id(dest_mar, mar_data["MAR_CHANNEL_ID"])


async def manage_partial(
    partial_def, artifacts_dir, tools_dir, downloads, semaphore, arch=None
):
    from_url = partial_def["from_mar"]
    to_url = partial_def["to_mar"]
    from_path = downloads[from_url]["extracted_path"]
    to_path = downloads[to_url]["extracted_path"]

    mar_data = {
        "MAR_CHANNEL_ID": os.environ["MAR_CHANNEL_ID"],
        "version": get_option(
            to_path, filename="application.ini", section="App", option="Version"
        ),
        "appName": get_option(
            from_path, filename="application.ini", section="App", option="Name"
        ),
        # Use Gecko repo and rev from platform.ini, not application.ini
        "repo": get_option(
            to_path, filename="platform.ini", section="Build", option="SourceRepository"
        ),
        "revision": get_option(
            to_path, filename="platform.ini", section="Build", option="SourceStamp"
        ),
        "locale": partial_def["locale"],
        "from_mar": partial_def["from_mar"],
        "from_size": os.path.getsize(downloads[from_url]["download_path"]),
        "from_hash": get_hash(downloads[from_url]["download_path"], hash_alg="sha512"),
        "from_buildid": get_option(
            from_path, filename="application.ini", section="App", option="BuildID"
        ),
        "to_mar": partial_def["to_mar"],
        "to_size": os.path.getsize(downloads[to_url]["download_path"]),
        "to_hash": get_hash(downloads[to_url]["download_path"], hash_alg="sha512"),
        "to_buildid": get_option(
            to_path, filename="application.ini", section="App", option="BuildID"
        ),
        "mar": partial_def["dest_mar"],
    }
    # if branch not set explicitly use repo-name
    mar_data["branch"] = partial_def.get("branch", Path(mar_data["repo"]).name)

    for field in (
        "update_number",
        "previousVersion",
        "previousBuildNumber",
        "toVersion",
        "toBuildNumber",
    ):
        if field in partial_def:
            mar_data[field] = partial_def[field]

    dest_mar = Path(artifacts_dir) / mar_data["mar"]

    async with semaphore:
        await generate_partial(from_path, to_path, dest_mar, mar_data, tools_dir, arch)

    mar_data["size"] = os.path.getsize(dest_mar)
    mar_data["hash"] = get_hash(dest_mar, hash_alg="sha512")
    return mar_data


async def async_main(args, signing_cert):
    tasks = []

    allowed_url_prefixes = list(ALLOWED_URL_PREFIXES)
    if args.allow_staging_prefixes:
        allowed_url_prefixes += STAGING_URL_PREFIXES

    task = json.load(args.task_definition)

    downloads = await download_and_verify_mars(
        task["extra"]["funsize"]["partials"], allowed_url_prefixes, signing_cert
    )

    tools_dir = Path(tempfile.mkdtemp())
    await download_buildsystem_bits(
        partials_config=task["extra"]["funsize"]["partials"],
        downloads=downloads,
        tools_dir=tools_dir,
    )

    # May want to consider os.cpu_count() if we ever run on osx/win.
    # sched_getaffinity is the list of cores we can run on, not the total.
    semaphore = asyncio.Semaphore(len(os.sched_getaffinity(0)))
    for definition in task["extra"]["funsize"]["partials"]:
        tasks.append(
            asyncio.ensure_future(
                retry_async(
                    manage_partial,
                    retry_exceptions=(aiohttp.ClientError, asyncio.TimeoutError),
                    kwargs=dict(
                        partial_def=definition,
                        artifacts_dir=args.artifacts_dir,
                        tools_dir=tools_dir,
                        arch=args.arch,
                        downloads=downloads,
                        semaphore=semaphore,
                    ),
                )
            )
        )
    manifest = await asyncio.gather(*tasks)

    for url in downloads:
        downloads[url]["download_path"].unlink()
        shutil.rmtree(downloads[url]["extracted_path"])
    shutil.rmtree(tools_dir)

    return manifest


def main():
    args = process_arguments()

    logging.basicConfig(format="%(asctime)s - %(levelname)s - %(message)s")
    log.setLevel(args.log_level)

    signing_cert = args.signing_cert.read()
    assert get_keysize(signing_cert) == 4096

    artifacts_dir = Path(args.artifacts_dir)
    if not artifacts_dir.exists():
        artifacts_dir.mkdir()

    loop = asyncio.get_event_loop()
    manifest = loop.run_until_complete(async_main(args, signing_cert))
    loop.close()

    manifest_file = artifacts_dir / "manifest.json"
    with open(manifest_file, "w") as fp:
        json.dump(manifest, fp, indent=2, sort_keys=True)

    log.debug("{}".format(json.dumps(manifest, indent=2, sort_keys=True)))


if __name__ == "__main__":
    main()
