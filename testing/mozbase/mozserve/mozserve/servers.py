# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import os
import re
import subprocess
import sys
import time
from argparse import Namespace
from contextlib import contextmanager
from subprocess import PIPE, Popen
from threading import Thread


@contextmanager
def popenCleanupHack(isWin):
    """
    Hack to work around https://bugs.python.org/issue37380
    The basic idea is that on old versions of Python on Windows,
    we need to clear subprocess._cleanup before we call Popen(),
    then restore it afterwards.
    """
    savedCleanup = None
    if isWin and sys.version_info[0] == 3 and sys.version_info < (3, 7, 5):
        savedCleanup = subprocess._cleanup
        subprocess._cleanup = lambda: None
    try:
        yield
    finally:
        if savedCleanup:
            subprocess._cleanup = savedCleanup


class Http3Server(object):
    """
    Class which encapsulates the Http3 server
    """

    def __init__(self, options, env, logger):
        if isinstance(options, Namespace):
            options = vars(options)
        self._log = logger
        self._profileDir = options["profilePath"]
        self._env = copy.deepcopy(env)
        self._ports = {}
        self._echConfig = ""
        self._isMochitest = options["isMochitest"]
        self._http3ServerPath = options["http3ServerPath"]
        self._isWin = options["isWin"]
        self._http3ServerProc = {}
        self._proxyPort = -1
        if options.get("proxyPort"):
            self._proxyPort = options["proxyPort"]

    def ports(self):
        return self._ports

    def echConfig(self):
        return self._echConfig

    def read_streams(self, name, proc, pipe):
        output = "stdout" if pipe == proc.stdout else "stderr"
        for line in iter(pipe.readline, ""):
            self._log.info("server: %s [%s] %s" % (name, output, line))

    def start(self):
        if not os.path.exists(self._http3ServerPath):
            raise Exception("Http3 server not found at %s" % self._http3ServerPath)

        self._log.info("mozserve | Found Http3Server path: %s" % self._http3ServerPath)

        dbPath = os.path.join(self._profileDir, "cert9.db")
        if not os.path.exists(dbPath):
            raise Exception("cert db not found at %s" % dbPath)

        dbPath = self._profileDir
        self._log.info("mozserve | cert db path: %s" % dbPath)

        try:
            if self._isMochitest:
                self._env["MOZ_HTTP3_MOCHITEST"] = "1"
            if self._proxyPort != -1:
                self._env["MOZ_HTTP3_PROXY_PORT"] = str(self._proxyPort)
            with popenCleanupHack(self._isWin):
                process = Popen(
                    [self._http3ServerPath, dbPath],
                    stdin=PIPE,
                    stdout=PIPE,
                    stderr=PIPE,
                    env=self._env,
                    cwd=os.getcwd(),
                    universal_newlines=True,
                )
            self._http3ServerProc["http3Server"] = process

            # Check to make sure the server starts properly by waiting for it to
            # tell us it's started
            msg = process.stdout.readline()
            self._log.info("mozserve | http3 server msg: %s" % msg)
            name = "http3server"
            t1 = Thread(
                target=self.read_streams,
                args=(name, process, process.stdout),
                daemon=True,
            )
            t1.start()
            t2 = Thread(
                target=self.read_streams,
                args=(name, process, process.stderr),
                daemon=True,
            )
            t2.start()
            if "server listening" in msg:
                searchObj = re.search(
                    r"HTTP3 server listening on ports ([0-9]+), ([0-9]+), ([0-9]+), ([0-9]+) and ([0-9]+)."
                    " EchConfig is @([\x00-\x7F]+)@",
                    msg,
                    0,
                )
                if searchObj:
                    self._ports["MOZHTTP3_PORT"] = searchObj.group(1)
                    self._ports["MOZHTTP3_PORT_FAILED"] = searchObj.group(2)
                    self._ports["MOZHTTP3_PORT_ECH"] = searchObj.group(3)
                    self._ports["MOZHTTP3_PORT_PROXY"] = searchObj.group(4)
                    self._ports["MOZHTTP3_PORT_NO_RESPONSE"] = searchObj.group(5)
                    self._echConfig = searchObj.group(6)
            else:
                self._log.error("http3server failed to start?")
        except OSError as e:
            # This occurs if the subprocess couldn't be started
            self._log.error("Could not run the http3 server: %s" % (str(e)))

    def stop(self):
        """
        Shutdown our http3Server process, if it exists
        """
        for name, proc in self._http3ServerProc.items():
            self._log.info("%s server shutting down ..." % name)
            if proc.poll() is not None:
                self._log.info("Http3 server %s already dead %s" % (name, proc.poll()))
            else:
                proc.terminate()
                retries = 0
                while proc.poll() is None:
                    time.sleep(0.1)
                    retries += 1
                    if retries > 40:
                        self._log.info("Killing proc")
                        proc.kill()
                        break
        self._http3ServerProc = {}


