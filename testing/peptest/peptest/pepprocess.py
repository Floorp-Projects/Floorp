# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozprocess import ProcessHandler
from pepresults import Results
from time import sleep
from threading import Thread
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

    def waitForQuit(self, timeout=5):
        for i in range(1, timeout):
            if self.proc.returncode != None:
                return
            sleep(1)
        self.proc.kill()

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
            if line.find('Test Suite Finished') != -1:
                thread = Thread(target=self.waitForQuit)
                thread.setDaemon(True) # don't hang on quit
                thread.start()

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
                      + ' | fail threshold: ' + str(threshold)
                if metric > threshold:
                    msg += ' < metric: ' + str(metric)
                    self.logger.testFail(msg)
                else:
                    msg += ' >= metric: ' + str(metric)
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
