#!/usr/bin/env python

import sys
import io
import os
import unittest
import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))

class ProcTestOutput(proctest.ProcTest):
    """ Class to test operations related to output handling """

    def test_process_output_nonewline(self):
        """
        Process is started, outputs data with no newline
        """
        p = processhandler.ProcessHandler([self.python, "procnonewline.py"],
                                          cwd=here)

        p.run()
        p.processOutput(timeout=5)
        p.wait()

        detected, output = proctest.check_for_process("procnonewline.py")
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              False,
                              ())

    def test_stream_process_output(self):
        """
        Process output stream does not buffer
        """
        expected = '\n'.join([str(n) for n in range(0,10)])

        stream = io.BytesIO()
        buf = io.BufferedRandom(stream)

        p = processhandler.ProcessHandler([self.python, "proccountfive.py"],
                                          cwd=here,
                                          stream=buf)

        p.run()
        p.wait()
        for i in range(5, 10):
            stream.write(str(i)+'\n')

        buf.flush()
        self.assertEquals(stream.getvalue().strip(), expected)

        # make sure mozprocess doesn't close the stream
        # since mozprocess didn't create it
        self.assertFalse(buf.closed)
        buf.close()

        detected, output = proctest.check_for_process("proccountfive.py")
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              False,
                              ())

    def test_stdout_stderr_mixing(self):
        """
        Test that stdout and stderr don't get intermingled within a line.
        """
        got_output = [False]
        def processOutput(line):
            got_output[0] = True
            self.assertTrue(len(line) == 100 and
                            (all(c == 'O' for c in line) or
                            all(c == 'X' for c in line)),
                            "stdout and stderr should not be mixed")

        test_script = os.path.join(here, "gen_stdout_stderr.py")
        proc = processhandler.ProcessHandler(
            [sys.executable, test_script],
            processOutputLine=[processOutput]
        )
        proc.run()
        proc.wait()
        self.assertTrue(got_output[0], "Saw output from process")


if __name__ == '__main__':
    unittest.main()
