'''Utility functions for Raptor'''
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mozlog import get_proxy_logger

LOG = get_proxy_logger(component="raptor-utils")


def transform_platform(str_to_transform, config_platform, config_processor=None):
    # transform platform name i.e. 'mitmproxy-rel-bin-{platform}.manifest'
    # transforms to 'mitmproxy-rel-bin-osx.manifest'
    # also will transform '{x64}' if needed for 64 bit / win 10
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
