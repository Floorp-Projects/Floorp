# -*- test-case-name: buildbot.test.test_locks -*-

import random

from twisted.trial import unittest
from twisted.internet import defer, reactor

from buildbot import master
from buildbot.steps import dummy
from buildbot.sourcestamp import SourceStamp
from buildbot.process.base import BuildRequest
from buildbot.test.runutils import RunMixin
from buildbot.twcompat import maybeWait
from buildbot import locks

def claimHarder(lock, owner):
    """Return a Deferred that will fire when the lock is claimed. Keep trying
    until we succeed."""
    if lock.isAvailable():
        #print "claimHarder(%s): claiming" % owner
        lock.claim(owner)
        return defer.succeed(lock)
    #print "claimHarder(%s): waiting" % owner
    d = lock.waitUntilMaybeAvailable(owner)
    d.addCallback(claimHarder, owner)
    return d

def hold(lock, owner, mode="now"):
    if mode == "now":
        lock.release(owner)
    elif mode == "very soon":
        reactor.callLater(0, lock.release, owner)
    elif mode == "soon":
        reactor.callLater(0.1, lock.release, owner)


class Unit(unittest.TestCase):
    def testNow(self):
        l = locks.BaseLock("name")
        self.failUnless(l.isAvailable())
        l.claim("owner1")
        self.failIf(l.isAvailable())
        l.release("owner1")
        self.failUnless(l.isAvailable())

    def testLater(self):
        lock = locks.BaseLock("name")
        d = claimHarder(lock, "owner1")
        d.addCallback(lambda lock: lock.release("owner1"))
        return maybeWait(d)

    def testCompetition(self):
        lock = locks.BaseLock("name")
        d = claimHarder(lock, "owner1")
        d.addCallback(self._claim1)
        return maybeWait(d)
    def _claim1(self, lock):
        # we should have claimed it by now
        self.failIf(lock.isAvailable())
        # now set up two competing owners. We don't know which will get the
        # lock first.
        d2 = claimHarder(lock, "owner2")
        d2.addCallback(hold, "owner2", "now")
        d3 = claimHarder(lock, "owner3")
        d3.addCallback(hold, "owner3", "soon")
        dl = defer.DeferredList([d2,d3])
        dl.addCallback(self._cleanup, lock)
        # and release the lock in a moment
        reactor.callLater(0.1, lock.release, "owner1")
        return dl

    def _cleanup(self, res, lock):
        d = claimHarder(lock, "cleanup")
        d.addCallback(lambda lock: lock.release("cleanup"))
        return d

    def testRandom(self):
        lock = locks.BaseLock("name")
        dl = []
        for i in range(100):
            owner = "owner%d" % i
            mode = random.choice(["now", "very soon", "soon"])
            d = claimHarder(lock, owner)
            d.addCallback(hold, owner, mode)
            dl.append(d)
        d = defer.DeferredList(dl)
        d.addCallback(self._cleanup, lock)
        return maybeWait(d)

class Multi(unittest.TestCase):
    def testNow(self):
        lock = locks.BaseLock("name", 2)
        self.failUnless(lock.isAvailable())
        lock.claim("owner1")
        self.failUnless(lock.isAvailable())
        lock.claim("owner2")
        self.failIf(lock.isAvailable())
        lock.release("owner1")
        self.failUnless(lock.isAvailable())
        lock.release("owner2")
        self.failUnless(lock.isAvailable())

    def testLater(self):
        lock = locks.BaseLock("name", 2)
        lock.claim("owner1")
        lock.claim("owner2")
        d = claimHarder(lock, "owner3")
        d.addCallback(lambda lock: lock.release("owner3"))
        lock.release("owner2")
        lock.release("owner1")
        return maybeWait(d)

    def _cleanup(self, res, lock, count):
        dl = []
        for i in range(count):
            d = claimHarder(lock, "cleanup%d" % i)
            dl.append(d)
        d2 = defer.DeferredList(dl)
        # once all locks are claimed, we know that any previous owners have
        # been flushed out
        def _release(res):
            for i in range(count):
                lock.release("cleanup%d" % i)
        d2.addCallback(_release)
        return d2

    def testRandom(self):
        COUNT = 5
        lock = locks.BaseLock("name", COUNT)
        dl = []
        for i in range(100):
            owner = "owner%d" % i
            mode = random.choice(["now", "very soon", "soon"])
            d = claimHarder(lock, owner)
            def _check(lock):
                self.failIf(len(lock.owners) > COUNT)
                return lock
            d.addCallback(_check)
            d.addCallback(hold, owner, mode)
            dl.append(d)
        d = defer.DeferredList(dl)
        d.addCallback(self._cleanup, lock, COUNT)
        return maybeWait(d)

