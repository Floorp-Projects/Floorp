# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import math
import os
import pprint
import re
import subprocess
import sys

from looseversion import LooseVersion
from mozilla_version.gecko import GeckoVersion
from mozilla_version.version import VersionType
from six.moves.urllib.parse import urljoin

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.log import DEBUG, FATAL, INFO, WARNING
from mozharness.base.script import BaseScript


# ensure all versions are 3 part (i.e. 99.1.0)
# ensure all text (i.e. 'esr') is in the last part
class CompareVersion(LooseVersion):
    version = ""

    def __init__(self, versionMap):
        parts = versionMap.split(".")
        # assume version is 99.9.0, look for 99.0
        if len(parts) == 2:
            intre = re.compile("([0-9.]+)(.*)")
            match = intre.match(parts[-1])
            if match:
                parts[-1] = match.group(1)
                parts.append("0%s" % match.group(2))
        else:
            parts.append("0")
        self.version = ".".join(parts)
        LooseVersion(versionMap)


def is_triangular(x):
    """Check if a number is triangular (0, 1, 3, 6, 10, 15, ...)
    see: https://en.wikipedia.org/wiki/Triangular_number#Triangular_roots_and_tests_for_triangular_numbers # noqa

    >>> is_triangular(0)
    True
    >>> is_triangular(1)
    True
    >>> is_triangular(2)
    False
    >>> is_triangular(3)
    True
    >>> is_triangular(4)
    False
    >>> all(is_triangular(x) for x in [0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105])
    True
    >>> all(not is_triangular(x) for x in [4, 5, 8, 9, 11, 17, 25, 29, 39, 44, 59, 61, 72, 98, 112])
    True
    """
    # pylint --py3k W1619
    n = (math.sqrt(8 * x + 1) - 1) / 2
    return n == int(n)


