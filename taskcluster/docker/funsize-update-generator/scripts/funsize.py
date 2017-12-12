#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function

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
from mardor.reader import MarReader
from mardor.signing import get_keysize

from datadog import initialize, ThreadStats


log = logging.getLogger(__name__)
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
        return {}
    r.raise_for_status()
    return r.json()


@redo.retriable()
def download(url, dest, mode=None):
    log.debug("Downloading %s to %s", url, dest)
    r = requests.get(url)
    r.raise_for_status()

    bytes_downloaded = 0
    with open(dest, 'wb') as fd:
        for chunk in r.iter_content(4096):
            fd.write(chunk)
            bytes_downloaded += len(chunk)

    log.debug('Downloaded %s bytes', bytes_downloaded)
    if 'content-length' in r.headers:
        log.debug('Content-Length: %s bytes', r.headers['content-length'])
        if bytes_downloaded != int(r.headers['content-length']):
            raise IOError('Unexpected number of bytes downloaded')

    if mode:
        log.debug("chmod %o %s", mode, dest)
        os.chmod(dest, mode)


def unpack(work_env, mar, dest_dir):
    os.mkdir(dest_dir)
    unwrap_cmd = sh.Command(os.path.join(work_env.workdir,
                                         "unwrap_full_update.pl"))
    log.debug("Unwrapping %s", mar)
    env = work_env.env
    if not is_lzma_compressed_mar(mar):
        env['MAR_OLD_FORMAT'] = '1'
    elif 'MAR_OLD_FORMAT' in env:
        del env['MAR_OLD_FORMAT']
    out = unwrap_cmd(mar, _cwd=dest_dir, _env=env, _timeout=240,
                     _err_to_out=True)
    if out:
        log.debug(out)


def find_file(directory, filename):
    log.debug("Searching for %s in %s", filename, directory)
    for root, dirs, files in os.walk(directory):
        if filename in files:
            f = os.path.join(root, filename)
            log.debug("Found %s", f)
            return f


def get_option(directory, filename, section, option):
    log.debug("Exctracting [%s]: %s from %s/**/%s", section, option, directory,
              filename)
    f = find_file(directory, filename)
    config = configparser.ConfigParser()
    config.read(f)
    rv = config.get(section, option)
    log.debug("Found %s", rv)
    return rv


def generate_partial(work_env, from_dir, to_dir, dest_mar, channel_ids,
                     version, use_old_format):
    log.debug("Generating partial %s", dest_mar)
    env = work_env.env
    env["MOZ_PRODUCT_VERSION"] = version
    env["MOZ_CHANNEL_ID"] = channel_ids
    if use_old_format:
        env['MAR_OLD_FORMAT'] = '1'
    elif 'MAR_OLD_FORMAT' in env:
        del env['MAR_OLD_FORMAT']
    make_incremental_update = os.path.join(work_env.workdir,
                                           "make_incremental_update.sh")
    out = sh.bash(make_incremental_update, dest_mar, from_dir, to_dir,
                  _cwd=work_env.workdir, _env=env, _timeout=900,
                  _err_to_out=True)
    if out:
        log.debug(out)


def get_hash(path, hash_type="sha512"):
    h = hashlib.new(hash_type)
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


class WorkEnv(object):

    def __init__(self):
        self.workdir = tempfile.mkdtemp()

    def setup(self):
        self.download_unwrap()
        self.download_martools()

    def download_unwrap(self):
        # unwrap_full_update.pl is not too sensitive to the revision
        url = "https://hg.mozilla.org/mozilla-central/raw-file/default/" \
            "tools/update-packaging/unwrap_full_update.pl"
        download(url, dest=os.path.join(self.workdir, "unwrap_full_update.pl"),
                 mode=0o755)

    def download_buildsystem_bits(self, repo, revision):
        prefix = "{repo}/raw-file/{revision}/tools/update-packaging"
        prefix = prefix.format(repo=repo, revision=revision)
        for f in ("make_incremental_update.sh", "common.sh"):
            url = "{prefix}/{f}".format(prefix=prefix, f=f)
            download(url, dest=os.path.join(self.workdir, f), mode=0o755)

    def download_martools(self):
        # TODO: check if the tools have to be branch specific
        prefix = "https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/" \
            "latest-mozilla-central/mar-tools/linux64"
        for f in ("mar", "mbsdiff"):
            url = "{prefix}/{f}".format(prefix=prefix, f=f)
            download(url, dest=os.path.join(self.workdir, f), mode=0o755)

    def cleanup(self):
        shutil.rmtree(self.workdir)

    @property
    def env(self):
        my_env = os.environ.copy()
        my_env['LC_ALL'] = 'C'
        my_env['MAR'] = os.path.join(self.workdir, "mar")
        my_env['MBSDIFF'] = os.path.join(self.workdir, "mbsdiff")
        return my_env


