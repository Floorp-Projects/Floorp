#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function

import asyncio
import aiohttp
import configparser
import argparse
import hashlib
import json
import logging
import os
import shutil
import tempfile
import time
import requests
import sh

import redo
from scriptworker.utils import retry_async
from mardor.reader import MarReader
from mardor.signing import get_keysize

from datadog import initialize, ThreadStats


log = logging.getLogger(__name__)

# Create this even when not sending metrics, so the context manager
# statements work.
ddstats = ThreadStats(namespace='releng.releases.partials')


ALLOWED_URL_PREFIXES = [
    "http://download.cdn.mozilla.net/pub/mozilla.org/firefox/nightly/",
    "http://download.cdn.mozilla.net/pub/firefox/nightly/",
    "https://mozilla-nightly-updates.s3.amazonaws.com",
    "https://queue.taskcluster.net/",
    "http://ftp.mozilla.org/",
    "http://download.mozilla.org/",
    "https://archive.mozilla.org/",
    "http://archive.mozilla.org/",
    "https://queue.taskcluster.net/v1/task/",
]

DEFAULT_FILENAME_TEMPLATE = "{appName}-{branch}-{version}-{platform}-" \
                            "{locale}-{from_buildid}-{to_buildid}.partial.mar"


def write_dogrc(api_key):
    """Datadog .dogrc file for command line interface."""
    dogrc_path = os.path.join(os.path.expanduser('~'), '.dogrc')
    config = configparser.ConfigParser()
    config['Connection'] = {
        'apikey': api_key,
        'appkey': '',
    }
    with open(dogrc_path, 'w') as f:
        config.write(f)


def verify_signature(mar, certs):
    log.info("Checking %s signature", mar)
    with open(mar, 'rb') as mar_fh:
        m = MarReader(mar_fh)
        m.verify(verify_key=certs.get(m.signature_type))


def is_lzma_compressed_mar(mar):
    log.info("Checking %s for lzma compression", mar)
    result = MarReader(open(mar, 'rb')).compression_type == 'xz'
    if result:
        log.info("%s is lzma compressed", mar)
    else:
        log.info("%s is not lzma compressed", mar)
    return result


@redo.retriable()
def get_secret(secret_name):
    secrets_url = "http://taskcluster/secrets/v1/secret/{}"
    log.debug("Fetching {}".format(secret_name))
    r = requests.get(secrets_url.format(secret_name))
    # 403: If unauthorized, just give up.
    if r.status_code == 403:
        log.info("Unable to get secret key")
        return {}
    r.raise_for_status()
    return r.json().get('secret', {})


async def retry_download(*args, **kwargs):  # noqa: E999
    """Retry download() calls."""
    await retry_async(
        download,
        retry_exceptions=(
            aiohttp.ClientError,
            asyncio.TimeoutError
        ),
        args=args,
        kwargs=kwargs
    )


async def download(url, dest, mode=None):  # noqa: E999
    log.info("Downloading %s to %s", url, dest)
    chunk_size = 4096
    bytes_downloaded = 0
    async with aiohttp.ClientSession(raise_for_status=True) as session:
        async with session.get(url, timeout=120) as resp:
            # Additional early logging for download timeouts.
            log.debug("Fetching from url %s", resp.url)
            for history in resp.history:
                log.debug("Redirection history: %s", history.url)
            if 'Content-Length' in resp.headers:
                log.debug('Content-Length expected for %s: %s',
                          url, resp.headers['Content-Length'])
            log_interval = chunk_size * 1024
            with open(dest, 'wb') as fd:
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

            log.debug('Downloaded %s bytes', bytes_downloaded)
            if 'content-length' in resp.headers:
                log.debug('Content-Length: %s bytes', resp.headers['content-length'])
                if bytes_downloaded != int(resp.headers['content-length']):
                    raise IOError('Unexpected number of bytes downloaded')
            if mode:
                log.debug("chmod %o %s", mode, dest)
                os.chmod(dest, mode)


async def run_command(cmd, cwd='/', env=None, label=None, silent=False):
    if not env:
        env = dict()
    process = await asyncio.create_subprocess_shell(cmd,
                                                    stdout=asyncio.subprocess.PIPE,
                                                    stderr=asyncio.subprocess.STDOUT,
                                                    cwd=cwd, env=env)
    stdout, stderr = await process.communicate()

    await process.wait()

    if silent:
        return

    if not stderr:
        stderr = ""
    if not stdout:
        stdout = ""

    label = "{}: ".format(label)

    for line in stdout.splitlines():
        log.debug("%s%s", label, line.decode('utf-8'))
    for line in stderr.splitlines():
        log.warn("%s%s", label, line.decode('utf-8'))


