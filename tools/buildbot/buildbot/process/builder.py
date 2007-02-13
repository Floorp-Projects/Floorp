#! /usr/bin/python

import warnings

from twisted.python import log, components
from twisted.spread import pb
from twisted.internet import reactor, defer

from buildbot import interfaces, sourcestamp
from buildbot.twcompat import implements
from buildbot.status.progress import Expectations
from buildbot.util import now
from buildbot.process import base

(ATTACHING, # slave attached, still checking hostinfo/etc
 IDLE, # idle, available for use
 PINGING, # build about to start, making sure it is still alive
 BUILDING, # build is running
 ) = range(4)

class SlaveBuilder(pb.Referenceable):
    """I am the master-side representative for one of the
    L{buildbot.slave.bot.SlaveBuilder} objects that lives in a remote
    buildbot. When a remote builder connects, I query it for command versions
    and then make it available to any Builds that are ready to run. """

    def __init__(self):
        self.ping_watchers = []
        self.state = ATTACHING
        self.remote = None

    def setBuilder(self, b):
        self.builder = b
        self.builder_name = b.name

    def getSlaveCommandVersion(self, command, oldversion=None):
        if self.remoteCommands is None:
            # the slave is 0.5.0 or earlier
            return oldversion
        return self.remoteCommands.get(command)

    def isAvailable(self):
        if self.state == IDLE:
            return True
        return False

    def attached(self, slave, remote, commands):
        self.slave = slave
        self.remote = remote
        self.remoteCommands = commands # maps command name to version
        log.msg("Buildslave %s attached to %s" % (slave.slavename,
                                                  self.builder_name))
        d = self.remote.callRemote("setMaster", self)
        d.addErrback(self._attachFailure, "Builder.setMaster")
        d.addCallback(self._attached2)
        return d

    def _attached2(self, res):
        d = self.remote.callRemote("print", "attached")
        d.addErrback(self._attachFailure, "Builder.print 'attached'")
        d.addCallback(self._attached3)
        return d

    def _attached3(self, res):
        # now we say they're really attached
        self.state = IDLE
        return self

    def _attachFailure(self, why, where):
        assert isinstance(where, str)
        log.msg(where)
        log.err(why)
        return why

    def detached(self):
        log.msg("Buildslave %s detached from %s" % (self.slave.slavename,
                                                    self.builder_name))
        self.slave = None
        self.remote = None
        self.remoteCommands = None

    def buildStarted(self):
        self.state = BUILDING

    def buildFinished(self):
        self.state = IDLE
        reactor.callLater(0, self.builder.maybeStartBuild)

    def ping(self, timeout, status=None):
        """Ping the slave to make sure it is still there. Returns a Deferred
        that fires with True if it is.

        @param status: if you point this at a BuilderStatus, a 'pinging'
                       event will be pushed.
        """

        self.state = PINGING
        newping = not self.ping_watchers
        d = defer.Deferred()
        self.ping_watchers.append(d)
        if newping:
            if status:
                event = status.addEvent(["pinging"], "yellow")
                d2 = defer.Deferred()
                d2.addCallback(self._pong_status, event)
                self.ping_watchers.insert(0, d2)
                # I think it will make the tests run smoother if the status
                # is updated before the ping completes
            Ping().ping(self.remote, timeout).addCallback(self._pong)

        return d

    def _pong(self, res):
        watchers, self.ping_watchers = self.ping_watchers, []
        for d in watchers:
            d.callback(res)

    def _pong_status(self, res, event):
        if res:
            event.text = ["ping", "success"]
            event.color = "green"
        else:
            event.text = ["ping", "failed"]
            event.color = "red"
        event.finish()

