# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re

from taskgraph.util.attributes import keymatch

# platform family is extracted from build platform by taking the alphabetic prefix
# and then translating win -> windows
_platform_re = re.compile(r"^[a-z]*")
_renames = {"win": "windows"}


_archive_formats = {
    "linux": ".tar.bz2",
    "macosx": ".tar.gz",
    "windows": ".zip",
}

_executable_extension = {
    "linux": "",
    "macosx": "",
    "windows": ".exe",
}

_architectures = {
    r"linux\b.*": "x86",
    r"linux64\b.*": "x86_64",
    r"macosx64\b.*": "macos-x86_64-aarch64",
    r"win32\b.*": "x86",
    r"win64\b(?!-aarch64).*": "x86_64",
    r"win64-aarch64\b.*": "aarch64",
}


def platform_family(build_platform):
    """Given a build platform, return the platform family (linux, macosx, etc.)"""
    family = _platform_re.match(build_platform).group(0)
    return _renames.get(family, family)


def archive_format(build_platform):
    """Given a build platform, return the archive format used on the platform."""
    return _archive_formats[platform_family(build_platform)]


def executable_extension(build_platform):
    """Given a build platform, return the executable extension used on the platform."""
    return _executable_extension[platform_family(build_platform)]


def architecture(build_platform):
    matches = keymatch(_architectures, build_platform)
    if len(matches) == 1:
        return matches[0]
    raise Exception(f"Could not determine architecture of platform `{build_platform}`.")
