# -*- test-case-name: buildbot.test.test_properties -*-

import os

from twisted.trial import unittest

from buildbot.twcompat import maybeWait
from buildbot.sourcestamp import SourceStamp
from buildbot.process import base
from buildbot.steps.shell import ShellCommand, WithProperties
from buildbot.status import builder
from buildbot.slave.commands import rmdirRecursive
from buildbot.test.runutils import RunMixin

class MyBuildStep(ShellCommand):
    def _interpolateProperties(self, command):
        command = ["tar", "czf",
                   "build-%s.tar.gz" % self.getProperty("revision"),
                   "source"]
        return ShellCommand._interpolateProperties(self, command)


class FakeBuild:
    pass
class FakeBuilder:
    statusbag = None
    name = "fakebuilder"
class FakeSlave:
    slavename = "bot12"
class FakeSlaveBuilder:
    slave = FakeSlave()
    def getSlaveCommandVersion(self, command, oldversion=None):
        return "1.10"

class Interpolate(unittest.TestCase):
    def setUp(self):
        self.builder = FakeBuilder()
        self.builder_status = builder.BuilderStatus("fakebuilder")
        self.builder_status.basedir = "test_properties"
        self.builder_status.nextBuildNumber = 5
        rmdirRecursive(self.builder_status.basedir)
        os.mkdir(self.builder_status.basedir)
        self.build_status = self.builder_status.newBuild()
        req = base.BuildRequest("reason", SourceStamp(branch="branch2",
                                                      revision=1234))
        self.build = base.Build([req])
        self.build.setBuilder(self.builder)
        self.build.setupStatus(self.build_status)
        self.build.setupSlaveBuilder(FakeSlaveBuilder())

    def testWithProperties(self):
        self.build.setProperty("revision", 47)
        self.failUnlessEqual(self.build_status.getProperty("revision"), 47)
        c = ShellCommand(workdir=dir, build=self.build,
                         command=["tar", "czf",
                                  WithProperties("build-%s.tar.gz",
                                                 "revision"),
                                  "source"])
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["tar", "czf", "build-47.tar.gz", "source"])

    def testWithPropertiesDict(self):
        self.build.setProperty("other", "foo")
        self.build.setProperty("missing", None)
        c = ShellCommand(workdir=dir, build=self.build,
                         command=["tar", "czf",
                                  WithProperties("build-%(other)s.tar.gz"),
                                  "source"])
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["tar", "czf", "build-foo.tar.gz", "source"])

    def testWithPropertiesEmpty(self):
        self.build.setProperty("empty", None)
        c = ShellCommand(workdir=dir, build=self.build,
                         command=["tar", "czf",
                                  WithProperties("build-%(empty)s.tar.gz"),
                                  "source"])
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["tar", "czf", "build-.tar.gz", "source"])

    def testCustomBuildStep(self):
        c = MyBuildStep(workdir=dir, build=self.build)
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["tar", "czf", "build-1234.tar.gz", "source"])

    def testSourceStamp(self):
        c = ShellCommand(workdir=dir, build=self.build,
                         command=["touch",
                                  WithProperties("%s-dir", "branch"),
                                  WithProperties("%s-rev", "revision"),
                                  ])
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["touch", "branch2-dir", "1234-rev"])

    def testSlaveName(self):
        c = ShellCommand(workdir=dir, build=self.build,
                         command=["touch",
                                  WithProperties("%s-slave", "slavename"),
                                  ])
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["touch", "bot12-slave"])

    def testBuildNumber(self):
        c = ShellCommand(workdir=dir, build=self.build,
                         command=["touch",
                                  WithProperties("build-%d", "buildnumber"),
                                  WithProperties("builder-%s", "buildername"),
                                  ])
        cmd = c._interpolateProperties(c.command)
        self.failUnlessEqual(cmd,
                             ["touch", "build-5", "builder-fakebuilder"])


run_config = """
from buildbot.process import factory
from buildbot.steps.shell import ShellCommand, WithProperties
s = factory.s

BuildmasterConfig = c = {}
c['bots'] = [('bot1', 'sekrit')]
c['sources'] = []
c['schedulers'] = []
c['slavePortnum'] = 0

# Note: when run against twisted-1.3.0, this locks up about 5% of the time. I
# suspect that a command with no output that finishes quickly triggers a race
# condition in 1.3.0's process-reaping code. The 'touch' process becomes a
# zombie and the step never completes. To keep this from messing up the unit
# tests too badly, this step runs with a reduced timeout.

f1 = factory.BuildFactory([s(ShellCommand,
                             flunkOnFailure=True,
                             command=['touch',
                                      WithProperties('%s-slave', 'slavename'),
                                      ],
                             workdir='.',
                             timeout=10,
                             )])

b1 = {'name': 'full1', 'slavename': 'bot1', 'builddir': 'bd1', 'factory': f1}
c['builders'] = [b1]

"""

class Run(RunMixin, unittest.TestCase):
    def testInterpolate(self):
        # run an actual build with a step that interpolates a build property
        d = self.master.loadConfig(run_config)
        d.addCallback(lambda res: self.master.startService())
        d.addCallback(lambda res: self.connectOneSlave("bot1"))
        d.addCallback(lambda res: self.requestBuild("full1"))
        d.addCallback(self.failUnlessBuildSucceeded)
        def _check_touch(res):
            f = os.path.join("slavebase-bot1", "bd1", "bot1-slave")
            self.failUnless(os.path.exists(f))
            return res
        d.addCallback(_check_touch)
        return maybeWait(d)


# we test got_revision in test_vc
