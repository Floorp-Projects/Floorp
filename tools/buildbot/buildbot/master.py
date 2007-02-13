# -*- test-case-name: buildbot.test.test_run -*-

import string, os
signal = None
try:
    import signal
except ImportError:
    pass
try:
    import cPickle
    pickle = cPickle
except ImportError:
    import pickle

from twisted.python import log, components
from twisted.internet import defer, reactor
from twisted.spread import pb
from twisted.cred import portal, checkers
from twisted.application import service, strports
from twisted.persisted import styles

# sibling imports
from buildbot.twcompat import implements
from buildbot.util import now
from buildbot.pbutil import NewCredPerspective
from buildbot.process.builder import Builder, IDLE
from buildbot.process.base import BuildRequest
from buildbot.status.builder import SlaveStatus, Status
from buildbot.changes.changes import Change, ChangeMaster
from buildbot.sourcestamp import SourceStamp
from buildbot import interfaces

########################################




class BotPerspective(NewCredPerspective):
    """This is the master-side representative for a remote buildbot slave.
    There is exactly one for each slave described in the config file (the
    c['bots'] list). When buildbots connect in (.attach), they get a
    reference to this instance. The BotMaster object is stashed as the
    .service attribute."""

    def __init__(self, name, botmaster):
        self.slavename = name
        self.botmaster = botmaster
        self.slave_status = SlaveStatus(name)
        self.slave = None # a RemoteReference to the Bot, when connected
        self.slave_commands = None

    def updateSlave(self):
        """Called to add or remove builders after the slave has connected.

        @return: a Deferred that indicates when an attached slave has
        accepted the new builders and/or released the old ones."""
        if self.slave:
            return self.sendBuilderList()
        return defer.succeed(None)

    def __repr__(self):
        return "<BotPerspective '%s', builders: %s>" % \
               (self.slavename,
                string.join(map(lambda b: b.name, self.builders), ','))

    def attached(self, bot):
        """This is called when the slave connects.

        @return: a Deferred that fires with a suitable pb.IPerspective to
                 give to the slave (i.e. 'self')"""

        if self.slave:
            # uh-oh, we've got a duplicate slave. The most likely
            # explanation is that the slave is behind a slow link, thinks we
            # went away, and has attempted to reconnect, so we've got two
            # "connections" from the same slave, but the previous one is
            # stale. Give the new one precedence.
            log.msg("duplicate slave %s replacing old one" % self.slavename)

            # just in case we've got two identically-configured slaves,
            # report the IP addresses of both so someone can resolve the
            # squabble
            tport = self.slave.broker.transport
            log.msg("old slave was connected from", tport.getPeer())
            log.msg("new slave is from", bot.broker.transport.getPeer())
            d = self.disconnect()
        else:
            d = defer.succeed(None)
        # now we go through a sequence of calls, gathering information, then
        # tell the Botmaster that it can finally give this slave to all the
        # Builders that care about it.

        # we accumulate slave information in this 'state' dictionary, then
        # set it atomically if we make it far enough through the process
        state = {}

        def _log_attachment_on_slave(res):
            d1 = bot.callRemote("print", "attached")
            d1.addErrback(lambda why: None)
            return d1
        d.addCallback(_log_attachment_on_slave)

        def _get_info(res):
            d1 = bot.callRemote("getSlaveInfo")
            def _got_info(info):
                log.msg("Got slaveinfo from '%s'" % self.slavename)
                # TODO: info{} might have other keys
                state["admin"] = info.get("admin")
                state["host"] = info.get("host")
            def _info_unavailable(why):
                # maybe an old slave, doesn't implement remote_getSlaveInfo
                log.msg("BotPerspective.info_unavailable")
                log.err(why)
            d1.addCallbacks(_got_info, _info_unavailable)
            return d1
        d.addCallback(_get_info)

        def _get_commands(res):
            d1 = bot.callRemote("getCommands")
            def _got_commands(commands):
                state["slave_commands"] = commands
            def _commands_unavailable(why):
                # probably an old slave
                log.msg("BotPerspective._commands_unavailable")
                if why.check(AttributeError):
                    return
                log.err(why)
            d1.addCallbacks(_got_commands, _commands_unavailable)
            return d1
        d.addCallback(_get_commands)

        def _accept_slave(res):
            self.slave_status.setAdmin(state.get("admin"))
            self.slave_status.setHost(state.get("host"))
            self.slave_status.setConnected(True)
            self.slave_commands = state.get("slave_commands")
            self.slave = bot
            log.msg("bot attached")
            return self.updateSlave()
        d.addCallback(_accept_slave)

        # Finally, the slave gets a reference to this BotPerspective. They
        # receive this later, after we've started using them.
        d.addCallback(lambda res: self)
        return d

    def detached(self, mind):
        self.slave = None
        self.slave_status.setConnected(False)
        self.botmaster.slaveLost(self)
        log.msg("BotPerspective.detached(%s)" % self.slavename)


    def disconnect(self):
        """Forcibly disconnect the slave.

        This severs the TCP connection and returns a Deferred that will fire
        (with None) when the connection is probably gone.

        If the slave is still alive, they will probably try to reconnect
        again in a moment.

        This is called in two circumstances. The first is when a slave is
        removed from the config file. In this case, when they try to
        reconnect, they will be rejected as an unknown slave. The second is
        when we wind up with two connections for the same slave, in which
        case we disconnect the older connection.
        """

        if not self.slave:
            return defer.succeed(None)
        log.msg("disconnecting old slave %s now" % self.slavename)

        # all kinds of teardown will happen as a result of
        # loseConnection(), but it happens after a reactor iteration or
        # two. Hook the actual disconnect so we can know when it is safe
        # to connect the new slave. We have to wait one additional
        # iteration (with callLater(0)) to make sure the *other*
        # notifyOnDisconnect handlers have had a chance to run.
        d = defer.Deferred()

        # notifyOnDisconnect runs the callback with one argument, the
        # RemoteReference being disconnected.
        def _disconnected(rref):
            reactor.callLater(0, d.callback, None)
        self.slave.notifyOnDisconnect(_disconnected)
        tport = self.slave.broker.transport
        # this is the polite way to request that a socket be closed
        tport.loseConnection()
        try:
            # but really we don't want to wait for the transmit queue to
            # drain. The remote end is unlikely to ACK the data, so we'd
            # probably have to wait for a (20-minute) TCP timeout.
            #tport._closeSocket()
            # however, doing _closeSocket (whether before or after
            # loseConnection) somehow prevents the notifyOnDisconnect
            # handlers from being run. Bummer.
            tport.offset = 0
            tport.dataBuffer = ""
            pass
        except:
            # however, these hacks are pretty internal, so don't blow up if
            # they fail or are unavailable
            log.msg("failed to accelerate the shutdown process")
            pass
        log.msg("waiting for slave to finish disconnecting")

        # When this Deferred fires, we'll be ready to accept the new slave
        return d

    def sendBuilderList(self):
        our_builders = self.botmaster.getBuildersForSlave(self.slavename)
        blist = [(b.name, b.builddir) for b in our_builders]
        d = self.slave.callRemote("setBuilderList", blist)
        def _sent(slist):
            dl = []
            for name, remote in slist.items():
                # use get() since we might have changed our mind since then
                b = self.botmaster.builders.get(name)
                if b:
                    d1 = b.attached(self, remote, self.slave_commands)
                    dl.append(d1)
            return defer.DeferredList(dl)
        def _set_failed(why):
            log.msg("BotPerspective.sendBuilderList (%s) failed" % self)
            log.err(why)
            # TODO: hang up on them?, without setBuilderList we can't use
            # them
        d.addCallbacks(_sent, _set_failed)
        return d

    def perspective_keepalive(self):
        pass

    