class NodeHttp2Server(object):
    """
    Class which encapsulates a Node Http/2 server
    """

    def __init__(self, name, options, env, logger):
        if isinstance(options, Namespace):
            options = vars(options)
        self._name = name
        self._log = logger
        self._port = options["port"]
        self._env = copy.deepcopy(env)
        self._nodeBin = options["nodeBin"]
        self._serverPath = options["serverPath"]
        self._dstServerPort = options["dstServerPort"]
        self._isWin = options["isWin"]
        self._nodeProc = None
        self._searchStr = options["searchStr"]
        self._alpn = options["alpn"]

    def port(self):
        return self._port

    def start(self):
        if not os.path.exists(self._serverPath):
            raise Exception(
                "%s server not found at %s" % (self._name, self._serverPath)
            )

        self._log.info(
            "mozserve | Found %s server path: %s" % (self._name, self._serverPath)
        )

        if not os.path.exists(self._nodeBin) or not os.path.isfile(self._nodeBin):
            raise Exception("node not found at path %s" % (self._nodeBin))

        self._log.info("Found node at %s" % (self._nodeBin))

        try:
            # We pipe stdin to node because the server will exit when its
            # stdin reaches EOF
            with popenCleanupHack(self._isWin):
                process = Popen(
                    [
                        self._nodeBin,
                        self._serverPath,
                        "serverPort={}".format(self._dstServerPort),
                        "listeningPort={}".format(self._port),
                        "alpn={}".format(self._alpn),
                    ],
                    stdin=PIPE,
                    stdout=PIPE,
                    stderr=PIPE,
                    env=self._env,
                    cwd=os.getcwd(),
                    universal_newlines=True,
                )
            self._nodeProc = process

            msg = process.stdout.readline()
            self._log.info("runtests.py | %s server msg: %s" % (self._name, msg))
            if "server listening" in msg:
                searchObj = re.search(self._searchStr, msg, 0)
                if searchObj:
                    self._port = int(searchObj.group(1))
                    self._log.info(
                        "%s server started at port: %d" % (self._name, self._port)
                    )
        except OSError as e:
            # This occurs if the subprocess couldn't be started
            self._log.error("Could not run %s server: %s" % (self._name, str(e)))

    def stop(self):
        """
        Shut down our node process, if it exists
        """
        if self._nodeProc is not None:
            if self._nodeProc.poll() is not None:
                self._log.info("Node server already dead %s" % (self._nodeProc.poll()))
            else:
                self._nodeProc.terminate()

            def dumpOutput(fd, label):
                firstTime = True
                for msg in fd:
                    if firstTime:
                        firstTime = False
                        self._log.info("Process %s" % label)
                    self._log.info(msg)

            dumpOutput(self._nodeProc.stdout, "stdout")
            dumpOutput(self._nodeProc.stderr, "stderr")

            self._nodeProc = None


class DoHServer(object):
    """
    Class which encapsulates the DoH server
    """

    def __init__(self, options, env, logger):
        options["searchStr"] = r"DoH server listening on ports ([0-9]+)"
        self._server = NodeHttp2Server("DoH", options, env, logger)

    def port(self):
        return self._server.port()

    def start(self):
        self._server.start()

    def stop(self):
        self._server.stop()


class Http2Server(object):
    """
    Class which encapsulates the Http2 server
    """

    def __init__(self, options, env, logger):
        options["searchStr"] = r"Http2 server listening on ports ([0-9]+)"
        options["dstServerPort"] = -1
        options["alpn"] = ""
        self._server = NodeHttp2Server("Http/2", options, env, logger)

    def port(self):
        return self._server.port()

    def start(self):
        self._server.start()

    def stop(self):
        self._server.stop()
