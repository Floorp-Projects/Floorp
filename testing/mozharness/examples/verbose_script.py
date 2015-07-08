#!/usr/bin/env python
"""verbose_script.py

Contrast to silent_script.py.
"""

import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

#from mozharness.base.errors import TarErrorList, SSHErrorList
from mozharness.base.script import BaseScript


# VerboseExample {{{1
class VerboseExample(BaseScript):
    def __init__(self, require_config_file=False):
        super(VerboseExample, self).__init__(
            all_actions=['verbosity', ],
            require_config_file=require_config_file,
            config={"tarball_name": "bar.tar.bz2"}
        )

    def verbosity(self):
        tarball_name = self.config["tarball_name"]
        self.download_file(
            "http://people.mozilla.org/~asasaki/foo.tar.bz2",
            file_name=tarball_name
        )
        # the error_list adds more error checking.
        # the halt_on_failure will kill the script at this point if
        # unsuccessful.  Be aware if you need to do any cleanup before you
        # actually fatal(), though.  If so, you may want to either use an
        # |if self.run_command(...):| construct, or define a self._post_fatal()
        # for a generic end-of-fatal-run method.
        self.run_command(
            ["tar", "xjvf", tarball_name],
#            error_list=TarErrorList,
#            halt_on_failure=True,
#            fatal_exit_code=3,
        )
        self.rmtree("x/ship2")
        self.rmtree(tarball_name)
        self.run_command(
            ["tar", "cjvf", tarball_name, "x"],
#            error_list=TarErrorList,
#            halt_on_failure=True,
#            fatal_exit_code=3,
        )
        self.rmtree("x")
        if self.run_command(
            ["scp", tarball_name, "people.mozilla.org:public_html/foo2.tar.bz2"],
#            error_list=SSHErrorList,
        ):
            self.error("There's been a problem with the scp.  We're going to proceed anyway.")
        self.rmtree(tarball_name)


# __main__ {{{1
if __name__ == '__main__':
    verbose_example = VerboseExample()
    verbose_example.run_and_exit()