class Dummy:
    pass

def slave(slavename):
    slavebuilder = Dummy()
    slavebuilder.slave = Dummy()
    slavebuilder.slave.slavename = slavename
    return slavebuilder

class MakeRealLock(unittest.TestCase):

    def make(self, lockid):
        return lockid.lockClass(lockid)

    def testMaster(self):
        mid1 = locks.MasterLock("name1")
        mid2 = locks.MasterLock("name1")
        mid3 = locks.MasterLock("name3")
        mid4 = locks.MasterLock("name1", 3)
        self.failUnlessEqual(mid1, mid2)
        self.failIfEqual(mid1, mid3)
        # they should all be hashable
        d = {mid1: 1, mid2: 2, mid3: 3, mid4: 4}

        l1 = self.make(mid1)
        self.failUnlessEqual(l1.name, "name1")
        self.failUnlessEqual(l1.maxCount, 1)
        self.failUnlessIdentical(l1.getLock(slave("slave1")), l1)
        l4 = self.make(mid4)
        self.failUnlessEqual(l4.name, "name1")
        self.failUnlessEqual(l4.maxCount, 3)
        self.failUnlessIdentical(l4.getLock(slave("slave1")), l4)

    def testSlave(self):
        sid1 = locks.SlaveLock("name1")
        sid2 = locks.SlaveLock("name1")
        sid3 = locks.SlaveLock("name3")
        sid4 = locks.SlaveLock("name1", maxCount=3)
        mcfs = {"bigslave": 4, "smallslave": 1}
        sid5 = locks.SlaveLock("name1", maxCount=3, maxCountForSlave=mcfs)
        mcfs2 = {"bigslave": 4, "smallslave": 1}
        sid5a = locks.SlaveLock("name1", maxCount=3, maxCountForSlave=mcfs2)
        mcfs3 = {"bigslave": 1, "smallslave": 99}
        sid5b = locks.SlaveLock("name1", maxCount=3, maxCountForSlave=mcfs3)
        self.failUnlessEqual(sid1, sid2)
        self.failIfEqual(sid1, sid3)
        self.failIfEqual(sid1, sid4)
        self.failIfEqual(sid1, sid5)
        self.failUnlessEqual(sid5, sid5a)
        self.failIfEqual(sid5a, sid5b)
        # they should all be hashable
        d = {sid1: 1, sid2: 2, sid3: 3, sid4: 4, sid5: 5, sid5a: 6, sid5b: 7}

        l1 = self.make(sid1)
        self.failUnlessEqual(l1.name, "name1")
        self.failUnlessEqual(l1.maxCount, 1)
        l1s1 = l1.getLock(slave("slave1"))
        self.failIfIdentical(l1s1, l1)

        l4 = self.make(sid4)
        self.failUnlessEqual(l4.maxCount, 3)
        l4s1 = l4.getLock(slave("slave1"))
        self.failUnlessEqual(l4s1.maxCount, 3)

        l5 = self.make(sid5)
        l5s1 = l5.getLock(slave("bigslave"))
        l5s2 = l5.getLock(slave("smallslave"))
        l5s3 = l5.getLock(slave("unnamedslave"))
        self.failUnlessEqual(l5s1.maxCount, 4)
        self.failUnlessEqual(l5s2.maxCount, 1)
        self.failUnlessEqual(l5s3.maxCount, 3)

