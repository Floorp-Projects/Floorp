# -*- test-case-name: buildbot.test.test_maildir -*-

from twisted.trial import unittest
import os, shutil
from buildbot.changes.mail import FCMaildirSource
from twisted.internet import reactor
from twisted.python import util

class MaildirTest(unittest.TestCase):
    def setUp(self):
        print "creating empty maildir"
        self.maildir = "test-maildir"
        if os.path.isdir(self.maildir):
            shutil.rmtree(self.maildir)
            print "removing stale maildir"
        os.mkdir(self.maildir)
        os.mkdir(os.path.join(self.maildir, "cur"))
        os.mkdir(os.path.join(self.maildir, "new"))
        os.mkdir(os.path.join(self.maildir, "tmp"))
        self.source = None
        self.done = 0

    def tearDown(self):
        print "removing old maildir"
        shutil.rmtree(self.maildir)
        if self.source:
            self.source.stopService()

    def addChange(self, c):
        # NOTE: this assumes every message results in a Change, which isn't
        # true for msg8-prefix
        print "got change"
        self.changes.append(c)

    def deliverMail(self, msg):
        print "delivering", msg
        newdir = os.path.join(self.maildir, "new")
        # to do this right, use safecat
        shutil.copy(msg, newdir)

    def do_timeout(self):
        self.done = 1

    def testMaildir(self):
        self.changes = []
        s = self.source = FCMaildirSource(self.maildir)
        s.parent = self
        s.startService()
        testfiles_dir = util.sibpath(__file__, "mail")
        testfiles = [msg for msg in os.listdir(testfiles_dir)
                     if msg.startswith("msg")]
        testfiles.sort()
        count = len(testfiles)
        for i in range(count):
            msg = testfiles[i]
            reactor.callLater(2*i, self.deliverMail,
                              os.path.join(testfiles_dir, msg))
        t = reactor.callLater(2*i + 15, self.do_timeout)
        while not (self.done or len(self.changes) == count):
            reactor.iterate(0.1)
        s.stopService()
        if self.done:
            return self.fail("timeout: messages weren't received on time")
        t.cancel()
        # TODO: verify the messages, should use code from test_mailparse but
        # I'm not sure how to factor the verification routines out in a
        # useful fashion
        #for i in range(count):
        #    msg, check = test_messages[i]
        #    check(self, self.changes[i])
        

if __name__ == '__main__':
    suite = unittest.TestSuite()
    suite.addTestClass(MaildirTest)
    import sys
    reporter = unittest.TextReporter(sys.stdout)
    suite.run(reporter)
    
