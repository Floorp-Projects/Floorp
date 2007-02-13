# -*- test-case-name: buildbot.test.test_slavecommand -*-

from twisted.trial import unittest
from twisted.internet import reactor, interfaces
from twisted.python import runtime, failure, util
from buildbot.twcompat import maybeWait

import os, sys

from buildbot.slave import commands
SlaveShellCommand = commands.SlaveShellCommand

from buildbot.test.runutils import SignalMixin, FakeSlaveBuilder

# test slavecommand.py by running the various commands with a fake
# SlaveBuilder object that logs the calls to sendUpdate()



class ShellBase(SignalMixin):

    def setUp(self):
        self.basedir = "test_slavecommand"
        if not os.path.isdir(self.basedir):
            os.mkdir(self.basedir)
        self.subdir = os.path.join(self.basedir, "subdir")
        if not os.path.isdir(self.subdir):
            os.mkdir(self.subdir)
        self.builder = FakeSlaveBuilder(self.usePTY, self.basedir)
        self.emitcmd = util.sibpath(__file__, "emit.py")
        self.subemitcmd = os.path.join(util.sibpath(__file__, "subdir"),
                                       "emit.py")
        self.sleepcmd = util.sibpath(__file__, "sleep.py")

    def failUnlessIn(self, substring, string):
        self.failUnless(string.find(substring) != -1,
                        "'%s' not in '%s'" % (substring, string))

    def getfile(self, which):
        got = ""
        for r in self.builder.updates:
            if r.has_key(which):
                got += r[which]
        return got

    def checkOutput(self, expected):
        """
        @type  expected: list of (streamname, contents) tuples
        @param expected: the expected output
        """
        expected_linesep = os.linesep
        if self.usePTY:
            # PTYs change the line ending. I'm not sure why.
            expected_linesep = "\r\n"
        expected = [(stream, contents.replace("\n", expected_linesep, 1000))
                    for (stream, contents) in expected]
        if self.usePTY:
            # PTYs merge stdout+stderr into a single stream
            expected = [('stdout', contents)
                        for (stream, contents) in expected]
        # now merge everything into one string per stream
        streams = {}
        for (stream, contents) in expected:
            streams[stream] = streams.get(stream, "") + contents
        for (stream, contents) in streams.items():
            got = self.getfile(stream)
            self.assertEquals(got, contents)

    def getrc(self):
        self.failUnless(self.builder.updates[-1].has_key('rc'))
        got = self.builder.updates[-1]['rc']
        return got
    def checkrc(self, expected):
        got = self.getrc()
        self.assertEquals(got, expected)
        
    def testShell1(self):
        targetfile = os.path.join(self.basedir, "log1.out")
        if os.path.exists(targetfile):
            os.unlink(targetfile)
        cmd = "%s %s 0" % (sys.executable, self.emitcmd)
        args = {'command': cmd, 'workdir': '.', 'timeout': 60}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        expected = [('stdout', "this is stdout\n"),
                    ('stderr', "this is stderr\n")]
        d.addCallback(self._checkPass, expected, 0)
        def _check_targetfile(res):
            self.failUnless(os.path.exists(targetfile))
        d.addCallback(_check_targetfile)
        return maybeWait(d)

    def _checkPass(self, res, expected, rc):
        self.checkOutput(expected)
        self.checkrc(rc)

    def testShell2(self):
        cmd = [sys.executable, self.emitcmd, "0"]
        args = {'command': cmd, 'workdir': '.', 'timeout': 60}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        expected = [('stdout', "this is stdout\n"),
                    ('stderr', "this is stderr\n")]
        d.addCallback(self._checkPass, expected, 0)
        return maybeWait(d)

    def testShellRC(self):
        cmd = [sys.executable, self.emitcmd, "1"]
        args = {'command': cmd, 'workdir': '.', 'timeout': 60}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        expected = [('stdout', "this is stdout\n"),
                    ('stderr', "this is stderr\n")]
        d.addCallback(self._checkPass, expected, 1)
        return maybeWait(d)

    def testShellEnv(self):
        cmd = "%s %s 0" % (sys.executable, self.emitcmd)
        args = {'command': cmd, 'workdir': '.',
                'env': {'EMIT_TEST': "envtest"}, 'timeout': 60}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        expected = [('stdout', "this is stdout\n"),
                    ('stderr', "this is stderr\n"),
                    ('stdout', "EMIT_TEST: envtest\n"),
                    ]
        d.addCallback(self._checkPass, expected, 0)
        return maybeWait(d)

    def testShellSubdir(self):
        targetfile = os.path.join(self.basedir, "subdir", "log1.out")
        if os.path.exists(targetfile):
            os.unlink(targetfile)
        cmd = "%s %s 0" % (sys.executable, self.subemitcmd)
        args = {'command': cmd, 'workdir': "subdir", 'timeout': 60}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        expected = [('stdout', "this is stdout in subdir\n"),
                    ('stderr', "this is stderr\n")]
        d.addCallback(self._checkPass, expected, 0)
        def _check_targetfile(res):
            self.failUnless(os.path.exists(targetfile))
        d.addCallback(_check_targetfile)
        return maybeWait(d)

    def testShellMissingCommand(self):
        args = {'command': "/bin/EndWorldHungerAndMakePigsFly",
                'workdir': '.', 'timeout': 10,
                'env': {"LC_ALL": "C"},
                }
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        d.addCallback(self._testShellMissingCommand_1)
        return maybeWait(d)
    def _testShellMissingCommand_1(self, res):
        self.failIfEqual(self.getrc(), 0)
        # we used to check the error message to make sure it said something
        # about a missing command, but there are a variety of shells out
        # there, and they emit message sin a variety of languages, so we
        # stopped trying.

    def testTimeout(self):
        args = {'command': [sys.executable, self.sleepcmd, "10"],
                'workdir': '.', 'timeout': 2}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        d.addCallback(self._testTimeout_1)
        return maybeWait(d)
    def _testTimeout_1(self, res):
        self.failIfEqual(self.getrc(), 0)
        got = self.getfile('header')
        self.failUnlessIn("command timed out: 2 seconds without output", got)
        if runtime.platformType == "posix":
            # the "killing pid" message is not present in windows
            self.failUnlessIn("killing pid", got)
        # but the process *ought* to be killed somehow
        self.failUnlessIn("process killed by signal", got)
        #print got
    if runtime.platformType != 'posix':
        testTimeout.todo = "timeout doesn't appear to work under windows"

    def testInterrupt1(self):
        args = {'command': [sys.executable, self.sleepcmd, "10"],
                'workdir': '.', 'timeout': 20}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        reactor.callLater(1, c.interrupt)
        d.addCallback(self._testInterrupt1_1)
        return maybeWait(d)
    def _testInterrupt1_1(self, res):
        self.failIfEqual(self.getrc(), 0)
        got = self.getfile('header')
        self.failUnlessIn("command interrupted", got)
        if runtime.platformType == "posix":
            self.failUnlessIn("process killed by signal", got)
    if runtime.platformType != 'posix':
        testInterrupt1.todo = "interrupt doesn't appear to work under windows"


    # todo: twisted-specific command tests

