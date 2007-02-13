# -*- test-case-name: buildbot.test.test_buildreq -*-

from twisted.trial import unittest

from buildbot import buildset, interfaces, sourcestamp
from buildbot.process import base
from buildbot.status import builder
from buildbot.changes.changes import Change

class Request(unittest.TestCase):
    def testMerge(self):
        R = base.BuildRequest
        S = sourcestamp.SourceStamp
        b1 =  R("why", S("branch1", None, None, None))
        b1r1 = R("why2", S("branch1", "rev1", None, None))
        b1r1a = R("why not", S("branch1", "rev1", None, None))
        b1r2 = R("why3", S("branch1", "rev2", None, None))
        b2r2 = R("why4", S("branch2", "rev2", None, None))
        b1r1p1 = R("why5", S("branch1", "rev1", (3, "diff"), None))
        c1 = Change("alice", [], "changed stuff", branch="branch1")
        c2 = Change("alice", [], "changed stuff", branch="branch1")
        c3 = Change("alice", [], "changed stuff", branch="branch1")
        c4 = Change("alice", [], "changed stuff", branch="branch1")
        c5 = Change("alice", [], "changed stuff", branch="branch1")
        c6 = Change("alice", [], "changed stuff", branch="branch1")
        b1c1 = R("changes", S("branch1", None, None, [c1,c2,c3]))
        b1c2 = R("changes", S("branch1", None, None, [c4,c5,c6]))

        self.failUnless(b1.canBeMergedWith(b1))
        self.failIf(b1.canBeMergedWith(b1r1))
        self.failIf(b1.canBeMergedWith(b2r2))
        self.failIf(b1.canBeMergedWith(b1r1p1))
        self.failIf(b1.canBeMergedWith(b1c1))

        self.failIf(b1r1.canBeMergedWith(b1))
        self.failUnless(b1r1.canBeMergedWith(b1r1))
        self.failIf(b1r1.canBeMergedWith(b2r2))
        self.failIf(b1r1.canBeMergedWith(b1r1p1))
        self.failIf(b1r1.canBeMergedWith(b1c1))

        self.failIf(b1r2.canBeMergedWith(b1))
        self.failIf(b1r2.canBeMergedWith(b1r1))
        self.failUnless(b1r2.canBeMergedWith(b1r2))
        self.failIf(b1r2.canBeMergedWith(b2r2))
        self.failIf(b1r2.canBeMergedWith(b1r1p1))

        self.failIf(b1r1p1.canBeMergedWith(b1))
        self.failIf(b1r1p1.canBeMergedWith(b1r1))
        self.failIf(b1r1p1.canBeMergedWith(b1r2))
        self.failIf(b1r1p1.canBeMergedWith(b2r2))
        self.failIf(b1r1p1.canBeMergedWith(b1c1))

        self.failIf(b1c1.canBeMergedWith(b1))
        self.failIf(b1c1.canBeMergedWith(b1r1))
        self.failIf(b1c1.canBeMergedWith(b1r2))
        self.failIf(b1c1.canBeMergedWith(b2r2))
        self.failIf(b1c1.canBeMergedWith(b1r1p1))
        self.failUnless(b1c1.canBeMergedWith(b1c1))
        self.failUnless(b1c1.canBeMergedWith(b1c2))

        sm = b1.mergeWith([])
        self.failUnlessEqual(sm.branch, "branch1")
        self.failUnlessEqual(sm.revision, None)
        self.failUnlessEqual(sm.patch, None)
        self.failUnlessEqual(sm.changes, [])

        ss = b1r1.mergeWith([b1r1])
        self.failUnlessEqual(ss, S("branch1", "rev1", None, None))
        why = b1r1.mergeReasons([b1r1])
        self.failUnlessEqual(why, "why2")
        why = b1r1.mergeReasons([b1r1a])
        self.failUnlessEqual(why, "why2, why not")

        ss = b1c1.mergeWith([b1c2])
        self.failUnlessEqual(ss, S("branch1", None, None, [c1,c2,c3,c4,c5,c6]))
        why = b1c1.mergeReasons([b1c2])
        self.failUnlessEqual(why, "changes")