class BotMaster(service.Service):

    """This is the master-side service which manages remote buildbot slaves.
    It provides them with BotPerspectives, and distributes file change
    notification messages to them.
    """

    debug = 0

    def __init__(self):
        self.builders = {}
        self.builderNames = []
        # builders maps Builder names to instances of bb.p.builder.Builder,
        # which is the master-side object that defines and controls a build.
        # They are added by calling botmaster.addBuilder() from the startup
        # code.

        # self.slaves contains a ready BotPerspective instance for each
        # potential buildslave, i.e. all the ones listed in the config file.
        # If the slave is connected, self.slaves[slavename].slave will
        # contain a RemoteReference to their Bot instance. If it is not
        # connected, that attribute will hold None.
        self.slaves = {} # maps slavename to BotPerspective
        self.statusClientService = None
        self.watchers = {}

        # self.locks holds the real Lock instances
        self.locks = {}

    # these four are convenience functions for testing

    def waitUntilBuilderAttached(self, name):
        b = self.builders[name]
        #if b.slaves:
        #    return defer.succeed(None)
        d = defer.Deferred()
        b.watchers['attach'].append(d)
        return d

    def waitUntilBuilderDetached(self, name):
        b = self.builders.get(name)
        if not b or not b.slaves:
            return defer.succeed(None)
        d = defer.Deferred()
        b.watchers['detach'].append(d)
        return d

    def waitUntilBuilderFullyDetached(self, name):
        b = self.builders.get(name)
        # TODO: this looks too deeply inside the Builder object
        if not b or not b.slaves:
            return defer.succeed(None)
        d = defer.Deferred()
        b.watchers['detach_all'].append(d)
        return d

    def waitUntilBuilderIdle(self, name):
        b = self.builders[name]
        # TODO: this looks way too deeply inside the Builder object
        for sb in b.slaves:
            if sb.state != IDLE:
                d = defer.Deferred()
                b.watchers['idle'].append(d)
                return d
        return defer.succeed(None)


    def addSlave(self, slavename):
        slave = BotPerspective(slavename, self)
        self.slaves[slavename] = slave

    def removeSlave(self, slavename):
        d = self.slaves[slavename].disconnect()
        del self.slaves[slavename]
        return d

    def slaveLost(self, bot):
        for name, b in self.builders.items():
            if bot.slavename in b.slavenames:
                b.detached(bot)

    def getBuildersForSlave(self, slavename):
        return [b
                for b in self.builders.values()
                if slavename in b.slavenames]

    def getBuildernames(self):
        return self.builderNames

    def getBuilders(self):
        allBuilders = [self.builders[name] for name in self.builderNames]
        return allBuilders

    def setBuilders(self, builders):
        self.builders = {}
        self.builderNames = []
        for b in builders:
            for slavename in b.slavenames:
                # this is actually validated earlier
                assert slavename in self.slaves
            self.builders[b.name] = b
            self.builderNames.append(b.name)
            b.setBotmaster(self)
        d = self._updateAllSlaves()
        return d

    def _updateAllSlaves(self):
        """Notify all buildslaves about changes in their Builders."""
        dl = [s.updateSlave() for s in self.slaves.values()]
        return defer.DeferredList(dl)

    def maybeStartAllBuilds(self):
        for b in self.builders.values():
            b.maybeStartBuild()

    def getPerspective(self, slavename):
        return self.slaves[slavename]

    def shutdownSlaves(self):
        # TODO: make this into a bot method rather than a builder method
        for b in self.slaves.values():
            b.shutdownSlave()

    def stopService(self):
        for b in self.builders.values():
            b.builder_status.addPointEvent(["master", "shutdown"])
            b.builder_status.saveYourself()
        return service.Service.stopService(self)

    def getLockByID(self, lockid):
        """Convert a Lock identifier into an actual Lock instance.
        @param lockid: a locks.MasterLock or locks.SlaveLock instance
        @return: a locks.RealMasterLock or locks.RealSlaveLock instance
        """
        if not lockid in self.locks:
            self.locks[lockid] = lockid.lockClass(lockid)
        # if the master.cfg file has changed maxCount= on the lock, the next
        # time a build is started, they'll get a new RealLock instance. Note
        # that this requires that MasterLock and SlaveLock (marker) instances
        # be hashable and that they should compare properly.
        return self.locks[lockid]