class GetLock(unittest.TestCase):
    def testGet(self):
        # the master.cfg file contains "lock ids", which are instances of
        # MasterLock and SlaveLock but which are not actually Locks per se.
        # When the build starts, these markers are turned into RealMasterLock
        # and RealSlaveLock instances. This insures that any builds running
        # on slaves that were unaffected by the config change are still
        # referring to the same Lock instance as new builds by builders that
        # *were* affected by the change. There have been bugs in the past in
        # which this didn't happen, and the Locks were bypassed because half
        # the builders were using one incarnation of the lock while the other
        # half were using a separate (but equal) incarnation.
        #
        # Changing the lock id in any way should cause it to be replaced in
        # the BotMaster. This will result in a couple of funky artifacts:
        # builds in progress might pay attention to a different lock, so we
        # might bypass the locking for the duration of a couple builds.
        # There's also the problem of old Locks lingering around in
        # BotMaster.locks, but they're small and shouldn't really cause a
        # problem.

        b = master.BotMaster()
        l1 = locks.MasterLock("one")
        l1a = locks.MasterLock("one")
        l2 = locks.MasterLock("one", maxCount=4)

        rl1 = b.getLockByID(l1)
        rl2 = b.getLockByID(l1a)
        self.failUnlessIdentical(rl1, rl2)
        rl3 = b.getLockByID(l2)
        self.failIfIdentical(rl1, rl3)

        s1 = locks.SlaveLock("one")
        s1a = locks.SlaveLock("one")
        s2 = locks.SlaveLock("one", maxCount=4)
        s3 = locks.SlaveLock("one", maxCount=4,
                             maxCountForSlave={"a":1, "b":2})
        s3a = locks.SlaveLock("one", maxCount=4,
                              maxCountForSlave={"a":1, "b":2})
        s4 = locks.SlaveLock("one", maxCount=4,
                             maxCountForSlave={"a":4, "b":4})

        rl1 = b.getLockByID(s1)
        rl2 = b.getLockByID(s1a)
        self.failUnlessIdentical(rl1, rl2)
        rl3 = b.getLockByID(s2)
        self.failIfIdentical(rl1, rl3)
        rl4 = b.getLockByID(s3)
        self.failIfIdentical(rl1, rl4)
        self.failIfIdentical(rl3, rl4)
        rl5 = b.getLockByID(s3a)
        self.failUnlessIdentical(rl4, rl5)
        rl6 = b.getLockByID(s4)
        self.failIfIdentical(rl5, rl6)



class LockStep(dummy.Dummy):
    def start(self):
        number = self.build.requests[0].number
        self.build.requests[0].events.append(("start", number))
        dummy.Dummy.start(self)
    def done(self):
        number = self.build.requests[0].number
        self.build.requests[0].events.append(("done", number))
        dummy.Dummy.done(self)

config_1 = """
from buildbot import locks
from buildbot.process import factory
s = factory.s
from buildbot.test.test_locks import LockStep

BuildmasterConfig = c = {}
c['bots'] = [('bot1', 'sekrit'), ('bot2', 'sekrit')]
c['sources'] = []
c['schedulers'] = []
c['slavePortnum'] = 0

first_lock = locks.SlaveLock('first')
second_lock = locks.MasterLock('second')
f1 = factory.BuildFactory([s(LockStep, timeout=2, locks=[first_lock])])
f2 = factory.BuildFactory([s(LockStep, timeout=3, locks=[second_lock])])
f3 = factory.BuildFactory([s(LockStep, timeout=2, locks=[])])

b1a = {'name': 'full1a', 'slavename': 'bot1', 'builddir': '1a', 'factory': f1}
b1b = {'name': 'full1b', 'slavename': 'bot1', 'builddir': '1b', 'factory': f1}
b1c = {'name': 'full1c', 'slavename': 'bot1', 'builddir': '1c', 'factory': f3,
       'locks': [first_lock, second_lock]}
b1d = {'name': 'full1d', 'slavename': 'bot1', 'builddir': '1d', 'factory': f2}
b2a = {'name': 'full2a', 'slavename': 'bot2', 'builddir': '2a', 'factory': f1}
b2b = {'name': 'full2b', 'slavename': 'bot2', 'builddir': '2b', 'factory': f3,
       'locks': [second_lock]}
c['builders'] = [b1a, b1b, b1c, b1d, b2a, b2b]
"""

config_1a = config_1 + \
"""
b1b = {'name': 'full1b', 'slavename': 'bot1', 'builddir': '1B', 'factory': f1}
c['builders'] = [b1a, b1b, b1c, b1d, b2a, b2b]
"""


