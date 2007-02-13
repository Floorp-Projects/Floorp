# -*- test-case-name: buildbot.test.test_vc -*-

from __future__ import generators

import sys, os, time, re
from email.Utils import mktime_tz, parsedate_tz

from twisted.trial import unittest
from twisted.internet import defer, reactor, utils, protocol, error
from twisted.python import failure

#defer.Deferred.debug = True

from twisted.python import log
#log.startLogging(sys.stderr)

from buildbot import master, interfaces
from buildbot.slave import bot, commands
from buildbot.slave.commands import rmdirRecursive
from buildbot.status.builder import SUCCESS, FAILURE
from buildbot.process import base
from buildbot.steps import source
from buildbot.changes import changes
from buildbot.sourcestamp import SourceStamp
from buildbot.twcompat import maybeWait, which
from buildbot.scripts import tryclient
from buildbot.test.runutils import SignalMixin

#step.LoggedRemoteCommand.debug = True

# buildbot.twcompat will patch these into t.i.defer if necessary
from twisted.internet.defer import waitForDeferred, deferredGenerator

# Most of these tests (all but SourceStamp) depend upon having a set of
# repositories from which we can perform checkouts. These repositories are
# created by the setUp method at the start of each test class. In earlier
# versions these repositories were created offline and distributed with a
# separate tarball named 'buildbot-test-vc-1.tar.gz'. This is no longer
# necessary.

# CVS requires a local file repository. Providing remote access is beyond
# the feasible abilities of this test program (needs pserver or ssh).

# SVN requires a local file repository. To provide remote access over HTTP
# requires an apache server with DAV support and mod_svn, way beyond what we
# can test from here.

# Arch and Darcs both allow remote (read-only) operation with any web
# server. We test both local file access and HTTP access (by spawning a
# small web server to provide access to the repository files while the test
# is running).

# Perforce starts the daemon running on localhost. Unfortunately, it must
# use a predetermined Internet-domain port number, unless we want to go
# all-out: bind the listen socket ourselves and pretend to be inetd.

try:
    import cStringIO
    StringIO = cStringIO
except ImportError:
    import StringIO

class _PutEverythingGetter(protocol.ProcessProtocol):
    def __init__(self, deferred, stdin):
        self.deferred = deferred
        self.outBuf = StringIO.StringIO()
        self.errBuf = StringIO.StringIO()
        self.outReceived = self.outBuf.write
        self.errReceived = self.errBuf.write
        self.stdin = stdin

    def connectionMade(self):
        if self.stdin is not None:
            self.transport.write(self.stdin)
            self.transport.closeStdin()

    def processEnded(self, reason):
        out = self.outBuf.getvalue()
        err = self.errBuf.getvalue()
        e = reason.value
        code = e.exitCode
        if e.signal:
            self.deferred.errback((out, err, e.signal))
        else:
            self.deferred.callback((out, err, code))

def myGetProcessOutputAndValue(executable, args=(), env={}, path='.',
                               reactor=None, stdin=None):
    """Like twisted.internet.utils.getProcessOutputAndValue but takes
    stdin, too."""
    if reactor is None:
        from twisted.internet import reactor
    d = defer.Deferred()
    p = _PutEverythingGetter(d, stdin)
    reactor.spawnProcess(p, executable, (executable,)+tuple(args), env, path)
    return d

config_vc = """
from buildbot.process import factory
from buildbot.steps import source
s = factory.s

f1 = factory.BuildFactory([
    %s,
    ])
c = {}
c['bots'] = [['bot1', 'sekrit']]
c['sources'] = []
c['schedulers'] = []
c['builders'] = [{'name': 'vc', 'slavename': 'bot1',
                  'builddir': 'vc-dir', 'factory': f1}]
c['slavePortnum'] = 0
BuildmasterConfig = c
"""

p0_diff = r"""
Index: subdir/subdir.c
===================================================================
RCS file: /home/warner/stuff/Projects/BuildBot/code-arch/_trial_temp/test_vc/repositories/CVS-Repository/sample/subdir/subdir.c,v
retrieving revision 1.1.1.1
diff -u -r1.1.1.1 subdir.c
--- subdir/subdir.c	14 Aug 2005 01:32:49 -0000	1.1.1.1
+++ subdir/subdir.c	14 Aug 2005 01:36:15 -0000
@@ -4,6 +4,6 @@
 int
 main(int argc, const char *argv[])
 {
-    printf("Hello subdir.\n");
+    printf("Hello patched subdir.\n");
     return 0;
 }
"""

# this patch does not include the filename headers, so it is
# patchlevel-neutral
TRY_PATCH = '''
@@ -5,6 +5,6 @@
 int
 main(int argc, const char *argv[])
 {
-    printf("Hello subdir.\\n");
+    printf("Hello try.\\n");
     return 0;
 }
'''

MAIN_C = '''
// this is main.c
#include <stdio.h>

int
main(int argc, const char *argv[])
{
    printf("Hello world.\\n");
    return 0;
}
'''

BRANCH_C = '''
// this is main.c
#include <stdio.h>

int
main(int argc, const char *argv[])
{
    printf("Hello branch.\\n");
    return 0;
}
'''

VERSION_C = '''
// this is version.c
#include <stdio.h>

int
main(int argc, const char *argv[])
{
    printf("Hello world, version=%d\\n");
    return 0;
}
'''

SUBDIR_C = '''
// this is subdir/subdir.c
#include <stdio.h>

int
main(int argc, const char *argv[])
{
    printf("Hello subdir.\\n");
    return 0;
}
'''

TRY_C = '''
// this is subdir/subdir.c
#include <stdio.h>

int
main(int argc, const char *argv[])
{
    printf("Hello try.\\n");
    return 0;
}
'''

def qw(s):
    return s.split()

class VCS_Helper:
    # this is a helper class which keeps track of whether each VC system is
    # available, and whether the repository for each has been created. There
    # is one instance of this class, at module level, shared between all test
    # cases.

    def __init__(self):
        self._helpers = {}
        self._isCapable = {}
        self._excuses = {}
        self._repoReady = {}

    def registerVC(self, name, helper):
        self._helpers[name] = helper
        self._repoReady[name] = False

    def skipIfNotCapable(self, name):
        """Either return None, or raise SkipTest"""
        d = self.capable(name)
        def _maybeSkip(res):
            if not res[0]:
                raise unittest.SkipTest(res[1])
        d.addCallback(_maybeSkip)
        return d

    def capable(self, name):
        """Return a Deferred that fires with (True,None) if this host offers
        the given VC tool, or (False,excuse) if it does not (and therefore
        the tests should be skipped)."""

        if self._isCapable.has_key(name):
            if self._isCapable[name]:
                return defer.succeed((True,None))
            else:
                return defer.succeed((False, self._excuses[name]))
        d = defer.maybeDeferred(self._helpers[name].capable)
        def _capable(res):
            if res[0]:
                self._isCapable[name] = True
            else:
                self._excuses[name] = res[1]
            return res
        d.addCallback(_capable)
        return d

    def getHelper(self, name):
        return self._helpers[name]

    def createRepository(self, name):
        """Return a Deferred that fires when the repository is set up."""
        if self._repoReady[name]:
            return defer.succeed(True)
        d = self._helpers[name].createRepository()
        def _ready(res):
            self._repoReady[name] = True
        d.addCallback(_ready)
        return d

VCS = VCS_Helper()


# the overall plan here:
#
# Each VC system is tested separately, all using the same source tree defined
# in the 'files' dictionary above. Each VC system gets its own TestCase
# subclass. The first test case that is run will create the repository during
# setUp(), making two branches: 'trunk' and 'branch'. The trunk gets a copy
# of all the files in 'files'. The variant of good.c is committed on the
# branch.
#
# then testCheckout is run, which does a number of checkout/clobber/update
# builds. These all use trunk r1. It then runs self.fix(), which modifies
# 'fixable.c', then performs another build and makes sure the tree has been
# updated.
#
# testBranch uses trunk-r1 and branch-r1, making sure that we clobber the
# tree properly when we switch between them
#
# testPatch does a trunk-r1 checkout and applies a patch.
#
# testTryGetPatch performs a trunk-r1 checkout, modifies some files, then
# verifies that tryclient.getSourceStamp figures out the base revision and
# what got changed.


# vc_create makes a repository at r1 with three files: main.c, version.c, and
# subdir/foo.c . It also creates a branch from r1 (called b1) in which main.c
# says "hello branch" instead of "hello world". self.trunk[] contains
# revision stamps for everything on the trunk, and self.branch[] does the
# same for the branch.

# vc_revise() checks out a tree at HEAD, changes version.c, then checks it
# back in. The new version stamp is appended to self.trunk[]. The tree is
# removed afterwards.

# vc_try_checkout(workdir, rev) checks out a tree at REV, then changes
# subdir/subdir.c to say 'Hello try'
# vc_try_finish(workdir) removes the tree and cleans up any VC state
# necessary (like deleting the Arch archive entry).


class BaseHelper:
    def __init__(self):
        self.trunk = []
        self.branch = []
        self.allrevs = []

    def capable(self):
        # this is also responsible for setting self.vcexe
        raise NotImplementedError

    def createBasedir(self):
        # you must call this from createRepository
        self.repbase = os.path.abspath(os.path.join("test_vc",
                                                    "repositories"))
        if not os.path.isdir(self.repbase):
            os.makedirs(self.repbase)

    def createRepository(self):
        # this will only be called once per process
        raise NotImplementedError

    def populate(self, basedir):
        os.makedirs(basedir)
        os.makedirs(os.path.join(basedir, "subdir"))
        open(os.path.join(basedir, "main.c"), "w").write(MAIN_C)
        self.version = 1
        version_c = VERSION_C % self.version
        open(os.path.join(basedir, "version.c"), "w").write(version_c)
        open(os.path.join(basedir, "main.c"), "w").write(MAIN_C)
        open(os.path.join(basedir, "subdir", "subdir.c"), "w").write(SUBDIR_C)

    def populate_branch(self, basedir):
        open(os.path.join(basedir, "main.c"), "w").write(BRANCH_C)

    def addTrunkRev(self, rev):
        self.trunk.append(rev)
        self.allrevs.append(rev)
    def addBranchRev(self, rev):
        self.branch.append(rev)
        self.allrevs.append(rev)

    def runCommand(self, basedir, command, failureIsOk=False, stdin=None):
        # all commands passed to do() should be strings or lists. If they are
        # strings, none of the arguments may have spaces. This makes the
        # commands less verbose at the expense of restricting what they can
        # specify.
        if type(command) not in (list, tuple):
            command = command.split(" ")
        DEBUG = False
        if DEBUG:
            print "do %s" % command
            print " in basedir %s" % basedir
            if stdin:
                print " STDIN:\n", stdin, "\n--STDIN DONE"
        env = os.environ.copy()
        env['LC_ALL'] = "C"
        d = myGetProcessOutputAndValue(command[0], command[1:],
                                       env=env, path=basedir,
                                       stdin=stdin)
        def check((out, err, code)):
            if DEBUG:
                print
                print "command was: %s" % command
                if out: print "out: %s" % out
                if err: print "err: %s" % err
                print "code: %s" % code
            if code != 0 and not failureIsOk:
                log.msg("command %s finished with exit code %d" %
                        (command, code))
                log.msg(" and stdout %s" % (out,))
                log.msg(" and stderr %s" % (err,))
                raise RuntimeError("command %s finished with exit code %d"
                                   % (command, code)
                                   + ": see logs for stdout")
            return out
        d.addCallback(check)
        return d

    def do(self, basedir, command, failureIsOk=False, stdin=None):
        d = self.runCommand(basedir, command, failureIsOk=failureIsOk,
                            stdin=stdin)
        return waitForDeferred(d)

    def dovc(self, basedir, command, failureIsOk=False, stdin=None):
        """Like do(), but the VC binary will be prepended to COMMAND."""
        if isinstance(command, (str, unicode)):
            command = self.vcexe + " " + command
        else:
            # command is a list
            command = [self.vcexe] + command
        return self.do(basedir, command, failureIsOk, stdin)