########################################



class DebugPerspective(NewCredPerspective):
    def attached(self, mind):
        return self
    def detached(self, mind):
        pass

    def perspective_requestBuild(self, buildername, reason, branch, revision):
        c = interfaces.IControl(self.master)
        bc = c.getBuilder(buildername)
        ss = SourceStamp(branch, revision)
        br = BuildRequest(reason, ss, buildername)
        bc.requestBuild(br)

    def perspective_pingBuilder(self, buildername):
        c = interfaces.IControl(self.master)
        bc = c.getBuilder(buildername)
        bc.ping()

    def perspective_fakeChange(self, file, revision=None, who="fakeUser",
                               branch=None):
        change = Change(who, [file], "some fake comments\n",
                        branch=branch, revision=revision)
        c = interfaces.IControl(self.master)
        c.addChange(change)

    def perspective_setCurrentState(self, buildername, state):
        builder = self.botmaster.builders.get(buildername)
        if not builder: return
        if state == "offline":
            builder.statusbag.currentlyOffline()
        if state == "idle":
            builder.statusbag.currentlyIdle()
        if state == "waiting":
            builder.statusbag.currentlyWaiting(now()+10)
        if state == "building":
            builder.statusbag.currentlyBuilding(None)
    def perspective_reload(self):
        print "doing reload of the config file"
        self.master.loadTheConfigFile()
    def perspective_pokeIRC(self):
        print "saying something on IRC"
        from buildbot.status import words
        for s in self.master:
            if isinstance(s, words.IRC):
                bot = s.f
                for channel in bot.channels:
                    print " channel", channel
                    bot.p.msg(channel, "Ow, quit it")

    def perspective_print(self, msg):
        print "debug", msg