class Shell(ShellBase, unittest.TestCase):
    usePTY = False

    def testInterrupt2(self):
        # test the backup timeout. This doesn't work under a PTY, because the
        # transport.loseConnection we do in the timeout handler actually
        # *does* kill the process.
        args = {'command': [sys.executable, self.sleepcmd, "5"],
                'workdir': '.', 'timeout': 20}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        c.command.BACKUP_TIMEOUT = 1
        # make it unable to kill the child, by changing the signal it uses
        # from SIGKILL to the do-nothing signal 0.
        c.command.KILL = None
        reactor.callLater(1, c.interrupt)
        d.addBoth(self._testInterrupt2_1)
        return maybeWait(d)
    def _testInterrupt2_1(self, res):
        # the slave should raise a TimeoutError exception. In a normal build
        # process (i.e. one that uses step.RemoteShellCommand), this
        # exception will be handed to the Step, which will acquire an ERROR
        # status. In our test environment, it isn't such a big deal.
        self.failUnless(isinstance(res, failure.Failure),
                        "res is not a Failure: %s" % (res,))
        self.failUnless(res.check(commands.TimeoutError))
        self.checkrc(-1)
        return
        # the command is still actually running. Start another command, to
        # make sure that a) the old command's output doesn't interfere with
        # the new one, and b) the old command's actual termination doesn't
        # break anything
        args = {'command': [sys.executable, self.sleepcmd, "5"],
                'workdir': '.', 'timeout': 20}
        c = SlaveShellCommand(self.builder, None, args)
        d = c.start()
        d.addCallback(self._testInterrupt2_2)
        return d
    def _testInterrupt2_2(self, res):
        self.checkrc(0)
        # N.B.: under windows, the trial process hangs out for another few
        # seconds. I assume that the win32eventreactor is waiting for one of
        # the lingering child processes to really finish.

haveProcess = interfaces.IReactorProcess(reactor, None)
if runtime.platformType == 'posix':
    # test with PTYs also
    class ShellPTY(ShellBase, unittest.TestCase):
        usePTY = True
    if not haveProcess:
        ShellPTY.skip = "this reactor doesn't support IReactorProcess"
if not haveProcess:
    Shell.skip = "this reactor doesn't support IReactorProcess"