class VCBase(SignalMixin):
    metadir = None
    createdRepository = False
    master = None
    slave = None
    helper = None
    httpServer = None
    httpPort = None
    skip = None
    has_got_revision = False
    has_got_revision_branches_are_merged = False # for SVN

    def failUnlessIn(self, substring, string, msg=None):
        # trial provides a version of this that requires python-2.3 to test
        # strings.
        if msg is None:
            msg = ("did not see the expected substring '%s' in string '%s'" %
                   (substring, string))
        self.failUnless(string.find(substring) != -1, msg)

    def setUp(self):
        d = VCS.skipIfNotCapable(self.vc_name)
        d.addCallback(self._setUp1)
        return maybeWait(d)

    def _setUp1(self, res):
        self.helper = VCS.getHelper(self.vc_name)

        if os.path.exists("basedir"):
            rmdirRecursive("basedir")
        os.mkdir("basedir")
        self.master = master.BuildMaster("basedir")
        self.slavebase = os.path.abspath("slavebase")
        if os.path.exists(self.slavebase):
            rmdirRecursive(self.slavebase)
        os.mkdir("slavebase")

        d = VCS.createRepository(self.vc_name)
        return d

    def connectSlave(self):
        port = self.master.slavePort._port.getHost().port
        slave = bot.BuildSlave("localhost", port, "bot1", "sekrit",
                               self.slavebase, keepalive=0, usePTY=1)
        self.slave = slave
        slave.startService()
        d = self.master.botmaster.waitUntilBuilderAttached("vc")
        return d

    def loadConfig(self, config):
        # reloading the config file causes a new 'listDirs' command to be
        # sent to the slave. To synchronize on this properly, it is easiest
        # to stop and restart the slave.
        d = defer.succeed(None)
        if self.slave:
            d = self.master.botmaster.waitUntilBuilderDetached("vc")
            self.slave.stopService()
        d.addCallback(lambda res: self.master.loadConfig(config))
        d.addCallback(lambda res: self.connectSlave())
        return d

    def serveHTTP(self):
        # launch an HTTP server to serve the repository files
        from twisted.web import static, server
        from twisted.internet import reactor
        self.root = static.File(self.helper.repbase)
        self.site = server.Site(self.root)
        self.httpServer = reactor.listenTCP(0, self.site)
        self.httpPort = self.httpServer.getHost().port

    def doBuild(self, shouldSucceed=True, ss=None):
        c = interfaces.IControl(self.master)

        if ss is None:
            ss = SourceStamp()
        #print "doBuild(ss: b=%s rev=%s)" % (ss.branch, ss.revision)
        req = base.BuildRequest("test_vc forced build", ss)
        d = req.waitUntilFinished()
        c.getBuilder("vc").requestBuild(req)
        d.addCallback(self._doBuild_1, shouldSucceed)
        return d
    def _doBuild_1(self, bs, shouldSucceed):
        r = bs.getResults()
        if r != SUCCESS and shouldSucceed:
            print
            print
            if not bs.isFinished():
                print "Hey, build wasn't even finished!"
            print "Build did not succeed:", r, bs.getText()
            for s in bs.getSteps():
                for l in s.getLogs():
                    print "--- START step %s / log %s ---" % (s.getName(),
                                                              l.getName())
                    print l.getTextWithHeaders()
                    print "--- STOP ---"
                    print
            self.fail("build did not succeed")
        return bs

    def printLogs(self, bs):
        for s in bs.getSteps():
            for l in s.getLogs():
                print "--- START step %s / log %s ---" % (s.getName(),
                                                          l.getName())
                print l.getTextWithHeaders()
                print "--- STOP ---"
                print

    def touch(self, d, f):
        open(os.path.join(d,f),"w").close()
    def shouldExist(self, *args):
        target = os.path.join(*args)
        self.failUnless(os.path.exists(target),
                        "expected to find %s but didn't" % target)
    def shouldNotExist(self, *args):
        target = os.path.join(*args)
        self.failIf(os.path.exists(target),
                    "expected to NOT find %s, but did" % target)
    def shouldContain(self, d, f, contents):
        c = open(os.path.join(d, f), "r").read()
        self.failUnlessIn(contents, c)

    def checkGotRevision(self, bs, expected):
        if self.has_got_revision:
            self.failUnlessEqual(bs.getProperty("got_revision"), expected)

    def checkGotRevisionIsLatest(self, bs):
        expected = self.helper.trunk[-1]
        if self.has_got_revision_branches_are_merged:
            expected = self.helper.allrevs[-1]
        self.checkGotRevision(bs, expected)

    def do_vctest(self, testRetry=True):
        vctype = self.vctype
        args = self.helper.vcargs
        m = self.master
        self.vcdir = os.path.join(self.slavebase, "vc-dir", "source")
        self.workdir = os.path.join(self.slavebase, "vc-dir", "build")
        # woo double-substitution
        s = "s(%s, timeout=200, workdir='build', mode='%%s'" % (vctype,)
        for k,v in args.items():
            s += ", %s=%s" % (k, repr(v))
        s += ")"
        config = config_vc % s

        m.loadConfig(config % 'clobber')
        m.readConfig = True
        m.startService()

        d = self.connectSlave()
        d.addCallback(lambda res: log.msg("testing clobber"))
        d.addCallback(self._do_vctest_clobber)
        d.addCallback(lambda res: log.msg("doing update"))
        d.addCallback(lambda res: self.loadConfig(config % 'update'))
        d.addCallback(lambda res: log.msg("testing update"))
        d.addCallback(self._do_vctest_update)
        if testRetry:
            d.addCallback(lambda res: log.msg("testing update retry"))
            d.addCallback(self._do_vctest_update_retry)
        d.addCallback(lambda res: log.msg("doing copy"))
        d.addCallback(lambda res: self.loadConfig(config % 'copy'))
        d.addCallback(lambda res: log.msg("testing copy"))
        d.addCallback(self._do_vctest_copy)
        if self.metadir:
            d.addCallback(lambda res: log.msg("doing export"))
            d.addCallback(lambda res: self.loadConfig(config % 'export'))
            d.addCallback(lambda res: log.msg("testing export"))
            d.addCallback(self._do_vctest_export)
        return d

    def _do_vctest_clobber(self, res):
        d = self.doBuild() # initial checkout
        d.addCallback(self._do_vctest_clobber_1)
        return d
    def _do_vctest_clobber_1(self, bs):
        self.shouldExist(self.workdir, "main.c")
        self.shouldExist(self.workdir, "version.c")
        self.shouldExist(self.workdir, "subdir", "subdir.c")
        if self.metadir:
            self.shouldExist(self.workdir, self.metadir)
        self.failUnlessEqual(bs.getProperty("revision"), None)
        self.failUnlessEqual(bs.getProperty("branch"), None)
        self.checkGotRevisionIsLatest(bs)

        self.touch(self.workdir, "newfile")
        self.shouldExist(self.workdir, "newfile")
        d = self.doBuild() # rebuild clobbers workdir
        d.addCallback(self._do_vctest_clobber_2)
        return d
    def _do_vctest_clobber_2(self, res):
        self.shouldNotExist(self.workdir, "newfile")

    def _do_vctest_update(self, res):
        log.msg("_do_vctest_update")
        d = self.doBuild() # rebuild with update
        d.addCallback(self._do_vctest_update_1)
        return d
    def _do_vctest_update_1(self, bs):
        log.msg("_do_vctest_update_1")
        self.shouldExist(self.workdir, "main.c")
        self.shouldExist(self.workdir, "version.c")
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % self.helper.version)
        if self.metadir:
            self.shouldExist(self.workdir, self.metadir)
        self.failUnlessEqual(bs.getProperty("revision"), None)
        self.checkGotRevisionIsLatest(bs)

        self.touch(self.workdir, "newfile")
        d = self.doBuild() # update rebuild leaves new files
        d.addCallback(self._do_vctest_update_2)
        return d
    def _do_vctest_update_2(self, bs):
        log.msg("_do_vctest_update_2")
        self.shouldExist(self.workdir, "main.c")
        self.shouldExist(self.workdir, "version.c")
        self.touch(self.workdir, "newfile")
        # now make a change to the repository and make sure we pick it up
        d = self.helper.vc_revise()
        d.addCallback(lambda res: self.doBuild())
        d.addCallback(self._do_vctest_update_3)
        return d
    def _do_vctest_update_3(self, bs):
        log.msg("_do_vctest_update_3")
        self.shouldExist(self.workdir, "main.c")
        self.shouldExist(self.workdir, "version.c")
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % self.helper.version)
        self.shouldExist(self.workdir, "newfile")
        self.failUnlessEqual(bs.getProperty("revision"), None)
        self.checkGotRevisionIsLatest(bs)

        # now "update" to an older revision
        d = self.doBuild(ss=SourceStamp(revision=self.helper.trunk[-2]))
        d.addCallback(self._do_vctest_update_4)
        return d
    def _do_vctest_update_4(self, bs):
        log.msg("_do_vctest_update_4")
        self.shouldExist(self.workdir, "main.c")
        self.shouldExist(self.workdir, "version.c")
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % (self.helper.version-1))
        self.failUnlessEqual(bs.getProperty("revision"),
                             self.helper.trunk[-2])
        self.checkGotRevision(bs, self.helper.trunk[-2])

        # now update to the newer revision
        d = self.doBuild(ss=SourceStamp(revision=self.helper.trunk[-1]))
        d.addCallback(self._do_vctest_update_5)
        return d
    def _do_vctest_update_5(self, bs):
        log.msg("_do_vctest_update_5")
        self.shouldExist(self.workdir, "main.c")
        self.shouldExist(self.workdir, "version.c")
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % self.helper.version)
        self.failUnlessEqual(bs.getProperty("revision"),
                             self.helper.trunk[-1])
        self.checkGotRevision(bs, self.helper.trunk[-1])


    def _do_vctest_update_retry(self, res):
        # certain local changes will prevent an update from working. The
        # most common is to replace a file with a directory, or vice
        # versa. The slave code should spot the failure and do a
        # clobber/retry.
        os.unlink(os.path.join(self.workdir, "main.c"))
        os.mkdir(os.path.join(self.workdir, "main.c"))
        self.touch(os.path.join(self.workdir, "main.c"), "foo")
        self.touch(self.workdir, "newfile")

        d = self.doBuild() # update, but must clobber to handle the error
        d.addCallback(self._do_vctest_update_retry_1)
        return d
    def _do_vctest_update_retry_1(self, bs):
        # SVN-1.4.0 doesn't seem to have any problem with the
        # file-turned-directory issue (although older versions did). So don't
        # actually check that the tree was clobbered.. as long as the update
        # succeeded (checked by doBuild), that should be good enough.
        #self.shouldNotExist(self.workdir, "newfile")
        pass

    def _do_vctest_copy(self, res):
        d = self.doBuild() # copy rebuild clobbers new files
        d.addCallback(self._do_vctest_copy_1)
        return d
    def _do_vctest_copy_1(self, bs):
        if self.metadir:
            self.shouldExist(self.workdir, self.metadir)
        self.shouldNotExist(self.workdir, "newfile")
        self.touch(self.workdir, "newfile")
        self.touch(self.vcdir, "newvcfile")
        self.failUnlessEqual(bs.getProperty("revision"), None)
        self.checkGotRevisionIsLatest(bs)

        d = self.doBuild() # copy rebuild clobbers new files
        d.addCallback(self._do_vctest_copy_2)
        return d
    def _do_vctest_copy_2(self, bs):
        if self.metadir:
            self.shouldExist(self.workdir, self.metadir)
        self.shouldNotExist(self.workdir, "newfile")
        self.shouldExist(self.vcdir, "newvcfile")
        self.shouldExist(self.workdir, "newvcfile")
        self.failUnlessEqual(bs.getProperty("revision"), None)
        self.checkGotRevisionIsLatest(bs)
        self.touch(self.workdir, "newfile")

    def _do_vctest_export(self, res):
        d = self.doBuild() # export rebuild clobbers new files
        d.addCallback(self._do_vctest_export_1)
        return d
    def _do_vctest_export_1(self, bs):
        self.shouldNotExist(self.workdir, self.metadir)
        self.shouldNotExist(self.workdir, "newfile")
        self.failUnlessEqual(bs.getProperty("revision"), None)
        #self.checkGotRevisionIsLatest(bs)
        # VC 'export' is not required to have a got_revision
        self.touch(self.workdir, "newfile")

        d = self.doBuild() # export rebuild clobbers new files
        d.addCallback(self._do_vctest_export_2)
        return d
    def _do_vctest_export_2(self, bs):
        self.shouldNotExist(self.workdir, self.metadir)
        self.shouldNotExist(self.workdir, "newfile")
        self.failUnlessEqual(bs.getProperty("revision"), None)
        #self.checkGotRevisionIsLatest(bs)
        # VC 'export' is not required to have a got_revision

    def do_patch(self):
        vctype = self.vctype
        args = self.helper.vcargs
        m = self.master
        self.vcdir = os.path.join(self.slavebase, "vc-dir", "source")
        self.workdir = os.path.join(self.slavebase, "vc-dir", "build")
        s = "s(%s, timeout=200, workdir='build', mode='%%s'" % (vctype,)
        for k,v in args.items():
            s += ", %s=%s" % (k, repr(v))
        s += ")"
        self.config = config_vc % s

        m.loadConfig(self.config % "clobber")
        m.readConfig = True
        m.startService()

        ss = SourceStamp(revision=self.helper.trunk[-1], patch=(0, p0_diff))

        d = self.connectSlave()
        d.addCallback(lambda res: self.doBuild(ss=ss))
        d.addCallback(self._doPatch_1)
        return d
    def _doPatch_1(self, bs):
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % self.helper.version)
        # make sure the file actually got patched
        subdir_c = os.path.join(self.slavebase, "vc-dir", "build",
                                "subdir", "subdir.c")
        data = open(subdir_c, "r").read()
        self.failUnlessIn("Hello patched subdir.\\n", data)
        self.failUnlessEqual(bs.getProperty("revision"),
                             self.helper.trunk[-1])
        self.checkGotRevision(bs, self.helper.trunk[-1])

        # make sure that a rebuild does not use the leftover patched workdir
        d = self.master.loadConfig(self.config % "update")
        d.addCallback(lambda res: self.doBuild(ss=None))
        d.addCallback(self._doPatch_2)
        return d
    def _doPatch_2(self, bs):
        # make sure the file is back to its original
        subdir_c = os.path.join(self.slavebase, "vc-dir", "build",
                                "subdir", "subdir.c")
        data = open(subdir_c, "r").read()
        self.failUnlessIn("Hello subdir.\\n", data)
        self.failUnlessEqual(bs.getProperty("revision"), None)
        self.checkGotRevisionIsLatest(bs)

        # now make sure we can patch an older revision. We need at least two
        # revisions here, so we might have to create one first
        if len(self.helper.trunk) < 2:
            d = self.helper.vc_revise()
            d.addCallback(self._doPatch_3)
            return d
        return self._doPatch_3()

    def _doPatch_3(self, res=None):
        ss = SourceStamp(revision=self.helper.trunk[-2], patch=(0, p0_diff))
        d = self.doBuild(ss=ss)
        d.addCallback(self._doPatch_4)
        return d
    def _doPatch_4(self, bs):
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % (self.helper.version-1))
        # and make sure the file actually got patched
        subdir_c = os.path.join(self.slavebase, "vc-dir", "build",
                                "subdir", "subdir.c")
        data = open(subdir_c, "r").read()
        self.failUnlessIn("Hello patched subdir.\\n", data)
        self.failUnlessEqual(bs.getProperty("revision"),
                             self.helper.trunk[-2])
        self.checkGotRevision(bs, self.helper.trunk[-2])

        # now check that we can patch a branch
        ss = SourceStamp(branch=self.helper.branchname,
                         revision=self.helper.branch[-1],
                         patch=(0, p0_diff))
        d = self.doBuild(ss=ss)
        d.addCallback(self._doPatch_5)
        return d
    def _doPatch_5(self, bs):
        self.shouldContain(self.workdir, "version.c",
                           "version=%d" % 1)
        self.shouldContain(self.workdir, "main.c", "Hello branch.")
        subdir_c = os.path.join(self.slavebase, "vc-dir", "build",
                                "subdir", "subdir.c")
        data = open(subdir_c, "r").read()
        self.failUnlessIn("Hello patched subdir.\\n", data)
        self.failUnlessEqual(bs.getProperty("revision"),
                             self.helper.branch[-1])
        self.failUnlessEqual(bs.getProperty("branch"), self.helper.branchname)
        self.checkGotRevision(bs, self.helper.branch[-1])


    def do_vctest_once(self, shouldSucceed):
        m = self.master
        vctype = self.vctype
        args = self.helper.vcargs
        vcdir = os.path.join(self.slavebase, "vc-dir", "source")
        workdir = os.path.join(self.slavebase, "vc-dir", "build")
        # woo double-substitution
        s = "s(%s, timeout=200, workdir='build', mode='clobber'" % (vctype,)
        for k,v in args.items():
            s += ", %s=%s" % (k, repr(v))
        s += ")"
        config = config_vc % s

        m.loadConfig(config)
        m.readConfig = True
        m.startService()

        self.connectSlave()
        d = self.doBuild(shouldSucceed) # initial checkout
        return d

    def do_branch(self):
        log.msg("do_branch")
        vctype = self.vctype
        args = self.helper.vcargs
        m = self.master
        self.vcdir = os.path.join(self.slavebase, "vc-dir", "source")
        self.workdir = os.path.join(self.slavebase, "vc-dir", "build")
        s = "s(%s, timeout=200, workdir='build', mode='%%s'" % (vctype,)
        for k,v in args.items():
            s += ", %s=%s" % (k, repr(v))
        s += ")"
        self.config = config_vc % s

        m.loadConfig(self.config % "update")
        m.readConfig = True
        m.startService()

        # first we do a build of the trunk
        d = self.connectSlave()
        d.addCallback(lambda res: self.doBuild(ss=SourceStamp()))
        d.addCallback(self._doBranch_1)
        return d
    def _doBranch_1(self, bs):
        log.msg("_doBranch_1")
        # make sure the checkout was of the trunk
        main_c = os.path.join(self.slavebase, "vc-dir", "build", "main.c")
        data = open(main_c, "r").read()
        self.failUnlessIn("Hello world.", data)

        # now do a checkout on the branch. The change in branch name should
        # trigger a clobber.
        self.touch(self.workdir, "newfile")
        d = self.doBuild(ss=SourceStamp(branch=self.helper.branchname))
        d.addCallback(self._doBranch_2)
        return d
    def _doBranch_2(self, bs):
        log.msg("_doBranch_2")
        # make sure it was on the branch
        main_c = os.path.join(self.slavebase, "vc-dir", "build", "main.c")
        data = open(main_c, "r").read()
        self.failUnlessIn("Hello branch.", data)
        # and make sure the tree was clobbered
        self.shouldNotExist(self.workdir, "newfile")

        # doing another build on the same branch should not clobber the tree
        self.touch(self.workdir, "newbranchfile")
        d = self.doBuild(ss=SourceStamp(branch=self.helper.branchname))
        d.addCallback(self._doBranch_3)
        return d
    def _doBranch_3(self, bs):
        log.msg("_doBranch_3")
        # make sure it is still on the branch
        main_c = os.path.join(self.slavebase, "vc-dir", "build", "main.c")
        data = open(main_c, "r").read()
        self.failUnlessIn("Hello branch.", data)
        # and make sure the tree was not clobbered
        self.shouldExist(self.workdir, "newbranchfile")

        # now make sure that a non-branch checkout clobbers the tree
        d = self.doBuild(ss=SourceStamp())
        d.addCallback(self._doBranch_4)
        return d
    def _doBranch_4(self, bs):
        log.msg("_doBranch_4")
        # make sure it was on the trunk
        main_c = os.path.join(self.slavebase, "vc-dir", "build", "main.c")
        data = open(main_c, "r").read()
        self.failUnlessIn("Hello world.", data)
        self.shouldNotExist(self.workdir, "newbranchfile")

    def do_getpatch(self, doBranch=True):
        log.msg("do_getpatch")
        # prepare a buildslave to do checkouts
        vctype = self.vctype
        args = self.helper.vcargs
        m = self.master
        self.vcdir = os.path.join(self.slavebase, "vc-dir", "source")
        self.workdir = os.path.join(self.slavebase, "vc-dir", "build")
        # woo double-substitution
        s = "s(%s, timeout=200, workdir='build', mode='%%s'" % (vctype,)
        for k,v in args.items():
            s += ", %s=%s" % (k, repr(v))
        s += ")"
        config = config_vc % s

        m.loadConfig(config % 'clobber')
        m.readConfig = True
        m.startService()

        d = self.connectSlave()

        # then set up the "developer's tree". first we modify a tree from the
        # head of the trunk
        tmpdir = "try_workdir"
        self.trydir = os.path.join(self.helper.repbase, tmpdir)
        rmdirRecursive(self.trydir)
        d.addCallback(self.do_getpatch_trunkhead)
        d.addCallback(self.do_getpatch_trunkold)
        if doBranch:
            d.addCallback(self.do_getpatch_branch)
        d.addCallback(self.do_getpatch_finish)
        return d

    def do_getpatch_finish(self, res):
        log.msg("do_getpatch_finish")
        self.helper.vc_try_finish(self.trydir)
        return res

    def try_shouldMatch(self, filename):
        devfilename = os.path.join(self.trydir, filename)
        devfile = open(devfilename, "r").read()
        slavefilename = os.path.join(self.workdir, filename)
        slavefile = open(slavefilename, "r").read()
        self.failUnlessEqual(devfile, slavefile,
                             ("slavefile (%s) contains '%s'. "
                              "developer's file (%s) contains '%s'. "
                              "These ought to match") %
                             (slavefilename, slavefile,
                              devfilename, devfile))

    def do_getpatch_trunkhead(self, res):
        log.msg("do_getpatch_trunkhead")
        d = self.helper.vc_try_checkout(self.trydir, self.helper.trunk[-1])
        d.addCallback(self._do_getpatch_trunkhead_1)
        return d
    def _do_getpatch_trunkhead_1(self, res):
        log.msg("_do_getpatch_trunkhead_1")
        d = tryclient.getSourceStamp(self.vctype_try, self.trydir, None)
        d.addCallback(self._do_getpatch_trunkhead_2)
        return d
    def _do_getpatch_trunkhead_2(self, ss):
        log.msg("_do_getpatch_trunkhead_2")
        d = self.doBuild(ss=ss)
        d.addCallback(self._do_getpatch_trunkhead_3)
        return d
    def _do_getpatch_trunkhead_3(self, res):
        log.msg("_do_getpatch_trunkhead_3")
        # verify that the resulting buildslave tree matches the developer's
        self.try_shouldMatch("main.c")
        self.try_shouldMatch("version.c")
        self.try_shouldMatch(os.path.join("subdir", "subdir.c"))

    def do_getpatch_trunkold(self, res):
        log.msg("do_getpatch_trunkold")
        # now try a tree from an older revision. We need at least two
        # revisions here, so we might have to create one first
        if len(self.helper.trunk) < 2:
            d = self.helper.vc_revise()
            d.addCallback(self._do_getpatch_trunkold_1)
            return d
        return self._do_getpatch_trunkold_1()
    def _do_getpatch_trunkold_1(self, res=None):
        log.msg("_do_getpatch_trunkold_1")
        d = self.helper.vc_try_checkout(self.trydir, self.helper.trunk[-2])
        d.addCallback(self._do_getpatch_trunkold_2)
        return d
    def _do_getpatch_trunkold_2(self, res):
        log.msg("_do_getpatch_trunkold_2")
        d = tryclient.getSourceStamp(self.vctype_try, self.trydir, None)
        d.addCallback(self._do_getpatch_trunkold_3)
        return d
    def _do_getpatch_trunkold_3(self, ss):
        log.msg("_do_getpatch_trunkold_3")
        d = self.doBuild(ss=ss)
        d.addCallback(self._do_getpatch_trunkold_4)
        return d
    def _do_getpatch_trunkold_4(self, res):
        log.msg("_do_getpatch_trunkold_4")
        # verify that the resulting buildslave tree matches the developer's
        self.try_shouldMatch("main.c")
        self.try_shouldMatch("version.c")
        self.try_shouldMatch(os.path.join("subdir", "subdir.c"))

    def do_getpatch_branch(self, res):
        log.msg("do_getpatch_branch")
        # now try a tree from a branch
        d = self.helper.vc_try_checkout(self.trydir, self.helper.branch[-1],
                                        self.helper.branchname)
        d.addCallback(self._do_getpatch_branch_1)
        return d
    def _do_getpatch_branch_1(self, res):
        log.msg("_do_getpatch_branch_1")
        d = tryclient.getSourceStamp(self.vctype_try, self.trydir,
                                     self.helper.try_branchname)
        d.addCallback(self._do_getpatch_branch_2)
        return d
    def _do_getpatch_branch_2(self, ss):
        log.msg("_do_getpatch_branch_2")
        d = self.doBuild(ss=ss)
        d.addCallback(self._do_getpatch_branch_3)
        return d
    def _do_getpatch_branch_3(self, res):
        log.msg("_do_getpatch_branch_3")
        # verify that the resulting buildslave tree matches the developer's
        self.try_shouldMatch("main.c")
        self.try_shouldMatch("version.c")
        self.try_shouldMatch(os.path.join("subdir", "subdir.c"))


    def dumpPatch(self, patch):
        # this exists to help me figure out the right 'patchlevel' value
        # should be returned by tryclient.getSourceStamp
        n = self.mktemp()
        open(n,"w").write(patch)
        d = self.runCommand(".", ["lsdiff", n])
        def p(res): print "lsdiff:", res.strip().split("\n")
        d.addCallback(p)
        return d


    def tearDown(self):
        d = defer.succeed(None)
        if self.slave:
            d2 = self.master.botmaster.waitUntilBuilderDetached("vc")
            d.addCallback(lambda res: self.slave.stopService())
            d.addCallback(lambda res: d2)
        if self.master:
            d.addCallback(lambda res: self.master.stopService())
        if self.httpServer:
            d.addCallback(lambda res: self.httpServer.stopListening())
            def stopHTTPTimer():
                try:
                    from twisted.web import http # Twisted-2.0
                except ImportError:
                    from twisted.protocols import http # Twisted-1.3
                http._logDateTimeStop() # shut down the internal timer. DUMB!
            d.addCallback(lambda res: stopHTTPTimer())
        d.addCallback(lambda res: self.tearDown2())
        return maybeWait(d)

    def tearDown2(self):
        pass

