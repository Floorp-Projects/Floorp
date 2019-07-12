"""Utility functions for Raptor"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import subprocess
import sys
import time
import yaml

from logger.logger import RaptorLogger

LOG = RaptorLogger(component='raptor-utils')
here = os.path.dirname(os.path.realpath(__file__))

external_tools_path = os.environ.get('EXTERNALTOOLSPATH', None)

if external_tools_path is not None:
    # running in production via mozharness
    TOOLTOOL_PATH = os.path.join(external_tools_path, 'tooltool.py')


def transform_platform(str_to_transform, config_platform, config_processor=None):
    """Transform platform name i.e. 'mitmproxy-rel-bin-{platform}.manifest'
    transforms to 'mitmproxy-rel-bin-osx.manifest'.
    Also transform '{x64}' if needed for 64 bit / win 10"""
    if '{platform}' not in str_to_transform and '{x64}' not in str_to_transform:
        return str_to_transform

    if 'win' in config_platform:
        platform_id = 'win'
    elif config_platform == 'mac':
        platform_id = 'osx'
    else:
        platform_id = 'linux64'

    if '{platform}' in str_to_transform:
        str_to_transform = str_to_transform.replace('{platform}', platform_id)

    if '{x64}' in str_to_transform and config_processor is not None:
        if 'x86_64' in config_processor:
            str_to_transform = str_to_transform.replace('{x64}', '_x64')
        else:
            str_to_transform = str_to_transform.replace('{x64}', '')

    return str_to_transform


def view_gecko_profile(ffox_bin):
    # automatically load the latest talos gecko-profile archive in profiler.firefox.com
    LOG_GECKO = RaptorLogger(component='raptor-view-gecko-profile')

    if sys.platform.startswith('win') and not ffox_bin.endswith(".exe"):
        ffox_bin = ffox_bin + ".exe"

    if not os.path.exists(ffox_bin):
        LOG_GECKO.info("unable to find Firefox bin, cannot launch view-gecko-profile")
        return

    profile_zip = os.environ.get('RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE', None)
    if profile_zip is None or not os.path.exists(profile_zip):
        LOG_GECKO.info("No local talos gecko profiles were found so not "
                       "launching profiler.firefox.com")
        return

    # need the view-gecko-profile tool, it's in repo/testing/tools
    repo_dir = os.environ.get('MOZ_DEVELOPER_REPO_DIR', None)
    if repo_dir is None:
        LOG_GECKO.info("unable to find MOZ_DEVELOPER_REPO_DIR, can't launch view-gecko-profile")
        return

    view_gp = os.path.join(repo_dir, 'testing', 'tools',
                           'view_gecko_profile', 'view_gecko_profile.py')
    if not os.path.exists(view_gp):
        LOG_GECKO.info("unable to find the view-gecko-profile tool, cannot launch it")
        return

    command = ['python',
               view_gp,
               '-b', ffox_bin,
               '-p', profile_zip]

    LOG_GECKO.info('Auto-loading this profile in perfhtml.io: %s' % profile_zip)
    LOG_GECKO.info(command)

    # if the view-gecko-profile tool fails to launch for some reason, we don't
    # want to crash talos! just dump error and finsh up talos as usual
    try:
        view_profile = subprocess.Popen(command,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        # that will leave it running in own instance and let talos finish up
    except Exception as e:
        LOG_GECKO.info("failed to launch view-gecko-profile tool, exeption: %s" % e)
        return

    time.sleep(5)
    ret = view_profile.poll()
    if ret is None:
        LOG_GECKO.info("view-gecko-profile successfully started as pid %d" % view_profile.pid)
    else:
        LOG_GECKO.error('view-gecko-profile process failed to start, poll returned: %s' % ret)


def write_yml_file(yml_file, yml_data):
    # write provided data to specified local yaml file
    LOG.info("writing %s to %s" % (yml_data, yml_file))

    try:
        with open(yml_file, 'w') as outfile:
            yaml.dump(yml_data, outfile, default_flow_style=False)
    except Exception as e:
        LOG.critical("failed to write yaml file, exeption: %s" % e)
