#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""MarMixin, manages mar files"""

import os
import sys
import ConfigParser

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))


CONFIG = {
    "buildid_section": 'App',
    "buildid_option": "BuildID",
}


def query_ini_file(ini_file, section, option):
    ini = ConfigParser.SafeConfigParser()
    ini.read(ini_file)
    return ini.get(section, option)


def buildid_from_ini(ini_file):
    """reads an ini_file and returns the buildid"""
    return query_ini_file(ini_file,
                          CONFIG.get('buildid_section'),
                          CONFIG.get('buildid_option'))


# MarMixin {{{1
class MarMixin(object):
    def _mar_tool_dir(self):
        """returns the path or the mar tool directory"""
        config = self.config
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['abs_objdir'], config["local_mar_tool_dir"])

    def _incremental_update_script(self):
        """returns the path of incremental update script"""
        config = self.config
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['abs_mozilla_dir'],
                            config['incremental_update_script'])

    def download_mar_tools(self):
        """downloads mar tools executables (mar,mbsdiff)
           and stores them local_dir()"""
        self.info("getting mar tools")
        dst_dir = self._mar_tool_dir()
        self.mkdir_p(dst_dir)
        config = self.config
        replace_dict = {'platform': config['platform'],
                        'branch': config['branch']}
        url = config['mar_tools_url'] % replace_dict
        binaries = (config['mar'], config['mbsdiff'])
        for binary in binaries:
            from_url = "/".join((url, binary))
            full_path = os.path.join(dst_dir, binary)
            if not os.path.exists(full_path):
                self.download_file(from_url, file_name=full_path)
                self.info("downloaded %s" % full_path)
            else:
                self.info("found %s, skipping download" % full_path)
            self.chmod(full_path, 0755)

    def _temp_mar_base_dir(self):
        """a base dir for unpacking mars"""
        dirs = self.query_abs_dirs()
        return dirs['abs_objdir']

    def _unpack_mar(self, mar_file, dst_dir):
        """unpacks a mar file into dst_dir"""
        cmd = ['perl', self._unpack_script(), mar_file]
        env = self.query_bootstrap_env()
        self.info("unpacking %s" % mar_file)
        self.mkdir_p(dst_dir)
        return self.run_command(cmd,
                                cwd=dst_dir,
                                env=env,
                                halt_on_failure=True)

    def do_incremental_update(self, previous_dir, current_dir, partial_filename):
        """create an incremental update from src_mar to dst_src.
           It stores the result in partial_filename"""
        # Usage: make_incremental_update.sh [OPTIONS] ARCHIVE FROMDIR TODIR
        cmd = [self._incremental_update_script(), partial_filename,
               previous_dir, current_dir]
        env = self.query_bootstrap_env()
        cwd = self._mar_dir('update_mar_dir')
        self.mkdir_p(cwd)
        result = self.run_command(cmd, cwd=cwd, env=env)
        return result

    def get_buildid_from_mar_dir(self, mar_unpack_dir):
        """returns the buildid of the current mar file"""
        config = self.config
        ini_file = config['application_ini']
        ini_file = os.path.join(mar_unpack_dir, ini_file)
        self.info("application.ini file: %s" % ini_file)

        # log the content of application.ini
        with self.opened(ini_file, 'r') as (ini, error):
            if error:
                self.fatal('cannot open %s' % ini_file)
            self.debug(ini.read())
        return buildid_from_ini(ini_file)