class FakeBuilder:
    name = "fake"
    def __init__(self):
        self.requests = []
    def submitBuildRequest(self, req):
        self.requests.append(req)


class Set(unittest.TestCase):
    def testBuildSet(self):
        S = buildset.BuildSet
        a,b = FakeBuilder(), FakeBuilder()

        # two builds, the first one fails, the second one succeeds. The
        # waitUntilSuccess watcher fires as soon as the first one fails,
        # while the waitUntilFinished watcher doesn't fire until all builds
        # are complete.

        source = sourcestamp.SourceStamp()
        s = S(["a","b"], source, "forced build")
        s.start([a,b])
        self.failUnlessEqual(len(a.requests), 1)
        self.failUnlessEqual(len(b.requests), 1)
        r1 = a.requests[0]
        self.failUnlessEqual(r1.reason, s.reason)
        self.failUnlessEqual(r1.source, s.source)

        st = s.status
        self.failUnlessEqual(st.getSourceStamp(), source)
        self.failUnlessEqual(st.getReason(), "forced build")
        self.failUnlessEqual(st.getBuilderNames(), ["a","b"])
        self.failIf(st.isFinished())
        brs = st.getBuildRequests()
        self.failUnlessEqual(len(brs), 2)

        res = []
        d1 = s.waitUntilSuccess()
        d1.addCallback(lambda r: res.append(("success", r)))
        d2 = s.waitUntilFinished()
        d2.addCallback(lambda r: res.append(("finished", r)))

        self.failUnlessEqual(res, [])

        # the first build finishes here, with FAILURE
        builderstatus_a = builder.BuilderStatus("a")
        bsa = builder.BuildStatus(builderstatus_a, 1)
        bsa.setResults(builder.FAILURE)
        a.requests[0].finished(bsa)

        # any FAILURE flunks the BuildSet immediately, so the
        # waitUntilSuccess deferred fires right away. However, the
        # waitUntilFinished deferred must wait until all builds have
        # completed.
        self.failUnlessEqual(len(res), 1)
        self.failUnlessEqual(res[0][0], "success")
        bss = res[0][1]
        self.failUnless(interfaces.IBuildSetStatus(bss, None))
        self.failUnlessEqual(bss.getResults(), builder.FAILURE)

        # here we finish the second build
        builderstatus_b = builder.BuilderStatus("b")
        bsb = builder.BuildStatus(builderstatus_b, 1)
        bsb.setResults(builder.SUCCESS)
        b.requests[0].finished(bsb)

        # .. which ought to fire the waitUntilFinished deferred
        self.failUnlessEqual(len(res), 2)
        self.failUnlessEqual(res[1][0], "finished")
        self.failUnlessEqual(res[1][1], bss)

        # and finish the BuildSet overall
        self.failUnless(st.isFinished())
        self.failUnlessEqual(st.getResults(), builder.FAILURE)

    def testSuccess(self):
        S = buildset.BuildSet
        a,b = FakeBuilder(), FakeBuilder()
        # this time, both builds succeed

        source = sourcestamp.SourceStamp()
        s = S(["a","b"], source, "forced build")
        s.start([a,b])

        st = s.status
        self.failUnlessEqual(st.getSourceStamp(), source)
        self.failUnlessEqual(st.getReason(), "forced build")
        self.failUnlessEqual(st.getBuilderNames(), ["a","b"])
        self.failIf(st.isFinished())

        builderstatus_a = builder.BuilderStatus("a")
        bsa = builder.BuildStatus(builderstatus_a, 1)
        bsa.setResults(builder.SUCCESS)
        a.requests[0].finished(bsa)

        builderstatus_b = builder.BuilderStatus("b")
        bsb = builder.BuildStatus(builderstatus_b, 1)
        bsb.setResults(builder.SUCCESS)
        b.requests[0].finished(bsb)

        self.failUnless(st.isFinished())
        self.failUnlessEqual(st.getResults(), builder.SUCCESS)
        
