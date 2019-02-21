"""Utility functions for Raptor"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import signal
import subprocess
import sys
import time
import urllib

from mozlog import get_proxy_logger, get_default_logger
from mozprocess import ProcessHandler

LOG = get_proxy_logger(component="raptor-utils")
here = os.path.dirname(os.path.realpath(__file__))

if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../mozharness')
sys.path.insert(0, mozharness_dir)

external_tools_path = os.environ.get('EXTERNALTOOLSPATH', None)

if external_tools_path is not None:
    # running in production via mozharness
    TOOLTOOL_PATH = os.path.join(external_tools_path, 'tooltool.py')
else:
    # running locally via mach
    TOOLTOOL_PATH = os.path.join(mozharness_dir, 'external_tools', 'tooltool.py')


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


def tooltool_download(manifest, run_local, raptor_dir):
    """Download a file from tooltool using the provided tooltool manifest"""
    def outputHandler(line):
        LOG.info(line)
    if run_local:
        command = [sys.executable,
                   TOOLTOOL_PATH,
                   'fetch',
                   '-o',
                   '-m', manifest]
    else:
        # we want to use the tooltool cache in production
        if os.environ.get('TOOLTOOLCACHE', None) is not None:
            _cache = os.environ['TOOLTOOLCACHE']
        else:
            _cache = "/builds/tooltool_cache"

        command = [sys.executable,
                   TOOLTOOL_PATH,
                   'fetch',
                   '-o',
                   '-m', manifest,
                   '-c',
                   _cache]

    proc = ProcessHandler(
        command, processOutputLine=outputHandler, storeOutput=False,
        cwd=raptor_dir)

    proc.run()

    try:
        proc.wait()
    except Exception:
        if proc.poll() is None:
            proc.kill(signal.SIGTERM)


def download_file_from_url(url, local_dest):
    """Receive a file in a URL and download it, i.e. for the hostutils tooltool manifest
    the url received would be formatted like this:
    https://hg.mozilla.org/try/raw-file/acb5abf52c04da7d4548fa13bd6c6848a90c32b8/testing/
      config/tooltool-manifests/linux64/hostutils.manifest"""
    if os.path.exists(local_dest):
        LOG.info("file already exists at: %s" % local_dest)
        return True
    LOG.info("downloading: %s to %s" % (url, local_dest))
    _file, _headers = urllib.urlretrieve(url, local_dest)
    return os.path.exists(local_dest)


def view_gecko_profile(ffox_bin):
    # automatically load the latest talos gecko-profile archive in profiler.firefox.com
    LOG = get_default_logger(component='raptor-view-gecko-profile')

    if sys.platform.startswith('win') and not ffox_bin.endswith(".exe"):
        ffox_bin = ffox_bin + ".exe"

    if not os.path.exists(ffox_bin):
        LOG.info("unable to find Firefox bin, cannot launch view-gecko-profile")
        return

    profile_zip = os.environ.get('RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE', None)
    if profile_zip is None or not os.path.exists(profile_zip):
        LOG.info("No local talos gecko profiles were found so not launching profiler.firefox.com")
        return

    # need the view-gecko-profile tool, it's in repo/testing/tools
    repo_dir = os.environ.get('MOZ_DEVELOPER_REPO_DIR', None)
    if repo_dir is None:
        LOG.info("unable to find MOZ_DEVELOPER_REPO_DIR, can't launch view-gecko-profile")
        return

    view_gp = os.path.join(repo_dir, 'testing', 'tools',
                           'view_gecko_profile', 'view_gecko_profile.py')
    if not os.path.exists(view_gp):
        LOG.info("unable to find the view-gecko-profile tool, cannot launch it")
        return

    command = ['python',
               view_gp,
               '-b', ffox_bin,
               '-p', profile_zip]

    LOG.info('Auto-loading this profile in perfhtml.io: %s' % profile_zip)
    LOG.info(command)

    # if the view-gecko-profile tool fails to launch for some reason, we don't
    # want to crash talos! just dump error and finsh up talos as usual
    try:
        view_profile = subprocess.Popen(command,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        # that will leave it running in own instance and let talos finish up
    except Exception as e:
        LOG.info("failed to launch view-gecko-profile tool, exeption: %s" % e)
        return

    time.sleep(5)
    ret = view_profile.poll()
    if ret is None:
        LOG.info("view-gecko-profile successfully started as pid %d" % view_profile.pid)
    else:
        LOG.error('view-gecko-profile process failed to start, poll returned: %s' % ret)
