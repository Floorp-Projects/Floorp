# -*- test-case-name: buildbot.test.test_locks -*-

from twisted.python import log
from twisted.internet import reactor, defer
from buildbot import util

if False: # for debugging
    def debuglog(msg):
        log.msg(msg)
else:
    def debuglog(msg):
        pass

class BaseLock:
    description = "<BaseLock>"

    def __init__(self, name, maxCount=1):
        self.name = name
        self.waiting = []
        self.owners = []
        self.maxCount=maxCount

    def __repr__(self):
        return self.description

    def isAvailable(self):
        debuglog("%s isAvailable: self.owners=%r" % (self, self.owners))
        return len(self.owners) < self.maxCount

    def claim(self, owner):
        debuglog("%s claim(%s)" % (self, owner))
        assert owner is not None
        assert len(self.owners) < self.maxCount, "ask for isAvailable() first"
        self.owners.append(owner)
        debuglog(" %s is claimed" % (self,))

    def release(self, owner):
        debuglog("%s release(%s)" % (self, owner))
        assert owner in self.owners
        self.owners.remove(owner)
        # who can we wake up?
        if self.waiting:
            d = self.waiting.pop(0)
            reactor.callLater(0, d.callback, self)

    def waitUntilMaybeAvailable(self, owner):
        """Fire when the lock *might* be available. The caller will need to
        check with isAvailable() when the deferred fires. This loose form is
        used to avoid deadlocks. If we were interested in a stronger form,
        this would be named 'waitUntilAvailable', and the deferred would fire
        after the lock had been claimed.
        """
        debuglog("%s waitUntilAvailable(%s)" % (self, owner))
        if self.isAvailable():
            return defer.succeed(self)
        d = defer.Deferred()
        self.waiting.append(d)
        return d


class RealMasterLock(BaseLock):
    def __init__(self, lockid):
        BaseLock.__init__(self, lockid.name, lockid.maxCount)
        self.description = "<MasterLock(%s, %s)>" % (self.name, self.maxCount)

    def getLock(self, slave):
        return self

class RealSlaveLock:
    def __init__(self, lockid):
        self.name = lockid.name
        self.maxCount = lockid.maxCount
        self.maxCountForSlave = lockid.maxCountForSlave
        self.description = "<SlaveLock(%s, %s, %s)>" % (self.name,
                                                        self.maxCount,
                                                        self.maxCountForSlave)
        self.locks = {}

    def __repr__(self):
        return self.description

    def getLock(self, slavebuilder):
        slavename = slavebuilder.slave.slavename
        if not self.locks.has_key(slavename):
            maxCount = self.maxCountForSlave.get(slavename,
                                                 self.maxCount)
            lock = self.locks[slavename] = BaseLock(self.name, maxCount)
            desc = "<SlaveLock(%s, %s)[%s] %d>" % (self.name, maxCount,
                                                   slavename, id(lock))
            lock.description = desc
            self.locks[slavename] = lock
        return self.locks[slavename]


# master.cfg should only reference the following MasterLock and SlaveLock
# classes. They are identifiers that will be turned into real Locks later,
# via the BotMaster.getLockByID method.

class MasterLock(util.ComparableMixin):
    """I am a semaphore that limits the number of simultaneous actions.

    Builds and BuildSteps can declare that they wish to claim me as they run.
    Only a limited number of such builds or steps will be able to run
    simultaneously. By default this number is one, but my maxCount parameter
    can be raised to allow two or three or more operations to happen at the
    same time.

    Use this to protect a resource that is shared among all builders and all
    slaves, for example to limit the load on a common SVN repository.
    """

    compare_attrs = ['name', 'maxCount']
    lockClass = RealMasterLock
    def __init__(self, name, maxCount=1):
        self.name = name
        self.maxCount = maxCount

class SlaveLock(util.ComparableMixin):
    """I am a semaphore that limits simultaneous actions on each buildslave.

    Builds and BuildSteps can declare that they wish to claim me as they run.
    Only a limited number of such builds or steps will be able to run
    simultaneously on any given buildslave. By default this number is one,
    but my maxCount parameter can be raised to allow two or three or more
    operations to happen on a single buildslave at the same time.

    Use this to protect a resource that is shared among all the builds taking
    place on each slave, for example to limit CPU or memory load on an
    underpowered machine.

    Each buildslave will get an independent copy of this semaphore. By
    default each copy will use the same owner count (set with maxCount), but
    you can provide maxCountForSlave with a dictionary that maps slavename to
    owner count, to allow some slaves more parallelism than others.

    """

    compare_attrs = ['name', 'maxCount', '_maxCountForSlaveList']
    lockClass = RealSlaveLock
    def __init__(self, name, maxCount=1, maxCountForSlave={}):
        self.name = name
        self.maxCount = maxCount
        self.maxCountForSlave = maxCountForSlave
        # for comparison purposes, turn this dictionary into a stably-sorted
        # list of tuples
        self._maxCountForSlaveList = self.maxCountForSlave.items()
        self._maxCountForSlaveList.sort()
        self._maxCountForSlaveList = tuple(self._maxCountForSlaveList)
