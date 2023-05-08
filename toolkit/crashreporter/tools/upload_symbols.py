#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script uploads a symbol archive file from a path or URL passed on the commandline
# to the symbol server at https://symbols.mozilla.org/ .
#
# Using this script requires you to have generated an authentication
# token in the symbol server web interface. You must store the token in a Taskcluster
# secret as the JSON blob `{"token": "<token>"}` and set the `SYMBOL_SECRET`
# environment variable to the name of the Taskcluster secret. Alternately,
# you can put the token in a file and set `SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE`
# environment variable to the path to the file.

import argparse
import logging
import os
import sys
import tempfile

import redo
import requests

log = logging.getLogger("upload-symbols")
log.setLevel(logging.INFO)

DEFAULT_URL = "https://symbols.mozilla.org/upload/"
MAX_RETRIES = 7
MAX_ZIP_SIZE = 500000000  # 500 MB


def print_error(r):
    if r.status_code < 400:
        log.error("Error: bad auth token? ({0}: {1})".format(r.status_code, r.reason))
    else:
        log.error("Error: got HTTP response {0}: {1}".format(r.status_code, r.reason))

    log.error(
        "Response body:\n{sep}\n{body}\n{sep}\n".format(sep="=" * 20, body=r.text)
    )


def get_taskcluster_secret(secret_name):
    secrets_url = "http://taskcluster/secrets/v1/secret/{}".format(secret_name)
    log.info(
        'Using symbol upload token from the secrets service: "{}"'.format(secrets_url)
    )
    res = requests.get(secrets_url)
    res.raise_for_status()
    secret = res.json()
    auth_token = secret["secret"]["token"]

    return auth_token


def main():
    logging.basicConfig()
    parser = argparse.ArgumentParser(
        description="Upload symbols in ZIP using token from Taskcluster secrets service."
    )
    parser.add_argument(
        "archive", help="Symbols archive file - URL or path to local file"
    )
    parser.add_argument(
        "--ignore-missing", help="No error on missing files", action="store_true"
    )
    args = parser.parse_args()

    def check_file_exists(url):
        for i, _ in enumerate(redo.retrier(attempts=MAX_RETRIES), start=1):
            try:
                resp = requests.head(url, allow_redirects=True)
                return resp.status_code == requests.codes.ok
            except requests.exceptions.RequestException as e:
                log.error("Error: {0}".format(e))
            log.info("Retrying...")
        return False

    if args.archive.startswith("http"):
        is_existing = check_file_exists(args.archive)
    else:
        is_existing = os.path.isfile(args.archive)

    if not is_existing:
        if args.ignore_missing:
            log.info('Archive file "{0}" does not exist!'.format(args.archive))
            return 0
        else:
            log.error('Error: archive file "{0}" does not exist!'.format(args.archive))
            return 1

    try:
        tmpdir = None
        if args.archive.endswith(".tar.zst"):
            tmpdir = tempfile.TemporaryDirectory()
            zip_paths = convert_zst_archive(args.archive, tmpdir)
        else:
            zip_paths = [args.archive]

        for zip_path in zip_paths:
            result = upload_symbols(zip_path)
            if result:
                return result
        return 0
    finally:
        if tmpdir:
            tmpdir.cleanup()