class Dispatcher(styles.Versioned):
    if implements:
        implements(portal.IRealm)
    else:
        __implements__ = portal.IRealm,
    persistenceVersion = 2

    def __init__(self):
        self.names = {}

    def upgradeToVersion1(self):
        self.master = self.botmaster.parent
    def upgradeToVersion2(self):
        self.names = {}

    def register(self, name, afactory):
        self.names[name] = afactory
    def unregister(self, name):
        del self.names[name]

    def requestAvatar(self, avatarID, mind, interface):
        assert interface == pb.IPerspective
        afactory = self.names.get(avatarID)
        if afactory:
            p = afactory.getPerspective()
        elif avatarID == "debug":
            p = DebugPerspective()
            p.master = self.master
            p.botmaster = self.botmaster
        elif avatarID == "statusClient":
            p = self.statusClientService.getPerspective()
        else:
            # it must be one of the buildslaves: no other names will make it
            # past the checker
            p = self.botmaster.getPerspective(avatarID)

        if not p:
            raise ValueError("no perspective for '%s'" % avatarID)

        d = defer.maybeDeferred(p.attached, mind)
        d.addCallback(self._avatarAttached, mind)
        return d

    def _avatarAttached(self, p, mind):
        return (pb.IPerspective, p, lambda p=p,mind=mind: p.detached(mind))

########################################

# service hierarchy:
#  BuildMaster
#   BotMaster
#   ChangeMaster
#    all IChangeSource objects
#   StatusClientService
#   TCPClient(self.ircFactory)
#   TCPServer(self.slaveFactory) -> dispatcher.requestAvatar
#   TCPServer(self.site)
#   UNIXServer(ResourcePublisher(self.site))


