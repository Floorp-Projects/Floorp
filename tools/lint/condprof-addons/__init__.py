# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import json
import os
import tarfile
import tempfile
from pathlib import Path

import requests
import yaml
from mozlint.pathutils import expand_exclusions

BROWSERTIME_FETCHES_PATH = Path("taskcluster/ci/fetch/browsertime.yml")
CUSTOMIZATIONS_PATH = Path("testing/condprofile/condprof/customization/")
DOWNLOAD_TIMEOUT = 30
ERR_FETCH_TASK_MISSING = "firefox-addons taskcluster fetch config section not found"
ERR_FETCH_TASK_ADDPREFIX = "firefox-addons taskcluster config 'add-prefix' attribute should be set to 'firefox-addons/'"
ERR_FETCH_TASK_ARCHIVE = (
    "Error downloading or opening archive from firefox-addons taskcluster fetch url"
)
LINTER_NAME = "condprof-addons"
MOZ_FETCHES_DIR = os.environ.get("MOZ_FETCHES_DIR")
RULE_DESC = "condprof addons all listed in firefox-addons.tar fetched archive"
MOZ_AUTOMATION = "MOZ_AUTOMATION" in os.environ

tempdir = tempfile.gettempdir()


def lint(paths, config, logger, fix=None, **lintargs):
    filepaths = [Path(p) for p in expand_exclusions(paths, config, lintargs["root"])]

    if len(filepaths) == 0:
        return

    linter = CondprofAddonsLinter(topsrcdir=lintargs["root"], logger=logger)

    for filepath in filepaths:
        linter.lint(filepath)


