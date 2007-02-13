
import tempfile
import os
import os.path
from cStringIO import StringIO

from twisted.python import log
from twisted.application import service
from twisted.internet import defer, protocol, error, reactor
from twisted.internet.task import LoopingCall

from buildbot import util
from buildbot.interfaces import IChangeSource
from buildbot.changes.changes import Change

class _MTProtocol(protocol.ProcessProtocol):

    def __init__(self, deferred, cmdline):
        self.cmdline = cmdline
        self.deferred = deferred
        self.s = StringIO()

    def errReceived(self, text):
        log.msg("stderr: %s" % text)

    def outReceived(self, text):
        log.msg("stdout: %s" % text)
        self.s.write(text)

    def processEnded(self, reason):
        log.msg("Command %r exited with value %s" % (self.cmdline, reason))
        if isinstance(reason.value, error.ProcessDone):
            self.deferred.callback(self.s.getvalue())
        else:
            self.deferred.errback(reason)

class Monotone:
    """All methods of this class return a Deferred."""

    def __init__(self, bin, db):
        self.bin = bin
        self.db = db

    def _run_monotone(self, args):
        d = defer.Deferred()
        cmdline = (self.bin, "--db=" + self.db) + tuple(args)
        p = _MTProtocol(d, cmdline)
        log.msg("Running command: %r" % (cmdline,))
        log.msg("wd: %s" % os.getcwd())
        reactor.spawnProcess(p, self.bin, cmdline)
        return d

    def _process_revision_list(self, output):
        if output:
            return output.strip().split("\n")
        else:
            return []

    def get_interface_version(self):
        d = self._run_monotone(["automate", "interface_version"])
        d.addCallback(self._process_interface_version)
        return d

    def _process_interface_version(self, output):
        return tuple(map(int, output.strip().split(".")))

    def db_init(self):
        return self._run_monotone(["db", "init"])

    def db_migrate(self):
        return self._run_monotone(["db", "migrate"])

    def pull(self, server, pattern):
        return self._run_monotone(["pull", server, pattern])

    def get_revision(self, rid):
        return self._run_monotone(["cat", "revision", rid])

    def get_heads(self, branch, rcfile=""):
        cmd = ["automate", "heads", branch]
        if rcfile:
            cmd += ["--rcfile=" + rcfile]
        d = self._run_monotone(cmd)
        d.addCallback(self._process_revision_list)
        return d

    def erase_ancestors(self, revs):
        d = self._run_monotone(["automate", "erase_ancestors"] + revs)
        d.addCallback(self._process_revision_list)
        return d

    def ancestry_difference(self, new_rev, old_revs):
        d = self._run_monotone(["automate", "ancestry_difference", new_rev]
                               + old_revs)
        d.addCallback(self._process_revision_list)
        return d

    def descendents(self, rev):
        d = self._run_monotone(["automate", "descendents", rev])
        d.addCallback(self._process_revision_list)
        return d

    def log(self, rev, depth=None):
        if depth is not None:
            depth_arg = ["--last=%i" % (depth,)]
        else:
            depth_arg = []
        return self._run_monotone(["log", "-r", rev] + depth_arg)