class Locks(RunMixin, unittest.TestCase):
    def setUp(self):
        RunMixin.setUp(self)
        self.req1 = req1 = BuildRequest("forced build", SourceStamp())
        req1.number = 1
        self.req2 = req2 = BuildRequest("forced build", SourceStamp())
        req2.number = 2
        self.req3 = req3 = BuildRequest("forced build", SourceStamp())
        req3.number = 3
        req1.events = req2.events = req3.events = self.events = []
        d = self.master.loadConfig(config_1)
        d.addCallback(lambda res: self.master.startService())
        d.addCallback(lambda res: self.connectSlaves(["bot1", "bot2"],
                                                     ["full1a", "full1b",
                                                      "full1c", "full1d",
                                                      "full2a", "full2b"]))
        return maybeWait(d)

    def testLock1(self):
        self.control.getBuilder("full1a").requestBuild(self.req1)
        self.control.getBuilder("full1b").requestBuild(self.req2)
        d = defer.DeferredList([self.req1.waitUntilFinished(),
                                self.req2.waitUntilFinished()])
        d.addCallback(self._testLock1_1)
        return maybeWait(d)

    def _testLock1_1(self, res):
        # full1a should complete its step before full1b starts it
        self.failUnlessEqual(self.events,
                             [("start", 1), ("done", 1),
                              ("start", 2), ("done", 2)])

    def testLock1a(self):
        # just like testLock1, but we reload the config file first, with a
        # change that causes full1b to be changed. This tickles a design bug
        # in which full1a and full1b wind up with distinct Lock instances.
        d = self.master.loadConfig(config_1a)
        d.addCallback(self._testLock1a_1)
        return maybeWait(d)
    def _testLock1a_1(self, res):
        self.control.getBuilder("full1a").requestBuild(self.req1)
        self.control.getBuilder("full1b").requestBuild(self.req2)
        d = defer.DeferredList([self.req1.waitUntilFinished(),
                                self.req2.waitUntilFinished()])
        d.addCallback(self._testLock1a_2)
        return d

    def _testLock1a_2(self, res):
        # full1a should complete its step before full1b starts it
        self.failUnlessEqual(self.events,
                             [("start", 1), ("done", 1),
                              ("start", 2), ("done", 2)])

    def testLock2(self):
        # two builds run on separate slaves with slave-scoped locks should
        # not interfere
        self.control.getBuilder("full1a").requestBuild(self.req1)
        self.control.getBuilder("full2a").requestBuild(self.req2)
        d = defer.DeferredList([self.req1.waitUntilFinished(),
                                self.req2.waitUntilFinished()])
        d.addCallback(self._testLock2_1)
        return maybeWait(d)

    def _testLock2_1(self, res):
        # full2a should start its step before full1a finishes it. They run on
        # different slaves, however, so they might start in either order.
        self.failUnless(self.events[:2] == [("start", 1), ("start", 2)] or
                        self.events[:2] == [("start", 2), ("start", 1)])

    def testLock3(self):
        # two builds run on separate slaves with master-scoped locks should
        # not overlap
        self.control.getBuilder("full1c").requestBuild(self.req1)
        self.control.getBuilder("full2b").requestBuild(self.req2)
        d = defer.DeferredList([self.req1.waitUntilFinished(),
                                self.req2.waitUntilFinished()])
        d.addCallback(self._testLock3_1)
        return maybeWait(d)

    def _testLock3_1(self, res):
        # full2b should not start until after full1c finishes. The builds run
        # on different slaves, so we can't really predict which will start
        # first. The important thing is that they don't overlap.
        self.failUnless(self.events == [("start", 1), ("done", 1),
                                        ("start", 2), ("done", 2)]
                        or self.events == [("start", 2), ("done", 2),
                                           ("start", 1), ("done", 1)]
                        )

    def testLock4(self):
        self.control.getBuilder("full1a").requestBuild(self.req1)
        self.control.getBuilder("full1c").requestBuild(self.req2)
        self.control.getBuilder("full1d").requestBuild(self.req3)
        d = defer.DeferredList([self.req1.waitUntilFinished(),
                                self.req2.waitUntilFinished(),
                                self.req3.waitUntilFinished()])
        d.addCallback(self._testLock4_1)
        return maybeWait(d)

    def _testLock4_1(self, res):
        # full1a starts, then full1d starts (because they do not interfere).
        # Once both are done, full1c can run.
        self.failUnlessEqual(self.events,
                             [("start", 1), ("start", 3),
                              ("done", 1), ("done", 3),
                              ("start", 2), ("done", 2)])

