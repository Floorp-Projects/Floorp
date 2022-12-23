#!/usr/bin/env python

# Copyright (c) 2015 Ted Mielczarek <ted@mielczarek.org>
# and Michael R. Miller <michaelrmmiller@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import argparse
import concurrent.futures
import logging
import os
import re
import shutil
import subprocess
import tempfile

import requests
import urlparse
from PackageSymbolDumper import find_packages, process_packages

OSX_RE = re.compile(r"10\.[0-9]+\.[0-9]+")


def extract_dmg(dmg_path, dest):
    logging.info("extract_dmg({}, {})".format(dmg_path, dest))
    with tempfile.NamedTemporaryFile() as f:
        subprocess.check_call(
            ["dmg", "extract", dmg_path, f.name], stdout=subprocess.DEVNULL
        )
        subprocess.check_call(["hfsplus", f.name, "extractall"], cwd=dest)


def get_update_packages():
    for i in range(16):
        logging.info("get_update_packages: page " + str(i))
        url = (
            "https://km.support.apple.com/kb/index?page=downloads_browse&sort=recency"
            "&facet=all&category=PF6&locale=en_US&offset=%d" % i
        )
        res = requests.get(url)
        if res.status_code != 200:
            break
        data = res.json()
        downloads = data.get("downloads", [])
        if not downloads:
            break
        for d in downloads:
            title = d.get("title", "")
            if OSX_RE.search(title) and "Combo" not in title:
                logging.info("Title: " + title)
                if "fileurl" in d:
                    yield d["fileurl"]
                else:
                    logging.warn("No fileurl in download!")


def fetch_url_to_file(url, download_dir):
    filename = os.path.basename(urlparse.urlsplit(url).path)
    local_filename = os.path.join(download_dir, filename)
    if os.path.isfile(local_filename):
        logging.info("{} already exists, skipping".format(local_filename))
        return None
    r = requests.get(url, stream=True)
    res_len = int(r.headers.get("content-length", "0"))
    logging.info("Downloading {} -> {} ({} bytes)".format(url, local_filename, res_len))
    with open(local_filename, "wb") as f:
        for chunk in r.iter_content(chunk_size=1024):
            if chunk:  # filter out keep-alive new chunks
                f.write(chunk)
    return local_filename


def fetch_and_extract_dmg(url, tmpdir):
    logging.info("fetch_and_extract_dmg: " + url)
    filename = fetch_url_to_file(url, tmpdir)
    if not filename:
        return []
    # Extract dmg contents to a subdir
    subdir = tempfile.mkdtemp(dir=tmpdir)
    extract_dmg(filename, subdir)
    packages = list(find_packages(subdir))
    logging.info(
        "fetch_and_extract_dmg({}): found packages: {}".format(url, str(packages))
    )
    return packages


def find_update_packages(tmpdir):
    logging.info("find_update_packages")
    # Only download 2 packages at a time.
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        jobs = dict(
            (executor.submit(fetch_and_extract_dmg, url, tmpdir), url)
            for url in get_update_packages()
        )
        for future in concurrent.futures.as_completed(jobs):
            url = jobs[future]
            if future.exception() is not None:
                logging.error(
                    "exception downloading {}: {}".format(url, future.exception())
                )
            else:
                for pkg in future.result():
                    yield pkg


def main():
    parser = argparse.ArgumentParser(
        description="Download OS X update packages and dump symbols from them"
    )
    parser.add_argument(
        "--dump_syms",
        default="dump_syms",
        type=str,
        help="path to the Breakpad dump_syms executable",
    )
    parser.add_argument("to", type=str, help="destination path for the symbols")
    args = parser.parse_args()
    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    )
    for p in ("requests.packages.urllib3.connectionpool", "urllib3"):
        urllib3_logger = logging.getLogger(p)
        urllib3_logger.setLevel(logging.ERROR)
    try:
        tmpdir = tempfile.mkdtemp(suffix=".osxupdates")

        def finder():
            return find_update_packages(tmpdir)

        process_packages(finder, args.to, None, args.dump_syms)
    finally:
        shutil.rmtree(tmpdir)


if __name__ == "__main__":
    main()
