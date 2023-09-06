# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import io
import os
import sys
import zipfile

import mozlog
from six.moves import configparser

from mozversion import errors

INI_DATA_MAPPING = (("application", "App"), ("platform", "Build"))


class Version(object):
    def __init__(self):
        self._info = {}
        self._logger = mozlog.get_default_logger(component="mozversion")
        if not self._logger:
            self._logger = mozlog.unstructured.getLogger("mozversion")

    def get_gecko_info(self, path):
        for type, section in INI_DATA_MAPPING:
            config_file = os.path.join(path, "%s.ini" % type)
            if os.path.exists(config_file):
                try:
                    with open(config_file) as fp:
                        self._parse_ini_file(fp, type, section)
                except OSError:
                    self._logger.warning("Unable to read %s" % config_file)
            else:
                self._logger.warning("Unable to find %s" % config_file)

    def _parse_ini_file(self, fp, type, section):
        config = configparser.RawConfigParser()
        config.read_file(fp)
        name_map = {
            "codename": "display_name",
            "milestone": "version",
            "sourcerepository": "repository",
            "sourcestamp": "changeset",
        }
        for key, value in config.items(section):
            name = name_map.get(key, key).lower()
            self._info["%s_%s" % (type, name)] = (
                config.has_option(section, key) and config.get(section, key) or None
            )

        if not self._info.get("application_display_name"):
            self._info["application_display_name"] = self._info.get("application_name")


class LocalFennecVersion(Version):
    def __init__(self, path, **kwargs):
        Version.__init__(self, **kwargs)
        self.get_gecko_info(path)

    def get_gecko_info(self, path):
        archive = zipfile.ZipFile(path, "r")
        archive_list = archive.namelist()
        for type, section in INI_DATA_MAPPING:
            filename = "%s.ini" % type
            if filename in archive_list:
                with io.TextIOWrapper(archive.open(filename)) as fp:
                    self._parse_ini_file(fp, type, section)
            else:
                self._logger.warning("Unable to find %s" % filename)

        if "package-name.txt" in archive_list:
            with io.TextIOWrapper(archive.open("package-name.txt")) as fp:
                self._info["package_name"] = fp.readlines()[0].strip()


class LocalVersion(Version):
    def __init__(self, binary, **kwargs):
        Version.__init__(self, **kwargs)

        if binary:
            # on Windows, the binary may be specified with or without the
            # .exe extension
            if not os.path.exists(binary) and not os.path.exists(binary + ".exe"):
                raise IOError("Binary path does not exist: %s" % binary)
            path = os.path.dirname(os.path.realpath(binary))
        else:
            path = os.getcwd()

        if not self.check_location(path):
            if sys.platform == "darwin":
                resources_path = os.path.join(os.path.dirname(path), "Resources")
                if self.check_location(resources_path):
                    path = resources_path
                else:
                    raise errors.LocalAppNotFoundError(path)

            else:
                raise errors.LocalAppNotFoundError(path)

        self.get_gecko_info(path)

    def check_location(self, path):
        return os.path.exists(os.path.join(path, "application.ini")) and os.path.exists(
            os.path.join(path, "platform.ini")
        )


def get_version(binary=None):
    """
    Returns the application version information as a dict. You can specify
    a path to the binary of the application or an Android APK file (to get
    version information for Firefox for Android). If this is omitted then the
    current directory is checked for the existance of an application.ini
    file.

    :param binary: Path to the binary for the application or Android APK file
    """
    if (
        binary
        and zipfile.is_zipfile(binary)
        and "AndroidManifest.xml" in zipfile.ZipFile(binary, "r").namelist()
    ):
        version = LocalFennecVersion(binary)
    else:
        version = LocalVersion(binary)

    for key, value in sorted(version._info.items()):
        if value:
            version._logger.info("%s: %s" % (key, value))

    return version._info


def cli(args=sys.argv[1:]):
    parser = argparse.ArgumentParser(
        description="Display version information for Mozilla applications"
    )
    parser.add_argument("--binary", help="path to application binary or apk")
    mozlog.commandline.add_logging_group(
        parser, include_formatters=mozlog.commandline.TEXT_FORMATTERS
    )

    args = parser.parse_args()

    mozlog.commandline.setup_logging("mozversion", args, {"mach": sys.stdout})

    get_version(binary=args.binary)


if __name__ == "__main__":
    cli()
