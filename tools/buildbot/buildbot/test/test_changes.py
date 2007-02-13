# -*- test-case-name: buildbot.test.test_changes -*-

from twisted.trial import unittest
from twisted.internet import defer, reactor

from buildbot import master
from buildbot.twcompat import maybeWait
from buildbot.changes import pb
from buildbot.scripts import runner

d1 = {'files': ["Project/foo.c", "Project/bar/boo.c"],
      'who': "marvin",
      'comments': "Some changes in Project"}
d2 = {'files': ["OtherProject/bar.c"],
      'who': "zaphod",
      'comments': "other changes"}
d3 = {'files': ["Project/baz.c", "OtherProject/bloo.c"],
      'who': "alice",
      'comments': "mixed changes"}
d4 = {'files': ["trunk/baz.c", "branches/foobranch/foo.c", "trunk/bar.c"],
      'who': "alice",
      'comments': "mixed changes"}

class TestChangePerspective(unittest.TestCase):

    def setUp(self):
        self.changes = []

    def addChange(self, c):
        self.changes.append(c)

    def testNoPrefix(self):
        p = pb.ChangePerspective(self, None)
        p.perspective_addChange(d1)
        self.failUnlessEqual(len(self.changes), 1)
        c1 = self.changes[0]
        self.failUnlessEqual(c1.files,
                             ["Project/foo.c", "Project/bar/boo.c"])
        self.failUnlessEqual(c1.comments, "Some changes in Project")
        self.failUnlessEqual(c1.who, "marvin")

    def testPrefix(self):
        p = pb.ChangePerspective(self, "Project/")

        p.perspective_addChange(d1)
        self.failUnlessEqual(len(self.changes), 1)
        c1 = self.changes[-1]
        self.failUnlessEqual(c1.files, ["foo.c", "bar/boo.c"])
        self.failUnlessEqual(c1.comments, "Some changes in Project")
        self.failUnlessEqual(c1.who, "marvin")

        p.perspective_addChange(d2) # should be ignored
        self.failUnlessEqual(len(self.changes), 1)

        p.perspective_addChange(d3) # should ignore the OtherProject file
        self.failUnlessEqual(len(self.changes), 2)

        c3 = self.changes[-1]
        self.failUnlessEqual(c3.files, ["baz.c"])
        self.failUnlessEqual(c3.comments, "mixed changes")
        self.failUnlessEqual(c3.who, "alice")

    def testPrefix2(self):
        p = pb.ChangePerspective(self, "Project/bar/")

        p.perspective_addChange(d1)
        self.failUnlessEqual(len(self.changes), 1)
        c1 = self.changes[-1]
        self.failUnlessEqual(c1.files, ["boo.c"])
        self.failUnlessEqual(c1.comments, "Some changes in Project")
        self.failUnlessEqual(c1.who, "marvin")

        p.perspective_addChange(d2) # should be ignored
        self.failUnlessEqual(len(self.changes), 1)

        p.perspective_addChange(d3) # should ignore this too
        self.failUnlessEqual(len(self.changes), 1)

    def testPrefix3(self):
        p = pb.ChangePerspective(self, "trunk/")

        p.perspective_addChange(d4)
        self.failUnlessEqual(len(self.changes), 1)
        c1 = self.changes[-1]
        self.failUnlessEqual(c1.files, ["baz.c", "bar.c"])
        self.failUnlessEqual(c1.comments, "mixed changes")

    def testPrefix4(self):
        p = pb.ChangePerspective(self, "branches/foobranch/")

        p.perspective_addChange(d4)
        self.failUnlessEqual(len(self.changes), 1)
        c1 = self.changes[-1]
        self.failUnlessEqual(c1.files, ["foo.c"])
        self.failUnlessEqual(c1.comments, "mixed changes")



config_empty = """
BuildmasterConfig = c = {}
c['bots'] = []
c['builders'] = []
c['sources'] = []
c['schedulers'] = []
c['slavePortnum'] = 0
"""

config_sender = config_empty + \
"""
from buildbot.changes import pb
c['sources'] = [pb.PBChangeSource(port=None)]
"""