def convert_zst_archive(zst_archive, tmpdir):
    """
    Convert a .tar.zst file to a zip file

    Our build tasks output .tar.zst files, but the tecken server only allows
    .zip files to be uploaded.

    :param zst_archive: path or URL to a .tar.zst source file
    :param tmpdir: TemporaryDirectory to store the output zip file in
    :returns: path to output zip file
    """
    import concurrent.futures
    import gzip
    import itertools
    import tarfile

    import zstandard
    from mozpack.files import File
    from mozpack.mozjar import Deflater, JarWriter

    def iter_files_from_tar(reader):
        ctx = zstandard.ZstdDecompressor()
        uncompressed = ctx.stream_reader(reader)
        with tarfile.open(mode="r|", fileobj=uncompressed, bufsize=1024 * 1024) as tar:
            while True:
                info = tar.next()
                if info is None:
                    break
                data = tar.extractfile(info).read()
                yield (info.name, data)

    def prepare_from(archive, tmpdir):
        if archive.startswith("http"):
            resp = requests.get(archive, allow_redirects=True, stream=True)
            resp.raise_for_status()
            reader = resp.raw
            # Work around taskcluster generic-worker possibly gzipping the tar.zst.
            if resp.headers.get("Content-Encoding") == "gzip":
                reader = gzip.GzipFile(fileobj=reader)
        else:
            reader = open(archive, "rb")

        def handle_file(data):
            name, data = data
            log.info("Compressing %s", name)
            path = os.path.join(tmpdir, name.lstrip("/"))
            if name.endswith(".dbg"):
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "wb") as fh:
                    with gzip.GzipFile(fileobj=fh, mode="wb", compresslevel=5) as c:
                        c.write(data)
                return (name + ".gz", File(path))
            elif name.endswith(".dSYM.tar"):
                import bz2

                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "wb") as fh:
                    fh.write(bz2.compress(data))
                return (name + ".bz2", File(path))
            elif name.endswith((".pdb", ".exe", ".dll")):
                import subprocess

                makecab = os.environ.get("MAKECAB", "makecab")
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "wb") as fh:
                    fh.write(data)

                subprocess.check_call(
                    [makecab, "-D", "CompressionType=MSZIP", path, path + "_"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.STDOUT,
                )

                return (name[:-1] + "_", File(path + "_"))
            else:
                deflater = Deflater(compress_level=5)
                deflater.write(data)
                return (name, deflater)

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=os.cpu_count()
        ) as executor:
            yield from executor.map(handle_file, iter_files_from_tar(reader))

        reader.close()

    zip_paths_iter = iter(
        os.path.join(tmpdir.name, "symbols{}.zip".format("" if i == 1 else i))
        for i in itertools.count(start=1)
    )
    zip_path = next(zip_paths_iter)
    log.info('Preparing symbol archive "{0}" from "{1}"'.format(zip_path, zst_archive))
    for i, _ in enumerate(redo.retrier(attempts=MAX_RETRIES), start=1):
        zip_paths = []
        jar = None
        try:
            for name, data in prepare_from(zst_archive, tmpdir.name):
                if not jar:
                    jar = JarWriter(zip_path)
                    zip_paths.append(zip_path)
                    size = 0
                log.info("Adding %s", name)
                jar.add(name, data, compress=not isinstance(data, File))
                size += data.size() if isinstance(data, File) else data.compressed_size
                if size > MAX_ZIP_SIZE:
                    jar.finish()
                    jar = None
                    zip_path = next(zip_paths_iter)
                    log.info('Continuing with symbol archive "{}"'.format(zip_path))
            if jar:
                jar.finish()
            return zip_paths
        except requests.exceptions.RequestException as e:
            log.error("Error: {0}".format(e))
            log.info("Retrying...")

    return []


def upload_symbols(zip_path):
    """
    Upload symbols to the tecken server

    :param zip_path: path to the zip file to upload
    :returns: 0 indicates the upload was successful, non-zero indicates an
              error that should be used for the script's exit code
    """
    secret_name = os.environ.get("SYMBOL_SECRET")
    if secret_name is not None:
        auth_token = get_taskcluster_secret(secret_name)
    elif "SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE" in os.environ:
        token_file = os.environ["SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE"]

        if not os.path.isfile(token_file):
            log.error(
                'SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE "{0}" does not exist!'.format(
                    token_file
                )
            )
            return 1
        auth_token = open(token_file, "r").read().strip()
    else:
        log.error(
            "You must set the SYMBOL_SECRET or SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE "
            "environment variables!"
        )
        return 1

    # Allow overwriting of the upload url with an environmental variable
    if "SOCORRO_SYMBOL_UPLOAD_URL" in os.environ:
        url = os.environ["SOCORRO_SYMBOL_UPLOAD_URL"]
    else:
        url = DEFAULT_URL

    log.info('Uploading symbol file "{0}" to "{1}"'.format(zip_path, url))

    for i, _ in enumerate(redo.retrier(attempts=MAX_RETRIES), start=1):
        log.info("Attempt %d of %d..." % (i, MAX_RETRIES))
        try:
            if zip_path.startswith("http"):
                zip_arg = {"data": {"url": zip_path}}
            else:
                zip_arg = {"files": {"symbols.zip": open(zip_path, "rb")}}
            r = requests.post(
                url,
                headers={"Auth-Token": auth_token},
                allow_redirects=False,
                # Allow a longer read timeout because uploading by URL means the server
                # has to fetch the entire zip file, which can take a while. The load balancer
                # in front of symbols.mozilla.org has a 300 second timeout, so we'll use that.
                timeout=(300, 300),
                **zip_arg
            )
            # 408, 429 or any 5XX is likely to be a transient failure.
            # Break out for success or other error codes.
            if r.ok or (r.status_code < 500 and (r.status_code not in (408, 429))):
                break
            print_error(r)
        except requests.exceptions.RequestException as e:
            log.error("Error: {0}".format(e))
        log.info("Retrying...")
    else:
        log.warning("Maximum retries hit, giving up!")
        return 1

    if r.status_code >= 200 and r.status_code < 300:
        log.info("Uploaded successfully!")
        return 0

    print_error(r)
    return 1


if __name__ == "__main__":
    sys.exit(main())