class MonotoneSource(service.Service, util.ComparableMixin):
    """This source will poll a monotone server for changes and submit them to
    the change master.

    @param server_addr: monotone server specification (host:portno)

    @param branch: monotone branch to watch

    @param trusted_keys: list of keys whose code you trust

    @param db_path: path to monotone database to pull into

    @param pollinterval: interval in seconds between polls, defaults to 10 minutes
    @param monotone_exec: path to monotone executable, defaults to "monotone"
    """

    __implements__ = IChangeSource, service.Service.__implements__
    compare_attrs = ["server_addr", "trusted_keys", "db_path",
                     "pollinterval", "branch", "monotone_exec"]

    parent = None # filled in when we're added
    done_revisions = []
    last_revision = None
    loop = None
    d = None
    tmpfile = None
    monotone = None
    volatile = ["loop", "d", "tmpfile", "monotone"]

    def __init__(self, server_addr, branch, trusted_keys, db_path,
                 pollinterval=60 * 10, monotone_exec="monotone"):
        self.server_addr = server_addr
        self.branch = branch
        self.trusted_keys = trusted_keys
        self.db_path = db_path
        self.pollinterval = pollinterval
        self.monotone_exec = monotone_exec
        self.monotone = Monotone(self.monotone_exec, self.db_path)

    def startService(self):
        self.loop = LoopingCall(self.start_poll)
        self.loop.start(self.pollinterval)
        service.Service.startService(self)

    def stopService(self):
        self.loop.stop()
        return service.Service.stopService(self)

    def describe(self):
        return "monotone_source %s %s" % (self.server_addr,
                                          self.branch)

    def start_poll(self):
        if self.d is not None:
            log.msg("last poll still in progress, skipping next poll")
            return
        log.msg("starting poll")
        self.d = self._maybe_init_db()
        self.d.addCallback(self._do_netsync)
        self.d.addCallback(self._get_changes)
        self.d.addErrback(self._handle_error)

    def _handle_error(self, failure):
        log.err(failure)
        self.d = None

    def _maybe_init_db(self):
        if not os.path.exists(self.db_path):
            log.msg("init'ing db")
            return self.monotone.db_init()
        else:
            log.msg("db already exists, migrating")
            return self.monotone.db_migrate()

    def _do_netsync(self, output):
        return self.monotone.pull(self.server_addr, self.branch)

    def _get_changes(self, output):
        d = self._get_new_head()
        d.addCallback(self._process_new_head)
        return d

    def _get_new_head(self):
        # This function returns a deferred that resolves to a good pick of new
        # head (or None if there is no good new head.)

        # First need to get all new heads...
        rcfile = """function get_revision_cert_trust(signers, id, name, val)
                      local trusted_signers = { %s }
                      local ts_table = {}
                      for k, v in pairs(trusted_signers) do ts_table[v] = 1 end
                      for k, v in pairs(signers) do
                        if ts_table[v] then
                          return true
                        end
                      end
                      return false
                    end
        """
        trusted_list = ", ".join(['"' + key + '"' for key in self.trusted_keys])
        # mktemp is unsafe, but mkstemp is not 2.2 compatible.
        tmpfile_name = tempfile.mktemp()
        f = open(tmpfile_name, "w")
        f.write(rcfile % trusted_list)
        f.close()
        d = self.monotone.get_heads(self.branch, tmpfile_name)
        d.addCallback(self._find_new_head, tmpfile_name)
        return d

    def _find_new_head(self, new_heads, tmpfile_name):
        os.unlink(tmpfile_name)
        # Now get the old head's descendents...
        if self.last_revision is not None:
            d = self.monotone.descendents(self.last_revision)
        else:
            d = defer.succeed(new_heads)
        d.addCallback(self._pick_new_head, new_heads)
        return d

    def _pick_new_head(self, old_head_descendents, new_heads):
        for r in new_heads:
            if r in old_head_descendents:
                return r
        return None

    def _process_new_head(self, new_head):
        if new_head is None:
            log.msg("No new head")
            self.d = None
            return None
        # Okay, we have a new head; we need to get all the revisions since
        # then and create change objects for them.
        # Step 1: simplify set of processed revisions.
        d = self._simplify_revisions()
        # Step 2: get the list of new revisions
        d.addCallback(self._get_new_revisions, new_head)
        # Step 3: add a change for each
        d.addCallback(self._add_changes_for_revisions)
        # Step 4: all done
        d.addCallback(self._finish_changes, new_head)
        return d

    def _simplify_revisions(self):
        d = self.monotone.erase_ancestors(self.done_revisions)
        d.addCallback(self._reset_done_revisions)
        return d

    def _reset_done_revisions(self, new_done_revisions):
        self.done_revisions = new_done_revisions
        return None

    def _get_new_revisions(self, blah, new_head):
        if self.done_revisions:
            return self.monotone.ancestry_difference(new_head,
                                                     self.done_revisions)
        else:
            # Don't force feed the builder with every change since the
            # beginning of time when it's first started up.
            return defer.succeed([new_head])

    def _add_changes_for_revisions(self, revs):
        d = defer.succeed(None)
        for rid in revs:
            d.addCallback(self._add_change_for_revision, rid)
        return d

    def _add_change_for_revision(self, blah, rid):
        d = self.monotone.log(rid, 1)
        d.addCallback(self._add_change_from_log, rid)
        return d

    def _add_change_from_log(self, log, rid):
        d = self.monotone.get_revision(rid)
        d.addCallback(self._add_change_from_log_and_revision, log, rid)
        return d

    def _add_change_from_log_and_revision(self, revision, log, rid):
        # Stupid way to pull out everything inside quotes (which currently
        # uniquely identifies filenames inside a changeset).
        pieces = revision.split('"')
        files = []
        for i in range(len(pieces)):
            if (i % 2) == 1:
                files.append(pieces[i])
        # Also pull out author key and date
        author = "unknown author"
        pieces = log.split('\n')
        for p in pieces:
            if p.startswith("Author:"):
                author = p.split()[1]
        self.parent.addChange(Change(author, files, log, revision=rid))

    def _finish_changes(self, blah, new_head):
        self.done_revisions.append(new_head)
        self.last_revision = new_head
        self.d = None