async def unpack(work_env, mar, dest_dir):
    os.mkdir(dest_dir)
    log.debug("Unwrapping %s", mar)
    env = work_env.env
    if not is_lzma_compressed_mar(mar):
        env['MAR_OLD_FORMAT'] = '1'
    elif 'MAR_OLD_FORMAT' in env:
        del env['MAR_OLD_FORMAT']

    cmd = "{} {}".format(work_env.paths['unwrap_full_update.pl'], mar)
    await run_command(cmd, cwd=dest_dir, env=env, label=dest_dir)


def find_file(directory, filename):
    log.debug("Searching for %s in %s", filename, directory)
    for root, _, files in os.walk(directory):
        if filename in files:
            f = os.path.join(root, filename)
            log.debug("Found %s", f)
            return f


def get_option(directory, filename, section, option):
    log.debug("Extracting [%s]: %s from %s/**/%s", section, option, directory,
              filename)
    f = find_file(directory, filename)
    config = configparser.ConfigParser()
    config.read(f)
    rv = config.get(section, option)
    log.debug("Found %s", rv)
    return rv


async def generate_partial(work_env, from_dir, to_dir, dest_mar, mar_data,
                           use_old_format):
    log.info("Generating partial %s", dest_mar)
    env = work_env.env
    env["MOZ_PRODUCT_VERSION"] = mar_data['version']
    env["MOZ_CHANNEL_ID"] = mar_data["ACCEPTED_MAR_CHANNEL_IDS"]
    env['BRANCH'] = mar_data['branch']
    env['PLATFORM'] = mar_data['platform']
    if use_old_format:
        env['MAR_OLD_FORMAT'] = '1'
    elif 'MAR_OLD_FORMAT' in env:
        del env['MAR_OLD_FORMAT']
    make_incremental_update = os.path.join(work_env.workdir,
                                           "make_incremental_update.sh")
    cmd = " ".join([make_incremental_update, dest_mar, from_dir, to_dir])

    await run_command(cmd, cwd=work_env.workdir, env=env, label=dest_mar.split('/')[-1])


def get_hash(path, hash_type="sha512"):
    h = hashlib.new(hash_type)
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


class WorkEnv(object):

    def __init__(self, mar=None, mbsdiff=None):
        self.workdir = tempfile.mkdtemp()
        self.paths = {
            'unwrap_full_update.pl': os.path.join(self.workdir, 'unwrap_full_update.pl'),
            'mar': os.path.join(self.workdir, 'mar'),
            'mbsdiff': os.path.join(self.workdir, 'mbsdiff')
        }
        self.urls = {
            'unwrap_full_update.pl': 'https://hg.mozilla.org/mozilla-central/raw-file/default/'
            'tools/update-packaging/unwrap_full_update.pl',
            'mar': 'https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/'
            'latest-mozilla-central/mar-tools/linux64/mar',
            'mbsdiff': 'https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/'
            'latest-mozilla-central/mar-tools/linux64/mbsdiff'
        }
        if mar:
            self.urls['mar'] = mar
        if mbsdiff:
            self.urls['mbsdiff'] = mbsdiff

    async def setup(self, mar=None, mbsdiff=None):
        for filename, url in self.urls.items():
            if filename not in self.paths:
                log.info("Been told about %s but don't know where to download it to!", filename)
                continue
            await retry_download(url, dest=self.paths[filename], mode=0o755)

    async def download_buildsystem_bits(self, repo, revision):
        prefix = "{repo}/raw-file/{revision}/tools/update-packaging"
        prefix = prefix.format(repo=repo, revision=revision)
        for f in ('make_incremental_update.sh', 'common.sh'):
            url = "{prefix}/{f}".format(prefix=prefix, f=f)
            await retry_download(url, dest=os.path.join(self.workdir, f), mode=0o755)

    def cleanup(self):
        shutil.rmtree(self.workdir)

    @property
    def env(self):
        my_env = os.environ.copy()
        my_env['LC_ALL'] = 'C'
        my_env['MAR'] = self.paths['mar']
        my_env['MBSDIFF'] = self.paths['mbsdiff']
        return my_env


def verify_allowed_url(mar):
    if not any(mar.startswith(prefix) for prefix in ALLOWED_URL_PREFIXES):
        raise ValueError("{mar} is not in allowed URL prefixes: {p}".format(
            mar=mar, p=ALLOWED_URL_PREFIXES
        ))


