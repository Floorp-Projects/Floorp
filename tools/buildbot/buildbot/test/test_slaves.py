# -*- test-case-name: buildbot.test.test_slaves -*-

from twisted.trial import unittest
from buildbot.twcompat import maybeWait
from twisted.internet import defer, reactor
from twisted.python import log

from buildbot.test.runutils import RunMixin
from buildbot.sourcestamp import SourceStamp
from buildbot.process.base import BuildRequest
from buildbot.status.builder import SUCCESS
from buildbot.slave import bot

config_1 = """
from buildbot.process import factory
from buildbot.steps import dummy
s = factory.s

BuildmasterConfig = c = {}
c['bots'] = [('bot1', 'sekrit'), ('bot2', 'sekrit'), ('bot3', 'sekrit')]
c['sources'] = []
c['schedulers'] = []
c['slavePortnum'] = 0
c['schedulers'] = []

f1 = factory.BuildFactory([s(dummy.RemoteDummy, timeout=1)])
f2 = factory.BuildFactory([s(dummy.RemoteDummy, timeout=2)])

c['builders'] = [
    {'name': 'b1', 'slavenames': ['bot1','bot2','bot3'],
     'builddir': 'b1', 'factory': f1},
    ]
"""

config_2 = config_1 + """

c['builders'] = [
    {'name': 'b1', 'slavenames': ['bot1','bot2','bot3'],
     'builddir': 'b1', 'factory': f2},
    ]

"""