class Ping:
    running = False
    timer = None

    def ping(self, remote, timeout):
        assert not self.running
        self.running = True
        log.msg("sending ping")
        self.d = defer.Deferred()
        # TODO: add a distinct 'ping' command on the slave.. using 'print'
        # for this purpose is kind of silly.
        remote.callRemote("print", "ping").addCallbacks(self._pong,
                                                        self._ping_failed,
                                                        errbackArgs=(remote,))

        # We use either our own timeout or the (long) TCP timeout to detect
        # silently-missing slaves. This might happen because of a NAT
        # timeout or a routing loop. If the slave just shuts down (and we
        # somehow missed the FIN), we should get a "connection refused"
        # message.
        self.timer = reactor.callLater(timeout, self._ping_timeout, remote)
        return self.d

    def _ping_timeout(self, remote):
        log.msg("ping timeout")
        # force the BotPerspective to disconnect, since this indicates that
        # the bot is unreachable.
        del self.timer
        remote.broker.transport.loseConnection()
        # the forcibly-lost connection will now cause the ping to fail

    def _stopTimer(self):
        if not self.running:
            return
        self.running = False

        if self.timer:
            self.timer.cancel()
            del self.timer

    def _pong(self, res):
        log.msg("ping finished: success")
        self._stopTimer()
        self.d.callback(True)

    def _ping_failed(self, res, remote):
        log.msg("ping finished: failure")
        self._stopTimer()
        # the slave has some sort of internal error, disconnect them. If we
        # don't, we'll requeue a build and ping them again right away,
        # creating a nasty loop.
        remote.broker.transport.loseConnection()
        # TODO: except, if they actually did manage to get this far, they'll
        # probably reconnect right away, and we'll do this game again. Maybe
        # it would be better to leave them in the PINGING state.
        self.d.callback(False)


