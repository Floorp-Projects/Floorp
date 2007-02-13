# -*- test-case-name: buildbot.test.test_step -*-

import types

from twisted.python import log
from twisted.python.failure import Failure
from twisted.internet import reactor, defer, error

from buildbot import interfaces
from buildbot.twcompat import implements
from buildbot.status.builder import SUCCESS, WARNINGS, FAILURE, EXCEPTION
from buildbot.status.builder import Results, BuildRequestStatus
from buildbot.status.progress import BuildProgress

class BuildRequest:
    """I represent a request to a specific Builder to run a single build.

    I have a SourceStamp which specifies what sources I will build. This may
    specify a specific revision of the source tree (so source.branch,
    source.revision, and source.patch are used). The .patch attribute is
    either None or a tuple of (patchlevel, diff), consisting of a number to
    use in 'patch -pN', and a unified-format context diff.

    Alternatively, the SourceStamp may specify a set of Changes to be built,
    contained in source.changes. In this case, I may be mergeable with other
    BuildRequests on the same branch.

    I may be part of a BuildSet, in which case I will report status results
    to it.

    I am paired with a BuildRequestStatus object, to which I feed status
    information.

    @type source: a L{buildbot.sourcestamp.SourceStamp} instance.   
    @ivar source: the source code that this BuildRequest use

    @type reason: string
    @ivar reason: the reason this Build is being requested. Schedulers
                  provide this, but for forced builds the user requesting the
                  build will provide a string.

    @ivar status: the IBuildStatus object which tracks our status

    @ivar submittedAt: a timestamp (seconds since epoch) when this request
                       was submitted to the Builder. This is used by the CVS
                       step to compute a checkout timestamp.
    """

    source = None
    builder = None
    startCount = 0 # how many times we have tried to start this build

    if implements:
        implements(interfaces.IBuildRequestControl)
    else:
        __implements__ = interfaces.IBuildRequestControl,

    def __init__(self, reason, source, builderName=None):
        # TODO: remove the =None on builderName, it is there so I don't have
        # to change a lot of tests that create BuildRequest objects
        assert interfaces.ISourceStamp(source, None)
        self.reason = reason
        self.source = source
        self.start_watchers = []
        self.finish_watchers = []
        self.status = BuildRequestStatus(source, builderName)

    def canBeMergedWith(self, other):
        return self.source.canBeMergedWith(other.source)

    def mergeWith(self, others):
        return self.source.mergeWith([o.source for o in others])

    def mergeReasons(self, others):
        """Return a reason for the merged build request."""
        reasons = []
        for req in [self] + others:
            if req.reason and req.reason not in reasons:
                reasons.append(req.reason)
        return ", ".join(reasons)

    def waitUntilFinished(self):
        """Get a Deferred that will fire (with a
        L{buildbot.interfaces.IBuildStatus} instance when the build
        finishes."""
        d = defer.Deferred()
        self.finish_watchers.append(d)
        return d

    # these are called by the Builder

    def requestSubmitted(self, builder):
        # the request has been placed on the queue
        self.builder = builder

    def buildStarted(self, build, buildstatus):
        """This is called by the Builder when a Build has been started in the
        hopes of satifying this BuildRequest. It may be called multiple
        times, since interrupted builds and lost buildslaves may force
        multiple Builds to be run until the fate of the BuildRequest is known
        for certain."""
        for o in self.start_watchers[:]:
            # these observers get the IBuildControl
            o(build)
        # while these get the IBuildStatus
        self.status.buildStarted(buildstatus)

    def finished(self, buildstatus):
        """This is called by the Builder when the BuildRequest has been
        retired. This happens when its Build has either succeeded (yay!) or
        failed (boo!). TODO: If it is halted due to an exception (oops!), or
        some other retryable error, C{finished} will not be called yet."""

        for w in self.finish_watchers:
            w.callback(buildstatus)
        self.finish_watchers = []

    # IBuildRequestControl

    def subscribe(self, observer):
        self.start_watchers.append(observer)
    def unsubscribe(self, observer):
        self.start_watchers.remove(observer)

    def cancel(self):
        """Cancel this request. This can only be successful if the Build has
        not yet been started.

        @return: a boolean indicating if the cancel was successful."""
        if self.builder:
            return self.builder.cancelBuildRequest(self)
        return False