class Slave(RunMixin, unittest.TestCase):

    def setUp(self):
        RunMixin.setUp(self)
        self.master.loadConfig(config_1)
        self.master.startService()
        d = self.connectSlave(["b1"])
        d.addCallback(lambda res: self.connectSlave(["b1"], "bot2"))
        return maybeWait(d)

    def doBuild(self, buildername):
        br = BuildRequest("forced", SourceStamp())
        d = br.waitUntilFinished()
        self.control.getBuilder(buildername).requestBuild(br)
        return d

    def testSequence(self):
        # make sure both slaves appear in the list.
        attached_slaves = [c for c in self.master.botmaster.slaves.values()
                           if c.slave]
        self.failUnlessEqual(len(attached_slaves), 2)
        b = self.master.botmaster.builders["b1"]
        self.failUnlessEqual(len(b.slaves), 2)

        # since the current scheduling algorithm is simple and does not
        # rotate or attempt any sort of load-balancing, two builds in
        # sequence should both use the first slave. This may change later if
        # we move to a more sophisticated scheme.

        d = self.doBuild("b1")
        d.addCallback(self._testSequence_1)
        return maybeWait(d)
    def _testSequence_1(self, res):
        self.failUnlessEqual(res.getResults(), SUCCESS)
        self.failUnlessEqual(res.getSlavename(), "bot1")

        d = self.doBuild("b1")
        d.addCallback(self._testSequence_2)
        return d
    def _testSequence_2(self, res):
        self.failUnlessEqual(res.getSlavename(), "bot1")


    def testSimultaneous(self):
        # make sure we can actually run two builds at the same time
        d1 = self.doBuild("b1")
        d2 = self.doBuild("b1")
        d1.addCallback(self._testSimultaneous_1, d2)
        return maybeWait(d1)
    def _testSimultaneous_1(self, res, d2):
        self.failUnlessEqual(res.getResults(), SUCCESS)
        self.failUnlessEqual(res.getSlavename(), "bot1")
        d2.addCallback(self._testSimultaneous_2)
        return d2
    def _testSimultaneous_2(self, res):
        self.failUnlessEqual(res.getResults(), SUCCESS)
        self.failUnlessEqual(res.getSlavename(), "bot2")

    def testFallback1(self):
        # detach the first slave, verify that a build is run using the second
        # slave instead
        d = self.shutdownSlave("bot1", "b1")
        d.addCallback(self._testFallback1_1)
        return maybeWait(d)
    def _testFallback1_1(self, res):
        attached_slaves = [c for c in self.master.botmaster.slaves.values()
                           if c.slave]
        self.failUnlessEqual(len(attached_slaves), 1)
        self.failUnlessEqual(len(self.master.botmaster.builders["b1"].slaves),
                             1)
        d = self.doBuild("b1")
        d.addCallback(self._testFallback1_2)
        return d
    def _testFallback1_2(self, res):
        self.failUnlessEqual(res.getResults(), SUCCESS)
        self.failUnlessEqual(res.getSlavename(), "bot2")

    def testFallback2(self):
        # Disable the first slave, so that a slaveping will timeout. Then
        # start a build, and verify that the non-failing (second) one is
        # claimed for the build, and that the failing one is removed from the
        # list.

        # reduce the ping time so we'll failover faster
        self.master.botmaster.builders["b1"].START_BUILD_TIMEOUT = 1
        self.disappearSlave("bot1", "b1")
        d = self.doBuild("b1")
        d.addCallback(self._testFallback2_1)
        return maybeWait(d)
    def _testFallback2_1(self, res):
        self.failUnlessEqual(res.getResults(), SUCCESS)
        self.failUnlessEqual(res.getSlavename(), "bot2")
        b1slaves = self.master.botmaster.builders["b1"].slaves
        self.failUnlessEqual(len(b1slaves), 1)
        self.failUnlessEqual(b1slaves[0].slave.slavename, "bot2")


    def notFinished(self, brs):
        # utility method
        builds = brs.getBuilds()
        self.failIf(len(builds) > 1)
        if builds:
            self.failIf(builds[0].isFinished())

    def testDontClaimPingingSlave(self):
        # have two slaves connect for the same builder. Do something to the
        # first one so that slavepings are delayed (but do not fail
        # outright).
        timers = []
        self.slaves['bot1'].debugOpts["stallPings"] = (10, timers)
        br = BuildRequest("forced", SourceStamp())
        d1 = br.waitUntilFinished()
        self.control.getBuilder("b1").requestBuild(br)
        s1 = br.status # this is a BuildRequestStatus
        # give it a chance to start pinging
        d2 = defer.Deferred()
        d2.addCallback(self._testDontClaimPingingSlave_1, d1, s1, timers)
        reactor.callLater(1, d2.callback, None)
        return maybeWait(d2)
    def _testDontClaimPingingSlave_1(self, res, d1, s1, timers):
        # now the first build is running (waiting on the ping), so start the
        # second build. This should claim the second slave, not the first,
        # because the first is busy doing the ping.
        self.notFinished(s1)
        d3 = self.doBuild("b1")
        d3.addCallback(self._testDontClaimPingingSlave_2, d1, s1, timers)
        return d3
    def _testDontClaimPingingSlave_2(self, res, d1, s1, timers):
        self.failUnlessEqual(res.getSlavename(), "bot2")
        self.notFinished(s1)
        # now let the ping complete
        self.failUnlessEqual(len(timers), 1)
        timers[0].reset(0)
        d1.addCallback(self._testDontClaimPingingSlave_3)
        return d1
    def _testDontClaimPingingSlave_3(self, res):
        self.failUnlessEqual(res.getSlavename(), "bot1")

config_3 = """
from buildbot.process import factory
from buildbot.steps import dummy
s = factory.s

BuildmasterConfig = c = {}
c['bots'] = [('bot1', 'sekrit')]
c['sources'] = []
c['schedulers'] = []
c['slavePortnum'] = 0
c['schedulers'] = []

f1 = factory.BuildFactory([s(dummy.Wait, handle='one')])
f2 = factory.BuildFactory([s(dummy.Wait, handle='two')])
f3 = factory.BuildFactory([s(dummy.Wait, handle='three')])

c['builders'] = [
    {'name': 'b1', 'slavenames': ['bot1'],
     'builddir': 'b1', 'factory': f1},
    ]
"""

config_4 = config_3 + """
c['builders'] = [
    {'name': 'b1', 'slavenames': ['bot1'],
     'builddir': 'b1', 'factory': f2},
    ]
"""