class CVSHelper(BaseHelper):
    branchname = "branch"
    try_branchname = "branch"

    def capable(self):
        cvspaths = which('cvs')
        if not cvspaths:
            return (False, "CVS is not installed")
        # cvs-1.10 (as shipped with OS-X 10.3 "Panther") is too old for this
        # test. There is a situation where we check out a tree, make a
        # change, then commit it back, and CVS refuses to believe that we're
        # operating in a CVS tree. I tested cvs-1.12.9 and it works ok, OS-X
        # 10.4 "Tiger" comes with cvs-1.11, but I haven't tested that yet.
        # For now, skip the tests if we've got 1.10 .
        log.msg("running %s --version.." % (cvspaths[0],))
        d = utils.getProcessOutput(cvspaths[0], ["--version"],
                                   env=os.environ)
        d.addCallback(self._capable, cvspaths[0])
        return d

    def _capable(self, v, vcexe):
        m = re.search(r'\(CVS\) ([\d\.]+) ', v)
        if not m:
            log.msg("couldn't identify CVS version number in output:")
            log.msg("'''%s'''" % v)
            log.msg("skipping tests")
            return (False, "Found CVS but couldn't identify its version")
        ver = m.group(1)
        log.msg("found CVS version '%s'" % ver)
        if ver == "1.10":
            return (False, "Found CVS, but it is too old")
        self.vcexe = vcexe
        return (True, None)

    def getdate(self):
        # this timestamp is eventually passed to CVS in a -D argument, and
        # strftime's %z specifier doesn't seem to work reliably (I get +0000
        # where I should get +0700 under linux sometimes, and windows seems
        # to want to put a verbose 'Eastern Standard Time' in there), so
        # leave off the timezone specifier and treat this as localtime. A
        # valid alternative would be to use a hard-coded +0000 and
        # time.gmtime().
        return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())

    def createRepository(self):
        self.createBasedir()
        self.cvsrep = cvsrep = os.path.join(self.repbase, "CVS-Repository")
        tmp = os.path.join(self.repbase, "cvstmp")

        w = self.dovc(self.repbase, "-d %s init" % cvsrep)
        yield w; w.getResult() # we must getResult() to raise any exceptions

        self.populate(tmp)
        cmd = ("-d %s import" % cvsrep +
               " -m sample_project_files sample vendortag start")
        w = self.dovc(tmp, cmd)
        yield w; w.getResult()
        rmdirRecursive(tmp)
        # take a timestamp as the first revision number
        time.sleep(2)
        self.addTrunkRev(self.getdate())
        time.sleep(2)

        w = self.dovc(self.repbase,
                      "-d %s checkout -d cvstmp sample" % self.cvsrep)
        yield w; w.getResult()

        w = self.dovc(tmp, "tag -b %s" % self.branchname)
        yield w; w.getResult()
        self.populate_branch(tmp)
        w = self.dovc(tmp,
                      "commit -m commit_on_branch -r %s" % self.branchname)
        yield w; w.getResult()
        rmdirRecursive(tmp)
        time.sleep(2)
        self.addBranchRev(self.getdate())
        time.sleep(2)
        self.vcargs = { 'cvsroot': self.cvsrep, 'cvsmodule': "sample" }
    createRepository = deferredGenerator(createRepository)


    def vc_revise(self):
        tmp = os.path.join(self.repbase, "cvstmp")

        w = self.dovc(self.repbase,
                      "-d %s checkout -d cvstmp sample" % self.cvsrep)
        yield w; w.getResult()
        self.version += 1
        version_c = VERSION_C % self.version
        open(os.path.join(tmp, "version.c"), "w").write(version_c)
        w = self.dovc(tmp,
                      "commit -m revised_to_%d version.c" % self.version)
        yield w; w.getResult()
        rmdirRecursive(tmp)
        time.sleep(2)
        self.addTrunkRev(self.getdate())
        time.sleep(2)
    vc_revise = deferredGenerator(vc_revise)

    def vc_try_checkout(self, workdir, rev, branch=None):
        # 'workdir' is an absolute path
        assert os.path.abspath(workdir) == workdir
        cmd = [self.vcexe, "-d", self.cvsrep, "checkout",
               "-d", workdir,
               "-D", rev]
        if branch is not None:
            cmd.append("-r")
            cmd.append(branch)
        cmd.append("sample")
        w = self.do(self.repbase, cmd)
        yield w; w.getResult()
        open(os.path.join(workdir, "subdir", "subdir.c"), "w").write(TRY_C)
    vc_try_checkout = deferredGenerator(vc_try_checkout)

    def vc_try_finish(self, workdir):
        rmdirRecursive(workdir)

