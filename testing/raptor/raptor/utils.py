"""Utility functions for Raptor"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import yaml

from distutils.util import strtobool
from logger.logger import RaptorLogger
from mozgeckoprofiler import view_gecko_profile

LOG = RaptorLogger(component="raptor-utils")
here = os.path.dirname(os.path.realpath(__file__))

external_tools_path = os.environ.get("EXTERNALTOOLSPATH", None)

if external_tools_path is not None:
    # running in production via mozharness
    TOOLTOOL_PATH = os.path.join(external_tools_path, "tooltool.py")


def transform_platform(str_to_transform, config_platform, config_processor=None):
    """Transform platform name i.e. 'mitmproxy-rel-bin-{platform}.manifest'
    transforms to 'mitmproxy-rel-bin-osx.manifest'.
    Also transform '{x64}' if needed for 64 bit / win 10"""
    if "{platform}" not in str_to_transform and "{x64}" not in str_to_transform:
        return str_to_transform

    if "win" in config_platform:
        platform_id = "win"
    elif config_platform == "mac":
        platform_id = "osx"
    else:
        platform_id = "linux64"

    if "{platform}" in str_to_transform:
        str_to_transform = str_to_transform.replace("{platform}", platform_id)

    if "{x64}" in str_to_transform and config_processor is not None:
        if "x86_64" in config_processor:
            str_to_transform = str_to_transform.replace("{x64}", "_x64")
        else:
            str_to_transform = str_to_transform.replace("{x64}", "")

    return str_to_transform


def transform_subtest(str_to_transform, subtest_name):
    """Transform subtest name i.e. 'mitm5-linux-firefox-{subtest}.manifest'
    transforms to 'mitm5-linux-firefox-amazon.manifest'."""
    if "{subtest}" not in str_to_transform:
        return str_to_transform

    return str_to_transform.replace("{subtest}", subtest_name)


def view_gecko_profile_from_raptor():
    # automatically load the latest raptor gecko-profile archive in profiler.firefox.com
    LOG_GECKO = RaptorLogger(component="raptor-view-gecko-profile")

    profile_zip_path = os.environ.get("RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE", None)
    if profile_zip_path is None or not os.path.exists(profile_zip_path):
        LOG_GECKO.info(
            "No local raptor gecko profiles were found so not "
            "launching profiler.firefox.com"
        )
        return

    LOG_GECKO.info("Profile saved locally to: %s" % profile_zip_path)
    view_gecko_profile(profile_zip_path)


def write_yml_file(yml_file, yml_data):
    # write provided data to specified local yaml file
    LOG.info("writing %s to %s" % (yml_data, yml_file))

    try:
        with open(yml_file, "w") as outfile:
            yaml.dump(yml_data, outfile, default_flow_style=False)
    except Exception as e:
        LOG.critical("failed to write yaml file, exeption: %s" % e)


def bool_from_str(boolean_string):
    return bool(strtobool(boolean_string))