class UpdateVerifyConfigCreator(BaseScript):
    config_options = [
        [
            ["--product"],
            {
                "dest": "product",
                "help": "Product being tested, as used in the update URL and filenames. Eg: firefox",  # NOQA: E501
            },
        ],
        [
            ["--stage-product"],
            {
                "dest": "stage_product",
                "help": "Product being tested, as used in stage directories and ship it"
                "If not passed this is assumed to be the same as product.",
            },
        ],
        [
            ["--app-name"],
            {
                "dest": "app_name",
                "help": "App name being tested. Eg: browser",
            },
        ],
        [
            ["--branch-prefix"],
            {
                "dest": "branch_prefix",
                "help": "Prefix of release branch names. Eg: mozilla, comm",
            },
        ],
        [
            ["--channel"],
            {
                "dest": "channel",
                "help": "Channel to run update verify against",
            },
        ],
        [
            ["--aus-server"],
            {
                "dest": "aus_server",
                "default": "https://aus5.mozilla.org",
                "help": "AUS server to run update verify against",
            },
        ],
        [
            ["--to-version"],
            {
                "dest": "to_version",
                "help": "The version of the release being updated to. Eg: 59.0b5",
            },
        ],
        [
            ["--to-app-version"],
            {
                "dest": "to_app_version",
                "help": "The in-app version of the release being updated to. Eg: 59.0",
            },
        ],
        [
            ["--to-display-version"],
            {
                "dest": "to_display_version",
                "help": "The human-readable version of the release being updated to. Eg: 59.0 Beta 9",  # NOQA: E501
            },
        ],
        [
            ["--to-build-number"],
            {
                "dest": "to_build_number",
                "help": "The build number of the release being updated to",
            },
        ],
        [
            ["--to-buildid"],
            {
                "dest": "to_buildid",
                "help": "The buildid of the release being updated to",
            },
        ],
        [
            ["--to-revision"],
            {
                "dest": "to_revision",
                "help": "The revision that the release being updated to was built against",
            },
        ],
        [
            ["--partial-version"],
            {
                "dest": "partial_versions",
                "default": [],
                "action": "append",
                "help": "A previous release version that is expected to receive a partial update. "
                "Eg: 59.0b4. May be specified multiple times.",
            },
        ],
        [
            ["--last-watershed"],
            {
                "dest": "last_watershed",
                "help": "The earliest version to include in the update verify config. Eg: 57.0b10",
            },
        ],
        [
            ["--include-version"],
            {
                "dest": "include_versions",
                "default": [],
                "action": "append",
                "help": "Only include versions that match one of these regexes. "
                "May be passed multiple times",
            },
        ],
        [
            ["--mar-channel-id-override"],
            {
                "dest": "mar_channel_id_options",
                "default": [],
                "action": "append",
                "help": "A version regex and channel id string to override those versions with."
                "Eg: ^\\d+\\.\\d+(\\.\\d+)?$,firefox-mozilla-beta,firefox-mozilla-release "
                "will set accepted mar channel ids to 'firefox-mozilla-beta' and "
                "'firefox-mozilla-release for x.y and x.y.z versions. "
                "May be passed multiple times",
            },
        ],
        [
            ["--override-certs"],
            {
                "dest": "override_certs",
                "default": None,
                "help": "Certs to override the updater with prior to running update verify."
                "If passed, should be one of: dep, nightly, release"
                "If not passed, no certificate overriding will be configured",
            },
        ],
        [
            ["--platform"],
            {
                "dest": "platform",
                "help": "The platform to generate the update verify config for, in FTP-style",
            },
        ],
        [
            ["--updater-platform"],
            {
                "dest": "updater_platform",
                "help": "The platform to run the updater on, in FTP-style."
                "If not specified, this is assumed to be the same as platform",
            },
        ],
        [
            ["--archive-prefix"],
            {
                "dest": "archive_prefix",
                "help": "The server/path to pull the current release from. "
                "Eg: https://archive.mozilla.org/pub",
            },
        ],
        [
            ["--previous-archive-prefix"],
            {
                "dest": "previous_archive_prefix",
                "help": "The server/path to pull the previous releases from"
                "If not specified, this is assumed to be the same as --archive-prefix",
            },
        ],
        [
            ["--repo-path"],
            {
                "dest": "repo_path",
                "help": (
                    "The repository (relative to the hg server root) that the current "
                    "release was built from Eg: releases/mozilla-beta"
                ),
            },
        ],
        [
            ["--output-file"],
            {
                "dest": "output_file",
                "help": "Where to write the update verify config to",
            },
        ],
        [
            ["--product-details-server"],
            {
                "dest": "product_details_server",
                "default": "https://product-details.mozilla.org",
                "help": "Product Details server to pull previous release info from. "
                "Using anything other than the production server is likely to "
                "cause issues with update verify.",
            },
        ],
        [
            ["--hg-server"],
            {
                "dest": "hg_server",
                "default": "https://hg.mozilla.org",
                "help": "Mercurial server to pull various previous and current version info from",
            },
        ],
        [
            ["--full-check-locale"],
            {
                "dest": "full_check_locales",
                "default": ["de", "en-US", "ru"],
                "action": "append",
                "help": "A list of locales to generate full update verify checks for",
            },
        ],
        [
            ["--local-repo"],
            {
                "dest": "local_repo",
                "help": "Path to local clone of the repository",
            },
        ],
    ]

    def __init__(self):
        BaseScript.__init__(
            self,
            config_options=self.config_options,
            config={},
            all_actions=[
                "gather-info",
                "create-config",
                "write-config",
            ],
            default_actions=[
                "gather-info",
                "create-config",
                "write-config",
            ],
        )

    def _pre_config_lock(self, rw_config):
        super(UpdateVerifyConfigCreator, self)._pre_config_lock(rw_config)

        if "updater_platform" not in self.config:
            self.config["updater_platform"] = self.config["platform"]
        if "stage_product" not in self.config:
            self.config["stage_product"] = self.config["product"]
        if "previous_archive_prefix" not in self.config:
            self.config["previous_archive_prefix"] = self.config["archive_prefix"]
        self.config["archive_prefix"].rstrip("/")
        self.config["previous_archive_prefix"].rstrip("/")
        self.config["mar_channel_id_overrides"] = {}
        for override in self.config["mar_channel_id_options"]:
            pattern, override_str = override.split(",", 1)
            self.config["mar_channel_id_overrides"][pattern] = override_str

    def _get_branch_url(self, branch_prefix, version):
        version = GeckoVersion.parse(version)
        branch = None
        if version.version_type == VersionType.BETA:
            branch = "releases/{}-beta".format(branch_prefix)
        elif version.version_type == VersionType.ESR:
            branch = "releases/{}-esr{}".format(branch_prefix, version.major_number)
        elif version.version_type == VersionType.RELEASE:
            branch = "releases/{}-release".format(branch_prefix)
        if not branch:
            raise Exception("Cannot determine branch, cannot continue!")

        return branch

    def _get_update_paths(self):
        from mozrelease.l10n import getPlatformLocales
        from mozrelease.paths import getCandidatesDir
        from mozrelease.platforms import ftp2infoFile
        from mozrelease.versions import MozillaVersion

        self.update_paths = {}

        ret = self._retry_download(
            "{}/1.0/{}.json".format(
                self.config["product_details_server"],
                self.config["stage_product"],
            ),
            "WARNING",
        )
        releases = json.load(ret)["releases"]
        for release_name, release_info in reversed(
            sorted(releases.items(), key=lambda x: MozillaVersion(x[1]["version"]))
        ):
            # we need to use releases_name instead of release_info since esr
            # string is included in the name. later we rely on this.
            product, version = release_name.split("-", 1)

            # Exclude any releases that don't match one of our include version
            # regexes. This is generally to avoid including versions from other
            # channels. Eg: including betas when testing releases
            for v in self.config["include_versions"]:
                if re.match(v, version):
                    break
            else:
                self.log(
                    "Skipping release whose version doesn't match any "
                    "include_version pattern: %s" % release_name,
                    level=INFO,
                )
                continue

            # We also have to trim out previous releases that aren't in the same
            # product line, too old, etc.
            if self.config["stage_product"] != product:
                self.log(
                    "Skipping release that doesn't match product name: %s"
                    % release_name,
                    level=INFO,
                )
                continue
            if MozillaVersion(version) < MozillaVersion(self.config["last_watershed"]):
                self.log(
                    "Skipping release that's behind the last watershed: %s"
                    % release_name,
                    level=INFO,
                )
                continue
            if version == self.config["to_version"]:
                self.log(
                    "Skipping release that is the same as to version: %s"
                    % release_name,
                    level=INFO,
                )
                continue
            if MozillaVersion(version) > MozillaVersion(self.config["to_version"]):
                self.log(
                    "Skipping release that's newer than to version: %s" % release_name,
                    level=INFO,
                )
                continue

            if version in self.update_paths:
                raise Exception("Found duplicate release for version: %s", version)

            # This is a crappy place to get buildids from, but we don't have a better one.
            # This will start to fail if old info files are deleted.
            info_file_url = "{}{}/{}_info.txt".format(
                self.config["previous_archive_prefix"],
                getCandidatesDir(
                    self.config["stage_product"],
                    version,
                    release_info["build_number"],
                ),
                ftp2infoFile(self.config["platform"]),
            )
            self.log(
                "Retrieving buildid from info file: %s" % info_file_url, level=DEBUG
            )
            ret = self._retry_download(info_file_url, "WARNING")
            buildID = ret.read().split(b"=")[1].strip().decode("utf-8")

            shipped_locales = self._get_file_from_repo_tag(
                product, version, f"{self.config['app_name']}/locales/shipped-locales"
            )
            app_version = self._get_file_from_repo_tag(
                product, version, f"{self.config['app_name']}/config/version.txt"
            )

            self.log("Adding {} to update paths".format(version), level=INFO)
            self.update_paths[version] = {
                "appVersion": app_version,
                "locales": getPlatformLocales(shipped_locales, self.config["platform"]),
                "buildID": buildID,
            }
            for pattern, mar_channel_ids in self.config[
                "mar_channel_id_overrides"
            ].items():
                if re.match(pattern, version):
                    self.update_paths[version]["marChannelIds"] = mar_channel_ids

    def _get_file_from_repo_tag(self, product, version, path):
        tag = "{}_{}_RELEASE".format(product.upper(), version.replace(".", "_"))
        branch = self._get_branch_url(self.config["branch_prefix"], version)
        return self._get_file_from_repo(tag, branch, path)

    def _get_file_from_repo(self, rev, branch, path):
        if self.config["local_repo"]:
            try:
                return (
                    subprocess.check_output(
                        [
                            "hg",
                            "--cwd",
                            self.config["local_repo"],
                            "cat",
                            "-r",
                            rev,
                            path,
                        ]
                    )
                    .strip()
                    .decode("utf-8")
                )
            except subprocess.CalledProcessError:
                # the tag may not exist locally
                pass

        url = urljoin(
            self.config["hg_server"],
            "{}/raw-file/{}/{}".format(
                branch,
                rev,
                path,
            ),
        )
        ret = self._retry_download(url, "WARNING")
        return ret.read().strip().decode("utf-8")

    def gather_info(self):
        from mozilla_version.gecko import GeckoVersion

        self._get_update_paths()
        if self.update_paths:
            self.log("Found update paths:", level=DEBUG)
            self.log(pprint.pformat(self.update_paths), level=DEBUG)
        elif GeckoVersion.parse(self.config["to_version"]) <= GeckoVersion.parse(
            self.config["last_watershed"]
        ):
            self.log(
                "Didn't find any update paths, but to_version {} is before the last_"
                "watershed {}, generating empty config".format(
                    self.config["to_version"],
                    self.config["last_watershed"],
                ),
                level=WARNING,
            )
        else:
            self.log("Didn't find any update paths, cannot continue", level=FATAL)

    def create_config(self):
        from mozrelease.l10n import getPlatformLocales
        from mozrelease.paths import (
            getCandidatesDir,
            getReleaseInstallerPath,
            getReleasesDir,
        )
        from mozrelease.platforms import ftp2updatePlatforms
        from mozrelease.update_verify import UpdateVerifyConfig
        from mozrelease.versions import getPrettyVersion

        candidates_dir = getCandidatesDir(
            self.config["stage_product"],
            self.config["to_version"],
            self.config["to_build_number"],
        )
        to_ = getReleaseInstallerPath(
            self.config["product"],
            self.config["product"].title(),
            self.config["to_version"],
            self.config["platform"],
            locale="%locale%",
        )
        to_path = "{}/{}".format(candidates_dir, to_)

        to_display_version = self.config.get("to_display_version")
        if not to_display_version:
            to_display_version = getPrettyVersion(self.config["to_version"])

        self.update_verify_config = UpdateVerifyConfig(
            product=self.config["product"].title(),
            channel=self.config["channel"],
            aus_server=self.config["aus_server"],
            to=to_path,
            to_build_id=self.config["to_buildid"],
            to_app_version=self.config["to_app_version"],
            to_display_version=to_display_version,
            override_certs=self.config.get("override_certs"),
        )

        to_shipped_locales = self._get_file_from_repo(
            self.config["to_revision"],
            self.config["repo_path"],
            "{}/locales/shipped-locales".format(self.config["app_name"]),
        )
        to_locales = set(
            getPlatformLocales(to_shipped_locales, self.config["platform"])
        )

        completes_only_index = 0
        for fromVersion in reversed(sorted(self.update_paths, key=CompareVersion)):
            from_ = self.update_paths[fromVersion]
            locales = sorted(list(set(from_["locales"]).intersection(to_locales)))
            appVersion = from_["appVersion"]
            build_id = from_["buildID"]
            mar_channel_IDs = from_.get("marChannelIds")

            # Use new build targets for Windows, but only on compatible
            #  versions (42+). See bug 1185456 for additional context.
            if self.config["platform"] not in ("win32", "win64") or LooseVersion(
                fromVersion
            ) < LooseVersion("42.0"):
                update_platform = ftp2updatePlatforms(self.config["platform"])[0]
            else:
                update_platform = ftp2updatePlatforms(self.config["platform"])[1]

            release_dir = getReleasesDir(self.config["stage_product"], fromVersion)
            path_ = getReleaseInstallerPath(
                self.config["product"],
                self.config["product"].title(),
                fromVersion,
                self.config["platform"],
                locale="%locale%",
            )
            from_path = "{}/{}".format(release_dir, path_)

            updater_package = "{}/{}".format(
                release_dir,
                getReleaseInstallerPath(
                    self.config["product"],
                    self.config["product"].title(),
                    fromVersion,
                    self.config["updater_platform"],
                    locale="%locale%",
                ),
            )

            # Exclude locales being full checked
            quick_check_locales = [
                l for l in locales if l not in self.config["full_check_locales"]
            ]
            # Get the intersection of from and to full_check_locales
            this_full_check_locales = [
                l for l in self.config["full_check_locales"] if l in locales
            ]

            if fromVersion in self.config["partial_versions"]:
                self.info(
                    "Generating configs for partial update checks for %s" % fromVersion
                )
                self.update_verify_config.addRelease(
                    release=appVersion,
                    build_id=build_id,
                    locales=locales,
                    patch_types=["complete", "partial"],
                    from_path=from_path,
                    ftp_server_from=self.config["previous_archive_prefix"],
                    ftp_server_to=self.config["archive_prefix"],
                    mar_channel_IDs=mar_channel_IDs,
                    platform=update_platform,
                    updater_package=updater_package,
                )
            else:
                if this_full_check_locales and is_triangular(completes_only_index):
                    self.info("Generating full check configs for %s" % fromVersion)
                    self.update_verify_config.addRelease(
                        release=appVersion,
                        build_id=build_id,
                        locales=this_full_check_locales,
                        from_path=from_path,
                        ftp_server_from=self.config["previous_archive_prefix"],
                        ftp_server_to=self.config["archive_prefix"],
                        mar_channel_IDs=mar_channel_IDs,
                        platform=update_platform,
                        updater_package=updater_package,
                    )
                # Quick test for other locales, no download
                if len(quick_check_locales) > 0:
                    self.info("Generating quick check configs for %s" % fromVersion)
                    if not is_triangular(completes_only_index):
                        # Assuming we skipped full check locales, using all locales
                        _locales = locales
                    else:
                        # Excluding full check locales from the quick check
                        _locales = quick_check_locales
                    self.update_verify_config.addRelease(
                        release=appVersion,
                        build_id=build_id,
                        locales=_locales,
                        platform=update_platform,
                    )
                completes_only_index += 1

    def write_config(self):
        # Needs to be opened in "bytes" mode because we perform relative seeks on it
        with open(self.config["output_file"], "wb+") as fh:
            self.update_verify_config.write(fh)


if __name__ == "__main__":
    UpdateVerifyConfigCreator().run_and_exit()