class CVS(VCBase, unittest.TestCase):
    vc_name = "cvs"

    metadir = "CVS"
    vctype = "source.CVS"
    vctype_try = "cvs"
    # CVS gives us got_revision, but it is based entirely upon the local
    # clock, which means it is unlikely to match the timestamp taken earlier.
    # This might be enough for common use, but won't be good enough for our
    # tests to accept, so pretend it doesn't have got_revision at all.
    has_got_revision = False

    def testCheckout(self):
        d = self.do_vctest()
        return maybeWait(d)

    def testPatch(self):
        d = self.do_patch()
        return maybeWait(d)

    def testCheckoutBranch(self):
        d = self.do_branch()
        return maybeWait(d)
        
    def testTry(self):
        d = self.do_getpatch(doBranch=False)
        return maybeWait(d)

VCS.registerVC(CVS.vc_name, CVSHelper())


class SVNHelper(BaseHelper):
    branchname = "sample/branch"
    try_branchname = "sample/branch"

    def capable(self):
        svnpaths = which('svn')
        svnadminpaths = which('svnadmin')
        if not svnpaths:
            return (False, "SVN is not installed")
        if not svnadminpaths:
            return (False, "svnadmin is not installed")
        # we need svn to be compiled with the ra_local access
        # module
        log.msg("running svn --version..")
        env = os.environ.copy()
        env['LC_ALL'] = "C"
        d = utils.getProcessOutput(svnpaths[0], ["--version"],
                                   env=env)
        d.addCallback(self._capable, svnpaths[0], svnadminpaths[0])
        return d

    def _capable(self, v, vcexe, svnadmin):
        if v.find("handles 'file' schem") != -1:
            # older versions say 'schema', 1.2.0 and beyond say 'scheme'
            self.vcexe = vcexe
            self.svnadmin = svnadmin
            return (True, None)
        excuse = ("%s found but it does not support 'file:' " +
                  "schema, skipping svn tests") % vcexe
        log.msg(excuse)
        return (False, excuse)

    def createRepository(self):
        self.createBasedir()
        self.svnrep = os.path.join(self.repbase,
                                   "SVN-Repository").replace('\\','/')
        tmp = os.path.join(self.repbase, "svntmp")
        if sys.platform == 'win32':
            # On Windows Paths do not start with a /
            self.svnurl = "file:///%s" % self.svnrep
        else:
            self.svnurl = "file://%s" % self.svnrep
        self.svnurl_trunk = self.svnurl + "/sample/trunk"
        self.svnurl_branch = self.svnurl + "/sample/branch"

        w = self.do(self.repbase, self.svnadmin+" create %s" % self.svnrep)
        yield w; w.getResult()

        self.populate(tmp)
        w = self.dovc(tmp,
                      "import -m sample_project_files %s" %
                      self.svnurl_trunk)
        yield w; out = w.getResult()
        rmdirRecursive(tmp)
        m = re.search(r'Committed revision (\d+)\.', out)
        assert m.group(1) == "1" # first revision is always "1"
        self.addTrunkRev(int(m.group(1)))

        w = self.dovc(self.repbase,
                      "checkout %s svntmp" % self.svnurl_trunk)
        yield w; w.getResult()

        w = self.dovc(tmp, "cp -m make_branch %s %s" % (self.svnurl_trunk,
                                                        self.svnurl_branch))
        yield w; w.getResult()
        w = self.dovc(tmp, "switch %s" % self.svnurl_branch)
        yield w; w.getResult()
        self.populate_branch(tmp)
        w = self.dovc(tmp, "commit -m commit_on_branch")
        yield w; out = w.getResult()
        rmdirRecursive(tmp)
        m = re.search(r'Committed revision (\d+)\.', out)
        self.addBranchRev(int(m.group(1)))
    createRepository = deferredGenerator(createRepository)

    def vc_revise(self):
        tmp = os.path.join(self.repbase, "svntmp")
        rmdirRecursive(tmp)
        log.msg("vc_revise" +  self.svnurl_trunk)
        w = self.dovc(self.repbase,
                      "checkout %s svntmp" % self.svnurl_trunk)
        yield w; w.getResult()
        self.version += 1
        version_c = VERSION_C % self.version
        open(os.path.join(tmp, "version.c"), "w").write(version_c)
        w = self.dovc(tmp, "commit -m revised_to_%d" % self.version)
        yield w; out = w.getResult()
        m = re.search(r'Committed revision (\d+)\.', out)
        self.addTrunkRev(int(m.group(1)))
        rmdirRecursive(tmp)
    vc_revise = deferredGenerator(vc_revise)

    def vc_try_checkout(self, workdir, rev, branch=None):
        assert os.path.abspath(workdir) == workdir
        if os.path.exists(workdir):
            rmdirRecursive(workdir)
        if not branch:
            svnurl = self.svnurl_trunk
        else:
            # N.B.: this is *not* os.path.join: SVN URLs use slashes
            # regardless of the host operating system's filepath separator
            svnurl = self.svnurl + "/" + branch
        w = self.dovc(self.repbase,
                      "checkout %s %s" % (svnurl, workdir))
        yield w; w.getResult()
        open(os.path.join(workdir, "subdir", "subdir.c"), "w").write(TRY_C)
    vc_try_checkout = deferredGenerator(vc_try_checkout)

    def vc_try_finish(self, workdir):
        rmdirRecursive(workdir)