class CondprofAddonsLinter:
    def __init__(self, topsrcdir, logger):
        self.topsrcdir = topsrcdir
        self.logger = logger
        self.BROWSERTIME_FETCHES_FULLPATH = Path(
            self.topsrcdir, BROWSERTIME_FETCHES_PATH
        )
        self.CUSTOMIZATIONS_FULLPATH = Path(self.topsrcdir, CUSTOMIZATIONS_PATH)
        self.tar_xpi_filenames = self.get_firefox_addons_tar_names()

    def lint(self, filepath):
        data = self.read_json(filepath)

        if "addons" not in data:
            return

        for addon_key in data["addons"]:
            xpi_url = data["addons"][addon_key]
            xpi_filename = xpi_url.split("/")[-1]
            self.logger.info(f"Found addon {xpi_filename}")
            if xpi_filename not in self.tar_xpi_filenames:
                self.logger.lint_error(
                    self.get_missing_xpi_msg(xpi_filename),
                    lineno=0,
                    column=None,
                    path=str(filepath),
                    linter=LINTER_NAME,
                    rule=RULE_DESC,
                )

    def get_missing_xpi_msg(self, xpi_filename):
        return f"{xpi_filename} is missing from the firefox-addons.tar archive"

    def read_json(self, filepath):
        with filepath.open("r") as f:
            return json.load(f)

    def read_yaml(self, filepath):
        with filepath.open("r") as f:
            return yaml.safe_load(f)

    def download_firefox_addons_tar(self, firefox_addons_tar_url, tar_tmp_path):
        self.logger.info(f"Downloading {firefox_addons_tar_url} to {tar_tmp_path}")
        res = requests.get(
            firefox_addons_tar_url, stream=True, timeout=DOWNLOAD_TIMEOUT
        )
        res.raise_for_status()
        with tar_tmp_path.open("wb") as f:
            for chunk in res.iter_content(chunk_size=1024):
                if chunk is not None:
                    f.write(chunk)
                    f.flush()

    def get_firefox_addons_tar_names(self):
        # Get firefox-addons fetch task config.
        browsertime_fetches = self.read_yaml(self.BROWSERTIME_FETCHES_FULLPATH)

        if not (
            "firefox-addons" in browsertime_fetches
            and "fetch" in browsertime_fetches["firefox-addons"]
        ):
            self.logger.lint_error(
                ERR_FETCH_TASK_MISSING,
                lineno=0,
                column=None,
                path=BROWSERTIME_FETCHES_PATH,
                linter=LINTER_NAME,
                rule=RULE_DESC,
            )
            return []

        fetch_config = browsertime_fetches["firefox-addons"]["fetch"]

        if not (
            "add-prefix" in fetch_config
            and fetch_config["add-prefix"] == "firefox-addons/"
        ):
            self.logger.lint_error(
                ERR_FETCH_TASK_ADDPREFIX,
                lineno=0,
                column=None,
                path=BROWSERTIME_FETCHES_PATH,
                linter=LINTER_NAME,
                rule=RULE_DESC,
            )
            return []

        firefox_addons_tar_url = fetch_config["url"]
        firefox_addons_tar_sha256 = fetch_config["sha256"]

        tar_xpi_files = list()

        # When running on the CI, try to retrieve the list of xpi files from the target MOZ_FETCHES_DIR
        # subdirectory instead of downloading the archive from the fetch url.
        if MOZ_AUTOMATION:
            fetches_path = (
                Path(MOZ_FETCHES_DIR) if MOZ_FETCHES_DIR is not None else None
            )
            if fetches_path is not None and fetches_path.exists():
                self.logger.info(
                    "Detected MOZ_FETCHES_DIR, look for pre-downloaded firefox-addons fetch results"
                )
                # add-prefix presence and value has been enforced at the start of this method.
                fetches_addons = Path(fetches_path, "firefox-addons/")
                if fetches_addons.exists():
                    self.logger.info(
                        f"Retrieve list of xpi files from firefox-addons fetch result at {str(fetches_addons)}"
                    )
                    for xpi_path in fetches_addons.iterdir():
                        if xpi_path.suffix == ".xpi":
                            tar_xpi_files.append(xpi_path.name)
                    return tar_xpi_files
                else:
                    self.logger.warning(
                        "No 'firefox-addons/' subdir found in MOZ_FETCHES_DIR"
                    )

        # Fallback to download the tar archive and retrieve the list of xpi file from it
        # (e.g. when linting the local changes on the developers environment).
        tar_tmp_path = Path(tempdir, "firefox-addons.tar")
        tar_tmp_ready = False

        # If the firefox-addons.tar file is found in the tempdir, check if the
        # file hash matches, if it does then don't download it again.
        if tar_tmp_path.exists():
            tar_tmp_hash = hashlib.sha256()
            with tar_tmp_path.open("rb") as f:
                while chunk := f.read(1024):
                    tar_tmp_hash.update(chunk)
                if tar_tmp_hash.hexdigest() == firefox_addons_tar_sha256:
                    self.logger.info(
                        f"Pre-downloaded file for {tar_tmp_path} found and sha256 matching"
                    )
                    tar_tmp_ready = True
                else:
                    self.logger.info(
                        f"{tar_tmp_path} sha256 does not match the fetch config"
                    )

        # If the file is not found or the hash doesn't match, download it from the fetch task url.
        if not tar_tmp_ready:
            try:
                self.download_firefox_addons_tar(firefox_addons_tar_url, tar_tmp_path)
            except requests.exceptions.HTTPError as http_err:
                self.logger.lint_error(
                    f"{ERR_FETCH_TASK_ARCHIVE}, {str(http_err)}",
                    lineno=0,
                    column=None,
                    path=BROWSERTIME_FETCHES_PATH,
                    linter=LINTER_NAME,
                    rule=RULE_DESC,
                )
                return []

        # Retrieve and return the list of xpi file names.
        try:
            with tarfile.open(tar_tmp_path, "r") as tf:
                names = tf.getnames()
                for name in names:
                    file_path = Path(name)
                    if file_path.suffix == ".xpi":
                        tar_xpi_files.append(file_path.name)
        except tarfile.ReadError as read_err:
            self.logger.lint_error(
                f"{ERR_FETCH_TASK_ARCHIVE}, {str(read_err)}",
                lineno=0,
                column=None,
                path=BROWSERTIME_FETCHES_PATH,
                linter=LINTER_NAME,
                rule=RULE_DESC,
            )
            return []

        return tar_xpi_files