config_5 = config_3 + """
c['builders'] = [
    {'name': 'b1', 'slavenames': ['bot1'],
     'builddir': 'b1', 'factory': f3},
    ]
"""

from buildbot.slave.commands import waitCommandRegistry

class Reconfig(RunMixin, unittest.TestCase):

    def setUp(self):
        RunMixin.setUp(self)
        self.master.loadConfig(config_3)
        self.master.startService()
        d = self.connectSlave(["b1"])
        return maybeWait(d)

    def _one_started(self):
        log.msg("testReconfig._one_started")
        self.build1_started = True
        self.d1.callback(None)
        return self.d2

    def _two_started(self):
        log.msg("testReconfig._two_started")
        self.build2_started = True
        self.d3.callback(None)
        return self.d4

    def _three_started(self):
        log.msg("testReconfig._three_started")
        self.build3_started = True
        self.d5.callback(None)
        return self.d6

    def testReconfig(self):
        # reconfiguring a Builder should not interrupt any running Builds. No
        # queued BuildRequests should be lost. The next Build started should
        # use the new process.
        slave1 = self.slaves['bot1']
        bot1 = slave1.getServiceNamed('bot')
        sb1 = bot1.builders['b1']
        self.failUnless(isinstance(sb1, bot.SlaveBuilder))
        self.failUnless(sb1.running)
        b1 = self.master.botmaster.builders['b1']
        self.orig_b1 = b1

        self.d1 = d1 = defer.Deferred()
        self.d2 = d2 = defer.Deferred()
        self.d3, self.d4 = defer.Deferred(), defer.Deferred()
        self.d5, self.d6 = defer.Deferred(), defer.Deferred()
        self.build1_started = False
        self.build2_started = False
        self.build3_started = False
        waitCommandRegistry[("one","build1")] = self._one_started
        waitCommandRegistry[("two","build2")] = self._two_started
        waitCommandRegistry[("three","build3")] = self._three_started

        # use different branches to make sure these cannot be merged
        br1 = BuildRequest("build1", SourceStamp(branch="1"))
        b1.submitBuildRequest(br1)
        br2 = BuildRequest("build2", SourceStamp(branch="2"))
        b1.submitBuildRequest(br2)
        br3 = BuildRequest("build3", SourceStamp(branch="3"))
        b1.submitBuildRequest(br3)
        self.requests = (br1, br2, br3)
        # all three are now in the queue

        # wait until the first one has started
        d1.addCallback(self._testReconfig_2)
        return d1

    def _testReconfig_2(self, res):
        log.msg("_testReconfig_2")
        # confirm that it is building
        brs = self.requests[0].status.getBuilds()
        self.failUnlessEqual(len(brs), 1)
        self.build1 = brs[0]
        self.failUnlessEqual(self.build1.getCurrentStep().getName(), "wait")
        # br1 is building, br2 and br3 are in the queue (in that order). Now
        # we reconfigure the Builder.
        self.failUnless(self.build1_started)
        d = self.master.loadConfig(config_4)
        d.addCallback(self._testReconfig_3)
        return d

    def _testReconfig_3(self, res):
        log.msg("_testReconfig_3")
        # now check to see that br1 is still building, and that br2 and br3
        # are in the queue of the new builder
        b1 = self.master.botmaster.builders['b1']
        self.failIfIdentical(b1, self.orig_b1)
        self.failIf(self.build1.isFinished())
        self.failUnlessEqual(self.build1.getCurrentStep().getName(), "wait")
        self.failUnlessEqual(len(b1.buildable), 2)
        self.failUnless(self.requests[1] in b1.buildable)
        self.failUnless(self.requests[2] in b1.buildable)

        # allow br1 to finish, and make sure its status is delivered normally
        d = self.requests[0].waitUntilFinished()
        d.addCallback(self._testReconfig_4)
        self.d2.callback(None)
        return d

    def _testReconfig_4(self, bs):
        log.msg("_testReconfig_4")
        self.failUnlessEqual(bs.getReason(), "build1")
        self.failUnless(bs.isFinished())
        self.failUnlessEqual(bs.getResults(), SUCCESS)

        # at this point, the first build has finished, and there is a pending
        # call to start the second build. Once that pending call fires, there
        # is a network roundtrip before the 'wait' RemoteCommand is delivered
        # to the slave. We need to wait for both events to happen before we
        # can check to make sure it is using the correct process. Just wait a
        # full second.
        d = defer.Deferred()
        d.addCallback(self._testReconfig_5)
        reactor.callLater(1, d.callback, None)
        return d

    def _testReconfig_5(self, res):
        log.msg("_testReconfig_5")
        # at this point the next build ought to be running
        b1 = self.master.botmaster.builders['b1']
        self.failUnlessEqual(len(b1.buildable), 1)
        self.failUnless(self.requests[2] in b1.buildable)
        self.failUnlessEqual(len(b1.building), 1)
        # and it ought to be using the new process
        self.failUnless(self.build2_started)

        # now, while the second build is running, change the config multiple
        # times.

        d = self.master.loadConfig(config_3)
        d.addCallback(lambda res: self.master.loadConfig(config_4))
        d.addCallback(lambda res: self.master.loadConfig(config_5))
        def _done(res):
            # then once that's done, allow the second build to finish and
            # wait for it to complete
            da = self.requests[1].waitUntilFinished()
            self.d4.callback(None)
            return da
        d.addCallback(_done)
        def _done2(res):
            # and once *that*'s done, wait another second to let the third
            # build start
            db = defer.Deferred()
            reactor.callLater(1, db.callback, None)
            return db
        d.addCallback(_done2)
        d.addCallback(self._testReconfig_6)
        return d

    def _testReconfig_6(self, res):
        log.msg("_testReconfig_6")
        # now check to see that the third build is running
        self.failUnless(self.build3_started)

        # we're done