class SVN(VCBase, unittest.TestCase):
    vc_name = "svn"

    metadir = ".svn"
    vctype = "source.SVN"
    vctype_try = "svn"
    has_got_revision = True
    has_got_revision_branches_are_merged = True

    def testCheckout(self):
        # we verify this one with the svnurl style of vcargs. We test the
        # baseURL/defaultBranch style in testPatch and testCheckoutBranch.
        self.helper.vcargs = { 'svnurl': self.helper.svnurl_trunk }
        d = self.do_vctest()
        return maybeWait(d)

    def testPatch(self):
        self.helper.vcargs = { 'baseURL': self.helper.svnurl + "/",
                               'defaultBranch': "sample/trunk",
                               }
        d = self.do_patch()
        return maybeWait(d)

    def testCheckoutBranch(self):
        self.helper.vcargs = { 'baseURL': self.helper.svnurl + "/",
                               'defaultBranch': "sample/trunk",
                               }
        d = self.do_branch()
        return maybeWait(d)

    def testTry(self):
        # extract the base revision and patch from a modified tree, use it to
        # create the same contents on the buildslave
        self.helper.vcargs = { 'baseURL': self.helper.svnurl + "/",
                               'defaultBranch': "sample/trunk",
                               }
        d = self.do_getpatch()
        return maybeWait(d)

VCS.registerVC(SVN.vc_name, SVNHelper())


class P4Helper(BaseHelper):
    branchname = "branch"
    p4port = 'localhost:1666'
    pid = None
    base_descr = 'Change: new\nDescription: asdf\nFiles:\n'

    def capable(self):
        p4paths = which('p4')
        p4dpaths = which('p4d')
        if not p4paths:
            return (False, "p4 is not installed")
        if not p4dpaths:
            return (False, "p4d is not installed")
        self.vcexe = p4paths[0]
        self.p4dexe = p4dpaths[0]
        return (True, None)

    class _P4DProtocol(protocol.ProcessProtocol):
        def __init__(self):
            self.started = defer.Deferred()
            self.ended = defer.Deferred()

        def outReceived(self, data):
            # When it says starting, it has bound to the socket.
            if self.started:
                if data.startswith('Perforce Server starting...'):
                    self.started.callback(None)
                else:
                    print "p4d said %r" % data
                    try:
                        raise Exception('p4d said %r' % data)
                    except:
                        self.started.errback(failure.Failure())
                self.started = None

        def errReceived(self, data):
            print "p4d stderr: %s" % data

        def processEnded(self, status_object):
            if status_object.check(error.ProcessDone):
                self.ended.callback(None)
            else:
                self.ended.errback(status_object)

    def _start_p4d(self):
        proto = self._P4DProtocol()
        reactor.spawnProcess(proto, self.p4dexe, ['p4d', '-p', self.p4port],
                             env=os.environ, path=self.p4rep)
        return proto.started, proto.ended

    def dop4(self, basedir, command, failureIsOk=False, stdin=None):
        # p4 looks at $PWD instead of getcwd(), which causes confusion when
        # we spawn commands without an intervening shell (sh -c). We can
        # override this with a -d argument.
        command = "-p %s -d %s %s" % (self.p4port, basedir, command)
        return self.dovc(basedir, command, failureIsOk, stdin)

    def createRepository(self):
        # this is only called once per VC system, so start p4d here.

        self.createBasedir()
        tmp = os.path.join(self.repbase, "p4tmp")
        self.p4rep = os.path.join(self.repbase, 'P4-Repository')
        os.mkdir(self.p4rep)

        # Launch p4d.
        started, self.p4d_shutdown = self._start_p4d()
        w = waitForDeferred(started)
        yield w; w.getResult()

        # Create client spec.
        os.mkdir(tmp)
        clispec = 'Client: creator\n'
        clispec += 'Root: %s\n' % tmp
        clispec += 'View:\n'
        clispec += '\t//depot/... //creator/...\n'
        w = self.dop4(tmp, 'client -i', stdin=clispec)
        yield w; w.getResult()

        # Create first rev (trunk).
        self.populate(os.path.join(tmp, 'trunk'))
        files = ['main.c', 'version.c', 'subdir/subdir.c']
        w = self.dop4(tmp, "-c creator add "
                      + " ".join(['trunk/%s' % f for f in files]))
        yield w; w.getResult()
        descr = self.base_descr
        for file in files:
            descr += '\t//depot/trunk/%s\n' % file
        w = self.dop4(tmp, "-c creator submit -i", stdin=descr)
        yield w; out = w.getResult()
        m = re.search(r'Change (\d+) submitted.', out)
        assert m.group(1) == '1'
        self.addTrunkRev(m.group(1))

        # Create second rev (branch).
        w = self.dop4(tmp, '-c creator integrate '
                      + '//depot/trunk/... //depot/branch/...')
        yield w; w.getResult()
        w = self.dop4(tmp, "-c creator edit branch/main.c")
        yield w; w.getResult()
        self.populate_branch(os.path.join(tmp, 'branch'))
        descr = self.base_descr
        for file in files:
            descr += '\t//depot/branch/%s\n' % file
        w = self.dop4(tmp, "-c creator submit -i", stdin=descr)
        yield w; out = w.getResult()
        m = re.search(r'Change (\d+) submitted.', out)
        self.addBranchRev(m.group(1))
    createRepository = deferredGenerator(createRepository)

    def vc_revise(self):
        tmp = os.path.join(self.repbase, "p4tmp")
        self.version += 1
        version_c = VERSION_C % self.version
        w = self.dop4(tmp, '-c creator edit trunk/version.c')
        yield w; w.getResult()
        open(os.path.join(tmp, "trunk/version.c"), "w").write(version_c)
        descr = self.base_descr + '\t//depot/trunk/version.c\n'
        w = self.dop4(tmp, "-c creator submit -i", stdin=descr)
        yield w; out = w.getResult()
        m = re.search(r'Change (\d+) submitted.', out)
        self.addTrunkRev(m.group(1))
    vc_revise = deferredGenerator(vc_revise)

    def shutdown_p4d(self):
        d = self.runCommand(self.repbase, '%s -p %s admin stop'
                            % (self.vcexe, self.p4port))
        return d.addCallback(lambda _: self.p4d_shutdown)

