# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""MarMixin, manages mar files"""

import os


# MarMixin {{{1
class MarMixin(object):
    def _mar_tool_dir(self):
        """returns the path or the mar tool directory"""
        config = self.config
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['abs_objdir'], config["local_mar_tool_dir"])

    def download_mar_tools(self):
        """downloads mar tools executables (mar,mbsdiff)
           and stores them local_dir()"""
        self.info("getting mar tools")
        dst_dir = self._mar_tool_dir()
        self.mkdir_p(dst_dir)
        config = self.config
        url = config['mar_tools_url']
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
