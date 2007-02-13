

# test step.ShellCommand and the slave-side commands.ShellCommand

import sys, time, os
from twisted.trial import unittest
from twisted.internet import reactor, defer
from twisted.python import util
from buildbot.slave.commands import SlaveShellCommand
from buildbot.twcompat import maybeWait
from buildbot.test.runutils import SlaveCommandTestBase

class SlaveSide(SlaveCommandTestBase, unittest.TestCase):
    def testOne(self):
        self.setUpBuilder("test_shell.testOne")
        emitcmd = util.sibpath(__file__, "emit.py")
        args = {
            'command': [sys.executable, emitcmd, "0"],
            'workdir': ".",
            }
        d = self.startCommand(SlaveShellCommand, args)
        d.addCallback(self.collectUpdates)
        def _check(logs):
            self.failUnlessEqual(logs['stdout'], "this is stdout\n")
            self.failUnlessEqual(logs['stderr'], "this is stderr\n")
        d.addCallback(_check)
        return maybeWait(d)

    # TODO: move test_slavecommand.Shell and .ShellPTY over here

    def _generateText(self, filename):
        lines = []
        for i in range(3):
            lines.append("this is %s %d\n" % (filename, i))
        return "".join(lines)

    def testLogFiles_0(self):
        return self._testLogFiles(0)

    def testLogFiles_1(self):
        return self._testLogFiles(1)

    def testLogFiles_2(self):
        return self._testLogFiles(2)

    def testLogFiles_3(self):
        return self._testLogFiles(3)

    def _testLogFiles(self, mode):
        basedir = "test_shell.testLogFiles"
        self.setUpBuilder(basedir)
        # emitlogs.py writes two lines to stdout and two logfiles, one second
        # apart. Then it waits for us to write something to stdin, then it
        # writes one more line.

        if mode != 3:
            # we write something to the log file first, to exercise the logic
            # that distinguishes between the old file and the one as modified
            # by the ShellCommand. We set the timestamp back 5 seconds so
            # that timestamps can be used to distinguish old from new.
            log2file = os.path.join(basedir, "log2.out")
            f = open(log2file, "w")
            f.write("dummy text\n")
            f.close()
            earlier = time.time() - 5
            os.utime(log2file, (earlier, earlier))

        if mode == 3:
            # mode=3 doesn't create the old logfiles in the first place, but
            # then behaves like mode=1 (where the command pauses before
            # creating them).
            mode = 1

        # mode=1 will cause emitlogs.py to delete the old logfiles first, and
        # then wait two seconds before creating the new files. mode=0 does
        # not do this.
        args = {
            'command': [sys.executable,
                        util.sibpath(__file__, "emitlogs.py"),
                        "%s" % mode],
            'workdir': ".",
            'logfiles': {"log2": "log2.out",
                         "log3": "log3.out"},
            'keep_stdin_open': True,
            }
        finishd = self.startCommand(SlaveShellCommand, args)
        # The first batch of lines is written immediately. The second is
        # written after a pause of one second. We poll once per second until
        # we see both batches.

        self._check_timeout = 10
        d = self._check_and_wait()
        def _wait_for_finish(res, finishd):
            return finishd
        d.addCallback(_wait_for_finish, finishd)
        d.addCallback(self.collectUpdates)
        def _check(logs):
            self.failUnlessEqual(logs['stdout'], self._generateText("stdout"))
            if mode == 2:
                self.failIf(('log','log2') in logs)
                self.failIf(('log','log3') in logs)
            else:
                self.failUnlessEqual(logs[('log','log2')],
                                     self._generateText("log2"))
                self.failUnlessEqual(logs[('log','log3')],
                                     self._generateText("log3"))
        d.addCallback(_check)
        d.addBoth(self._maybePrintError)
        return maybeWait(d)

    def _check_and_wait(self, res=None):
        self._check_timeout -= 1
        if self._check_timeout <= 0:
            raise defer.TimeoutError("gave up on command")
        logs = self.collectUpdates()
        if logs.get('stdout') == "this is stdout 0\nthis is stdout 1\n":
            # the emitlogs.py process is now waiting for something to arrive
            # on stdin
            self.cmd.command.pp.transport.write("poke\n")
            return
        if not self.cmd.running:
            self.fail("command finished too early")
        spin = defer.Deferred()
        spin.addCallback(self._check_and_wait)
        reactor.callLater(1, spin.callback, None)
        return spin

    def _maybePrintError(self, res):
        rc = self.findRC()
        if rc != 0:
            print "Command ended with rc=%s" % rc
            print "STDERR:"
            self.printStderr()
        return res

    # MAYBE TODO: a command which appends to an existing logfile should
    # result in only the new text being sent up to the master. I need to
    # think about this more first.