class BuildMaster(service.MultiService, styles.Versioned):
    debug = 0
    persistenceVersion = 3
    manhole = None
    debugPassword = None
    projectName = "(unspecified)"
    projectURL = None
    buildbotURL = None
    change_svc = None

    def __init__(self, basedir, configFileName="master.cfg"):
        service.MultiService.__init__(self)
        self.setName("buildmaster")
        self.basedir = basedir
        self.configFileName = configFileName

        # the dispatcher is the realm in which all inbound connections are
        # looked up: slave builders, change notifications, status clients, and
        # the debug port
        dispatcher = Dispatcher()
        dispatcher.master = self
        self.dispatcher = dispatcher
        self.checker = checkers.InMemoryUsernamePasswordDatabaseDontUse()
        # the checker starts with no user/passwd pairs: they are added later
        p = portal.Portal(dispatcher)
        p.registerChecker(self.checker)
        self.slaveFactory = pb.PBServerFactory(p)
        self.slaveFactory.unsafeTracebacks = True # let them see exceptions

        self.slavePortnum = None
        self.slavePort = None

        self.botmaster = BotMaster()
        self.botmaster.setName("botmaster")
        self.botmaster.setServiceParent(self)
        dispatcher.botmaster = self.botmaster

        self.status = Status(self.botmaster, self.basedir)

        self.statusTargets = []

        self.bots = []
        # this ChangeMaster is a dummy, only used by tests. In the real
        # buildmaster, where the BuildMaster instance is activated
        # (startService is called) by twistd, this attribute is overwritten.
        self.useChanges(ChangeMaster())

        self.readConfig = False

    def upgradeToVersion1(self):
        self.dispatcher = self.slaveFactory.root.portal.realm

    def upgradeToVersion2(self): # post-0.4.3
        self.webServer = self.webTCPPort
        del self.webTCPPort
        self.webDistribServer = self.webUNIXPort
        del self.webUNIXPort
        self.configFileName = "master.cfg"

    def upgradeToVersion3(self):
        # post 0.6.3, solely to deal with the 0.6.3 breakage. Starting with
        # 0.6.5 I intend to do away with .tap files altogether
        self.services = []
        self.namedServices = {}
        del self.change_svc

    def startService(self):
        service.MultiService.startService(self)
        self.loadChanges() # must be done before loading the config file
        if not self.readConfig:
            # TODO: consider catching exceptions during this call to
            # loadTheConfigFile and bailing (reactor.stop) if it fails,
            # since without a config file we can't do anything except reload
            # the config file, and it would be nice for the user to discover
            # this quickly.
            self.loadTheConfigFile()
        if signal and hasattr(signal, "SIGHUP"):
            signal.signal(signal.SIGHUP, self._handleSIGHUP)
        for b in self.botmaster.builders.values():
            b.builder_status.addPointEvent(["master", "started"])
            b.builder_status.saveYourself()

    def useChanges(self, changes):
        if self.change_svc:
            # TODO: can return a Deferred
            self.change_svc.disownServiceParent()
        self.change_svc = changes
        self.change_svc.basedir = self.basedir
        self.change_svc.setName("changemaster")
        self.dispatcher.changemaster = self.change_svc
        self.change_svc.setServiceParent(self)

    def loadChanges(self):
        filename = os.path.join(self.basedir, "changes.pck")
        try:
            changes = pickle.load(open(filename, "rb"))
            styles.doUpgrade()
        except IOError:
            log.msg("changes.pck missing, using new one")
            changes = ChangeMaster()
        except EOFError:
            log.msg("corrupted changes.pck, using new one")
            changes = ChangeMaster()
        self.useChanges(changes)

    def _handleSIGHUP(self, *args):
        reactor.callLater(0, self.loadTheConfigFile)

    def getStatus(self):
        """
        @rtype: L{buildbot.status.builder.Status}
        """
        return self.status

    def loadTheConfigFile(self, configFile=None):
        if not configFile:
            configFile = os.path.join(self.basedir, self.configFileName)

        log.msg("loading configuration from %s" % configFile)
        configFile = os.path.expanduser(configFile)

        try:
            f = open(configFile, "r")
        except IOError, e:
            log.msg("unable to open config file '%s'" % configFile)
            log.msg("leaving old configuration in place")
            log.err(e)
            return

        try:
            self.loadConfig(f)
        except:
            log.msg("error during loadConfig")
            log.err()
            log.msg("The new config file is unusable, so I'll ignore it.")
            log.msg("I will keep using the previous config file instead.")
        f.close()

    def loadConfig(self, f):
        """Internal function to load a specific configuration file. Any
        errors in the file will be signalled by raising an exception.

        @return: a Deferred that will fire (with None) when the configuration
        changes have been completed. This may involve a round-trip to each
        buildslave that was involved."""

        localDict = {'basedir': os.path.expanduser(self.basedir)}
        try:
            exec f in localDict
        except:
            log.msg("error while parsing config file")
            raise

        try:
            config = localDict['BuildmasterConfig']
        except KeyError:
            log.err("missing config dictionary")
            log.err("config file must define BuildmasterConfig")
            raise

        known_keys = "bots sources schedulers builders slavePortnum " + \
                     "debugPassword manhole " + \
                     "status projectName projectURL buildbotURL"
        known_keys = known_keys.split()
        for k in config.keys():
            if k not in known_keys:
                log.msg("unknown key '%s' defined in config dictionary" % k)

        try:
            # required
            bots = config['bots']
            sources = config['sources']
            schedulers = config['schedulers']
            builders = config['builders']
            slavePortnum = config['slavePortnum']

            # optional
            debugPassword = config.get('debugPassword')
            manhole = config.get('manhole')
            status = config.get('status', [])
            projectName = config.get('projectName')
            projectURL = config.get('projectURL')
            buildbotURL = config.get('buildbotURL')

        except KeyError, e:
            log.msg("config dictionary is missing a required parameter")
            log.msg("leaving old configuration in place")
            raise

        # do some validation first
        for name, passwd in bots:
            if name in ("debug", "change", "status"):
                raise KeyError, "reserved name '%s' used for a bot" % name
        if config.has_key('interlocks'):
            raise KeyError("c['interlocks'] is no longer accepted")

        assert isinstance(sources, (list, tuple))
        for s in sources:
            assert interfaces.IChangeSource(s, None)
        # this assertion catches c['schedulers'] = Scheduler(), since
        # Schedulers are service.MultiServices and thus iterable.
        errmsg = "c['schedulers'] must be a list of Scheduler instances"
        assert isinstance(schedulers, (list, tuple)), errmsg
        for s in schedulers:
            assert interfaces.IScheduler(s, None), errmsg
        assert isinstance(status, (list, tuple))
        for s in status:
            assert interfaces.IStatusReceiver(s, None)

        slavenames = [name for name,pw in bots]
        buildernames = []
        dirnames = []
        for b in builders:
            if type(b) is tuple:
                raise ValueError("builder %s must be defined with a dict, "
                                 "not a tuple" % b[0])
            if b.has_key('slavename') and b['slavename'] not in slavenames:
                raise ValueError("builder %s uses undefined slave %s" \
                                 % (b['name'], b['slavename']))
            for n in b.get('slavenames', []):
                if n not in slavenames:
                    raise ValueError("builder %s uses undefined slave %s" \
                                     % (b['name'], n))
            if b['name'] in buildernames:
                raise ValueError("duplicate builder name %s"
                                 % b['name'])
            buildernames.append(b['name'])
            if b['builddir'] in dirnames:
                raise ValueError("builder %s reuses builddir %s"
                                 % (b['name'], b['builddir']))
            dirnames.append(b['builddir'])

        schedulernames = []
        for s in schedulers:
            for b in s.listBuilderNames():
                assert b in buildernames, \
                       "%s uses unknown builder %s" % (s, b)
            if s.name in schedulernames:
                # TODO: schedulers share a namespace with other Service
                # children of the BuildMaster node, like status plugins, the
                # Manhole, the ChangeMaster, and the BotMaster (although most
                # of these don't have names)
                msg = ("Schedulers must have unique names, but "
                       "'%s' was a duplicate" % (s.name,))
                raise ValueError(msg)
            schedulernames.append(s.name)

        # assert that all locks used by the Builds and their Steps are
        # uniquely named.
        locks = {}
        for b in builders:
            for l in b.get('locks', []):
                if locks.has_key(l.name):
                    if locks[l.name] is not l:
                        raise ValueError("Two different locks (%s and %s) "
                                         "share the name %s"
                                         % (l, locks[l.name], l.name))
                else:
                    locks[l.name] = l
            # TODO: this will break with any BuildFactory that doesn't use a
            # .steps list, but I think the verification step is more
            # important.
            for s in b['factory'].steps:
                for l in s[1].get('locks', []):
                    if locks.has_key(l.name):
                        if locks[l.name] is not l:
                            raise ValueError("Two different locks (%s and %s)"
                                             " share the name %s"
                                             % (l, locks[l.name], l.name))
                    else:
                        locks[l.name] = l

        # slavePortnum supposed to be a strports specification
        if type(slavePortnum) is int:
            slavePortnum = "tcp:%d" % slavePortnum

        # now we're committed to implementing the new configuration, so do
        # it atomically
        # TODO: actually, this is spread across a couple of Deferreds, so it
        # really isn't atomic.

        d = defer.succeed(None)

        self.projectName = projectName
        self.projectURL = projectURL
        self.buildbotURL = buildbotURL

        # self.bots: Disconnect any that were attached and removed from the
        # list. Update self.checker with the new list of passwords,
        # including debug/change/status.
        d.addCallback(lambda res: self.loadConfig_Slaves(bots))

        # self.debugPassword
        if debugPassword:
            self.checker.addUser("debug", debugPassword)
            self.debugPassword = debugPassword

        # self.manhole
        if manhole != self.manhole:
            # changing
            if self.manhole:
                # disownServiceParent may return a Deferred
                d.addCallback(lambda res: self.manhole.disownServiceParent())
                def _remove(res):
                    self.manhole = None
                    return res
                d.addCallback(_remove)
            if manhole:
                def _add(res):
                    self.manhole = manhole
                    manhole.setServiceParent(self)
                d.addCallback(_add)

        # add/remove self.botmaster.builders to match builders. The
        # botmaster will handle startup/shutdown issues.
        d.addCallback(lambda res: self.loadConfig_Builders(builders))

        d.addCallback(lambda res: self.loadConfig_status(status))

        # Schedulers are added after Builders in case they start right away
        d.addCallback(lambda res: self.loadConfig_Schedulers(schedulers))
        # and Sources go after Schedulers for the same reason
        d.addCallback(lambda res: self.loadConfig_Sources(sources))

        # self.slavePort
        if self.slavePortnum != slavePortnum:
            if self.slavePort:
                def closeSlavePort(res):
                    d1 = self.slavePort.disownServiceParent()
                    self.slavePort = None
                    return d1
                d.addCallback(closeSlavePort)
            if slavePortnum is not None:
                def openSlavePort(res):
                    self.slavePort = strports.service(slavePortnum,
                                                      self.slaveFactory)
                    self.slavePort.setServiceParent(self)
                d.addCallback(openSlavePort)
                log.msg("BuildMaster listening on port %s" % slavePortnum)
            self.slavePortnum = slavePortnum

        log.msg("configuration update started")
        def _done(res):
            self.readConfig = True
            log.msg("configuration update complete")
        d.addCallback(_done)
        d.addCallback(lambda res: self.botmaster.maybeStartAllBuilds())
        return d

    def loadConfig_Slaves(self, bots):
        # set up the Checker with the names and passwords of all valid bots
        self.checker.users = {} # violates abstraction, oh well
        for user, passwd in bots:
            self.checker.addUser(user, passwd)
        self.checker.addUser("change", "changepw")

        # identify new/old bots
        old = self.bots; oldnames = [name for name,pw in old]
        new = bots; newnames = [name for name,pw in new]
        # removeSlave will hang up on the old bot
        dl = [self.botmaster.removeSlave(name)
              for name in oldnames if name not in newnames]
        [self.botmaster.addSlave(name)
         for name in newnames if name not in oldnames]

        # all done
        self.bots = bots
        return defer.DeferredList(dl, fireOnOneErrback=1, consumeErrors=0)

    def loadConfig_Sources(self, sources):
        log.msg("loadConfig_Sources, change_svc is", self.change_svc,
                self.change_svc.parent)
        # shut down any that were removed, start any that were added
        deleted_sources = [s for s in self.change_svc if s not in sources]
        added_sources = [s for s in sources if s not in self.change_svc]
        dl = [self.change_svc.removeSource(s) for s in deleted_sources]
        def addNewOnes(res):
            [self.change_svc.addSource(s) for s in added_sources]
        d = defer.DeferredList(dl, fireOnOneErrback=1, consumeErrors=0)
        d.addCallback(addNewOnes)
        return d

    def allSchedulers(self):
        # TODO: when twisted-1.3 compatibility is dropped, switch to the
        # providedBy form, because it's faster (no actual adapter lookup)
        return [child for child in self
                #if interfaces.IScheduler.providedBy(child)]
                if interfaces.IScheduler(child, None)]


    def loadConfig_Schedulers(self, newschedulers):
        oldschedulers = self.allSchedulers()
        removed = [s for s in oldschedulers if s not in newschedulers]
        added = [s for s in newschedulers if s not in oldschedulers]
        dl = [defer.maybeDeferred(s.disownServiceParent) for s in removed]
        def addNewOnes(res):
            for s in added:
                s.setServiceParent(self)
        d = defer.DeferredList(dl, fireOnOneErrback=1)
        d.addCallback(addNewOnes)
        return d

    def loadConfig_Builders(self, newBuilderData):
        somethingChanged = False
        newList = {}
        newBuilderNames = []
        allBuilders = self.botmaster.builders.copy()
        for data in newBuilderData:
            name = data['name']
            newList[name] = data
            newBuilderNames.append(name)

        # identify all that were removed
        for oldname in self.botmaster.getBuildernames():
            if oldname not in newList:
                log.msg("removing old builder %s" % oldname)
                del allBuilders[oldname]
                somethingChanged = True
                # announce the change
                self.status.builderRemoved(oldname)

        # everything in newList is either unchanged, changed, or new
        for name, data in newList.items():
            old = self.botmaster.builders.get(name)
            basedir = data['builddir'] # used on both master and slave
            #name, slave, builddir, factory = data
            if not old: # new
                # category added after 0.6.2
                category = data.get('category', None)
                log.msg("adding new builder %s for category %s" %
                        (name, category))
                statusbag = self.status.builderAdded(name, basedir, category)
                builder = Builder(data, statusbag)
                allBuilders[name] = builder
                somethingChanged = True
            elif old.compareToSetup(data):
                # changed: try to minimize the disruption and only modify the
                # pieces that really changed
                diffs = old.compareToSetup(data)
                log.msg("updating builder %s: %s" % (name, "\n".join(diffs)))

                statusbag = old.builder_status
                statusbag.saveYourself() # seems like a good idea
                # TODO: if the basedir was changed, we probably need to make
                # a new statusbag
                new_builder = Builder(data, statusbag)
                new_builder.consumeTheSoulOfYourPredecessor(old)
                # that migrates any retained slavebuilders too

                # point out that the builder was updated. On the Waterfall,
                # this will appear just after any currently-running builds.
                statusbag.addPointEvent(["config", "updated"])

                allBuilders[name] = new_builder
                somethingChanged = True
            else:
                # unchanged: leave it alone
                log.msg("builder %s is unchanged" % name)
                pass

        if somethingChanged:
            sortedAllBuilders = [allBuilders[name] for name in newBuilderNames]
            d = self.botmaster.setBuilders(sortedAllBuilders)
            return d
        return None

    def loadConfig_status(self, status):
        dl = []

        # remove old ones
        for s in self.statusTargets[:]:
            if not s in status:
                log.msg("removing IStatusReceiver", s)
                d = defer.maybeDeferred(s.disownServiceParent)
                dl.append(d)
                self.statusTargets.remove(s)
        # after those are finished going away, add new ones
        def addNewOnes(res):
            for s in status:
                if not s in self.statusTargets:
                    log.msg("adding IStatusReceiver", s)
                    s.setServiceParent(self)
                    self.statusTargets.append(s)
        d = defer.DeferredList(dl, fireOnOneErrback=1)
        d.addCallback(addNewOnes)
        return d


    def addChange(self, change):
        for s in self.allSchedulers():
            s.addChange(change)

    def submitBuildSet(self, bs):
        # determine the set of Builders to use
        builders = []
        for name in bs.builderNames:
            b = self.botmaster.builders.get(name)
            if b:
                if b not in builders:
                    builders.append(b)
                continue
            # TODO: add aliases like 'all'
            raise KeyError("no such builder named '%s'" % name)

        # now tell the BuildSet to create BuildRequests for all those
        # Builders and submit them
        bs.start(builders)
        self.status.buildsetSubmitted(bs.status)


class Control:
    if implements:
        implements(interfaces.IControl)
    else:
        __implements__ = interfaces.IControl,

    def __init__(self, master):
        self.master = master

    def addChange(self, change):
        self.master.change_svc.addChange(change)

    def submitBuildSet(self, bs):
        self.master.submitBuildSet(bs)

    def getBuilder(self, name):
        b = self.master.botmaster.builders[name]
        return interfaces.IBuilderControl(b)

components.registerAdapter(Control, BuildMaster, interfaces.IControl)

# so anybody who can get a handle on the BuildMaster can cause a build with:
#  IControl(master).getBuilder("full-2.3").requestBuild(buildrequest)