class P4(VCBase, unittest.TestCase):
    metadir = None
    vctype = "source.P4"
    vc_name = "p4"

    def tearDownClass(self):
        if self.helper:
            return maybeWait(self.helper.shutdown_p4d())

    def testCheckout(self):
        self.helper.vcargs = { 'p4port': self.helper.p4port,
                               'p4base': '//depot/',
                               'defaultBranch': 'trunk' }
        d = self.do_vctest(testRetry=False)
        # TODO: like arch and darcs, sync does nothing when server is not
        # changed.
        return maybeWait(d)

    def testCheckoutBranch(self):
        self.helper.vcargs = { 'p4port': self.helper.p4port,
                               'p4base': '//depot/',
                               'defaultBranch': 'trunk' }
        d = self.do_branch()
        return maybeWait(d)

    def testPatch(self):
        self.helper.vcargs = { 'p4port': self.helper.p4port,
                               'p4base': '//depot/',
                               'defaultBranch': 'trunk' }
        d = self.do_patch()
        return maybeWait(d)

VCS.registerVC(P4.vc_name, P4Helper())


class DarcsHelper(BaseHelper):
    branchname = "branch"
    try_branchname = "branch"

    def capable(self):
        darcspaths = which('darcs')
        if not darcspaths:
            return (False, "Darcs is not installed")
        self.vcexe = darcspaths[0]
        return (True, None)

    def createRepository(self):
        self.createBasedir()
        self.darcs_base = os.path.join(self.repbase, "Darcs-Repository")
        self.rep_trunk = os.path.join(self.darcs_base, "trunk")
        self.rep_branch = os.path.join(self.darcs_base, "branch")
        tmp = os.path.join(self.repbase, "darcstmp")

        os.makedirs(self.rep_trunk)
        w = self.dovc(self.rep_trunk, ["initialize"])
        yield w; w.getResult()
        os.makedirs(self.rep_branch)
        w = self.dovc(self.rep_branch, ["initialize"])
        yield w; w.getResult()

        self.populate(tmp)
        w = self.dovc(tmp, qw("initialize"))
        yield w; w.getResult()
        w = self.dovc(tmp, qw("add -r ."))
        yield w; w.getResult()
        w = self.dovc(tmp, qw("record -a -m initial_import --skip-long-comment -A test@buildbot.sf.net"))
        yield w; w.getResult()
        w = self.dovc(tmp, ["push", "-a", self.rep_trunk])
        yield w; w.getResult()
        w = self.dovc(tmp, qw("changes --context"))
        yield w; out = w.getResult()
        self.addTrunkRev(out)

        self.populate_branch(tmp)
        w = self.dovc(tmp, qw("record -a --ignore-times -m commit_on_branch --skip-long-comment -A test@buildbot.sf.net"))
        yield w; w.getResult()
        w = self.dovc(tmp, ["push", "-a", self.rep_branch])
        yield w; w.getResult()
        w = self.dovc(tmp, qw("changes --context"))
        yield w; out = w.getResult()
        self.addBranchRev(out)
        rmdirRecursive(tmp)
    createRepository = deferredGenerator(createRepository)

    def vc_revise(self):
        tmp = os.path.join(self.repbase, "darcstmp")
        os.makedirs(tmp)
        w = self.dovc(tmp, qw("initialize"))
        yield w; w.getResult()
        w = self.dovc(tmp, ["pull", "-a", self.rep_trunk])
        yield w; w.getResult()

        self.version += 1
        version_c = VERSION_C % self.version
        open(os.path.join(tmp, "version.c"), "w").write(version_c)
        w = self.dovc(tmp, qw("record -a --ignore-times -m revised_to_%d --skip-long-comment -A test@buildbot.sf.net" % self.version))
        yield w; w.getResult()
        w = self.dovc(tmp, ["push", "-a", self.rep_trunk])
        yield w; w.getResult()
        w = self.dovc(tmp, qw("changes --context"))
        yield w; out = w.getResult()
        self.addTrunkRev(out)
        rmdirRecursive(tmp)
    vc_revise = deferredGenerator(vc_revise)

    def vc_try_checkout(self, workdir, rev, branch=None):
        assert os.path.abspath(workdir) == workdir
        if os.path.exists(workdir):
            rmdirRecursive(workdir)
        os.makedirs(workdir)
        w = self.dovc(workdir, qw("initialize"))
        yield w; w.getResult()
        if not branch:
            rep = self.rep_trunk
        else:
            rep = os.path.join(self.darcs_base, branch)
        w = self.dovc(workdir, ["pull", "-a", rep])
        yield w; w.getResult()
        open(os.path.join(workdir, "subdir", "subdir.c"), "w").write(TRY_C)
    vc_try_checkout = deferredGenerator(vc_try_checkout)

    def vc_try_finish(self, workdir):
        rmdirRecursive(workdir)


class Darcs(VCBase, unittest.TestCase):
    vc_name = "darcs"

    # Darcs has a metadir="_darcs", but it does not have an 'export'
    # mode
    metadir = None
    vctype = "source.Darcs"
    vctype_try = "darcs"
    has_got_revision = True

    def testCheckout(self):
        self.helper.vcargs = { 'repourl': self.helper.rep_trunk }
        d = self.do_vctest(testRetry=False)

        # TODO: testRetry has the same problem with Darcs as it does for
        # Arch
        return maybeWait(d)

    def testPatch(self):
        self.helper.vcargs = { 'baseURL': self.helper.darcs_base + "/",
                               'defaultBranch': "trunk" }
        d = self.do_patch()
        return maybeWait(d)

    def testCheckoutBranch(self):
        self.helper.vcargs = { 'baseURL': self.helper.darcs_base + "/",
                               'defaultBranch': "trunk" }
        d = self.do_branch()
        return maybeWait(d)

    def testCheckoutHTTP(self):
        self.serveHTTP()
        repourl = "http://localhost:%d/Darcs-Repository/trunk" % self.httpPort
        self.helper.vcargs =  { 'repourl': repourl }
        d = self.do_vctest(testRetry=False)
        return maybeWait(d)
        
    def testTry(self):
        self.helper.vcargs = { 'baseURL': self.helper.darcs_base + "/",
                               'defaultBranch': "trunk" }
        d = self.do_getpatch()
        return maybeWait(d)

VCS.registerVC(Darcs.vc_name, DarcsHelper())


class ArchCommon:
    def registerRepository(self, coordinates):
        a = self.archname
        w = self.dovc(self.repbase, "archives %s" % a)
        yield w; out = w.getResult()
        if out:
            w = self.dovc(self.repbase, "register-archive -d %s" % a)
            yield w; w.getResult()
        w = self.dovc(self.repbase, "register-archive %s" % coordinates)
        yield w; w.getResult()
    registerRepository = deferredGenerator(registerRepository)

    def unregisterRepository(self):
        a = self.archname
        w = self.dovc(self.repbase, "archives %s" % a)
        yield w; out = w.getResult()
        if out:
            w = self.dovc(self.repbase, "register-archive -d %s" % a)
            yield w; out = w.getResult()
    unregisterRepository = deferredGenerator(unregisterRepository)

