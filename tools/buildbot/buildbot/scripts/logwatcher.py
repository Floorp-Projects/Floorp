
import os
from twisted.python.failure import Failure
from twisted.internet import task, defer, reactor
from twisted.protocols.basic import LineOnlyReceiver

class FakeTransport:
    disconnecting = False

class BuildmasterTimeoutError(Exception):
    pass
class BuildslaveTimeoutError(Exception):
    pass
class ReconfigError(Exception):
    pass
class BuildSlaveDetectedError(Exception):
    pass

class LogWatcher(LineOnlyReceiver):
    POLL_INTERVAL = 0.1
    TIMEOUT_DELAY = 5.0
    delimiter = os.linesep

    def __init__(self, logfile):
        self.logfile = logfile
        self.in_reconfig = False
        self.transport = FakeTransport()
        self.f = None
        self.processtype = "buildmaster"

    def start(self):
        # return a Deferred that fires when the reconfig process has
        # finished. It errbacks with TimeoutError if the finish line has not
        # been seen within 5 seconds, and with ReconfigError if the error
        # line was seen. If the logfile could not be opened, it errbacks with
        # an IOError.
        self.running = True
        d = defer.maybeDeferred(self._start)
        return d

    def _start(self):
        self.d = defer.Deferred()
        try:
            self.f = open(self.logfile, "rb")
            self.f.seek(0, 2) # start watching from the end
        except IOError:
            pass
        reactor.callLater(self.TIMEOUT_DELAY, self.timeout)
        self.poller = task.LoopingCall(self.poll)
        self.poller.start(self.POLL_INTERVAL)
        return self.d

    def timeout(self):
        if self.processtype == "buildmaster":
            self.d.errback(BuildmasterTimeoutError())
        else:
            self.d.errback(BuildslaveTimeoutError())

    def finished(self, results):
        self.running = False
        self.in_reconfig = False
        self.d.callback(results)

    def lineReceived(self, line):
        if not self.running:
            return
        if "Log opened." in line:
            self.in_reconfig = True
        if "loading configuration from" in line:
            self.in_reconfig = True
        if "Creating BuildSlave" in line:
            self.processtype = "buildslave"

        if self.in_reconfig:
            print line

        if "message from master: attached" in line:
            return self.finished("buildslave")
        if "I will keep using the previous config file" in line:
            return self.finished(Failure(ReconfigError()))
        if "configuration update complete" in line:
            return self.finished("buildmaster")

    def poll(self):
        if not self.f:
            try:
                self.f = open(self.logfile, "rb")
            except IOError:
                return
        while True:
            data = self.f.read(1000)
            if not data:
                return
            self.dataReceived(data)

