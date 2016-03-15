# vim: set ts=4 et sw=4 tw=80
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from twisted.internet import protocol, reactor
from twisted.internet.task import LoopingCall
import txws
import psutil

import sys
import os

# maps a command issued via websocket to running an executable with args
commands = {
    'iceserver' : [sys.executable,
                   "-u",
                   os.path.join("iceserver", "iceserver.py")]
}

class ProcessSide(protocol.ProcessProtocol):
    """Handles the spawned process (I/O, process termination)"""

    def __init__(self, socketSide):
        self.socketSide = socketSide

    def outReceived(self, data):
        if self.socketSide:
            lines = data.splitlines()
            for line in lines:
                self.socketSide.transport.write(line)

    def errReceived(self, data):
        self.outReceived(data)

    def processEnded(self, reason):
        if self.socketSide:
            self.outReceived(str(reason))
            self.socketSide.processGone()

    def socketGone(self):
        self.socketSide = None
        self.transport.loseConnection()
        self.transport.signalProcess("KILL")


class SocketSide(protocol.Protocol):
    """
    Handles the websocket (I/O, closed connection), and spawning the process
    """

    def __init__(self):
        self.processSide = None

    def dataReceived(self, data):
        if not self.processSide:
            self.processSide = ProcessSide(self)
            # We deliberately crash if |data| isn't on the "menu",
            # or there is some problem spawning.
            reactor.spawnProcess(self.processSide,
                                 commands[data][0],
                                 commands[data],
                                 env=os.environ)

    def connectionLost(self, reason):
        if self.processSide:
            self.processSide.socketGone()

    def processGone(self):
        self.processSide = None
        self.transport.loseConnection()


class ProcessSocketBridgeFactory(protocol.Factory):
    """Builds sockets that can launch/bridge to a process"""

    def buildProtocol(self, addr):
        return SocketSide()

# Parent process could have already exited, so this is slightly racy. Only
# alternative is to set up a pipe between parent and child, but that requires
# special cooperation from the parent.
parent_process = psutil.Process(os.getpid()).parent()

def check_parent():
    """ Checks if parent process is still alive, and exits if not """
    if not parent_process.is_running():
        print("websocket/process bridge exiting because parent process is gone")
        reactor.stop()

if __name__ == "__main__":
    parent_checker = LoopingCall(check_parent)
    parent_checker.start(1)

    bridgeFactory = ProcessSocketBridgeFactory()
    reactor.listenTCP(8191, txws.WebSocketFactory(bridgeFactory))
    print("websocket/process bridge listening on port 8191")
    reactor.run()


