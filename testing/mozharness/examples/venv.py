#!/usr/bin/env python
"""venv.py

Test virtualenv creation. This installs talos in the local venv; that's it.
"""

import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import PythonErrorList
from mozharness.base.python import virtualenv_config_options, VirtualenvMixin
from mozharness.base.script import BaseScript

# VirtualenvExample {{{1
class VirtualenvExample(VirtualenvMixin, BaseScript):
    config_options = [[
     ["--talos-url"],
     {"action": "store",
      "dest": "talos_url",
      "default": "https://hg.mozilla.org/build/talos/archive/tip.tar.gz",
      "help": "Specify the talos pip url"
     }
    ]] + virtualenv_config_options

    def __init__(self, require_config_file=False):
        super(VirtualenvExample, self).__init__(
         config_options=self.config_options,
         all_actions=['create-virtualenv',
                      ],
         default_actions=['create-virtualenv',
                          ],
         require_config_file=require_config_file,
         config={"virtualenv_modules": ["talos"]},
        )

# __main__ {{{1
if __name__ == '__main__':
    venv_example = VirtualenvExample()
    venv_example.run_and_exit()