class Build:
    """I represent a single build by a single bot. Specialized Builders can
    use subclasses of Build to hold status information unique to those build
    processes.

    I control B{how} the build proceeds. The actual build is broken up into a
    series of steps, saved in the .buildSteps[] array as a list of
    L{buildbot.process.step.BuildStep} objects. Each step is a single remote
    command, possibly a shell command.

    During the build, I put status information into my C{BuildStatus}
    gatherer.

    After the build, I go away.

    I can be used by a factory by setting buildClass on
    L{buildbot.process.factory.BuildFactory}

    @ivar request: the L{BuildRequest} that triggered me
    @ivar build_status: the L{buildbot.status.builder.BuildStatus} that
                        collects our status
    """

    if implements:
        implements(interfaces.IBuildControl)
    else:
        __implements__ = interfaces.IBuildControl,

    workdir = "build"
    build_status = None
    reason = "changes"
    finished = False
    results = None

    def __init__(self, requests):
        self.requests = requests
        for req in self.requests:
            req.startCount += 1
        self.locks = []
        # build a source stamp
        self.source = requests[0].mergeWith(requests[1:])
        self.reason = requests[0].mergeReasons(requests[1:])

        #self.abandoned = False

        self.progress = None
        self.currentStep = None
        self.slaveEnvironment = {}

    def setBuilder(self, builder):
        """
        Set the given builder as our builder.

        @type  builder: L{buildbot.process.builder.Builder}
        """
        self.builder = builder

    def setLocks(self, locks):
        self.locks = locks

    def getSourceStamp(self):
        return self.source

    def setProperty(self, propname, value):
        """Set a property on this build. This may only be called after the
        build has started, so that it has a BuildStatus object where the
        properties can live."""
        self.build_status.setProperty(propname, value)

    def getProperty(self, propname):
        return self.build_status.properties[propname]


    def allChanges(self):
        return self.source.changes

    def allFiles(self):
        # return a list of all source files that were changed
        files = []
        havedirs = 0
        for c in self.allChanges():
            for f in c.files:
                files.append(f)
            if c.isdir:
                havedirs = 1
        return files

    def __repr__(self):
        return "<Build %s>" % (self.builder.name,)

    def __getstate__(self):
        d = self.__dict__.copy()
        if d.has_key('remote'):
            del d['remote']
        return d

    def blamelist(self):
        blamelist = []
        for c in self.allChanges():
            if c.who not in blamelist:
                blamelist.append(c.who)
        blamelist.sort()
        return blamelist

    def changesText(self):
        changetext = ""
        for c in self.allChanges():
            changetext += "-" * 60 + "\n\n" + c.asText() + "\n"
        # consider sorting these by number
        return changetext

    def setSteps(self, steps):
        """Set a list of StepFactories, which are generally just class
        objects which derive from step.BuildStep . These are used to create
        the Steps themselves when the Build starts (as opposed to when it is
        first created). By creating the steps later, their __init__ method
        will have access to things like build.allFiles() ."""
        self.stepFactories = steps # tuples of (factory, kwargs)
        for s in steps:
            pass




    useProgress = True

    def getSlaveCommandVersion(self, command, oldversion=None):
        return self.slavebuilder.getSlaveCommandVersion(command, oldversion)
    def getSlaveName(self):
        return self.slavebuilder.slave.slavename

    def setupStatus(self, build_status):
        self.build_status = build_status
        self.setProperty("buildername", self.builder.name)
        self.setProperty("buildnumber", self.build_status.number)
        self.setProperty("branch", self.source.branch)
        self.setProperty("revision", self.source.revision)

    def setupSlaveBuilder(self, slavebuilder):
        self.slavebuilder = slavebuilder
        self.slavename = slavebuilder.slave.slavename
        self.build_status.setSlavename(self.slavename)
        self.setProperty("slavename", self.slavename)

    def startBuild(self, build_status, expectations, slavebuilder):
        """This method sets up the build, then starts it by invoking the
        first Step. It returns a Deferred which will fire when the build
        finishes. This Deferred is guaranteed to never errback."""

        # we are taking responsibility for watching the connection to the
        # remote. This responsibility was held by the Builder until our
        # startBuild was called, and will not return to them until we fire
        # the Deferred returned by this method.

        log.msg("%s.startBuild" % self)
        self.setupStatus(build_status)
        # now that we have a build_status, we can set properties
        self.setupSlaveBuilder(slavebuilder)

        # convert all locks into their real forms
        self.locks = [self.builder.botmaster.getLockByID(l)
                      for l in self.locks]
        # then narrow SlaveLocks down to the right slave
        self.locks = [l.getLock(self.slavebuilder) for l in self.locks]
        self.remote = slavebuilder.remote
        self.remote.notifyOnDisconnect(self.lostRemote)
        d = self.deferred = defer.Deferred()
        def _release_slave(res):
            self.slavebuilder.buildFinished()
            return res
        d.addCallback(_release_slave)

        try:
            self.setupBuild(expectations) # create .steps
        except:
            # the build hasn't started yet, so log the exception as a point
            # event instead of flunking the build. TODO: associate this
            # failure with the build instead. this involves doing
            # self.build_status.buildStarted() from within the exception
            # handler
            log.msg("Build.setupBuild failed")
            log.err(Failure())
            self.builder.builder_status.addPointEvent(["setupBuild",
                                                       "exception"],
                                                      color="purple")
            self.finished = True
            self.results = FAILURE
            self.deferred = None
            d.callback(self)
            return d

        self.build_status.buildStarted(self)
        self.acquireLocks().addCallback(self._startBuild_2)
        return d

    def acquireLocks(self, res=None):
        log.msg("acquireLocks(step %s, locks %s)" % (self, self.locks))
        if not self.locks:
            return defer.succeed(None)
        for lock in self.locks:
            if not lock.isAvailable():
                log.msg("Build %s waiting for lock %s" % (self, lock))
                d = lock.waitUntilMaybeAvailable(self)
                d.addCallback(self.acquireLocks)
                return d
        # all locks are available, claim them all
        for lock in self.locks:
            lock.claim(self)
        return defer.succeed(None)

    def _startBuild_2(self, res):
        self.startNextStep()

    def setupBuild(self, expectations):
        # create the actual BuildSteps. If there are any name collisions, we
        # add a count to the loser until it is unique.
        self.steps = []
        self.stepStatuses = {}
        stepnames = []
        sps = []

        for factory, args in self.stepFactories:
            args = args.copy()
            if not args.has_key("workdir"):
                args['workdir'] = self.workdir
            try:
                step = factory(build=self, **args)
            except:
                log.msg("error while creating step, factory=%s, args=%s"
                        % (factory, args))
                raise
            name = step.name
            count = 1
            while name in stepnames and count < 100:
                count += 1
                name = step.name + "_%d" % count
            if name in stepnames:
                raise RuntimeError("duplicate step '%s'" % step.name)
            step.name = name
            stepnames.append(name)
            self.steps.append(step)

            # tell the BuildStatus about the step. This will create a
            # BuildStepStatus and bind it to the Step.
            step_status = self.build_status.addStepWithName(name)
            step.setStepStatus(step_status)

            sp = None
            if self.useProgress:
                # XXX: maybe bail if step.progressMetrics is empty? or skip
                # progress for that one step (i.e. "it is fast"), or have a
                # separate "variable" flag that makes us bail on progress
                # tracking
                sp = step.setupProgress()
            if sp:
                sps.append(sp)

        # Create a buildbot.status.progress.BuildProgress object. This is
        # called once at startup to figure out how to build the long-term
        # Expectations object, and again at the start of each build to get a
        # fresh BuildProgress object to track progress for that individual
        # build. TODO: revisit at-startup call

        if self.useProgress:
            self.progress = BuildProgress(sps)
            if self.progress and expectations:
                self.progress.setExpectationsFrom(expectations)

        # we are now ready to set up our BuildStatus.
        self.build_status.setSourceStamp(self.source)
        self.build_status.setReason(self.reason)
        self.build_status.setBlamelist(self.blamelist())
        self.build_status.setProgress(self.progress)

        self.results = [] # list of FAILURE, SUCCESS, WARNINGS, SKIPPED
        self.result = SUCCESS # overall result, may downgrade after each step
        self.text = [] # list of text string lists (text2)

    def getNextStep(self):
        """This method is called to obtain the next BuildStep for this build.
        When it returns None (or raises a StopIteration exception), the build
        is complete."""
        if not self.steps:
            return None
        return self.steps.pop(0)

    def startNextStep(self):
        try:
            s = self.getNextStep()
        except StopIteration:
            s = None
        if not s:
            return self.allStepsDone()
        self.currentStep = s
        d = defer.maybeDeferred(s.startStep, self.remote)
        d.addCallback(self._stepDone, s)
        d.addErrback(self.buildException)

    def _stepDone(self, results, step):
        self.currentStep = None
        if self.finished:
            return # build was interrupted, don't keep building
        terminate = self.stepDone(results, step) # interpret/merge results
        if terminate:
            return self.allStepsDone()
        self.startNextStep()

    def stepDone(self, result, step):
        """This method is called when the BuildStep completes. It is passed a
        status object from the BuildStep and is responsible for merging the
        Step's results into those of the overall Build."""

        terminate = False
        text = None
        if type(result) == types.TupleType:
            result, text = result
        assert type(result) == type(SUCCESS)
        log.msg(" step '%s' complete: %s" % (step.name, Results[result]))
        self.results.append(result)
        if text:
            self.text.extend(text)
        if not self.remote:
            terminate = True
        if result == FAILURE:
            if step.warnOnFailure:
                if self.result != FAILURE:
                    self.result = WARNINGS
            if step.flunkOnFailure:
                self.result = FAILURE
            if step.haltOnFailure:
                self.result = FAILURE
                terminate = True
        elif result == WARNINGS:
            if step.warnOnWarnings:
                if self.result != FAILURE:
                    self.result = WARNINGS
            if step.flunkOnWarnings:
                self.result = FAILURE
        elif result == EXCEPTION:
            self.result = EXCEPTION
            terminate = True
        return terminate

    def lostRemote(self, remote=None):
        # the slave went away. There are several possible reasons for this,
        # and they aren't necessarily fatal. For now, kill the build, but
        # TODO: see if we can resume the build when it reconnects.
        log.msg("%s.lostRemote" % self)
        self.remote = None
        if self.currentStep:
            # this should cause the step to finish.
            log.msg(" stopping currentStep", self.currentStep)
            self.currentStep.interrupt(Failure(error.ConnectionLost()))

    def stopBuild(self, reason="<no reason given>"):
        # the idea here is to let the user cancel a build because, e.g.,
        # they realized they committed a bug and they don't want to waste
        # the time building something that they know will fail. Another
        # reason might be to abandon a stuck build. We want to mark the
        # build as failed quickly rather than waiting for the slave's
        # timeout to kill it on its own.

        log.msg(" %s: stopping build: %s" % (self, reason))
        if self.finished:
            return
        # TODO: include 'reason' in this point event
        self.builder.builder_status.addPointEvent(['interrupt'])
        self.currentStep.interrupt(reason)
        if 0:
            # TODO: maybe let its deferred do buildFinished
            if self.currentStep and self.currentStep.progress:
                # XXX: really .fail or something
                self.currentStep.progress.finish()
            text = ["stopped", reason]
            self.buildFinished(text, "red", FAILURE)

    def allStepsDone(self):
        if self.result == FAILURE:
            color = "red"
            text = ["failed"]
        elif self.result == WARNINGS:
            color = "orange"
            text = ["warnings"]
        elif self.result == EXCEPTION:
            color = "purple"
            text = ["exception"]
        else:
            color = "green"
            text = ["build", "successful"]
        text.extend(self.text)
        return self.buildFinished(text, color, self.result)

    def buildException(self, why):
        log.msg("%s.buildException" % self)
        log.err(why)
        self.buildFinished(["build", "exception"], "purple", FAILURE)

    def buildFinished(self, text, color, results):
        """This method must be called when the last Step has completed. It
        marks the Build as complete and returns the Builder to the 'idle'
        state.

        It takes three arguments which describe the overall build status:
        text, color, results. 'results' is one of SUCCESS, WARNINGS, or
        FAILURE.

        If 'results' is SUCCESS or WARNINGS, we will permit any dependant
        builds to start. If it is 'FAILURE', those builds will be
        abandoned."""

        self.finished = True
        if self.remote:
            self.remote.dontNotifyOnDisconnect(self.lostRemote)
        self.results = results

        log.msg(" %s: build finished" % self)
        self.build_status.setText(text)
        self.build_status.setColor(color)
        self.build_status.setResults(results)
        self.build_status.buildFinished()
        if self.progress:
            # XXX: also test a 'timing consistent' flag?
            log.msg(" setting expectations for next time")
            self.builder.setExpectations(self.progress)
        reactor.callLater(0, self.releaseLocks)
        self.deferred.callback(self)
        self.deferred = None

    def releaseLocks(self):
        log.msg("releaseLocks(%s): %s" % (self, self.locks))
        for lock in self.locks:
            lock.release(self)

    # IBuildControl

    def getStatus(self):
        return self.build_status

    # stopBuild is defined earlier