def verify_allowed_url(mar):
    if not any(mar.startswith(prefix) for prefix in ALLOWED_URL_PREFIXES):
        raise ValueError("{mar} is not in allowed URL prefixes: {p}".format(
            mar=mar, p=ALLOWED_URL_PREFIXES
        ))


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
    task = json.load(args.task_definition)
    # TODO: verify task["extra"]["funsize"]["partials"] with jsonschema

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

    # Create this even when not sending metrics, so the context manager
    # statements work.
    ddstats = ThreadStats(namespace='releng.releases.partials')

    if dd_api_key:
        dd_options = {
            'api_key': dd_api_key,
        }
        log.info("Starting metric collection")
        initialize(**dd_options)
        ddstats.start(flush_interval=1)
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

    manifest = []
    for e in task["extra"]["funsize"]["partials"]:
        for mar in (e["from_mar"], e["to_mar"]):
            verify_allowed_url(mar)

        work_env = WorkEnv()
        # TODO: run setup once
        work_env.setup()
        complete_mars = {}
        use_old_format = False
        for mar_type, f in (("from", e["from_mar"]), ("to", e["to_mar"])):
            dest = os.path.join(work_env.workdir, "{}.mar".format(mar_type))
            unpack_dir = os.path.join(work_env.workdir, mar_type)
            with ddstats.timer('mar.download.time'):
                download(f, dest)
            if not os.getenv("MOZ_DISABLE_MAR_CERT_VERIFICATION"):
                verify_signature(dest, signing_certs)
            complete_mars["%s_size" % mar_type] = os.path.getsize(dest)
            complete_mars["%s_hash" % mar_type] = get_hash(dest)
            with ddstats.timer('mar.unpack.time'):
                unpack(work_env, dest, unpack_dir)
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
                "platform:{}".format(e['platform']),
            ]
            with ddstats.timer('mar.clamscan.time', tags=metric_tags):
                sh.clamscan("-r", unpack_dir, _timeout=600, _err_to_out=True)
            log.info("Done.")

        path = os.path.join(work_env.workdir, "to")
        from_path = os.path.join(work_env.workdir, "from")
        mar_data = {
            "ACCEPTED_MAR_CHANNEL_IDS": get_option(
                path, filename="update-settings.ini", section="Settings",
                option="ACCEPTED_MAR_CHANNEL_IDS"),
            "version": get_option(path, filename="application.ini",
                                  section="App", option="Version"),
            "to_buildid": get_option(path, filename="application.ini",
                                     section="App", option="BuildID"),
            "from_buildid": get_option(from_path, filename="application.ini",
                                       section="App", option="BuildID"),
            "appName": get_option(from_path, filename="application.ini",
                                  section="App", option="Name"),
            # Use Gecko repo and rev from platform.ini, not application.ini
            "repo": get_option(path, filename="platform.ini", section="Build",
                               option="SourceRepository"),
            "revision": get_option(path, filename="platform.ini",
                                   section="Build", option="SourceStamp"),
            "from_mar": e["from_mar"],
            "to_mar": e["to_mar"],
            "platform": e["platform"],
            "locale": e["locale"],
        }
        # Override ACCEPTED_MAR_CHANNEL_IDS if needed
        if "ACCEPTED_MAR_CHANNEL_IDS" in os.environ:
            mar_data["ACCEPTED_MAR_CHANNEL_IDS"] = os.environ["ACCEPTED_MAR_CHANNEL_IDS"]
        for field in ("update_number", "previousVersion",
                      "previousBuildNumber", "toVersion",
                      "toBuildNumber"):
            if field in e:
                mar_data[field] = e[field]
        mar_data.update(complete_mars)
        # if branch not set explicitly use repo-name
        mar_data["branch"] = e.get("branch",
                                   mar_data["repo"].rstrip("/").split("/")[-1])
        if 'dest_mar' in e:
            mar_name = e['dest_mar']
        else:
            # default to formatted name if not specified
            mar_name = args.filename_template.format(**mar_data)
        mar_data["mar"] = mar_name
        dest_mar = os.path.join(work_env.workdir, mar_name)
        # TODO: download these once
        work_env.download_buildsystem_bits(repo=mar_data["repo"],
                                           revision=mar_data["revision"])

        metric_tags = [
            "branch:{}".format(mar_data['branch']),
            "platform:{}".format(mar_data['platform']),
            # If required. Shouldn't add much useful info, but increases
            # cardinality of metrics substantially, so avoided.
            # "locale:{}".format(mar_data['locale']),
        ]

        with ddstats.timer('generate_partial.time', tags=metric_tags):
            generate_partial(work_env, from_path, path, dest_mar,
                             mar_data["ACCEPTED_MAR_CHANNEL_IDS"],
                             mar_data["version"],
                             use_old_format)

        mar_data["size"] = os.path.getsize(dest_mar)
        metric_tags.append("unit:bytes")
        # Allows us to find out how many releases there were between the two,
        # making buckets of the file sizes easier.
        metric_tags.append("update_number:{}".format(mar_data.get('update_number', 0)))
        ddstats.gauge('partial_mar_size', mar_data['size'], tags=metric_tags)

        mar_data["hash"] = get_hash(dest_mar)

        shutil.copy(dest_mar, args.artifacts_dir)
        work_env.cleanup()
        manifest.append(mar_data)

    manifest_file = os.path.join(args.artifacts_dir, "manifest.json")
    with open(manifest_file, "w") as fp:
        json.dump(manifest, fp, indent=2, sort_keys=True)

    # Warning: Assumption that one partials task will always be for one branch.
    metric_tags = [
        "branch:{}".format(mar_data['branch']),
    ]

    ddstats.timing('task_duration', time.time() - start,
                   start, tags=metric_tags)
    # Wait for all the metrics to flush. If the program ends before
    # they've been sent, they'll be dropped.
    # Should be more than the flush_interval for the ThreadStats object
    time.sleep(10)


if __name__ == '__main__':
    main()
