#! /usr/bin/python

import sys, re

from twisted.spread import pb
from twisted.cred import credentials, error
from twisted.internet import reactor

class StatusClient(pb.Referenceable):
    """To use this, call my .connected method with a RemoteReference to the
    buildmaster's StatusClientPerspective object.
    """

    def __init__(self, events):
        self.builders = {}
        self.events = events

    def connected(self, remote):
        print "connected"
        self.remote = remote
        remote.callRemote("subscribe", self.events, 5, self)

    def remote_builderAdded(self, buildername, builder):
        print "builderAdded", buildername

    def remote_builderRemoved(self, buildername):
        print "builderRemoved", buildername

    def remote_builderChangedState(self, buildername, state, eta):
        print "builderChangedState", buildername, state, eta

    def remote_buildStarted(self, buildername, build):
        print "buildStarted", buildername

    def remote_buildFinished(self, buildername, build, results):
        print "buildFinished", results

    def remote_buildETAUpdate(self, buildername, build, eta):
        print "ETA", buildername, eta

    def remote_stepStarted(self, buildername, build, stepname, step):
        print "stepStarted", buildername, stepname

    def remote_stepFinished(self, buildername, build, stepname, step, results):
        print "stepFinished", buildername, stepname, results

    def remote_stepETAUpdate(self, buildername, build, stepname, step,
                             eta, expectations):
        print "stepETA", buildername, stepname, eta

    def remote_logStarted(self, buildername, build, stepname, step,
                          logname, log):
        print "logStarted", buildername, stepname

    def remote_logFinished(self, buildername, build, stepname, step,
                           logname, log):
        print "logFinished", buildername, stepname

    def remote_logChunk(self, buildername, build, stepname, step, logname, log,
                        channel, text):
        ChunkTypes = ["STDOUT", "STDERR", "HEADER"]
        print "logChunk[%s]: %s" % (ChunkTypes[channel], text)

class TextClient:
    def __init__(self, master, events="steps"):
        """
        @type  events: string, one of builders, builds, steps, logs, full
        @param events: specify what level of detail should be reported.
         - 'builders': only announce new/removed Builders
         - 'builds': also announce builderChangedState, buildStarted, and
           buildFinished
         - 'steps': also announce buildETAUpdate, stepStarted, stepFinished
         - 'logs': also announce stepETAUpdate, logStarted, logFinished
         - 'full': also announce log contents
        """        
        self.master = master
        self.listener = StatusClient(events)

    def run(self):
        """Start the TextClient."""
        self.startConnecting()
        reactor.run()

    def startConnecting(self):
        try:
            host, port = re.search(r'(.+):(\d+)', self.master).groups()
            port = int(port)
        except:
            print "unparseable master location '%s'" % self.master
            print " expecting something more like localhost:8007"
            raise
        cf = pb.PBClientFactory()
        creds = credentials.UsernamePassword("statusClient", "clientpw")
        d = cf.login(creds)
        reactor.connectTCP(host, port, cf)
        d.addCallbacks(self.connected, self.not_connected)
        return d
    def connected(self, ref):
        ref.notifyOnDisconnect(self.disconnected)
        self.listener.connected(ref)
    def not_connected(self, why):
        if why.check(error.UnauthorizedLogin):
            print """
Unable to login.. are you sure we are connecting to a
buildbot.status.client.PBListener port and not to the slaveport?
"""
        reactor.stop()
        return why
    def disconnected(self, ref):
        print "lost connection"
        # we can get here in one of two ways: the buildmaster has
        # disconnected us (probably because it shut itself down), or because
        # we've been SIGINT'ed. In the latter case, our reactor is already
        # shut down, but we have no easy way of detecting that. So protect
        # our attempt to shut down the reactor.
        try:
            reactor.stop()
        except RuntimeError:
            pass

if __name__ == '__main__':
    master = "localhost:8007"
    if len(sys.argv) > 1:
        master = sys.argv[1]
    c = TextClient()
    c.run()
