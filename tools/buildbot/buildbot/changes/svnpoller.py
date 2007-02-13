# -*- test-case-name: buildbot.test.test_svnpoller -*-

# Based on the work of Dave Peticolas for the P4poll
# Changed to svn (using xml.dom.minidom) by Niklaus Giger
# Hacked beyond recognition by Brian Warner

import time

from twisted.python import log
from twisted.internet import defer, reactor, utils
from twisted.internet.task import LoopingCall

from buildbot import util
from buildbot.changes import base
from buildbot.changes.changes import Change

import xml.dom.minidom

def _assert(condition, msg):
    if condition:
        return True
    raise AssertionError(msg)

def dbgMsg(myString):
    log.msg(myString)
    return 1

# these split_file_* functions are available for use as values to the
# split_file= argument.
def split_file_alwaystrunk(path):
    return (None, path)

def split_file_branches(path):
    # turn trunk/subdir/file.c into (None, "subdir/file.c")
    # and branches/1.5.x/subdir/file.c into ("branches/1.5.x", "subdir/file.c")
    pieces = path.split('/')
    if pieces[0] == 'trunk':
        return (None, '/'.join(pieces[1:]))
    elif pieces[0] == 'branches':
        return ('/'.join(pieces[0:2]), '/'.join(pieces[2:]))
    else:
        return None