class Builder(pb.Referenceable):
    """I manage all Builds of a given type.

    Each Builder is created by an entry in the config file (the c['builders']
    list), with a number of parameters.

    One of these parameters is the L{buildbot.process.factory.BuildFactory}
    object that is associated with this Builder. The factory is responsible
    for creating new L{Build<buildbot.process.base.Build>} objects. Each
    Build object defines when and how the build is performed, so a new
    Factory or Builder should be defined to control this behavior.

    The Builder holds on to a number of L{base.BuildRequest} objects in a
    list named C{.buildable}. Incoming BuildRequest objects will be added to
    this list, or (if possible) merged into an existing request. When a slave
    becomes available, I will use my C{BuildFactory} to turn the request into
    a new C{Build} object. The C{BuildRequest} is forgotten, the C{Build}
    goes into C{.building} while it runs. Once the build finishes, I will
    discard it.

    I maintain a list of available SlaveBuilders, one for each connected
    slave that the C{slavenames} parameter says we can use. Some of these
    will be idle, some of them will be busy running builds for me. If there
    are multiple slaves, I can run multiple builds at once.

    I also manage forced builds, progress expectation (ETA) management, and
    some status delivery chores.

    I am persisted in C{BASEDIR/BUILDERNAME/builder}, so I can remember how
    long a build usually takes to run (in my C{expectations} attribute). This
    pickle also includes the L{buildbot.status.builder.BuilderStatus} object,
    which remembers the set of historic builds.

    @type buildable: list of L{buildbot.process.base.BuildRequest}
    @ivar buildable: BuildRequests that are ready to build, but which are
                     waiting for a buildslave to be available.

    @type building: list of L{buildbot.process.base.Build}
    @ivar building: Builds that are actively running

    """

    expectations = None # this is created the first time we get a good build
    START_BUILD_TIMEOUT = 10

    def __init__(self, setup, builder_status):
        """
        @type  setup: dict
        @param setup: builder setup data, as stored in
                      BuildmasterConfig['builders'].  Contains name,
                      slavename(s), builddir, factory, locks.
        @type  builder_status: L{buildbot.status.builder.BuilderStatus}
        """
        self.name = setup['name']
        self.slavenames = []
        if setup.has_key('slavename'):
            self.slavenames.append(setup['slavename'])
        if setup.has_key('slavenames'):
            self.slavenames.extend(setup['slavenames'])
        self.builddir = setup['builddir']
        self.buildFactory = setup['factory']
        self.locks = setup.get("locks", [])
        if setup.has_key('periodicBuildTime'):
            raise ValueError("periodicBuildTime can no longer be defined as"
                             " part of the Builder: use scheduler.Periodic"
                             " instead")

        # build/wannabuild slots: Build objects move along this sequence
        self.buildable = []
        self.building = []

        # buildslaves which have connected but which are not yet available.
        # These are always in the ATTACHING state.
        self.attaching_slaves = []

        # buildslaves at our disposal. Each SlaveBuilder instance has a
        # .state that is IDLE, PINGING, or BUILDING. "PINGING" is used when a
        # Build is about to start, to make sure that they're still alive.
        self.slaves = []

        self.builder_status = builder_status
        self.builder_status.setSlavenames(self.slavenames)

        # for testing, to help synchronize tests
        self.watchers = {'attach': [], 'detach': [], 'detach_all': [],
                         'idle': []}

    def setBotmaster(self, botmaster):
        self.botmaster = botmaster

    def compareToSetup(self, setup):
        diffs = []
        setup_slavenames = []
        if setup.has_key('slavename'):
            setup_slavenames.append(setup['slavename'])
        setup_slavenames.extend(setup.get('slavenames', []))
        if setup_slavenames != self.slavenames:
            diffs.append('slavenames changed from %s to %s' \
                         % (self.slavenames, setup_slavenames))
        if setup['builddir'] != self.builddir:
            diffs.append('builddir changed from %s to %s' \
                         % (self.builddir, setup['builddir']))
        if setup['factory'] != self.buildFactory: # compare objects
            diffs.append('factory changed')
        oldlocks = [(lock.__class__, lock.name)
                    for lock in setup.get('locks',[])]
        newlocks = [(lock.__class__, lock.name)
                    for lock in self.locks]
        if oldlocks != newlocks:
            diffs.append('locks changed from %s to %s' % (oldlocks, newlocks))
        return diffs

    def __repr__(self):
        return "<Builder '%s' at %d>" % (self.name, id(self))


    def submitBuildRequest(self, req):
        req.submittedAt = now()
        self.buildable.append(req)
        req.requestSubmitted(self)
        self.builder_status.addBuildRequest(req.status)
        self.maybeStartBuild()

    def cancelBuildRequest(self, req):
        if req in self.buildable:
            self.buildable.remove(req)
            self.builder_status.removeBuildRequest(req.status)
            return True
        return False

    def __getstate__(self):
        d = self.__dict__.copy()
        # TODO: note that d['buildable'] can contain Deferreds
        del d['building'] # TODO: move these back to .buildable?
        del d['slaves']
        return d

    def __setstate__(self, d):
        self.__dict__ = d
        self.building = []
        self.slaves = []

    def consumeTheSoulOfYourPredecessor(self, old):
        """Suck the brain out of an old Builder.

        This takes all the runtime state from an existing Builder and moves
        it into ourselves. This is used when a Builder is changed in the
        master.cfg file: the new Builder has a different factory, but we want
        all the builds that were queued for the old one to get processed by
        the new one. Any builds which are already running will keep running.
        The new Builder will get as many of the old SlaveBuilder objects as
        it wants."""

        log.msg("consumeTheSoulOfYourPredecessor: %s feeding upon %s" %
                (self, old))
        # we claim all the pending builds, removing them from the old
        # Builder's queue. This insures that the old Builder will not start
        # any new work.
        log.msg(" stealing %s buildrequests" % len(old.buildable))
        self.buildable.extend(old.buildable)
        old.buildable = []

        # old.building is not migrated: it keeps track of builds which were
        # in progress in the old Builder. When those builds finish, the old
        # Builder will be notified, not us. However, since the old
        # SlaveBuilder will point to us, it is our maybeStartBuild() that
        # will be triggered.
        if old.building:
            self.builder_status.setBigState("building")

        # Our set of slavenames may be different. Steal any of the old
        # buildslaves that we want to keep using.
        for sb in old.slaves[:]:
            if sb.slave.slavename in self.slavenames:
                log.msg(" stealing buildslave %s" % sb)
                self.slaves.append(sb)
                old.slaves.remove(sb)
                sb.setBuilder(self)

        # old.attaching_slaves:
        #  these SlaveBuilders are waiting on a sequence of calls:
        #  remote.setMaster and remote.print . When these two complete,
        #  old._attached will be fired, which will add a 'connect' event to
        #  the builder_status and try to start a build. However, we've pulled
        #  everything out of the old builder's queue, so it will have no work
        #  to do. The outstanding remote.setMaster/print call will be holding
        #  the last reference to the old builder, so it will disappear just
        #  after that response comes back.
        #
        #  The BotMaster will ask the slave to re-set their list of Builders
        #  shortly after this function returns, which will cause our
        #  attached() method to be fired with a bunch of references to remote
        #  SlaveBuilders, some of which we already have (by stealing them
        #  from the old Builder), some of which will be new. The new ones
        #  will be re-attached.

        #  Therefore, we don't need to do anything about old.attaching_slaves

        return # all done

    def fireTestEvent(self, name, with=None):
        if with is None:
            with = self
        watchers = self.watchers[name]
        self.watchers[name] = []
        for w in watchers:
            reactor.callLater(0, w.callback, with)

    def attached(self, slave, remote, commands):
        """This is invoked by the BotPerspective when the self.slavename bot
        registers their builder.

        @type  slave: L{buildbot.master.BotPerspective}
        @param slave: the BotPerspective that represents the buildslave as a
                      whole
        @type  remote: L{twisted.spread.pb.RemoteReference}
        @param remote: a reference to the L{buildbot.slave.bot.SlaveBuilder}
        @type  commands: dict: string -> string, or None
        @param commands: provides the slave's version of each RemoteCommand

        @rtype:  L{twisted.internet.defer.Deferred}
        @return: a Deferred that fires (with 'self') when the slave-side
                 builder is fully attached and ready to accept commands.
        """
        for s in self.attaching_slaves + self.slaves:
            if s.slave == slave:
                # already attached to them. This is fairly common, since
                # attached() gets called each time we receive the builder
                # list from the slave, and we ask for it each time we add or
                # remove a builder. So if the slave is hosting builders
                # A,B,C, and the config file changes A, we'll remove A and
                # re-add it, triggering two builder-list requests, getting
                # two redundant calls to attached() for B, and another two
                # for C.
                #
                # Therefore, when we see that we're already attached, we can
                # just ignore it. TODO: build a diagram of the state
                # transitions here, I'm concerned about sb.attached() failing
                # and leaving sb.state stuck at 'ATTACHING', and about
                # the detached() message arriving while there's some
                # transition pending such that the response to the transition
                # re-vivifies sb
                return defer.succeed(self)

        sb = SlaveBuilder()
        sb.setBuilder(self)
        self.attaching_slaves.append(sb)
        d = sb.attached(slave, remote, commands)
        d.addCallback(self._attached)
        d.addErrback(self._not_attached, slave)
        return d

    def _attached(self, sb):
        # TODO: make this .addSlaveEvent(slave.slavename, ['connect']) ?
        self.builder_status.addPointEvent(['connect', sb.slave.slavename])
        self.attaching_slaves.remove(sb)
        self.slaves.append(sb)
        reactor.callLater(0, self.maybeStartBuild)

        self.fireTestEvent('attach')
        return self

    def _not_attached(self, why, slave):
        # already log.err'ed by SlaveBuilder._attachFailure
        # TODO: make this .addSlaveEvent?
        # TODO: remove from self.slaves (except that detached() should get
        #       run first, right?)
        self.builder_status.addPointEvent(['failed', 'connect',
                                           slave.slave.slavename])
        # TODO: add an HTMLLogFile of the exception
        self.fireTestEvent('attach', why)

    def detached(self, slave):
        """This is called when the connection to the bot is lost."""
        log.msg("%s.detached" % self, slave.slavename)
        for sb in self.attaching_slaves + self.slaves:
            if sb.slave == slave:
                break
        else:
            log.msg("WEIRD: Builder.detached(%s) (%s)"
                    " not in attaching_slaves(%s)"
                    " or slaves(%s)" % (slave, slave.slavename,
                                        self.attaching_slaves,
                                        self.slaves))
            return
        if sb.state == BUILDING:
            # the Build's .lostRemote method (invoked by a notifyOnDisconnect
            # handler) will cause the Build to be stopped, probably right
            # after the notifyOnDisconnect that invoked us finishes running.

            # TODO: should failover to a new Build
            #self.retryBuild(sb.build)
            pass

        if sb in self.attaching_slaves:
            self.attaching_slaves.remove(sb)
        if sb in self.slaves:
            self.slaves.remove(sb)

        # TODO: make this .addSlaveEvent?
        self.builder_status.addPointEvent(['disconnect', slave.slavename])
        sb.detached() # inform the SlaveBuilder that their slave went away
        self.updateBigStatus()
        self.fireTestEvent('detach')
        if not self.slaves:
            self.fireTestEvent('detach_all')

    def updateBigStatus(self):
        if not self.slaves:
            self.builder_status.setBigState("offline")
        elif self.building:
            self.builder_status.setBigState("building")
        else:
            self.builder_status.setBigState("idle")
            self.fireTestEvent('idle')

    def maybeStartBuild(self):
        log.msg("maybeStartBuild %s: %s %s" %
                (self, self.buildable, self.slaves))
        if not self.buildable:
            self.updateBigStatus()
            return # nothing to do
        # find the first idle slave
        for sb in self.slaves:
            if sb.isAvailable():
                break
        else:
            log.msg("%s: want to start build, but we don't have a remote"
                    % self)
            self.updateBigStatus()
            return

        # there is something to build, and there is a slave on which to build
        # it. Grab the oldest request, see if we can merge it with anything
        # else.
        req = self.buildable.pop(0)
        self.builder_status.removeBuildRequest(req.status)
        mergers = []
        for br in self.buildable[:]:
            if req.canBeMergedWith(br):
                self.buildable.remove(br)
                self.builder_status.removeBuildRequest(br.status)
                mergers.append(br)
        requests = [req] + mergers

        # Create a new build from our build factory and set ourself as the
        # builder.
        build = self.buildFactory.newBuild(requests)
        build.setBuilder(self)
        build.setLocks(self.locks)

        # start it
        self.startBuild(build, sb)

    def startBuild(self, build, sb):
        """Start a build on the given slave.
        @param build: the L{base.Build} to start
        @param sb: the L{SlaveBuilder} which will host this build

        @return: a Deferred which fires with a
        L{buildbot.interfaces.IBuildControl} that can be used to stop the
        Build, or to access a L{buildbot.interfaces.IBuildStatus} which will
        watch the Build as it runs. """

        self.building.append(build)
        self.updateBigStatus()

        log.msg("starting build %s.. pinging the slave" % build)
        # ping the slave to make sure they're still there. If they're fallen
        # off the map (due to a NAT timeout or something), this will fail in
        # a couple of minutes, depending upon the TCP timeout. TODO: consider
        # making this time out faster, or at least characterize the likely
        # duration.
        d = sb.ping(self.START_BUILD_TIMEOUT)
        d.addCallback(self._startBuild_1, build, sb)
        return d

    def _startBuild_1(self, res, build, sb):
        if not res:
            return self._startBuildFailed("slave ping failed", build, sb)
        # The buildslave is ready to go. sb.buildStarted() sets its state to
        # BUILDING (so we won't try to use it for any other builds). This
        # gets set back to IDLE by the Build itself when it finishes.
        sb.buildStarted()
        d = sb.remote.callRemote("startBuild")
        d.addCallbacks(self._startBuild_2, self._startBuildFailed,
                       callbackArgs=(build,sb), errbackArgs=(build,sb))
        return d

    def _startBuild_2(self, res, build, sb):
        # create the BuildStatus object that goes with the Build
        bs = self.builder_status.newBuild()

        # start the build. This will first set up the steps, then tell the
        # BuildStatus that it has started, which will announce it to the
        # world (through our BuilderStatus object, which is its parent).
        # Finally it will start the actual build process.
        d = build.startBuild(bs, self.expectations, sb)
        d.addCallback(self.buildFinished, sb)
        d.addErrback(log.err) # this shouldn't happen. if it does, the slave
                              # will be wedged
        for req in build.requests:
            req.buildStarted(build, bs)
        return build # this is the IBuildControl

    def _startBuildFailed(self, why, build, sb):
        # put the build back on the buildable list
        log.msg("I tried to tell the slave that the build %s started, but "
                "remote_startBuild failed: %s" % (build, why))
        # release the slave. This will queue a call to maybeStartBuild, which
        # will fire after other notifyOnDisconnect handlers have marked the
        # slave as disconnected (so we don't try to use it again).
        sb.buildFinished()

        log.msg("re-queueing the BuildRequest")
        self.building.remove(build)
        for req in build.requests:
            self.buildable.insert(0, req) # the interrupted build gets first
                                          # priority
            self.builder_status.addBuildRequest(req.status)


    def buildFinished(self, build, sb):
        """This is called when the Build has finished (either success or
        failure). Any exceptions during the build are reported with
        results=FAILURE, not with an errback."""

        # by the time we get here, the Build has already released the slave
        # (which queues a call to maybeStartBuild)

        self.building.remove(build)
        for req in build.requests:
            req.finished(build.build_status)

    def setExpectations(self, progress):
        """Mark the build as successful and update expectations for the next
        build. Only call this when the build did not fail in any way that
        would invalidate the time expectations generated by it. (if the
        compile failed and thus terminated early, we can't use the last
        build to predict how long the next one will take).
        """
        if self.expectations:
            self.expectations.update(progress)
        else:
            # the first time we get a good build, create our Expectations
            # based upon its results
            self.expectations = Expectations(progress)
        log.msg("new expectations: %s seconds" % \
                self.expectations.expectedBuildTime())

    def shutdownSlave(self):
        if self.remote:
            self.remote.callRemote("shutdown")


