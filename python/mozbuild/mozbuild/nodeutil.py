# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import subprocess
import platform
from distutils.version import StrictVersion

from mozboot.util import get_tools_dir
from mozfile import which
from six import PY3

NODE_MIN_VERSION = StrictVersion("10.24.1")
NPM_MIN_VERSION = StrictVersion("6.14.12")


def find_node_paths():
    """Determines the possible paths for node executables.

    Returns a list of paths, which includes the build state directory.
    """
    mozbuild_tools_dir = get_tools_dir()

    if platform.system() == "Windows":
        mozbuild_node_path = os.path.join(mozbuild_tools_dir, "node")
    else:
        mozbuild_node_path = os.path.join(mozbuild_tools_dir, "node", "bin")

    # We still fallback to the PATH, since on OSes that don't have toolchain
    # artifacts available to download, Node may be coming from $PATH.
    paths = [mozbuild_node_path] + os.environ.get("PATH").split(os.pathsep)

    if platform.system() == "Windows":
        paths += [
            "%s\\nodejs" % os.environ.get("SystemDrive"),
            os.path.join(os.environ.get("ProgramFiles"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMW6432"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMFILES"), "nodejs"),
        ]

    return paths


def check_executable_version(exe, wrap_call_with_node=False):
    """Determine the version of a Node executable by invoking it.

    May raise ``subprocess.CalledProcessError`` or ``ValueError`` on failure.
    """
    out = None
    # npm may be a script (Except on Windows), so we must call it with node.
    if wrap_call_with_node and platform.system() != "Windows":
        binary, _ = find_node_executable()
        if binary:
            out = (
                subprocess.check_output(
                    [binary, exe, "--version"], universal_newlines=PY3
                )
                .lstrip("v")
                .rstrip()
            )

    # If we can't find node, or we don't need to wrap it, fallback to calling
    # direct.
    if not out:
        out = (
            subprocess.check_output([exe, "--version"], universal_newlines=PY3)
            .lstrip("v")
            .rstrip()
        )
    return StrictVersion(out)


def find_node_executable(
    nodejs_exe=os.environ.get("NODEJS"), min_version=NODE_MIN_VERSION
):
    """Find a Node executable from the mozbuild directory.

    Returns a tuple containing the the path to an executable binary and a
    version tuple. Both tuple entries will be None if a Node executable
    could not be resolved.
    """
    if nodejs_exe:
        try:
            version = check_executable_version(nodejs_exe)
        except (subprocess.CalledProcessError, ValueError):
            return None, None

        if version >= min_version:
            return nodejs_exe, version.version

        return None, None

    # "nodejs" is first in the tuple on the assumption that it's only likely to
    # exist on systems (probably linux distros) where there is a program in the path
    # called "node" that does something else.
    return find_executable("node", min_version)


def find_npm_executable(min_version=NPM_MIN_VERSION):
    """Find a Node executable from the mozbuild directory.

    Returns a tuple containing the the path to an executable binary and a
    version tuple. Both tuple entries will be None if a Node executable
    could not be resolved.
    """
    return find_executable("npm", min_version, True)


def find_executable(name, min_version, use_node_for_version_check=False):
    paths = find_node_paths()
    exe = which(name, path=paths)

    if not exe:
        return None, None

    # Verify we can invoke the executable and its version is acceptable.
    try:
        version = check_executable_version(exe, use_node_for_version_check)
    except (subprocess.CalledProcessError, ValueError):
        return None, None

    if version < min_version:
        return None, None

    return exe, version.version
