#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""sourcetool.py

Port of tools/buildfarm/utils/hgtool.py.

TODO: sourcetool.py currently ignores work_dir completely.
Maybe we should use it instead of dest ?
Maybe I need to rethink work_dir?
"""

import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.config import parse_config_file
from mozharness.base.script import BaseScript
from mozharness.base.vcs.mercurial import MercurialVCS

# These variables allow us to create a sourcetool.py that will [hopefully]
# work with a selection of VCSs.
#
# To avoid needing to specify --vcs VCS, softlink sourcetool.py to
# VCStool.py (where VCS is the VCS you want to use, and is a member of
# VCS_CHOICES).
VCS_CHOICES = ['hg', ]
VCS_DEFAULT = None
VCS_REQUIRED_OPTION = "--vcs VCS "
VCS_NAME = os.path.basename(sys.argv[0]).replace('tool.py', '')
if VCS_NAME in VCS_CHOICES:
    VCS_DEFAULT = VCS_NAME
    VCS_REQUIRED_OPTION = ""
SOURCE_TOOL_USAGE = """Usage:
    %%prog [options] %srepo [dest]

    %%prog %s--repo REPOSITORY [options]

    %%prog [-h|--help]""" % (VCS_REQUIRED_OPTION, VCS_REQUIRED_OPTION)


# SourceTool {{{1
class SourceTool(BaseScript):
    # These options were chosen with an eye towards backwards
    # compatibility with the existing hgtool.
    #
    # TODO: get rid of env options, or at least remove HG from the names.
    config_options = [[
        ["--rev", "-r"],
        {"action": "store",
         "dest": "vcs_revision",
         "default": os.environ.get('HG_REV'),
         "help": "Specify which revision to update to."
         }
    ], [
        ["--branch", "-b"],
        {"action": "store",
         "dest": "vcs_branch",
         "default": os.environ.get('HG_BRANCH', 'default'),
         "help": "Specify which branch to update to."
         }
    ], [
        ["--vcs", ],
        {"action": "store",
         "type": "choice",
         "dest": "vcs",
         "default": VCS_DEFAULT,
         "choices": VCS_CHOICES,
         "help": "Specify which VCS to use."
         }
    ], [
        ["--props-file", "-p"],
        {"action": "store",
         "dest": "vcs_propsfile",
         "default": os.environ.get('PROPERTIES_FILE'),
         "help": "build json file containing revision information"
         }
    ], [
        # TODO --tbox and --no-tbox should DIAF once we fix bug 630538.
        ["--tbox", ],
        {"action": "store_true",
         "dest": "tbox_output",
         "default": bool(os.environ.get('PROPERTIES_FILE')),
         "help": "Output TinderboxPrint messages."
         }
    ], [
        ["--no-tbox", ],
        {"action": "store_false",
         "dest": "tbox_output",
         "help": "Don't output TinderboxPrint messages."
         }
    ], [
        ["--repo", ],
        {"action": "store",
         "dest": "vcs_repo",
         "help": "Specify the VCS repo."
         }
    ], [
        ["--dest", ],
        {"action": "store",
         "dest": "vcs_dest",
         "help": "Specify the destination directory (optional)"
         }
    ], [
        # TODO Are the shared options HG-specific?
        # I think there are, or we can create, similar behavior in other
        # VCSs.
        ["--shared-dir", '-s'],
        {"action": "store",
         "dest": "vcs_shared_dir",
         "default": os.environ.get('HG_SHARE_BASE_DIR'),
         "help": "clone to a shared directory"
         }
    ], [
        ["--allow-unshared-local-clones", ],
        {"action": "store_true",
         "dest": "vcs_allow_unshared_local_clones",
         "default": False,
         "help": "Allow unshared checkouts if --shared-dir is specified"
         }
    ], [
        ["--check-outgoing", ],
        {"action": "store_true",
         "dest": "vcs_strip_outgoing",
         "default": False,
         "help": "check for and clobber outgoing changesets"
         }
    ]]

    def __init__(self, require_config_file=False):
        BaseScript.__init__(self, config_options=self.config_options,
                            all_actions=['source', ],
                            usage=SOURCE_TOOL_USAGE,
                            require_config_file=require_config_file)

    def _pre_config_lock(self, rw_config):
        # This is a workaround for legacy compatibility with the original
        # hgtool.py.
        #
        # Since we need to read the buildbot json props, as well as parse
        # additional commandline arguments that aren't specified via
        # options, we call this function before locking the config.
        #
        # rw_config is the BaseConfig object that parsed the options;
        # self.config is the soon-to-be-locked runtime configuration.
        #
        # This is a powerful way to hack the config before locking;
        # we need to be careful not to abuse it.
        args = rw_config.args
        c = self.config
        if c.get('vcs') is None:
            self.fatal("Must specify --vcs!\n\n%s" %
                       rw_config.config_parser.format_help())
        if c.get('vcs_repo') is None:
            if len(args) not in (1, 2):
                self.fatal("""Invalid number of arguments!
You need to either specify --repo or specify it after the options:
%s""" % rw_config.config_parser.get_usage())

            self.config['vcs_repo'] = args[0]
        if len(args) == 2:
            self.config['vcs_dest'] = args[1]
        elif not self.config.get('vcs_dest'):
            self.config['vcs_dest'] = os.path.basename(self.config['vcs_repo'])

        # This is a buildbot-specific props file.
        if self.config.get('vcs_propsfile'):
            js = parse_config_file(self.config['vcs_propsfile'])
            if self.config.get('vcs_revision') is None:
                self.config['vcs_revision'] = js['sourcestamp']['revision']
            if self.config.get('vcs_branch') is None:
                self.config['vcs_branch'] = js['sourcestamp']['branch']

    def source(self):
        vcs_obj = None
        if self.config['vcs'] == 'hg':
            vcs_obj = MercurialVCS(
                log_obj=self.log_obj,
                #
                # Torn between creating a smaller, more flexible config per
                # helper object, or passing the read-only master config as
                # vcs_obj.config and creating a smaller vcs_obj.vcs_config.
                #
                # Deciding on the latter for now, while reserving the right
                # to change my mind later.
                config=self.config,
                vcs_config={
                    'repo': self.config['vcs_repo'],
                    'dest': self.config['vcs_dest'],
                    'branch': self.config.get('vcs_branch'),
                    'revision': self.config.get('vcs_revision'),
                    'vcs_share_base': self.config.get('vcs_shared_dir'),
                    'allow_unshared_local_clones': self.config.get('vcs_allow_unshared_local_clones'),
                    'halt_on_failure': self.config.get('halt_on_failure', True),
                }
            )
        else:
            self.fatal("I don't know how to handle vcs '%s'!" % self.config['vcs'])
        got_revision = vcs_obj.ensure_repo_and_revision()

        self.info("Got revision %s\n" % got_revision)

# __main__ {{{1
if __name__ == '__main__':
    source_tool = SourceTool()
    source_tool.run_and_exit()
