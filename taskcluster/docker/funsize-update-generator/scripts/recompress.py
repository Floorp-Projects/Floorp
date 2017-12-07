#!/usr/bin/env python3

from __future__ import absolute_import, division, print_function

import configparser
import argparse
import functools
import hashlib
import json
import logging
import os
import shutil
import tempfile
import requests
import sh

import redo
from mardor.reader import MarReader
from mardor.signing import get_keysize


log = logging.getLogger(__name__)
ALLOWED_URL_PREFIXES = [
    "http://download.cdn.mozilla.net/pub/mozilla.org/firefox/nightly/",
    "http://download.cdn.mozilla.net/pub/firefox/nightly/",
    "https://mozilla-nightly-updates.s3.amazonaws.com",
    "https://queue.taskcluster.net/",
    "http://ftp.mozilla.org/",
    "http://download.mozilla.org/",
    "https://archive.mozilla.org/",
]


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


def change_mar_compression(work_env, file_path):
    """Toggles MAR compression format between BZ2 and LZMA"""
    cmd = sh.Command(os.path.join(work_env.workdir,
                                  "change_mar_compression.pl"))
    log.debug("Changing MAR compression of %s", file_path)
    out = cmd("-r", file_path, _env=work_env.env, _timeout=240,
              _err_to_out=True)
    if out:
        log.debug(out)


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


def get_hash(path, hash_type="sha512"):
    h = hashlib.new(hash_type)
    with open(path, "rb") as f:
        for chunk in iter(functools.partial(f.read, 4096), ''):
            h.update(chunk)
    return h.hexdigest()


class WorkEnv(object):

    def __init__(self):
        self.workdir = tempfile.mkdtemp()

    def setup(self):
        self.download_scripts()
        self.download_martools()

    def download_scripts(self):
        # unwrap_full_update.pl is not too sensitive to the revision
        prefix = "https://hg.mozilla.org/mozilla-central/raw-file/default/" \
            "tools/update-packaging"
        for f in ("unwrap_full_update.pl", "change_mar_compression.pl"):
            url = "{prefix}/{f}".format(prefix=prefix, f=f)
            download(url, dest=os.path.join(self.workdir, f), mode=0o755)

    def download_martools(self):
        url = "https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/" \
            "latest-mozilla-central/mar-tools/linux64/mar"
        download(url, dest=os.path.join(self.workdir, "mar"), mode=0o755)

    def cleanup(self):
        shutil.rmtree(self.workdir)

    @property
    def env(self):
        my_env = os.environ.copy()
        my_env['LC_ALL'] = 'C'
        my_env['MAR'] = os.path.join(self.workdir, "mar")
        return my_env


def verify_allowed_url(mar):
    if not any(mar.startswith(prefix) for prefix in ALLOWED_URL_PREFIXES):
        raise ValueError("{mar} is not in allowed URL prefixes: {p}".format(
            mar=mar, p=ALLOWED_URL_PREFIXES
        ))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts-dir", required=True)
    parser.add_argument("--sha1-signing-cert", required=True)
    parser.add_argument("--sha384-signing-cert", required=True)
    parser.add_argument("--task-definition", required=True,
                        type=argparse.FileType('r'))
    parser.add_argument("--output-filename", required=True)
    parser.add_argument("-q", "--quiet", dest="log_level",
                        action="store_const", const=logging.WARNING,
                        default=logging.DEBUG)
    args = parser.parse_args()

    logging.basicConfig(format="%(asctime)s - %(levelname)s - %(message)s",
                        level=args.log_level)
    task = json.load(args.task_definition)

    signing_certs = {
        'sha1': open(args.sha1_signing_cert, 'rb').read(),
        'sha384': open(args.sha384_signing_cert, 'rb').read(),
    }

    assert(get_keysize(signing_certs['sha1']) == 2048)
    assert(get_keysize(signing_certs['sha384']) == 4096)

    manifest = []
    for e in task["extra"]["funsize"]["completes"]:
        to_mar = e["to_mar"]
        locale = e["locale"]
        output_filename = args.output_filename.format(locale=locale)
        verify_allowed_url(to_mar)

        work_env = WorkEnv()
        work_env.setup()
        complete_mars = {}
        dest = os.path.join(work_env.workdir, "to.mar")
        unpack_dir = os.path.join(work_env.workdir, "to")
        download(to_mar, dest)
        if not os.getenv("MOZ_DISABLE_MAR_CERT_VERIFICATION"):
            verify_signature(dest, signing_certs)
        # Changing the compression strips the signature
        change_mar_compression(work_env, dest)
        complete_mars["hash"] = get_hash(dest)
        unpack(work_env, dest, unpack_dir)
        log.info("AV-scanning %s ...", unpack_dir)
        sh.clamscan("-r", unpack_dir, _timeout=600, _err_to_out=True)
        log.info("Done.")

        mar_data = {
            "file_to_sign": output_filename,
            "hash": get_hash(dest),
            "appName": get_option(unpack_dir, filename="application.ini",
                                  section="App", option="Name"),
            "version": get_option(unpack_dir, filename="application.ini",
                                  section="App", option="Version"),
            "to_buildid": get_option(unpack_dir, filename="application.ini",
                                     section="App", option="BuildID"),
            "toVersion": e["toVersion"],
            "toBuildNumber": e["toBuildNumber"],
            "platform": e["platform"],
            "locale": e["locale"],
        }
        shutil.copy(dest, os.path.join(args.artifacts_dir, output_filename))
        work_env.cleanup()
        manifest.append(mar_data)
    manifest_file = os.path.join(args.artifacts_dir, "manifest.json")
    with open(manifest_file, "w") as fp:
        json.dump(manifest, fp, indent=2, sort_keys=True)


if __name__ == '__main__':
    main()
