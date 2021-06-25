# vim: set ts=4 et sw=4 tw=80
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from twisted.internet import protocol, reactor
from twisted.internet.task import LoopingCall
from autobahn.twisted.websocket import WebSocketServerProtocol, WebSocketServerFactory

import psutil

import argparse
import six
import sys
import os

# maps a command issued via websocket to running an executable with args
commands = {
    "iceserver": [sys.executable, "-u", os.path.join("iceserver", "iceserver.py")]
}


class ProcessSide(protocol.ProcessProtocol):
    """Handles the spawned process (I/O, process termination)"""

    def __init__(self, socketSide):
        self.socketSide = socketSide

    def outReceived(self, data):
        data = six.ensure_str(data)
        if self.socketSide:
            lines = data.splitlines()
            for line in lines:
                self.socketSide.sendMessage(line.encode("utf8"), False)

    def errReceived(self, data):
        self.outReceived(data)

    def processEnded(self, reason):
        if self.socketSide:
            self.outReceived(reason.getTraceback())
            self.socketSide.processGone()

    def socketGone(self):
        self.socketSide = None
        self.transport.loseConnection()
        self.transport.signalProcess("KILL")


class SocketSide(WebSocketServerProtocol):
    """
    Handles the websocket (I/O, closed connection), and spawning the process
    """

    def __init__(self):
        super(SocketSide, self).__init__()
        self.processSide = None

    def onConnect(self, request):
        return None

    def onOpen(self):
        return None

    def onMessage(self, payload, isBinary):
        # We only expect a single message, which tells us what kind of process
        # we're supposed to launch. ProcessSide pipes output to us for sending
        # back to the websocket client.
        if not self.processSide:
            self.processSide = ProcessSide(self)
            # We deliberately crash if |data| isn't on the "menu",
            # or there is some problem spawning.
            data = six.ensure_str(payload)
            try:
                reactor.spawnProcess(
                    self.processSide, commands[data][0], commands[data], env=os.environ
                )
            except BaseException as e:
                print(e.str())
                self.sendMessage(e.str())
                self.processGone()

    def onClose(self, wasClean, code, reason):
        if self.processSide:
            self.processSide.socketGone()

    def processGone(self):
        self.processSide = None
        self.transport.loseConnection()


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
    parser = argparse.ArgumentParser(description="Starts websocket/process bridge.")
    parser.add_argument(
        "--port",
        type=str,
        dest="port",
        default="8191",
        help="Port for websocket/process bridge. Default 8191.",
    )
    args = parser.parse_args()

    parent_checker = LoopingCall(check_parent)
    parent_checker.start(1)

    bridgeFactory = WebSocketServerFactory()
    bridgeFactory.protocol = SocketSide
    reactor.listenTCP(int(args.port), bridgeFactory)
    print("websocket/process bridge listening on port %s" % args.port)
    reactor.run()
