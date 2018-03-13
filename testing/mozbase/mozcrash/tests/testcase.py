from __future__ import absolute_import

import os
import unittest
import subprocess
import tempfile
import shutil

import mozlog.unstructured as mozlog


# Make logs go away
try:
    log = mozlog.getLogger("mozcrash", handler=mozlog.FileHandler(os.devnull))
except ValueError:
    pass


def popen_factory(stdouts):
    """Generate a class that can mock subprocess.Popen.

    :param stdouts: Iterable that should return an iterable for the
                    stdout of each process in turn.
    """
    class mock_popen(object):

        def __init__(self, args, *args_rest, **kwargs):
            self.stdout = stdouts.next()
            self.returncode = 0

        def wait(self):
            return 0

        def communicate(self):
            return (self.stdout.next(), "")

    return mock_popen


class CrashTestCase(unittest.TestCase):

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()

        # a fake file to use as a stackwalk binary
        self.stackwalk = os.path.join(self.tempdir, "stackwalk")
        open(self.stackwalk, "w").write("fake binary")

        # set mock for subprocess.Popen
        self._subprocess_popen = subprocess.Popen
        subprocess.Popen = popen_factory(self.next_mock_stdout())
        self.stdouts = []

    def tearDown(self):
        subprocess.Popen = self._subprocess_popen
        shutil.rmtree(self.tempdir)

    def create_minidump(self, name):
        open(os.path.join(self.tempdir, "{}.dmp".format(name)), "w").write("foo")
        open(os.path.join(self.tempdir, "{}.extra".format(name)), "w").write("bar")

        self.stdouts.append(["This is some output for {}".format(name)])

    def next_mock_stdout(self):
        if not self.stdouts:
            yield iter([])

        for s in self.stdouts:
            yield iter(s)
