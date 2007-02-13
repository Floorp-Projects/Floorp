# -*- test-case-name: buildbot.test.test_steps.ReorgCompatibility -*-

# legacy compatibility

import warnings
warnings.warn("buildbot.process.step is deprecated. Please import things like ShellCommand from one of the buildbot.steps.* modules instead.",
              DeprecationWarning)

from buildbot.steps.shell import ShellCommand, WithProperties, TreeSize, Configure, Compile, Test
from buildbot.steps.source import CVS, SVN, Darcs, Git, Arch, Bazaar, Mercurial, P4, P4Sync
from buildbot.steps.dummy import Dummy, FailingDummy, RemoteDummy

from buildbot.process.buildstep import LogObserver, LogLineObserver
from buildbot.process.buildstep import RemoteShellCommand
from buildbot.process.buildstep import BuildStep, LoggingBuildStep

