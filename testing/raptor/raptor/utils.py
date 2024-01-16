"""Utility functions for Raptor"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import importlib
import inspect
import os
import pathlib
from collections.abc import Iterable
from distutils.util import strtobool

import yaml
from base_python_support import BasePythonSupport
from logger.logger import RaptorLogger
from mozgeckoprofiler import view_gecko_profile

LOG = RaptorLogger(component="raptor-utils")
here = os.path.dirname(os.path.realpath(__file__))

external_tools_path = os.environ.get("EXTERNALTOOLSPATH", None)

if external_tools_path is not None:
    # running in production via mozharness
    TOOLTOOL_PATH = os.path.join(external_tools_path, "tooltool.py")


def flatten(data, parent_dir, sep="/"):
    """
    Converts a dictionary with nested entries like this
        {
            "dict1": {
                "dict2": {
                    "key1": value1,
                    "key2": value2,
                    ...
                },
                ...
            },
            ...
            "dict3": {
                "key3": value3,
                "key4": value4,
                ...
            }
            ...
        }

    to a "flattened" dictionary like this that has no nested entries:
        {
            "dict1-dict2-key1": value1,
            "dict1-dict2-key2": value2,
            ...
            "dict3-key3": value3,
            "dict3-key4": value4,
            ...
        }

    :param Iterable data : json data.
    :param tuple parent_dir: json fields.

    :return dict: {subtest: value}
    """
    result = {}

    if not data:
        return result

    if isinstance(data, list):
        for item in data:
            for k, v in flatten(item, parent_dir, sep=sep).items():
                result.setdefault(k, []).extend(v)

    if isinstance(data, dict):
        for k, v in data.items():
            current_dir = parent_dir + (k,)
            subtest = sep.join(current_dir)
            if isinstance(v, Iterable) and not isinstance(v, str):
                for x, y in flatten(v, current_dir, sep=sep).items():
                    result.setdefault(x, []).extend(y)
            elif v or v == 0:
                result.setdefault(subtest, []).append(v)

    return result


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


def import_support_class(path):
    """This function returns a Transformer class with the given path.

    :param str path: The path points to the custom transformer.
    :param bool ret_members: If true then return inspect.getmembers().
    :return Transformer if not ret_members else inspect.getmembers().
    """
    file = pathlib.Path(path)

    if not file.exists():
        raise Exception(f"The support_class path {path} does not exist.")

    # Importing a source file directly
    spec = importlib.util.spec_from_file_location(name=file.name, location=path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    # TODO: Add checks for methods that can be used for results/output parsing
    members = inspect.getmembers(
        module,
        lambda c: inspect.isclass(c)
        and c != BasePythonSupport
        and issubclass(c, BasePythonSupport),
    )

    if not members:
        raise Exception(
            f"The path {path} was found but it was not a valid support_class."
        )

    return members[0][-1]