class Sender(unittest.TestCase):
    def setUp(self):
        self.master = master.BuildMaster(".")
    def tearDown(self):
        d = defer.maybeDeferred(self.master.stopService)
        # TODO: something in Twisted-2.0.0 (and probably 2.0.1) doesn't shut
        # down the Broker listening socket when it's supposed to.
        # Twisted-1.3.0, and current SVN (which will be post-2.0.1) are ok.
        # This iterate() is a quick hack to deal with the problem. I need to
        # investigate more thoroughly and find a better solution.
        d.addCallback(self.stall, 0.1)
        return maybeWait(d)

    def stall(self, res, timeout):
        d = defer.Deferred()
        reactor.callLater(timeout, d.callback, res)
        return d

    def testSender(self):
        self.master.loadConfig(config_empty)
        self.master.startService()
        # TODO: BuildMaster.loadChanges replaces the change_svc object, so we
        # have to load it twice. Clean this up.
        d = self.master.loadConfig(config_sender)
        d.addCallback(self._testSender_1)
        return maybeWait(d)

    def _testSender_1(self, res):
        self.cm = cm = self.master.change_svc
        s1 = list(self.cm)[0]
        port = self.master.slavePort._port.getHost().port

        self.options = {'username': "alice",
                        'master': "localhost:%d" % port,
                        'files': ["foo.c"],
                        }

        d = runner.sendchange(self.options)
        d.addCallback(self._testSender_2)
        return d

    def _testSender_2(self, res):
        # now check that the change was received
        self.failUnlessEqual(len(self.cm.changes), 1)
        c = self.cm.changes.pop()
        self.failUnlessEqual(c.who, "alice")
        self.failUnlessEqual(c.files, ["foo.c"])
        self.failUnlessEqual(c.comments, "")
        self.failUnlessEqual(c.revision, None)

        self.options['revision'] = "r123"
        self.options['comments'] = "test change"

        d = runner.sendchange(self.options)
        d.addCallback(self._testSender_3)
        return d

    def _testSender_3(self, res):
        self.failUnlessEqual(len(self.cm.changes), 1)
        c = self.cm.changes.pop()
        self.failUnlessEqual(c.who, "alice")
        self.failUnlessEqual(c.files, ["foo.c"])
        self.failUnlessEqual(c.comments, "test change")
        self.failUnlessEqual(c.revision, "r123")

        # test options['logfile'] by creating a temporary file
        logfile = self.mktemp()
        f = open(logfile, "wt")
        f.write("longer test change")
        f.close()
        self.options['comments'] = None
        self.options['logfile'] = logfile

        d = runner.sendchange(self.options)
        d.addCallback(self._testSender_4)
        return d

    def _testSender_4(self, res):
        self.failUnlessEqual(len(self.cm.changes), 1)
        c = self.cm.changes.pop()
        self.failUnlessEqual(c.who, "alice")
        self.failUnlessEqual(c.files, ["foo.c"])
        self.failUnlessEqual(c.comments, "longer test change")
        self.failUnlessEqual(c.revision, "r123")

        # make sure that numeric revisions work too
        self.options['logfile'] = None
        del self.options['revision']
        self.options['revision_number'] = 42

        d = runner.sendchange(self.options)
        d.addCallback(self._testSender_5)
        return d

    def _testSender_5(self, res):
        self.failUnlessEqual(len(self.cm.changes), 1)
        c = self.cm.changes.pop()
        self.failUnlessEqual(c.who, "alice")
        self.failUnlessEqual(c.files, ["foo.c"])
        self.failUnlessEqual(c.comments, "")
        self.failUnlessEqual(c.revision, 42)

        # verify --branch too
        self.options['branch'] = "branches/test"

        d = runner.sendchange(self.options)
        d.addCallback(self._testSender_6)
        return d

    def _testSender_6(self, res):
        self.failUnlessEqual(len(self.cm.changes), 1)
        c = self.cm.changes.pop()
        self.failUnlessEqual(c.who, "alice")
        self.failUnlessEqual(c.files, ["foo.c"])
        self.failUnlessEqual(c.comments, "")
        self.failUnlessEqual(c.revision, 42)
        self.failUnlessEqual(c.branch, "branches/test")
