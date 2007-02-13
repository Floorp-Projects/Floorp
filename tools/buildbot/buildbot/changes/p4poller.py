# -*- test-case-name: buildbot.test.test_p4poller -*-

# Many thanks to Dave Peticolas for contributing this module

import re
import time

from twisted.python import log, failure
from twisted.internet import defer, reactor
from twisted.internet.utils import getProcessOutput
from twisted.internet.task import LoopingCall

from buildbot import util
from buildbot.changes import base, changes

def get_simple_split(branchfile):
    """Splits the branchfile argument and assuming branch is 
       the first path component in branchfile, will return
       branch and file else None."""

    index = branchfile.find('/')
    if index == -1: return None, None
    branch, file = branchfile.split('/', 1)
    return branch, file

class P4Source(base.ChangeSource, util.ComparableMixin):
    """This source will poll a perforce repository for changes and submit
    them to the change master."""

    compare_attrs = ["p4port", "p4user", "p4passwd", "p4base",
                     "p4bin", "pollinterval", "histmax"]

    changes_line_re = re.compile(
            r"Change (?P<num>\d+) on \S+ by \S+@\S+ '.+'$")
    describe_header_re = re.compile(
            r"Change \d+ by (?P<who>\S+)@\S+ on (?P<when>.+)$")
    file_re = re.compile(r"^\.\.\. (?P<path>[^#]+)#\d+ \w+$")
    datefmt = '%Y/%m/%d %H:%M:%S'

    parent = None # filled in when we're added
    last_change = None
    loop = None
    working = False

    def __init__(self, p4port=None, p4user=None, p4passwd=None,
                 p4base='//', p4bin='p4',
                 split_file=lambda branchfile: (None, branchfile),
                 pollinterval=60 * 10, histmax=100):
        """
        @type  p4port:       string
        @param p4port:       p4 port definition (host:portno)
        @type  p4user:       string
        @param p4user:       p4 user
        @type  p4passwd:     string
        @param p4passwd:     p4 passwd
        @type  p4base:       string
        @param p4base:       p4 file specification to limit a poll to
                             without the trailing '...' (i.e., //)
        @type  p4bin:        string
        @param p4bin:        path to p4 binary, defaults to just 'p4'
        @type  split_file:   func
        $param split_file:   splits a filename into branch and filename.
        @type  pollinterval: int
        @param pollinterval: interval in seconds between polls
        @type  histmax:      int
        @param histmax:      maximum number of changes to look back through
        """

        self.p4port = p4port
        self.p4user = p4user
        self.p4passwd = p4passwd
        self.p4base = p4base
        self.p4bin = p4bin
        self.split_file = split_file
        self.pollinterval = pollinterval
        self.histmax = histmax
        self.loop = LoopingCall(self.checkp4)

    def startService(self):
        base.ChangeSource.startService(self)

        # Don't start the loop just yet because the reactor isn't running.
        # Give it a chance to go and install our SIGCHLD handler before
        # spawning processes.
        reactor.callLater(0, self.loop.start, self.pollinterval)

    def stopService(self):
        self.loop.stop()
        return base.ChangeSource.stopService(self)

    def describe(self):
        return "p4source %s %s" % (self.p4port, self.p4base)

    def checkp4(self):
        # Our return value is only used for unit testing.
        if self.working:
            log.msg("Skipping checkp4 because last one has not finished")
            return defer.succeed(None)
        else:
            self.working = True
            d = self._get_changes()
            d.addCallback(self._process_changes)
            d.addBoth(self._finished)
            return d

    def _finished(self, res):
        assert self.working
        self.working = False

        # Again, the return value is only for unit testing.
        # If there's a failure, log it so it isn't lost.
        if isinstance(res, failure.Failure):
            log.msg('P4 poll failed: %s' % res)
        return res

    def _get_changes(self):
        args = []
        if self.p4port:
            args.extend(['-p', self.p4port])
        if self.p4user:
            args.extend(['-u', self.p4user])
        if self.p4passwd:
            args.extend(['-P', self.p4passwd])
        args.extend(['changes', '-m', str(self.histmax), self.p4base + '...'])
        env = {}
        return getProcessOutput(self.p4bin, args, env)

    def _process_changes(self, result):
        last_change = self.last_change
        changelists = []
        for line in result.split('\n'):
            line = line.strip()
            if not line: continue
            m = self.changes_line_re.match(line)
            assert m, "Unexpected 'p4 changes' output: %r" % result
            num = m.group('num')
            if last_change is None:
                log.msg('P4Poller: starting at change %s' % num)
                self.last_change = num
                return []
            if last_change == num:
                break
            changelists.append(num)
        changelists.reverse() # oldest first

        # Retrieve each sequentially.
        d = defer.succeed(None)
        for c in changelists:
            d.addCallback(self._get_describe, c)
            d.addCallback(self._process_describe, c)
        return d

    def _get_describe(self, dummy, num):
        args = []
        if self.p4port:
            args.extend(['-p', self.p4port])
        if self.p4user:
            args.extend(['-u', self.p4user])
        if self.p4passwd:
            args.extend(['-P', self.p4passwd])
        args.extend(['describe', '-s', num])
        env = {}
        d = getProcessOutput(self.p4bin, args, env)
        return d

    def _process_describe(self, result, num):
        lines = result.split('\n')
        # SF#1555985: Wade Brainerd reports a stray ^M at the end of the date
        # field. The rstrip() is intended to remove that.
        lines[0] = lines[0].rstrip()
        m = self.describe_header_re.match(lines[0])
        assert m, "Unexpected 'p4 describe -s' result: %r" % result
        who = m.group('who')
        when = time.mktime(time.strptime(m.group('when'), self.datefmt))
        comments = ''
        while not lines[0].startswith('Affected files'):
            comments += lines.pop(0) + '\n'
        lines.pop(0) # affected files

        branch_files = {} # dict for branch mapped to file(s)
        while lines:
            line = lines.pop(0).strip()
            if not line: continue
            m = self.file_re.match(line)
            assert m, "Invalid file line: %r" % line
            path = m.group('path')
            if path.startswith(self.p4base):
                branch, file = self.split_file(path[len(self.p4base):])
                if (branch == None and file == None): continue
                if branch_files.has_key(branch):
                    branch_files[branch].append(file)
                else:
                    branch_files[branch] = [file]

        for branch in branch_files:
            c = changes.Change(who=who,
                               files=branch_files[branch],
                               comments=comments,
                               revision=num,
                               when=when,
                               branch=branch)
            self.parent.addChange(c)

        self.last_change = num