class BuilderControl(components.Adapter):
    if implements:
        implements(interfaces.IBuilderControl)
    else:
        __implements__ = interfaces.IBuilderControl,

    def requestBuild(self, req):
        """Submit a BuildRequest to this Builder."""
        self.original.submitBuildRequest(req)

    def requestBuildSoon(self, req):
        """Submit a BuildRequest like requestBuild, but raise a
        L{buildbot.interfaces.NoSlaveError} if no slaves are currently
        available, so it cannot be used to queue a BuildRequest in the hopes
        that a slave will eventually connect. This method is appropriate for
        use by things like the web-page 'Force Build' button."""
        if not self.original.slaves:
            raise interfaces.NoSlaveError
        self.requestBuild(req)

    def resubmitBuild(self, bs, reason="<rebuild, no reason given>"):
        if not bs.isFinished():
            return
        branch, revision, patch = bs.getSourceStamp()
        changes = bs.getChanges()
        ss = sourcestamp.SourceStamp(branch, revision, patch, changes)
        req = base.BuildRequest(reason, ss, self.original.name)
        self.requestBuild(req)

    def getPendingBuilds(self):
        # return IBuildRequestControl objects
        raise NotImplementedError

    def getBuild(self, number):
        for b in self.original.building:
            if b.build_status.number == number:
                return b
        return None

    def ping(self, timeout=30):
        if not self.original.slaves:
            self.original.builder_status.addPointEvent(["ping", "no slave"],
                                                       "red")
            return defer.succeed(False) # interfaces.NoSlaveError
        dl = []
        for s in self.original.slaves:
            dl.append(s.ping(timeout, self.original.builder_status))
        d = defer.DeferredList(dl)
        d.addCallback(self._gatherPingResults)
        return d

    def _gatherPingResults(self, res):
        for ignored,success in res:
            if not success:
                return False
        return True

components.registerAdapter(BuilderControl, Builder, interfaces.IBuilderControl)