class TlaHelper(BaseHelper, ArchCommon):
    defaultbranch = "testvc--mainline--1"
    branchname = "testvc--branch--1"
    try_branchname = None # TlaExtractor can figure it out by itself
    archcmd = "tla"

    def capable(self):
        tlapaths = which('tla')
        if not tlapaths:
            return (False, "Arch (tla) is not installed")
        self.vcexe = tlapaths[0]
        return (True, None)

    def do_get(self, basedir, archive, branch, newdir):
        # the 'get' syntax is different between tla and baz. baz, while
        # claiming to honor an --archive argument, in fact ignores it. The
        # correct invocation is 'baz get archive/revision newdir'.
        if self.archcmd == "tla":
            w = self.dovc(basedir,
                          "get -A %s %s %s" % (archive, branch, newdir))
        else:
            w = self.dovc(basedir,
                          "get %s/%s %s" % (archive, branch, newdir))
        return w

    def createRepository(self):
        self.createBasedir()
        # first check to see if bazaar is around, since we'll need to know
        # later
        d = VCS.capable(Bazaar.vc_name)
        d.addCallback(self._createRepository_1)
        return d

    def _createRepository_1(self, res):
        has_baz = res[0]

        # pick a hopefully unique string for the archive name, in the form
        # test-%d@buildbot.sf.net--testvc, since otherwise multiple copies of
        # the unit tests run in the same user account will collide (since the
        # archive names are kept in the per-user ~/.arch-params/ directory).
        pid = os.getpid()
        self.archname = "test-%s-%d@buildbot.sf.net--testvc" % (self.archcmd,
                                                                pid)
        trunk = self.defaultbranch
        branch = self.branchname

        repword = self.archcmd.capitalize()
        self.archrep = os.path.join(self.repbase, "%s-Repository" % repword)
        tmp = os.path.join(self.repbase, "archtmp")
        a = self.archname

        self.populate(tmp)

        w = self.dovc(tmp, "my-id", failureIsOk=True)
        yield w; res = w.getResult()
        if not res:
            # tla will fail a lot of operations if you have not set an ID
            w = self.do(tmp, [self.vcexe, "my-id",
                              "Buildbot Test Suite <test@buildbot.sf.net>"])
            yield w; w.getResult()

        if has_baz:
            # bazaar keeps a cache of revisions, but this test creates a new
            # archive each time it is run, so the cache causes errors.
            # Disable the cache to avoid these problems. This will be
            # slightly annoying for people who run the buildbot tests under
            # the same UID as one which uses baz on a regular basis, but
            # bazaar doesn't give us a way to disable the cache just for this
            # one archive.
            cmd = "%s cache-config --disable" % VCS.getHelper('bazaar').vcexe
            w = self.do(tmp, cmd)
            yield w; w.getResult()

        w = waitForDeferred(self.unregisterRepository())
        yield w; w.getResult()

        # these commands can be run in any directory
        w = self.dovc(tmp, "make-archive -l %s %s" % (a, self.archrep))
        yield w; w.getResult()
        if self.archcmd == "tla":
            w = self.dovc(tmp, "archive-setup -A %s %s" % (a, trunk))
            yield w; w.getResult()
            w = self.dovc(tmp, "archive-setup -A %s %s" % (a, branch))
            yield w; w.getResult()
        else:
            # baz does not require an 'archive-setup' step
            pass

        # these commands must be run in the directory that is to be imported
        w = self.dovc(tmp, "init-tree --nested %s/%s" % (a, trunk))
        yield w; w.getResult()
        files = " ".join(["main.c", "version.c", "subdir",
                          os.path.join("subdir", "subdir.c")])
        w = self.dovc(tmp, "add-id %s" % files)
        yield w; w.getResult()

        w = self.dovc(tmp, "import %s/%s" % (a, trunk))
        yield w; out = w.getResult()
        self.addTrunkRev("base-0")

        # create the branch
        if self.archcmd == "tla":
            branchstart = "%s--base-0" % trunk
            w = self.dovc(tmp, "tag -A %s %s %s" % (a, branchstart, branch))
            yield w; w.getResult()
        else:
            w = self.dovc(tmp, "branch %s" % branch)
            yield w; w.getResult()

        rmdirRecursive(tmp)

        # check out the branch
        w = self.do_get(self.repbase, a, branch, "archtmp")
        yield w; w.getResult()
        # and edit the file
        self.populate_branch(tmp)
        logfile = "++log.%s--%s" % (branch, a)
        logmsg = "Summary: commit on branch\nKeywords:\n\n"
        open(os.path.join(tmp, logfile), "w").write(logmsg)
        w = self.dovc(tmp, "commit")
        yield w; out = w.getResult()
        m = re.search(r'committed %s/%s--([\S]+)' % (a, branch),
                      out)
        assert (m.group(1) == "base-0" or m.group(1).startswith("patch-"))
        self.addBranchRev(m.group(1))

        w = waitForDeferred(self.unregisterRepository())
        yield w; w.getResult()
        rmdirRecursive(tmp)

        # we unregister the repository each time, because we might have
        # changed the coordinates (since we switch from a file: URL to an
        # http: URL for various tests). The buildslave code doesn't forcibly
        # unregister the archive, so we have to do it here.
        w = waitForDeferred(self.unregisterRepository())
        yield w; w.getResult()

    _createRepository_1 = deferredGenerator(_createRepository_1)

    def vc_revise(self):
        # the fix needs to be done in a workspace that is linked to a
        # read-write version of the archive (i.e., using file-based
        # coordinates instead of HTTP ones), so we re-register the repository
        # before we begin. We unregister it when we're done to make sure the
        # build will re-register the correct one for whichever test is
        # currently being run.

        # except, that source.Bazaar really doesn't like it when the archive
        # gets unregistered behind its back. The slave tries to do a 'baz
        # replay' in a tree with an archive that is no longer recognized, and
        # baz aborts with a botched invariant exception. This causes
        # mode=update to fall back to clobber+get, which flunks one of the
        # tests (the 'newfile' check in _do_vctest_update_3 fails)

        # to avoid this, we take heroic steps here to leave the archive
        # registration in the same state as we found it.

        tmp = os.path.join(self.repbase, "archtmp")
        a = self.archname

        w = self.dovc(self.repbase, "archives %s" % a)
        yield w; out = w.getResult()
        assert out
        lines = out.split("\n")
        coordinates = lines[1].strip()

        # now register the read-write location
        w = waitForDeferred(self.registerRepository(self.archrep))
        yield w; w.getResult()

        trunk = self.defaultbranch

        w = self.do_get(self.repbase, a, trunk, "archtmp")
        yield w; w.getResult()

        # tla appears to use timestamps to determine which files have
        # changed, so wait long enough for the new file to have a different
        # timestamp
        time.sleep(2)
        self.version += 1
        version_c = VERSION_C % self.version
        open(os.path.join(tmp, "version.c"), "w").write(version_c)

        logfile = "++log.%s--%s" % (trunk, a)
        logmsg = "Summary: revised_to_%d\nKeywords:\n\n" % self.version
        open(os.path.join(tmp, logfile), "w").write(logmsg)
        w = self.dovc(tmp, "commit")
        yield w; out = w.getResult()
        m = re.search(r'committed %s/%s--([\S]+)' % (a, trunk),
                      out)
        assert (m.group(1) == "base-0" or m.group(1).startswith("patch-"))
        self.addTrunkRev(m.group(1))

        # now re-register the original coordinates
        w = waitForDeferred(self.registerRepository(coordinates))
        yield w; w.getResult()
        rmdirRecursive(tmp)
    vc_revise = deferredGenerator(vc_revise)

    def vc_try_checkout(self, workdir, rev, branch=None):
        assert os.path.abspath(workdir) == workdir
        if os.path.exists(workdir):
            rmdirRecursive(workdir)

        a = self.archname

        # register the read-write location, if it wasn't already registered
        w = waitForDeferred(self.registerRepository(self.archrep))
        yield w; w.getResult()

        w = self.do_get(self.repbase, a, "testvc--mainline--1", workdir)
        yield w; w.getResult()

        # timestamps. ick.
        time.sleep(2)
        open(os.path.join(workdir, "subdir", "subdir.c"), "w").write(TRY_C)
    vc_try_checkout = deferredGenerator(vc_try_checkout)

    def vc_try_finish(self, workdir):
        rmdirRecursive(workdir)

class Arch(VCBase, unittest.TestCase):
    vc_name = "tla"

    metadir = None
    # Arch has a metadir="{arch}", but it does not have an 'export' mode.
    vctype = "source.Arch"
    vctype_try = "tla"
    has_got_revision = True

    def testCheckout(self):
        # these are the coordinates of the read-write archive used by all the
        # non-HTTP tests. testCheckoutHTTP overrides these.
        self.helper.vcargs = {'url': self.helper.archrep,
                              'version': self.helper.defaultbranch }
        d = self.do_vctest(testRetry=False)
        # the current testRetry=True logic doesn't have the desired effect:
        # "update" is a no-op because arch knows that the repository hasn't
        # changed. Other VC systems will re-checkout missing files on
        # update, arch just leaves the tree untouched. TODO: come up with
        # some better test logic, probably involving a copy of the
        # repository that has a few changes checked in.

        return maybeWait(d)

    def testCheckoutHTTP(self):
        self.serveHTTP()
        url = "http://localhost:%d/Tla-Repository" % self.httpPort
        self.helper.vcargs = { 'url': url,
                               'version': "testvc--mainline--1" }
        d = self.do_vctest(testRetry=False)
        return maybeWait(d)

    def testPatch(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              'version': self.helper.defaultbranch }
        d = self.do_patch()
        return maybeWait(d)

    def testCheckoutBranch(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              'version': self.helper.defaultbranch }
        d = self.do_branch()
        return maybeWait(d)

    def testTry(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              'version': self.helper.defaultbranch }
        d = self.do_getpatch()
        return maybeWait(d)

VCS.registerVC(Arch.vc_name, TlaHelper())


class BazaarHelper(TlaHelper):
    archcmd = "baz"

    def capable(self):
        bazpaths = which('baz')
        if not bazpaths:
            return (False, "Arch (baz) is not installed")
        self.vcexe = bazpaths[0]
        return (True, None)

    def setUp2(self, res):
        # we unregister the repository each time, because we might have
        # changed the coordinates (since we switch from a file: URL to an
        # http: URL for various tests). The buildslave code doesn't forcibly
        # unregister the archive, so we have to do it here.
        d = self.unregisterRepository()
        return d


class Bazaar(Arch):
    vc_name = "bazaar"

    vctype = "source.Bazaar"
    vctype_try = "baz"
    has_got_revision = True

    fixtimer = None

    def testCheckout(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              # Baz adds the required 'archive' argument
                              'archive': self.helper.archname,
                              'version': self.helper.defaultbranch,
                              }
        d = self.do_vctest(testRetry=False)
        # the current testRetry=True logic doesn't have the desired effect:
        # "update" is a no-op because arch knows that the repository hasn't
        # changed. Other VC systems will re-checkout missing files on
        # update, arch just leaves the tree untouched. TODO: come up with
        # some better test logic, probably involving a copy of the
        # repository that has a few changes checked in.

        return maybeWait(d)

    def testCheckoutHTTP(self):
        self.serveHTTP()
        url = "http://localhost:%d/Baz-Repository" % self.httpPort
        self.helper.vcargs = { 'url': url,
                               'archive': self.helper.archname,
                               'version': self.helper.defaultbranch,
                               }
        d = self.do_vctest(testRetry=False)
        return maybeWait(d)

    def testPatch(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              # Baz adds the required 'archive' argument
                              'archive': self.helper.archname,
                              'version': self.helper.defaultbranch,
                              }
        d = self.do_patch()
        return maybeWait(d)

    def testCheckoutBranch(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              # Baz adds the required 'archive' argument
                              'archive': self.helper.archname,
                              'version': self.helper.defaultbranch,
                              }
        d = self.do_branch()
        return maybeWait(d)

    def testTry(self):
        self.helper.vcargs = {'url': self.helper.archrep,
                              # Baz adds the required 'archive' argument
                              'archive': self.helper.archname,
                              'version': self.helper.defaultbranch,
                              }
        d = self.do_getpatch()
        return maybeWait(d)

    def fixRepository(self):
        self.fixtimer = None
        self.site.resource = self.root

    def testRetry(self):
        # we want to verify that source.Source(retry=) works, and the easiest
        # way to make VC updates break (temporarily) is to break the HTTP
        # server that's providing the repository. Anything else pretty much
        # requires mutating the (read-only) BUILDBOT_TEST_VC repository, or
        # modifying the buildslave's checkout command while it's running.

        # this test takes a while to run, so don't bother doing it with
        # anything other than baz

        self.serveHTTP()

        # break the repository server
        from twisted.web import static
        self.site.resource = static.Data("Sorry, repository is offline",
                                         "text/plain")
        # and arrange to fix it again in 5 seconds, while the test is
        # running.
        self.fixtimer = reactor.callLater(5, self.fixRepository)
        
        url = "http://localhost:%d/Baz-Repository" % self.httpPort
        self.helper.vcargs = { 'url': url,
                               'archive': self.helper.archname,
                               'version': self.helper.defaultbranch,
                               'retry': (5.0, 4),
                               }
        d = self.do_vctest_once(True)
        d.addCallback(self._testRetry_1)
        return maybeWait(d)
    def _testRetry_1(self, bs):
        # make sure there was mention of the retry attempt in the logs
        l = bs.getLogs()[0]
        self.failUnlessIn("unable to access URL", l.getText(),
                          "funny, VC operation didn't fail at least once")
        self.failUnlessIn("update failed, trying 4 more times after 5 seconds",
                          l.getTextWithHeaders(),
                          "funny, VC operation wasn't reattempted")

    def testRetryFails(self):
        # make sure that the build eventually gives up on a repository which
        # is completely unavailable

        self.serveHTTP()

        # break the repository server, and leave it broken
        from twisted.web import static
        self.site.resource = static.Data("Sorry, repository is offline",
                                         "text/plain")

        url = "http://localhost:%d/Baz-Repository" % self.httpPort
        self.helper.vcargs = {'url': url,
                              'archive': self.helper.archname,
                              'version': self.helper.defaultbranch,
                              'retry': (0.5, 3),
                              }
        d = self.do_vctest_once(False)
        d.addCallback(self._testRetryFails_1)
        return maybeWait(d)
    def _testRetryFails_1(self, bs):
        self.failUnlessEqual(bs.getResults(), FAILURE)

    def tearDown2(self):
        if self.fixtimer:
            self.fixtimer.cancel()
        # tell tla to get rid of the leftover archive this test leaves in the
        # user's 'tla archives' listing. The name of this archive is provided
        # by the repository tarball, so the following command must use the
        # same name. We could use archive= to set it explicitly, but if you
        # change it from the default, then 'tla update' won't work.
        d = self.helper.unregisterRepository()
        return d

