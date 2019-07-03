#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import absolute_import

import os

import mozunit

import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))


class ProcTestMisc(proctest.ProcTest):
    """ Class to test misc operations """

    def test_process_timeout_no_kill(self):
        """ Process is started, runs but we time out waiting on it
            to complete. Process should not be killed.
        """
        p = None

        def timeout_handler():
            self.assertEqual(p.proc.poll(), None)
            p.kill()
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout.ini"],
                                          cwd=here,
                                          onTimeout=(timeout_handler,),
                                          kill_on_timeout=False)
        p.run(timeout=1)
        p.wait()
        self.assertTrue(p.didTimeout)

        self.determine_status(p, False, ['returncode', 'didtimeout'])

    def test_unicode_in_environment(self):
        env = {
            'FOOBAR': 'ʘ',
        }
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here, env=env)
        # passes if no exceptions are raised
        p.run()
        p.wait()


if __name__ == '__main__':
    mozunit.main()