async def manage_partial(partial_def, work_env, filename_template, artifacts_dir, signing_certs):
    """Manage the creation of partial mars based on payload."""
    for mar in (partial_def["from_mar"], partial_def["to_mar"]):
        verify_allowed_url(mar)

    complete_mars = {}
    use_old_format = False

    for mar_type, f in (("from", partial_def["from_mar"]), ("to", partial_def["to_mar"])):
        dest = os.path.join(work_env.workdir, "{}.mar".format(mar_type))
        unpack_dir = os.path.join(work_env.workdir, mar_type)

        metric_tags = [
            "platform:{}".format(partial_def['platform']),
        ]
        with ddstats.timer('mar.download.time', tags=metric_tags):
            await retry_download(f, dest)

        if not os.getenv("MOZ_DISABLE_MAR_CERT_VERIFICATION"):
            verify_signature(dest, signing_certs)

        complete_mars["%s_size" % mar_type] = os.path.getsize(dest)
        complete_mars["%s_hash" % mar_type] = get_hash(dest)

        with ddstats.timer('mar.unpack.time'):
            await unpack(work_env, dest, unpack_dir)

        if mar_type == 'from':
            version = get_option(unpack_dir, filename="application.ini",
                                 section="App", option="Version")
            major = int(version.split(".")[0])
            # The updater for versions less than 56.0 requires BZ2
            # compressed MAR files
            if major < 56:
                use_old_format = True
                log.info("Forcing BZ2 compression for %s", f)

        log.info("AV-scanning %s ...", unpack_dir)
        metric_tags = [
            "platform:{}".format(partial_def['platform']),
        ]
        with ddstats.timer('mar.clamscan.time', tags=metric_tags):
            await run_command("clamscan -r {}".format(unpack_dir), label='clamscan')
        log.info("Done.")

    to_path = os.path.join(work_env.workdir, "to")
    from_path = os.path.join(work_env.workdir, "from")

    mar_data = {
        "ACCEPTED_MAR_CHANNEL_IDS": get_option(
            to_path, filename="update-settings.ini", section="Settings",
            option="ACCEPTED_MAR_CHANNEL_IDS"),
        "version": get_option(to_path, filename="application.ini",
                              section="App", option="Version"),
        "to_buildid": get_option(to_path, filename="application.ini",
                                 section="App", option="BuildID"),
        "from_buildid": get_option(from_path, filename="application.ini",
                                   section="App", option="BuildID"),
        "appName": get_option(from_path, filename="application.ini",
                              section="App", option="Name"),
        # Use Gecko repo and rev from platform.ini, not application.ini
        "repo": get_option(to_path, filename="platform.ini", section="Build",
                           option="SourceRepository"),
        "revision": get_option(to_path, filename="platform.ini",
                               section="Build", option="SourceStamp"),
        "from_mar": partial_def["from_mar"],
        "to_mar": partial_def["to_mar"],
        "platform": partial_def["platform"],
        "locale": partial_def["locale"],
    }
    # Override ACCEPTED_MAR_CHANNEL_IDS if needed
    if "ACCEPTED_MAR_CHANNEL_IDS" in os.environ:
        mar_data["ACCEPTED_MAR_CHANNEL_IDS"] = os.environ["ACCEPTED_MAR_CHANNEL_IDS"]
    for field in ("update_number", "previousVersion", "previousBuildNumber",
                  "toVersion", "toBuildNumber"):
        if field in partial_def:
            mar_data[field] = partial_def[field]
    mar_data.update(complete_mars)

    # if branch not set explicitly use repo-name
    mar_data['branch'] = partial_def.get('branch', mar_data['repo'].rstrip('/').split('/')[-1])

    if 'dest_mar' in partial_def:
        mar_name = partial_def['dest_mar']
    else:
        # default to formatted name if not specified
        mar_name = filename_template.format(**mar_data)

    mar_data['mar'] = mar_name
    dest_mar = os.path.join(work_env.workdir, mar_name)

    # TODO: download these once
    await work_env.download_buildsystem_bits(repo=mar_data["repo"],
                                             revision=mar_data["revision"])

    metric_tags = [
        "branch:{}".format(mar_data['branch']),
        "platform:{}".format(mar_data['platform']),
        # If required. Shouldn't add much useful info, but increases
        # cardinality of metrics substantially, so avoided.
        # "locale:{}".format(mar_data['locale']),
    ]
    with ddstats.timer('generate_partial.time', tags=metric_tags):
        await generate_partial(work_env, from_path, to_path, dest_mar,
                               mar_data, use_old_format)

    mar_data["size"] = os.path.getsize(dest_mar)

    metric_tags.append("unit:bytes")
    # Allows us to find out how many releases there were between the two,
    # making buckets of the file sizes easier.
    metric_tags.append("update_number:{}".format(mar_data.get('update_number', 0)))
    ddstats.gauge('partial_mar_size', mar_data['size'], tags=metric_tags)

    mar_data["hash"] = get_hash(dest_mar)

    shutil.copy(dest_mar, artifacts_dir)
    work_env.cleanup()

    return mar_data


