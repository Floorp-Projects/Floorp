#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit

import proctest
from mozprocess import processhandler
import six

here = os.path.dirname(os.path.abspath(__file__))


class ProcTestOutput(proctest.ProcTest):
    """ Class to test operations related to output handling """

    def test_process_output_twice(self):
        """
        Process is started, then processOutput is called a second time explicitly
        """
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_10s.ini"],
                                          cwd=here)

        p.run()
        p.processOutput(timeout=5)
        p.wait()

        self.determine_status(p, False, ())

    def test_process_output_nonewline(self):
        """
        Process is started, outputs data with no newline
        """
        p = processhandler.ProcessHandler([self.python, "scripts", "procnonewline.py"],
                                          cwd=here)

        p.run()
        p.processOutput(timeout=5)
        p.wait()

        self.determine_status(p, False, ())

    def test_stream_process_output(self):
        """
        Process output stream does not buffer
        """
        expected = '\n'.join([str(n) for n in range(0, 10)])

        stream = six.StringIO()

        p = processhandler.ProcessHandler([self.python,
                                           os.path.join("scripts", "proccountfive.py")],
                                          cwd=here,
                                          stream=stream)

        p.run()
        p.wait()
        for i in range(5, 10):
            stream.write(str(i) + '\n')

        self.assertEquals(stream.getvalue().strip(), expected)

        # make sure mozprocess doesn't close the stream
        # since mozprocess didn't create it
        self.assertFalse(stream.closed)
        stream.close()

        self.determine_status(p, False, ())


if __name__ == '__main__':
    mozunit.main()