class Slave2(RunMixin, unittest.TestCase):

    revision = 0

    def setUp(self):
        RunMixin.setUp(self)
        self.master.loadConfig(config_1)
        self.master.startService()

    def doBuild(self, buildername, reason="forced"):
        # we need to prevent these builds from being merged, so we create
        # each of them with a different revision specifier. The revision is
        # ignored because our build process does not have a source checkout
        # step.
        self.revision += 1
        br = BuildRequest(reason, SourceStamp(revision=self.revision))
        d = br.waitUntilFinished()
        self.control.getBuilder(buildername).requestBuild(br)
        return d

    def testFirstComeFirstServed(self):
        # submit three builds, then connect a slave which fails the
        # slaveping. The first build will claim the slave, do the slaveping,
        # give up, and re-queue the build. Verify that the build gets
        # re-queued in front of all other builds. This may be tricky, because
        # the other builds may attempt to claim the just-failed slave.

        d1 = self.doBuild("b1", "first")
        d2 = self.doBuild("b1", "second")
        #buildable = self.master.botmaster.builders["b1"].buildable
        #print [b.reason for b in buildable]

        # specifically, I want the poor build to get precedence over any
        # others that were waiting. To test this, we need more builds than
        # slaves.

        # now connect a broken slave. The first build started as soon as it
        # connects, so by the time we get to our _1 method, the ill-fated
        # build has already started.
        d = self.connectSlave(["b1"], opts={"failPingOnce": True})
        d.addCallback(self._testFirstComeFirstServed_1, d1, d2)
        return maybeWait(d)
    def _testFirstComeFirstServed_1(self, res, d1, d2):
        # the master has send the slaveping. When this is received, it will
        # fail, causing the master to hang up on the slave. When it
        # reconnects, it should find the first build at the front of the
        # queue. If we simply wait for both builds to complete, then look at
        # the status logs, we should see that the builds ran in the correct
        # order.

        d = defer.DeferredList([d1,d2])
        d.addCallback(self._testFirstComeFirstServed_2)
        return d
    def _testFirstComeFirstServed_2(self, res):
        b = self.status.getBuilder("b1")
        builds = b.getBuild(0), b.getBuild(1)
        reasons = [build.getReason() for build in builds]
        self.failUnlessEqual(reasons, ["first", "second"])

