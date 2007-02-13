import time

from twisted.python import failure
from twisted.internet import defer
from twisted.trial import unittest

from buildbot.twcompat import maybeWait
from buildbot.changes.changes import Change
from buildbot.changes.p4poller import P4Source, get_simple_split

first_p4changes = \
"""Change 1 on 2006/04/13 by slamb@testclient 'first rev'
"""

second_p4changes = \
"""Change 3 on 2006/04/13 by bob@testclient 'short desc truncated'
Change 2 on 2006/04/13 by slamb@testclient 'bar'
"""

third_p4changes = \
"""Change 5 on 2006/04/13 by mpatel@testclient 'first rev'
"""

change_4_log = \
"""Change 4 by mpatel@testclient on 2006/04/13 21:55:39

	short desc truncated because this is a long description.
"""
change_3_log = \
"""Change 3 by bob@testclient on 2006/04/13 21:51:39

	short desc truncated because this is a long description.
"""

change_2_log = \
"""Change 2 by slamb@testclient on 2006/04/13 21:46:23

	creation
"""

p4change = {
    '3': change_3_log +
"""Affected files ...

... //depot/myproject/branch_b/branch_b_file#1 add
... //depot/myproject/branch_b/whatbranch#1 branch
... //depot/myproject/branch_c/whatbranch#1 branch
""",
    '2': change_2_log +
"""Affected files ...

... //depot/myproject/trunk/whatbranch#1 add
... //depot/otherproject/trunk/something#1 add
""",
    '5': change_4_log +
"""Affected files ...

... //depot/myproject/branch_b/branch_b_file#1 add
... //depot/myproject/branch_b#75 edit
... //depot/myproject/branch_c/branch_c_file#1 add
""",
}


class MockP4Source(P4Source):
    """Test P4Source which doesn't actually invoke p4."""
    invocation = 0

    def __init__(self, p4changes, p4change, *args, **kwargs):
        P4Source.__init__(self, *args, **kwargs)
        self.p4changes = p4changes
        self.p4change = p4change

    def _get_changes(self):
        assert self.working
        result = self.p4changes[self.invocation]
        self.invocation += 1
        return defer.succeed(result)

    def _get_describe(self, dummy, num):
        assert self.working
        return defer.succeed(self.p4change[num])

class TestP4Poller(unittest.TestCase):
    def setUp(self):
        self.changes = []
        self.addChange = self.changes.append

    def failUnlessIn(self, substr, string):
        # this is for compatibility with python2.2
        if isinstance(string, str):
            self.failUnless(string.find(substr) != -1)
        else:
            self.assertIn(substr, string)

    def testCheck(self):
        """successful checks"""
        self.t = MockP4Source(p4changes=[first_p4changes, second_p4changes],
                              p4change=p4change,
                              p4port=None, p4user=None,
                              p4base='//depot/myproject/',
                              split_file=lambda x: x.split('/', 1))
        self.t.parent = self

        # The first time, it just learns the change to start at.
        self.assert_(self.t.last_change is None)
        self.assert_(not self.t.working)
        return maybeWait(self.t.checkp4().addCallback(self._testCheck2))

    def _testCheck2(self, res):
        self.assertEquals(self.changes, [])
        self.assertEquals(self.t.last_change, '1')

        # Subsequent times, it returns Change objects for new changes.
        return self.t.checkp4().addCallback(self._testCheck3)

    def _testCheck3(self, res):
        self.assertEquals(len(self.changes), 3)
        self.assertEquals(self.t.last_change, '3')
        self.assert_(not self.t.working)

        # They're supposed to go oldest to newest, so this one must be first.
        self.assertEquals(self.changes[0].asText(),
            Change(who='slamb',
                   files=['whatbranch'],
                   comments=change_2_log,
                   revision='2',
                   when=self.makeTime("2006/04/13 21:46:23"),
                   branch='trunk').asText())

        # These two can happen in either order, since they're from the same
        # Perforce change.
        self.failUnlessIn(
            Change(who='bob',
                   files=['branch_b_file',
                          'whatbranch'],
                   comments=change_3_log,
                   revision='3',
                   when=self.makeTime("2006/04/13 21:51:39"),
                   branch='branch_b').asText(),
            [c.asText() for c in self.changes])
        self.failUnlessIn(
            Change(who='bob',
                   files=['whatbranch'],
                   comments=change_3_log,
                   revision='3',
                   when=self.makeTime("2006/04/13 21:51:39"),
                   branch='branch_c').asText(),
            [c.asText() for c in self.changes])

    def makeTime(self, timestring):
        datefmt = '%Y/%m/%d %H:%M:%S'
        when = time.mktime(time.strptime(timestring, datefmt))
        return when

    def testFailedChanges(self):
        """'p4 changes' failure is properly reported"""
        self.t = MockP4Source(p4changes=['Perforce client error:\n...'],
                              p4change={},
                              p4port=None, p4user=None)
        self.t.parent = self
        d = self.t.checkp4()
        d.addBoth(self._testFailedChanges2)
        return maybeWait(d)

    def _testFailedChanges2(self, f):
        self.assert_(isinstance(f, failure.Failure))
        self.failUnlessIn('Perforce client error', str(f))
        self.assert_(not self.t.working)

    def testFailedDescribe(self):
        """'p4 describe' failure is properly reported"""
        c = dict(p4change)
        c['3'] = 'Perforce client error:\n...'
        self.t = MockP4Source(p4changes=[first_p4changes, second_p4changes],
                              p4change=c, p4port=None, p4user=None)
        self.t.parent = self
        d = self.t.checkp4()
        d.addCallback(self._testFailedDescribe2)
        return maybeWait(d)

    def _testFailedDescribe2(self, res):
        # first time finds nothing; check again.
        return self.t.checkp4().addBoth(self._testFailedDescribe3)

    def _testFailedDescribe3(self, f):
        self.assert_(isinstance(f, failure.Failure))
        self.failUnlessIn('Perforce client error', str(f))
        self.assert_(not self.t.working)
        self.assertEquals(self.t.last_change, '2')

    def testAlreadyWorking(self):
        """don't launch a new poll while old is still going"""
        self.t = P4Source()
        self.t.working = True
        self.assert_(self.t.last_change is None)
        d = self.t.checkp4()
        d.addCallback(self._testAlreadyWorking2)

    def _testAlreadyWorking2(self, res):
        self.assert_(self.t.last_change is None)

    def testSplitFile(self):
        """Make sure split file works on branch only changes"""
        self.t = MockP4Source(p4changes=[third_p4changes],
                              p4change=p4change,
                              p4port=None, p4user=None,
                              p4base='//depot/myproject/',
                              split_file=get_simple_split)
        self.t.parent = self
        self.t.last_change = 50
        d = self.t.checkp4()
        d.addCallback(self._testSplitFile)

    def _testSplitFile(self, res):
        self.assertEquals(len(self.changes), 2)
        self.assertEquals(self.t.last_change, '5')