VCS.registerVC(Bazaar.vc_name, BazaarHelper())

class MercurialHelper(BaseHelper):
    branchname = "branch"
    try_branchname = "branch"

    def capable(self):
        hgpaths = which("hg")
        if not hgpaths:
            return (False, "Mercurial is not installed")
        self.vcexe = hgpaths[0]
        return (True, None)

    def extract_id(self, output):
        m = re.search(r'^(\w+)', output)
        return m.group(0)

    def createRepository(self):
        self.createBasedir()
        self.hg_base = os.path.join(self.repbase, "Mercurial-Repository")
        self.rep_trunk = os.path.join(self.hg_base, "trunk")
        self.rep_branch = os.path.join(self.hg_base, "branch")
        tmp = os.path.join(self.hg_base, "hgtmp")

        os.makedirs(self.rep_trunk)
        w = self.dovc(self.rep_trunk, "init")
        yield w; w.getResult()
        os.makedirs(self.rep_branch)
        w = self.dovc(self.rep_branch, "init")
        yield w; w.getResult()

        self.populate(tmp)
        w = self.dovc(tmp, "init")
        yield w; w.getResult()
        w = self.dovc(tmp, "add")
        yield w; w.getResult()
        w = self.dovc(tmp, "commit -m initial_import")
        yield w; w.getResult()
        w = self.dovc(tmp, "push %s" % self.rep_trunk)
        # note that hg-push does not actually update the working directory
        yield w; w.getResult()
        w = self.dovc(tmp, "identify")
        yield w; out = w.getResult()
        self.addTrunkRev(self.extract_id(out))

        self.populate_branch(tmp)
        w = self.dovc(tmp, "commit -m commit_on_branch")
        yield w; w.getResult()
        w = self.dovc(tmp, "push %s" % self.rep_branch)
        yield w; w.getResult()
        w = self.dovc(tmp, "identify")
        yield w; out = w.getResult()
        self.addBranchRev(self.extract_id(out))
        rmdirRecursive(tmp)
    createRepository = deferredGenerator(createRepository)

    def vc_revise(self):
        tmp = os.path.join(self.hg_base, "hgtmp2")
        w = self.dovc(self.hg_base, "clone %s %s" % (self.rep_trunk, tmp))
        yield w; w.getResult()

        self.version += 1
        version_c = VERSION_C % self.version
        version_c_filename = os.path.join(tmp, "version.c")
        open(version_c_filename, "w").write(version_c)
        # hg uses timestamps to distinguish files which have changed, so we
        # force the mtime forward a little bit
        future = time.time() + 2*self.version
        os.utime(version_c_filename, (future, future))
        w = self.dovc(tmp, "commit -m revised_to_%d" % self.version)
        yield w; w.getResult()
        w = self.dovc(tmp, "push %s" % self.rep_trunk)
        yield w; w.getResult()
        w = self.dovc(tmp, "identify")
        yield w; out = w.getResult()
        self.addTrunkRev(self.extract_id(out))
        rmdirRecursive(tmp)
    vc_revise = deferredGenerator(vc_revise)

    def vc_try_checkout(self, workdir, rev, branch=None):
        assert os.path.abspath(workdir) == workdir
        if os.path.exists(workdir):
            rmdirRecursive(workdir)
        if branch:
            src = self.rep_branch
        else:
            src = self.rep_trunk
        w = self.dovc(self.hg_base, "clone %s %s" % (src, workdir))
        yield w; w.getResult()
        try_c_filename = os.path.join(workdir, "subdir", "subdir.c")
        open(try_c_filename, "w").write(TRY_C)
        future = time.time() + 2*self.version
        os.utime(try_c_filename, (future, future))
    vc_try_checkout = deferredGenerator(vc_try_checkout)

    def vc_try_finish(self, workdir):
        rmdirRecursive(workdir)


class Mercurial(VCBase, unittest.TestCase):
    vc_name = "hg"

    # Mercurial has a metadir=".hg", but it does not have an 'export' mode.
    metadir = None
    vctype = "source.Mercurial"
    vctype_try = "hg"
    has_got_revision = True

    def testCheckout(self):
        self.helper.vcargs = { 'repourl': self.helper.rep_trunk }
        d = self.do_vctest(testRetry=False)

        # TODO: testRetry has the same problem with Mercurial as it does for
        # Arch
        return maybeWait(d)

    def testPatch(self):
        self.helper.vcargs = { 'baseURL': self.helper.hg_base + "/",
                               'defaultBranch': "trunk" }
        d = self.do_patch()
        return maybeWait(d)

    def testCheckoutBranch(self):
        self.helper.vcargs = { 'baseURL': self.helper.hg_base + "/",
                               'defaultBranch': "trunk" }
        d = self.do_branch()
        return maybeWait(d)

    def testCheckoutHTTP(self):
        self.serveHTTP()
        repourl = "http://localhost:%d/Mercurial-Repository/trunk/.hg" % self.httpPort
        self.helper.vcargs =  { 'repourl': repourl }
        d = self.do_vctest(testRetry=False)
        return maybeWait(d)
    # TODO: The easiest way to publish hg over HTTP is by running 'hg serve'
    # as a child process while the test is running. (you can also use a CGI
    # script, which sounds difficult, or you can publish the files directly,
    # which isn't well documented).
    testCheckoutHTTP.skip = "not yet implemented, use 'hg serve'"

    def testTry(self):
        self.helper.vcargs = { 'baseURL': self.helper.hg_base + "/",
                               'defaultBranch': "trunk" }
        d = self.do_getpatch()
        return maybeWait(d)

VCS.registerVC(Mercurial.vc_name, MercurialHelper())


class Sources(unittest.TestCase):
    # TODO: this needs serious rethink
    def makeChange(self, when=None, revision=None):
        if when:
            when = mktime_tz(parsedate_tz(when))
        return changes.Change("fred", [], "", when=when, revision=revision)

    def testCVS1(self):
        r = base.BuildRequest("forced build", SourceStamp())
        b = base.Build([r])
        s = source.CVS(cvsroot=None, cvsmodule=None, workdir=None, build=b)
        self.failUnlessEqual(s.computeSourceRevision(b.allChanges()), None)

    def testCVS2(self):
        c = []
        c.append(self.makeChange("Wed, 08 Sep 2004 09:00:00 -0700"))
        c.append(self.makeChange("Wed, 08 Sep 2004 09:01:00 -0700"))
        c.append(self.makeChange("Wed, 08 Sep 2004 09:02:00 -0700"))
        r = base.BuildRequest("forced", SourceStamp(changes=c))
        submitted = "Wed, 08 Sep 2004 09:04:00 -0700"
        r.submittedAt = mktime_tz(parsedate_tz(submitted))
        b = base.Build([r])
        s = source.CVS(cvsroot=None, cvsmodule=None, workdir=None, build=b)
        self.failUnlessEqual(s.computeSourceRevision(b.allChanges()),
                             "Wed, 08 Sep 2004 16:03:00 -0000")

    def testCVS3(self):
        c = []
        c.append(self.makeChange("Wed, 08 Sep 2004 09:00:00 -0700"))
        c.append(self.makeChange("Wed, 08 Sep 2004 09:01:00 -0700"))
        c.append(self.makeChange("Wed, 08 Sep 2004 09:02:00 -0700"))
        r = base.BuildRequest("forced", SourceStamp(changes=c))
        submitted = "Wed, 08 Sep 2004 09:04:00 -0700"
        r.submittedAt = mktime_tz(parsedate_tz(submitted))
        b = base.Build([r])
        s = source.CVS(cvsroot=None, cvsmodule=None, workdir=None, build=b,
                       checkoutDelay=10)
        self.failUnlessEqual(s.computeSourceRevision(b.allChanges()),
                             "Wed, 08 Sep 2004 16:02:10 -0000")

    def testCVS4(self):
        c = []
        c.append(self.makeChange("Wed, 08 Sep 2004 09:00:00 -0700"))
        c.append(self.makeChange("Wed, 08 Sep 2004 09:01:00 -0700"))
        c.append(self.makeChange("Wed, 08 Sep 2004 09:02:00 -0700"))
        r1 = base.BuildRequest("forced", SourceStamp(changes=c))
        submitted = "Wed, 08 Sep 2004 09:04:00 -0700"
        r1.submittedAt = mktime_tz(parsedate_tz(submitted))

        c = []
        c.append(self.makeChange("Wed, 08 Sep 2004 09:05:00 -0700"))
        r2 = base.BuildRequest("forced", SourceStamp(changes=c))
        submitted = "Wed, 08 Sep 2004 09:07:00 -0700"
        r2.submittedAt = mktime_tz(parsedate_tz(submitted))

        b = base.Build([r1, r2])
        s = source.CVS(cvsroot=None, cvsmodule=None, workdir=None, build=b)
        self.failUnlessEqual(s.computeSourceRevision(b.allChanges()),
                             "Wed, 08 Sep 2004 16:06:00 -0000")

    def testSVN1(self):
        r = base.BuildRequest("forced", SourceStamp())
        b = base.Build([r])
        s = source.SVN(svnurl="dummy", workdir=None, build=b)
        self.failUnlessEqual(s.computeSourceRevision(b.allChanges()), None)

    def testSVN2(self):
        c = []
        c.append(self.makeChange(revision=4))
        c.append(self.makeChange(revision=10))
        c.append(self.makeChange(revision=67))
        r = base.BuildRequest("forced", SourceStamp(changes=c))
        b = base.Build([r])
        s = source.SVN(svnurl="dummy", workdir=None, build=b)
        self.failUnlessEqual(s.computeSourceRevision(b.allChanges()), 67)

class Patch(VCBase, unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    def testPatch(self):
        # invoke 'patch' all by itself, to see if it works the way we think
        # it should. This is intended to ferret out some windows test
        # failures.
        helper = BaseHelper()
        self.workdir = os.path.join("test_vc", "testPatch")
        helper.populate(self.workdir)
        patch = which("patch")[0]

        command = [patch, "-p0"]
        class FakeBuilder:
            usePTY = False
            def sendUpdate(self, status):
                pass
        c = commands.ShellCommand(FakeBuilder(), command, self.workdir,
                                  sendRC=False, initialStdin=p0_diff)
        d = c.start()
        d.addCallback(self._testPatch_1)
        return maybeWait(d)

    def _testPatch_1(self, res):
        # make sure the file actually got patched
        subdir_c = os.path.join(self.workdir, "subdir", "subdir.c")
        data = open(subdir_c, "r").read()
        self.failUnlessIn("Hello patched subdir.\\n", data)
