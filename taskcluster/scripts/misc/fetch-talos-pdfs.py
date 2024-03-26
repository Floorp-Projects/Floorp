#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script downloads all the required PDFs from the test_manifest.json
file found in the mozilla pdf.js repo.
"""

import json
import os
import pathlib
import shutil

import requests
from redo import retriable


def log(msg):
    print("fetch-talos-pdf: %s" % msg)


@retriable(attempts=7, sleeptime=5, sleepscale=2)
def fetch_file(url, filepath):
    """Download a file from the given url to a given file.

    :param str url: URL to download file from.
    :param Path filepath: Location to ouput the downloaded file
        (includes the name of the file).
    """
    size = 4096
    r = requests.get(url, stream=True)
    r.raise_for_status()

    with filepath.open("wb") as fd:
        for chunk in r.iter_content(size):
            fd.write(chunk)


def fetch_talos_pdf_link(pdf_path, output_file):
    """Fetches a PDF file with a link into the output file location.

    :param Path pdf_path: Path to a PDF file that contains a URL to download from.
    :param Path output_file: Location (including the file name) to download PDF to.
    """
    pdf_link = pdf_path.read_text().strip()
    log(f"Downloading from PDF link: {pdf_link}")
    fetch_file(pdf_link, output_file)


def gather_talos_pdf(test_folder, pdf_info, output_dir):
    """Gathers a PDF file into the output directory.

    :param Path test_folder: The test folder that the pdfs can be found in.
    :param Path pdf_info: Information about the pdf we're currently gathering, and
        found in the test/test_manifest.json file from the pdf.js repo.
    :param Path output_dir: The directory to move/download the PDF to.
    """
    pdf_file = pdf_info["file"]
    output_pdf_path = pathlib.Path(output_dir, pathlib.Path(pdf_file).name)

    log(f"Gathering PDF {pdf_file}...")
    if output_pdf_path.exists():
        log(f"{pdf_file} already exists in output location")
    elif pdf_info.get("link", False):
        fetch_talos_pdf_link(
            pathlib.Path(test_folder, pdf_file + ".link"), output_pdf_path
        )
    else:
        log(f"Copying PDF to output location {output_pdf_path}")
        shutil.copy(pathlib.Path(test_folder, pdf_file), output_pdf_path)


def gather_talos_pdfs(pdf_js_repo, output_dir):
    """Gather all pdfs to be used in the talos pdfpaint test.

    Uses the pdf.js repo to gather the files from it's test/test_manifest.json
    file. Some of these are also links that need to be downloaded. These
    are output in an output directory.

    :param Path pdf_js_repo: Path to the Mozilla Github pdf.js repo.
    :param Path output_dir: Output directory for the PDFs.
    """
    test_manifest_path = pathlib.Path(
        pdf_js_repo, "test", "test_manifest.json"
    ).resolve()
    test_folder = test_manifest_path.parent

    # Gather all the PDFs into the output directory
    test_manifest = json.loads(test_manifest_path.read_text())
    for pdf_info in test_manifest:
        gather_talos_pdf(test_folder, pdf_info, output_dir)

    # Include the test manifest in the output directory as it
    # contains the names of the tests
    shutil.copy(test_manifest_path, pathlib.Path(output_dir, test_manifest_path.name))


if __name__ == "__main__":
    moz_fetches_dir = os.environ.get("MOZ_FETCHES_DIR", "")
    if not moz_fetches_dir:
        raise Exception(
            "MOZ_FETCHES_DIR is not set to the path containing the pdf.js repo"
        )

    pdf_js_repo = pathlib.Path(moz_fetches_dir, "pdf.js")
    if not pdf_js_repo.exists():
        raise Exception("Can't find the pdf.js repository in MOZ_FETCHES_DIR")

    output_dir = os.environ.get("OUTPUT_DIR", "")
    if not output_dir:
        raise Exception("OUTPUT_DIR is not set for the file output")

    output_dir_path = pathlib.Path(output_dir)
    output_dir_path.mkdir(parents=True, exist_ok=True)
    gather_talos_pdfs(pdf_js_repo, output_dir_path)
