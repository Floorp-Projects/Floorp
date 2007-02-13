
# This is a fake test which just logs the version of Twisted, to make it
# easier to track down failures in other tests.

from twisted.trial import unittest
from twisted.python import log
from twisted import copyright
import sys
import buildbot

class Versions(unittest.TestCase):
    def test_versions(self):
        log.msg("Python Version: %s" % sys.version)
        log.msg("Twisted Version: %s" % copyright.version)
        log.msg("Buildbot Version: %s" % buildbot.version)

