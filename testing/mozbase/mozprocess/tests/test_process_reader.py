import unittest
import subprocess
import sys

import mozunit

from mozprocess.processhandler import ProcessReader, StoreOutput


def run_python(str_code, stdout=subprocess.PIPE, stderr=subprocess.PIPE):
    cmd = [sys.executable, '-c', str_code]
    return subprocess.Popen(cmd, stdout=stdout, stderr=stderr)


class TestProcessReader(unittest.TestCase):

    def setUp(self):
        self.out = StoreOutput()
        self.err = StoreOutput()
        self.finished = False

        def on_finished():
            self.finished = True
        self.timeout = False

        def on_timeout():
            self.timeout = True
        self.reader = ProcessReader(stdout_callback=self.out,
                                    stderr_callback=self.err,
                                    finished_callback=on_finished,
                                    timeout_callback=on_timeout)

    def test_stdout_callback(self):
        proc = run_python('print 1; print 2')
        self.reader.start(proc)
        self.reader.thread.join()

        self.assertEqual(self.out.output, ['1', '2'])
        self.assertEqual(self.err.output, [])

    def test_stderr_callback(self):
        proc = run_python('import sys; sys.stderr.write("hello world\\n")')
        self.reader.start(proc)
        self.reader.thread.join()

        self.assertEqual(self.out.output, [])
        self.assertEqual(self.err.output, ['hello world'])

    def test_stdout_and_stderr_callbacks(self):
        proc = run_python('import sys; sys.stderr.write("hello world\\n"); print 1; print 2')
        self.reader.start(proc)
        self.reader.thread.join()

        self.assertEqual(self.out.output, ['1', '2'])
        self.assertEqual(self.err.output, ['hello world'])

    def test_finished_callback(self):
        self.assertFalse(self.finished)
        proc = run_python('')
        self.reader.start(proc)
        self.reader.thread.join()
        self.assertTrue(self.finished)

    def test_timeout(self):
        self.reader.timeout = 0.05
        self.assertFalse(self.timeout)
        proc = run_python('import time; time.sleep(0.1)')
        self.reader.start(proc)
        self.reader.thread.join()
        self.assertTrue(self.timeout)
        self.assertFalse(self.finished)

    def test_output_timeout(self):
        self.reader.output_timeout = 0.05
        self.assertFalse(self.timeout)
        proc = run_python('import time; time.sleep(0.1)')
        self.reader.start(proc)
        self.reader.thread.join()
        self.assertTrue(self.timeout)
        self.assertFalse(self.finished)

    def test_read_without_eol(self):
        proc = run_python('import sys; sys.stdout.write("1")')
        self.reader.start(proc)
        self.reader.thread.join()
        self.assertEqual(self.out.output, ['1'])

    def test_read_with_strange_eol(self):
        proc = run_python('import sys; sys.stdout.write("1\\r\\r\\r\\n")')
        self.reader.start(proc)
        self.reader.thread.join()
        self.assertEqual(self.out.output, ['1'])

    def test_mixed_stdout_stderr(self):
        proc = run_python('import sys; sys.stderr.write("hello world\\n"); print 1; print 2',
                          stderr=subprocess.STDOUT)
        self.reader.start(proc)
        self.reader.thread.join()

        self.assertEqual(sorted(self.out.output), sorted(['1', '2', 'hello world']))
        self.assertEqual(self.err.output, [])


if __name__ == '__main__':
    mozunit.main()