async def async_main(args, signing_certs):
    tasks = []

    task = json.load(args.task_definition)
    # TODO: verify task["extra"]["funsize"]["partials"] with jsonschema
    for definition in task["extra"]["funsize"]["partials"]:
        workenv = WorkEnv(
            mar=definition.get('mar_binary'),
            mbsdiff=definition.get('mbsdiff_binary')
        )
        await workenv.setup()
        tasks.append(asyncio.ensure_future(retry_async(
                                           manage_partial,
                                           retry_exceptions=(
                                               aiohttp.ClientError,
                                               asyncio.TimeoutError
                                           ),
                                           kwargs=dict(
                                               partial_def=definition,
                                               filename_template=args.filename_template,
                                               artifacts_dir=args.artifacts_dir,
                                               work_env=workenv,
                                               signing_certs=signing_certs
                                           ))))
    manifest = await asyncio.gather(*tasks)
    return manifest


def main():

    start = time.time()

    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts-dir", required=True)
    parser.add_argument("--sha1-signing-cert", required=True)
    parser.add_argument("--sha384-signing-cert", required=True)
    parser.add_argument("--task-definition", required=True,
                        type=argparse.FileType('r'))
    parser.add_argument("--filename-template",
                        default=DEFAULT_FILENAME_TEMPLATE)
    parser.add_argument("--no-freshclam", action="store_true", default=False,
                        help="Do not refresh ClamAV DB")
    parser.add_argument("-q", "--quiet", dest="log_level",
                        action="store_const", const=logging.WARNING,
                        default=logging.DEBUG)
    args = parser.parse_args()

    logging.basicConfig(format="%(asctime)s - %(levelname)s - %(message)s")
    log.setLevel(args.log_level)

    signing_certs = {
        'sha1': open(args.sha1_signing_cert, 'rb').read(),
        'sha384': open(args.sha384_signing_cert, 'rb').read(),
    }

    assert(get_keysize(signing_certs['sha1']) == 2048)
    assert(get_keysize(signing_certs['sha384']) == 4096)

    # Intended for local testing.
    dd_api_key = os.environ.get('DATADOG_API_KEY')
    # Intended for Taskcluster.
    if not dd_api_key and os.environ.get('DATADOG_API_SECRET'):
        dd_api_key = get_secret(os.environ.get('DATADOG_API_SECRET')).get('key')

    if dd_api_key:
        dd_options = {
            'api_key': dd_api_key,
        }
        log.info("Starting metric collection")
        initialize(**dd_options)
        ddstats.start(flush_interval=1)
        # For use in shell scripts.
        write_dogrc(dd_api_key)
    else:
        log.info("No metric collection")

    if args.no_freshclam:
        log.info("Skipping freshclam")
    else:
        log.info("Refreshing clamav db...")
        try:
            redo.retry(lambda: sh.freshclam("--stdout", "--verbose",
                                            _timeout=300, _err_to_out=True))
            log.info("Done.")
        except sh.ErrorReturnCode:
            log.warning("Freshclam failed, skipping DB update")

    loop = asyncio.get_event_loop()
    manifest = loop.run_until_complete(async_main(args, signing_certs))
    loop.close()

    manifest_file = os.path.join(args.artifacts_dir, "manifest.json")
    with open(manifest_file, "w") as fp:
        json.dump(manifest, fp, indent=2, sort_keys=True)

    log.debug("{}".format(json.dumps(manifest, indent=2, sort_keys=True)))

    # Warning: Assumption that one partials task will always be for one branch.
    metric_tags = [
        "branch:{}".format(manifest[0]['branch']),
        "platform:{}".format(manifest[0]['platform']),
    ]

    ddstats.timing('task_duration', time.time() - start,
                   start, tags=metric_tags)

    # Wait for all the metrics to flush. If the program ends before
    # they've been sent, they'll be dropped.
    # Should be more than the flush_interval for the ThreadStats object
    if dd_api_key:
        time.sleep(10)


if __name__ == '__main__':
    main()