class SVNPoller(base.ChangeSource, util.ComparableMixin):
    """This source will poll a Subversion repository for changes and submit
    them to the change master."""

    compare_attrs = ["svnurl", "split_file_function",
                     "svnuser", "svnpasswd",
                     "pollinterval", "histmax",
                     "svnbin"]

    parent = None # filled in when we're added
    last_change = None
    loop = None
    working = False

    def __init__(self, svnurl, split_file=None,
                 svnuser=None, svnpasswd=None,
                 pollinterval=10*60, histmax=100,
                 svnbin='svn'):
        """
        @type  svnurl: string
        @param svnurl: the SVN URL that describes the repository and
                       subdirectory to watch. If this ChangeSource should
                       only pay attention to a single branch, this should
                       point at the repository for that branch, like
                       svn://svn.twistedmatrix.com/svn/Twisted/trunk . If it
                       should follow multiple branches, point it at the
                       repository directory that contains all the branches
                       like svn://svn.twistedmatrix.com/svn/Twisted and also
                       provide a branch-determining function.

                       Each file in the repository has a SVN URL in the form
                       (SVNURL)/(BRANCH)/(FILEPATH), where (BRANCH) could be
                       empty or not, depending upon your branch-determining
                       function. Only files that start with (SVNURL)/(BRANCH)
                       will be monitored. The Change objects that are sent to
                       the Schedulers will see (FILEPATH) for each modified
                       file.

        @type  split_file: callable or None
        @param split_file: a function that is called with a string of the
                           form (BRANCH)/(FILEPATH) and should return a tuple
                           (BRANCH, FILEPATH). This function should match
                           your repository's branch-naming policy. Each
                           changed file has a fully-qualified URL that can be
                           split into a prefix (which equals the value of the
                           'svnurl' argument) and a suffix; it is this suffix
                           which is passed to the split_file function.

                           If the function returns None, the file is ignored.
                           Use this to indicate that the file is not a part
                           of this project.
                           
                           For example, if your repository puts the trunk in
                           trunk/... and branches are in places like
                           branches/1.5/..., your split_file function could
                           look like the following (this function is
                           available as svnpoller.split_file_branches)::

                            pieces = path.split('/')
                            if pieces[0] == 'trunk':
                                return (None, '/'.join(pieces[1:]))
                            elif pieces[0] == 'branches':
                                return ('/'.join(pieces[0:2]),
                                        '/'.join(pieces[2:]))
                            else:
                                return None

                           If instead your repository layout puts the trunk
                           for ProjectA in trunk/ProjectA/... and the 1.5
                           branch in branches/1.5/ProjectA/..., your
                           split_file function could look like::

                            pieces = path.split('/')
                            if pieces[0] == 'trunk':
                                branch = None
                                pieces.pop(0) # remove 'trunk'
                            elif pieces[0] == 'branches':
                                pieces.pop(0) # remove 'branches'
                                # grab branch name
                                branch = 'branches/' + pieces.pop(0)
                            else:
                                return None # something weird
                            projectname = pieces.pop(0)
                            if projectname != 'ProjectA':
                                return None # wrong project
                            return (branch, '/'.join(pieces))

                           The default of split_file= is None, which
                           indicates that no splitting should be done. This
                           is equivalent to the following function::

                            return (None, path)

                           If you wish, you can override the split_file
                           method with the same sort of function instead of
                           passing in a split_file= argument.


        @type  svnuser:      string
        @param svnuser:      If set, the --username option will be added to
                             the 'svn log' command. You may need this to get
                             access to a private repository.
        @type  svnpasswd:    string
        @param svnpasswd:    If set, the --password option will be added.

        @type  pollinterval: int
        @param pollinterval: interval in seconds between polls. The default
                             is 600 seconds (10 minutes). Smaller values
                             decrease the latency between the time a change
                             is recorded and the time the buildbot notices
                             it, but it also increases the system load.

        @type  histmax:      int
        @param histmax:      maximum number of changes to look back through.
                             The default is 100. Smaller values decrease
                             system load, but if more than histmax changes
                             are recorded between polls, the extra ones will
                             be silently lost.

        @type  svnbin:       string
        @param svnbin:       path to svn binary, defaults to just 'svn'. Use
                             this if your subversion command lives in an
                             unusual location.
        """

        if svnurl.endswith("/"):
            svnurl = svnurl[:-1] # strip the trailing slash
        self.svnurl = svnurl
        self.split_file_function = split_file or split_file_alwaystrunk
        self.svnuser = svnuser
        self.svnpasswd = svnpasswd

        self.svnbin = svnbin
        self.pollinterval = pollinterval
        self.histmax = histmax
        self._prefix = None
        self.overrun_counter = 0
        self.loop = LoopingCall(self.checksvn)

    def split_file(self, path):
        # use getattr() to avoid turning this function into a bound method,
        # which would require it to have an extra 'self' argument
        f = getattr(self, "split_file_function")
        return f(path)

    def startService(self):
        log.msg("SVNPoller(%s) starting" % self.svnurl)
        base.ChangeSource.startService(self)
        # Don't start the loop just yet because the reactor isn't running.
        # Give it a chance to go and install our SIGCHLD handler before
        # spawning processes.
        reactor.callLater(0, self.loop.start, self.pollinterval)

    def stopService(self):
        log.msg("SVNPoller(%s) shutting down" % self.svnurl)
        self.loop.stop()
        return base.ChangeSource.stopService(self)

    def describe(self):
        return "SVNPoller watching %s" % self.svnurl

    def checksvn(self):
        # Our return value is only used for unit testing.

        # we need to figure out the repository root, so we can figure out
        # repository-relative pathnames later. Each SVNURL is in the form
        # (ROOT)/(PROJECT)/(BRANCH)/(FILEPATH), where (ROOT) is something
        # like svn://svn.twistedmatrix.com/svn/Twisted (i.e. there is a
        # physical repository at /svn/Twisted on that host), (PROJECT) is
        # something like Projects/Twisted (i.e. within the repository's
        # internal namespace, everything under Projects/Twisted/ has
        # something to do with Twisted, but these directory names do not
        # actually appear on the repository host), (BRANCH) is something like
        # "trunk" or "branches/2.0.x", and (FILEPATH) is a tree-relative
        # filename like "twisted/internet/defer.py".

        # our self.svnurl attribute contains (ROOT)/(PROJECT) combined
        # together in a way that we can't separate without svn's help. If the
        # user is not using the split_file= argument, then self.svnurl might
        # be (ROOT)/(PROJECT)/(BRANCH) . In any case, the filenames we will
        # get back from 'svn log' will be of the form
        # (PROJECT)/(BRANCH)/(FILEPATH), but we want to be able to remove
        # that (PROJECT) prefix from them. To do this without requiring the
        # user to tell us how svnurl is split into ROOT and PROJECT, we do an
        # 'svn info --xml' command at startup. This command will include a
        # <root> element that tells us ROOT. We then strip this prefix from
        # self.svnurl to determine PROJECT, and then later we strip the
        # PROJECT prefix from the filenames reported by 'svn log --xml' to
        # get a (BRANCH)/(FILEPATH) that can be passed to split_file() to
        # turn into separate BRANCH and FILEPATH values.

        # whew.

        if self.working:
            log.msg("SVNPoller(%s) overrun: timer fired but the previous "
                    "poll had not yet finished.")
            self.overrun_counter += 1
            return defer.succeed(None)
        self.working = True

        log.msg("SVNPoller polling")
        if not self._prefix:
            # this sets self._prefix when it finishes. It fires with
            # self._prefix as well, because that makes the unit tests easier
            # to write.
            d = self.get_root()
            d.addCallback(self.determine_prefix)
        else:
            d = defer.succeed(self._prefix)

        d.addCallback(self.get_logs)
        d.addCallback(self.parse_logs)
        d.addCallback(self.get_new_logentries)
        d.addCallback(self.create_changes)
        d.addCallback(self.submit_changes)
        d.addBoth(self.finished)
        return d

    def getProcessOutput(self, args):
        # this exists so we can override it during the unit tests
        d = utils.getProcessOutput(self.svnbin, args, {})
        return d

    def get_root(self):
        args = ["info", "--xml", "--non-interactive", self.svnurl]
        if self.svnuser:
            args.extend(["--username=%s" % self.svnuser])
        if self.svnpasswd:
            args.extend(["--password=%s" % self.svnpasswd])
        d = self.getProcessOutput(args)
        return d

    def determine_prefix(self, output):
        try:
            doc = xml.dom.minidom.parseString(output)
        except xml.parsers.expat.ExpatError:
            dbgMsg("_process_changes: ExpatError in %s" % output)
            log.msg("SVNPoller._determine_prefix_2: ExpatError in '%s'"
                    % output)
            raise
        rootnodes = doc.getElementsByTagName("root")
        if not rootnodes:
            # this happens if the URL we gave was already the root. In this
            # case, our prefix is empty.
            self._prefix = ""
            return self._prefix
        rootnode = rootnodes[0]
        root = "".join([c.data for c in rootnode.childNodes])
        # root will be a unicode string
        _assert(self.svnurl.startswith(root),
                "svnurl='%s' doesn't start with <root>='%s'" %
                (self.svnurl, root))
        self._prefix = self.svnurl[len(root):]
        if self._prefix.startswith("/"):
            self._prefix = self._prefix[1:]
        log.msg("SVNPoller: svnurl=%s, root=%s, so prefix=%s" %
                (self.svnurl, root, self._prefix))
        return self._prefix

    def get_logs(self, ignored_prefix=None):
        args = []
        args.extend(["log", "--xml", "--verbose", "--non-interactive"])
        if self.svnuser:
            args.extend(["--username=%s" % self.svnuser])
        if self.svnpasswd:
            args.extend(["--password=%s" % self.svnpasswd])
        args.extend(["--limit=%d" % (self.histmax), self.svnurl])
        d = self.getProcessOutput(args)
        return d

    def parse_logs(self, output):
        # parse the XML output, return a list of <logentry> nodes
        try:
            doc = xml.dom.minidom.parseString(output)
        except xml.parsers.expat.ExpatError:
            dbgMsg("_process_changes: ExpatError in %s" % output)
            log.msg("SVNPoller._parse_changes: ExpatError in '%s'" % output)
            raise
        logentries = doc.getElementsByTagName("logentry")
        return logentries


    def _filter_new_logentries(self, logentries, last_change):
        # given a list of logentries, return a tuple of (new_last_change,
        # new_logentries), where new_logentries contains only the ones after
        # last_change
        if not logentries:
            # no entries, so last_change must stay at None
            return (None, [])

        mostRecent = int(logentries[0].getAttribute("revision"))

        if last_change is None:
            # if this is the first time we've been run, ignore any changes
            # that occurred before now. This prevents a build at every
            # startup.
            log.msg('svnPoller: starting at change %s' % mostRecent)
            return (mostRecent, [])

        if last_change == mostRecent:
            # an unmodified repository will hit this case
            log.msg('svnPoller: _process_changes last %s mostRecent %s' % (
                      last_change, mostRecent))
            return (mostRecent, [])

        new_logentries = []
        for el in logentries:
            if last_change == int(el.getAttribute("revision")):
                break
            new_logentries.append(el)
        new_logentries.reverse() # return oldest first
        return (mostRecent, new_logentries)

    def get_new_logentries(self, logentries):
        last_change = self.last_change
        (new_last_change,
         new_logentries) = self._filter_new_logentries(logentries,
                                                       self.last_change)
        self.last_change = new_last_change
        log.msg('svnPoller: _process_changes %s .. %s' %
                (last_change, new_last_change))
        return new_logentries


    def _get_text(self, element, tag_name):
        child_nodes = element.getElementsByTagName(tag_name)[0].childNodes
        text = "".join([t.data for t in child_nodes])
        return text

    def _transform_path(self, path):
        _assert(path.startswith(self._prefix),
                "filepath '%s' should start with prefix '%s'" %
                (path, self._prefix))
        relative_path = path[len(self._prefix):]
        if relative_path.startswith("/"):
            relative_path = relative_path[1:]
        where = self.split_file(relative_path)
        # 'where' is either None or (branch, final_path)
        return where

    def create_changes(self, new_logentries):
        changes = []

        for el in new_logentries:
            branch_files = [] # get oldest change first
            # TODO: revisit this, I think I've settled on Change.revision
            # being a string everywhere, and leaving the interpretation
            # of that string up to b.s.source.SVN methods
            revision = int(el.getAttribute("revision"))
            dbgMsg("Adding change revision %s" % (revision,))
            # TODO: the rest of buildbot may not be ready for unicode 'who'
            # values
            author   = self._get_text(el, "author")
            comments = self._get_text(el, "msg")
            # there is a "date" field, but it provides localtime in the
            # repository's timezone, whereas we care about buildmaster's
            # localtime (since this will get used to position the boxes on
            # the Waterfall display, etc). So ignore the date field and use
            # our local clock instead.
            #when     = self._get_text(el, "date")
            #when     = time.mktime(time.strptime("%.19s" % when,
            #                                     "%Y-%m-%dT%H:%M:%S"))
            branches = {}
            pathlist = el.getElementsByTagName("paths")[0]
            for p in pathlist.getElementsByTagName("path"):
                path = "".join([t.data for t in p.childNodes])
                # the rest of buildbot is certaily not yet ready to handle
                # unicode filenames, because they get put in RemoteCommands
                # which get sent via PB to the buildslave, and PB doesn't
                # handle unicode.
                path = path.encode("ascii")
                if path.startswith("/"):
                    path = path[1:]
                where = self._transform_path(path)
                # if 'where' is None, the file was outside any project that
                # we care about and we should ignore it
                if where:
                    branch, filename = where
                    if not branch in branches:
                        branches[branch] = []
                    branches[branch].append(filename)

            for branch in branches:
                c = Change(who=author,
                           files=branches[branch],
                           comments=comments,
                           revision=revision,
                           branch=branch)
                changes.append(c)

        return changes

    def submit_changes(self, changes):
        for c in changes:
            self.parent.addChange(c)

    def finished(self, res):
        log.msg("SVNPoller finished polling")
        dbgMsg('_finished : %s' % res)
        assert self.working
        self.working = False
        return res
