#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""release.py

"""

import os
from distutils.version import LooseVersion, StrictVersion

from mozharness.base.config import parse_config_file


# SignAndroid {{{1
class ReleaseMixin():
    release_config = {}

    def query_release_config(self):
        if self.release_config:
            return self.release_config
        c = self.config
        dirs = self.query_abs_dirs()
        if c.get("release_config_file"):
            self.info("Getting release config from %s..." % c["release_config_file"])
            rc = None
            try:
                rc = parse_config_file(
                    os.path.join(dirs['abs_work_dir'],
                                 c["release_config_file"]),
                    config_dict_name="releaseConfig"
                )
            except IOError:
                self.fatal("Release config file %s not found!" % c["release_config_file"])
            except RuntimeError:
                self.fatal("Invalid release config file %s!" % c["release_config_file"])
            self.release_config['version'] = rc['version']
            self.release_config['buildnum'] = rc['buildNumber']
            self.release_config['ftp_server'] = rc['stagingServer']
            self.release_config['ftp_user'] = c.get('ftp_user', rc['hgUsername'])
            self.release_config['ftp_ssh_key'] = c.get('ftp_ssh_key', rc['hgSshKey'])
            self.release_config['release_channel'] = rc['releaseChannel']
        else:
            self.info("No release config file; using default config.")
            for key in ('version', 'buildnum',
                        'ftp_server', 'ftp_user', 'ftp_ssh_key'):
                self.release_config[key] = c[key]
        self.info("Release config:\n%s" % self.release_config)
        return self.release_config


def get_previous_version(version, partial_versions):
    """ The patcher config bumper needs to know the exact previous version
    We use LooseVersion for ESR because StrictVersion can't parse the trailing
    'esr', but StrictVersion otherwise because it can sort X.0bN lower than X.0.
    The current version is excluded to avoid an error if build1 is aborted
    before running the updates builder and now we're doing build2
    """
    if version.endswith('esr'):
        return str(max(LooseVersion(v) for v in partial_versions if
                       v != version))
    else:
        # StrictVersion truncates trailing zero in versions with more than 1
        # dot. Compose a structure that will be sorted by StrictVersion and
        # return untouched version
        composed = sorted([(v, StrictVersion(v)) for v in partial_versions if
                           v != version], key=lambda x: x[1], reverse=True)
        return composed[0][0]


