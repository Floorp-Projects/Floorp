#! /usr/bin/python

# Build classes specific to the Twisted codebase

from buildbot.process.base import Build
from buildbot.process.factory import BuildFactory
from buildbot.steps import shell
from buildbot.steps.python_twisted import HLint, ProcessDocs, BuildDebs, \
     Trial, RemovePYCs

class TwistedBuild(Build):
    workdir = "Twisted" # twisted's bin/trial expects to live in here
    def isFileImportant(self, filename):
        if filename.startswith("doc/fun/"):
            return 0
        if filename.startswith("sandbox/"):
            return 0
        return 1

class TwistedTrial(Trial):
    tests = "twisted"
    # the Trial in Twisted >=2.1.0 has --recurse on by default, and -to
    # turned into --reporter=bwverbose .
    recurse = False
    trialMode = ["--reporter=bwverbose"]
    testpath = None
    trial = "./bin/trial"

class TwistedBaseFactory(BuildFactory):
    buildClass = TwistedBuild
    # bin/trial expects its parent directory to be named "Twisted": it uses
    # this to add the local tree to PYTHONPATH during tests
    workdir = "Twisted"

    def __init__(self, source):
        BuildFactory.__init__(self, [source])

class QuickTwistedBuildFactory(TwistedBaseFactory):
    treeStableTimer = 30
    useProgress = 0

    def __init__(self, source, python="python"):
        TwistedBaseFactory.__init__(self, source)
        if type(python) is str:
            python = [python]
        self.addStep(HLint, python=python[0])
        self.addStep(RemovePYCs)
        for p in python:
            cmd = [p, "setup.py", "build_ext", "-i"]
            self.addStep(shell.Compile, command=cmd, flunkOnFailure=True)
            self.addStep(TwistedTrial, python=p, testChanges=True)

class FullTwistedBuildFactory(TwistedBaseFactory):
    treeStableTimer = 5*60

    def __init__(self, source, python="python",
                 processDocs=False, runTestsRandomly=False,
                 compileOpts=[], compileOpts2=[]):
        TwistedBaseFactory.__init__(self, source)
        if processDocs:
            self.addStep(ProcessDocs)

        if type(python) == str:
            python = [python]
        assert isinstance(compileOpts, list)
        assert isinstance(compileOpts2, list)
        cmd = (python + compileOpts + ["setup.py", "build_ext"]
               + compileOpts2 + ["-i"])

        self.addStep(shell.Compile, command=cmd, flunkOnFailure=True)
        self.addStep(RemovePYCs)
        self.addStep(TwistedTrial, python=python, randomly=runTestsRandomly)

class TwistedDebsBuildFactory(TwistedBaseFactory):
    treeStableTimer = 10*60

    def __init__(self, source, python="python"):
        TwistedBaseFactory.__init__(self, source)
        self.addStep(ProcessDocs, haltOnFailure=True)
        self.addStep(BuildDebs, warnOnWarnings=True)

class TwistedReactorsBuildFactory(TwistedBaseFactory):
    treeStableTimer = 5*60

    def __init__(self, source,
                 python="python", compileOpts=[], compileOpts2=[],
                 reactors=None):
        TwistedBaseFactory.__init__(self, source)

        if type(python) == str:
            python = [python]
        assert isinstance(compileOpts, list)
        assert isinstance(compileOpts2, list)
        cmd = (python + compileOpts + ["setup.py", "build_ext"]
               + compileOpts2 + ["-i"])

        self.addStep(shell.Compile, command=cmd, warnOnFailure=True)

        if reactors == None:
            reactors = [
                'gtk2',
                'gtk',
                #'kqueue',
                'poll',
                'c',
                'qt',
                #'win32',
                ]
        for reactor in reactors:
            flunkOnFailure = 1
            warnOnFailure = 0
            #if reactor in ['c', 'qt', 'win32']:
            #    # these are buggy, so tolerate failures for now
            #    flunkOnFailure = 0
            #    warnOnFailure = 1
            self.addStep(RemovePYCs) # TODO: why?
            self.addStep(TwistedTrial, name=reactor, python=python,
                         reactor=reactor, flunkOnFailure=flunkOnFailure,
                         warnOnFailure=warnOnFailure)
