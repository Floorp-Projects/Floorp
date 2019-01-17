"""Utility functions for Raptor"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import signal
import sys
import urllib

from mozlog import get_proxy_logger
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
