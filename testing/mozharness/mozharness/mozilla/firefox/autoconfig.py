""" This module helps modifying Firefox with autoconfig files."""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os

from mozharness.base.script import platform_name

AUTOCONFIG_TEXT = """// Any comment. You must start the file with a comment!
// This entry tells the browser to load a mozilla.cfg
pref("general.config.sandbox_enabled", false);
pref("general.config.filename", "mozilla.cfg");
pref("general.config.obscure_value", 0);
"""


def write_autoconfig_files(
    fx_install_dir, cfg_contents, autoconfig_contents=AUTOCONFIG_TEXT
):
    """Generate autoconfig files to modify Firefox's set up

    Read documentation in here:
    https://developer.mozilla.org/en-US/Firefox/Enterprise_deployment#Configuration

    fx_install_dir - path to Firefox installation
    cfg_contents - .cfg file containing JavaScript changes for Firefox
    autoconfig_contents - autoconfig.js content to refer to .cfg gile
    """
    with open(_cfg_file_path(fx_install_dir), "w") as fd:
        fd.write(cfg_contents)
    with open(_autoconfig_path(fx_install_dir), "w") as fd:
        fd.write(autoconfig_contents)


def read_autoconfig_file(fx_install_dir):
    """Read autoconfig file that modifies Firefox startup

    fx_install_dir - path to Firefox installation
    """
    with open(_cfg_file_path(fx_install_dir), "r") as fd:
        return fd.read()


def _autoconfig_path(fx_install_dir):
    platform = platform_name()
    if platform in ("win32", "win64"):
        return os.path.join(fx_install_dir, "defaults", "pref", "autoconfig.js")
    elif platform in ("linux", "linux64"):
        return os.path.join(fx_install_dir, "defaults/pref/autoconfig.js")
    elif platform in ("macosx"):
        return os.path.join(
            fx_install_dir, "Contents/Resources/defaults/pref/autoconfig.js"
        )
    else:
        raise Exception("Invalid platform.")


def _cfg_file_path(fx_install_dir):
    r"""
    Windows:        defaults\pref
    Mac:            Firefox.app/Contents/Resources/defaults/pref
    Linux:          defaults/pref
    """
    platform = platform_name()
    if platform in ("win32", "win64"):
        return os.path.join(fx_install_dir, "mozilla.cfg")
    elif platform in ("linux", "linux64"):
        return os.path.join(fx_install_dir, "mozilla.cfg")
    elif platform in ("macosx"):
        return os.path.join(fx_install_dir, "Contents/Resources/mozilla.cfg")
    else:
        raise Exception("Invalid platform.")
