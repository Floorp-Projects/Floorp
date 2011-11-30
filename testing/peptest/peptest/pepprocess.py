# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/ #
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Peptest.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

from mozprocess import ProcessHandler
from pepresults import Results
import mozlog
import os

results = Results()

class PepProcess(ProcessHandler):
    """
    Process handler for running peptests
    """
    def __init__(self, cmd,
                       args=None, cwd=None,
                       env=os.environ.copy(),
                       ignore_children=False,
                       **kwargs):

        ProcessHandler.__init__(self, cmd, args=args, cwd=cwd, env=env,
                                ignore_children=ignore_children, **kwargs)

        self.logger = mozlog.getLogger('PEP')
        results.fails[str(None)] = []

    def processOutputLine(self, line):
        """
        Callback called on each line of output
        Responsible for determining which output lines are relevant
        and writing them to a log
        """
        tokens = line.split(' ')
        if len(tokens) > 1 and tokens[0] == 'PEP':
            # The output is generated from the Peptest extension
            # Format is 'PEP <LEVEL> <MSG>' where <MSG> can have multiple tokens
            # The content of <MSG> depends on the <LEVEL>
            level = tokens[1]
            if level == 'TEST-START':
                results.currentTest = tokens[2].rstrip()
                results.fails[results.currentTest] = []
                self.logger.testStart(results.currentTest)
            elif level == 'TEST-END':
                metric = results.get_metric(results.currentTest)
                if len(tokens) > 4:
                    threshold = float(tokens[4].rstrip())
                else:
                    threshold = 0.0

                msg = results.currentTest \
                      + ' | fail threshold: ' + str(threshold) \
                      + ' | metric: ' + str(metric)
                if metric > threshold:
                    self.logger.testFail(msg)
                else:
                    self.logger.testPass(msg)

                self.logger.testEnd(
                        results.currentTest +
                        ' | finished in: ' + tokens[3].rstrip() + ' ms')
                results.currentTest = None
            elif level == 'ACTION-START':
                results.currentAction = tokens[3].rstrip()
                self.logger.debug(level + ' | ' + results.currentAction)
            elif level == 'ACTION-END':
                self.logger.debug(level + ' | ' + results.currentAction)
                results.currentAction = None
            elif level in ['DEBUG', 'INFO', 'WARNING', 'ERROR']:
                line = line[len('PEP ' + level)+1:]
                getattr(self.logger, level.lower())(line.rstrip())
                if level == 'ERROR':
                    results.fails[str(results.currentTest)].append(0)
            else:
                line = line[len('PEP'):]
                self.logger.debug(line.rstrip())
        elif tokens[0] == 'MOZ_EVENT_TRACE' and results.currentAction is not None:
            # The output is generated from EventTracer
            # Format is 'MOZ_EVENT_TRACE sample <TIMESTAMP> <VALUE>
            # <VALUE> is the unresponsive time in ms
            self.logger.warning(
                    results.currentTest + ' | ' + results.currentAction +
                    ' | unresponsive time: ' + tokens[3].rstrip() + ' ms')
            results.fails[results.currentTest].append(int(tokens[3].rstrip()))
