#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""fx_source.py.

Create a source readme.

"""
import sys
import os

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript
from mozharness.base.log import FATAL


class FxSource(BaseScript):
    config_options = [
        [["--repo"], {
            "dest": "repo",
            "help": "the URL to the source repo",
        }],
        [["--revision"], {
            "dest": "revision",
            "help": "the revision of the source",
        }],
        [["--disable-mock"], {
            "dest": "disable_mock",
            "action": "store_true",
            "help": "dummy option",
        }],
        [["--scm-level"], {
            "dest": "scm_level",
            "help": "dummy option",
        }],
        [["--branch"], {
            "dest": "branch",
            "help": "dummy option",
        }],
        [["--build-pool"], {
            "dest": "build_pool",
            "help": "dummy option",
        }],
    ]

    def __init__(self):
        buildscript_kwargs = {
            'config_options': self.config_options,
            'all_actions': [
                'build',
            ],
            'require_config_file': True,
        }
        super(FxSource, self).__init__(**buildscript_kwargs)

    def build(self):
        c = self.config
        contents = c['tmpl'].format(**c)
        self.write_to_file(c['out_path'], contents, create_parent_dir=True,
                           error_level=FATAL)


if __name__ == '__main__':
    fx_source = FxSource()
    fx_source.run_and_exit()
