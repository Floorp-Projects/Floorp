import time
from urllib import urlopen
from xml.dom import minidom, Node

from twisted.python import log, failure
from twisted.internet import defer, reactor
from twisted.internet.task import LoopingCall

from buildbot.changes import base, changes

class InvalidResultError(Exception):
    def __init__(self, value="InvalidResultError"):
        self.value = value
    def __str__(self):
        return repr(self.value)

class EmptyResult(Exception):
    pass

class NoMoreCiNodes(Exception):
    pass

class NoMoreFileNodes(Exception):
    pass

class BonsaiResult:
    """I hold a list of CiNodes"""
    def __init__(self, nodes=[]):
        self.nodes = nodes

    def __cmp__(self, other):
        if len(self.nodes) != len(other.nodes):
            return False
        for i in range(len(self.nodes)):
            if self.nodes[i].log != other.nodes[i].log \
              or self.nodes[i].who != other.nodes[i].who \
              or self.nodes[i].date != other.nodes[i].date \
              or len(self.nodes[i].files) != len(other.nodes[i].files):
                return -1

	        for j in range(len(self.nodes[i].files)):
	            if self.nodes[i].files[j].revision \
	              != other.nodes[i].files[j].revision \
	              or self.nodes[i].files[j].filename \
	              != other.nodes[i].files[j].filename:
	                return -1

        return 0

class CiNode:
    """I hold information baout one <ci> node, including a list of files"""
    def __init__(self, log="", who="", date=0, files=[]):
        self.log = log
        self.who = who
        self.date = date
        self.files = files

class FileNode:
    """I hold information about one <f> node"""
    def __init__(self, revision="", filename=""):
        self.revision = revision
        self.filename = filename

class BonsaiParser:
    """I parse the XML result from a bonsai cvsquery."""

    def __init__(self, bonsaiQuery):
        try:
        # this is a fix for non-ascii characters
        # readlines() + join is being used because read() is not guaranteed
        # to work. because bonsai does not give us an encoding to work with
        # it impossible to be 100% sure what to decode it as but latin1 covers
        # the broadest base
            data = "".join(bonsaiQuery.readlines())
            data = data.decode("latin1")
            data = data.encode("ascii", "replace")
            self.dom = minidom.parseString(data)
            log.msg(data)
        except:
            raise InvalidResultError("Malformed XML in result")

        self.ciNodes = self.dom.getElementsByTagName("ci")
        self.currentCiNode = None # filled in by _nextCiNode()
        self.fileNodes = None # filled in by _nextCiNode()
        self.currentFileNode = None # filled in by _nextFileNode()
        self.bonsaiResult = self._parseData()

    def getData(self):
        return self.bonsaiResult

    def _parseData(self):
        """Returns data from a Bonsai cvsquery in a BonsaiResult object"""
        nodes = []
        try:
            while self._nextCiNode():
                files = []
                try:
                    while self._nextFileNode():
                        files.append(FileNode(self._getRevision(),
                                              self._getFilename()))
                except NoMoreFileNodes:
                    pass
                except InvalidResultError:
                    raise
                nodes.append(CiNode(self._getLog(), self._getWho(),
                                    self._getDate(), files))

        except NoMoreCiNodes:
            pass
        except InvalidResultError, EmptyResult:
            raise

        return BonsaiResult(nodes)


    def _nextCiNode(self):
        """Iterates to the next <ci> node and fills self.fileNodes with
           child <f> nodes"""
        try:
            self.currentCiNode = self.ciNodes.pop(0)
            if len(self.currentCiNode.getElementsByTagName("files")) > 1:
                raise InvalidResultError("Multiple <files> for one <ci>")

            self.fileNodes = self.currentCiNode.getElementsByTagName("f")
        except IndexError:
            # if there was zero <ci> nodes in the result
            if not self.currentCiNode:
                raise EmptyResult
            else:
                raise NoMoreCiNodes

        return True

    def _nextFileNode(self):
        """Iterates to the next <f> node"""
        try:
            self.currentFileNode = self.fileNodes.pop(0)
        except IndexError:
            raise NoMoreFileNodes

        return True

    def _getLog(self):
        """Returns the log of the current <ci> node"""
        logs = self.currentCiNode.getElementsByTagName("log")
        if len(logs) < 1:
            raise InvalidResultError("No log present")
        elif len(logs) > 1:
            raise InvalidResultError("Multiple logs present")

        return logs[0].firstChild.data

    def _getWho(self):
        """Returns the e-mail address of the commiter"""
        # convert unicode string to regular string
        return str(self.currentCiNode.getAttribute("who"))

    def _getDate(self):
        """Returns the date (unix time) of the commit"""
        # convert unicode number to regular one
        try:
            commitDate = int(self.currentCiNode.getAttribute("date"))
        except ValueError:
            raise InvalidResultError

        return commitDate

    def _getFilename(self):
        """Returns the filename of the current <f> node"""
        try:
            filename = self.currentFileNode.firstChild.data
        except AttributeError:
            raise InvalidResultError("Missing filename")

        return filename

    def _getRevision(self):
        return self.currentFileNode.getAttribute("rev")


class BonsaiPoller(base.ChangeSource):
    """This source will poll a bonsai server for changes and submit
    them to the change master."""

    compare_attrs = ["bonsaiURL", "pollInterval", "tree",
                     "module", "branch", "cvsroot"]

    parent = None # filled in when we're added
    loop = None
    volatile = ['loop']
    working = False

    def __init__(self, bonsaiURL, module, branch, tree="default",
                 cvsroot="/cvsroot", pollInterval=30):
        """
        @type   bonsaiURL:      string
        @param  bonsaiURL:      The base URL of the Bonsai server
                                (ie. http://bonsai.mozilla.org)
        @type   module:         string
        @param  module:         The module to look for changes in. Commonly
                                this is 'all'
        @type   branch:         string
        @param  branch:         The branch to look for changes in. This must
                                match the
                                'branch' option for the Scheduler.
        @type   tree:           string
        @param  tree:           The tree to look for changes in. Commonly this
                                is 'all'
        @type   cvsroot:        string
        @param  cvsroot:        The cvsroot of the repository. Usually this is
                                '/cvsroot'
        @type   pollInterval:   int
        @param  pollInterval:   The time (in seconds) between queries for
                                changes
        """

        self.bonsaiURL = bonsaiURL
        self.module = module
        self.branch = branch
        self.tree = tree
        self.cvsroot = cvsroot
        self.pollInterval = pollInterval
        self.lastChange = time.time()
        self.lastPoll = time.time()

    def startService(self):
        self.loop = LoopingCall(self.poll)
        base.ChangeSource.startService(self)

        reactor.callLater(0, self.loop.start, self.pollInterval)

    def stopService(self):
        self.loop.stop()
        return base.ChangeSource.stopService(self)

    def describe(self):
        str = ""
        str += "Getting changes from the Bonsai service running at %s " \
                % self.bonsaiURL
        str += "<br>Using tree: %s, branch: %s, and module: %s" % (self.tree, \
                self.branch, self.module)
        return str

    def poll(self):
        if self.working:
            log.msg("Not polling Bonsai because last poll is still working")
        else:
            self.working = True
            d = self._get_changes()
            d.addCallback(self._process_changes)
            d.addBoth(self._finished)
        return

    def _finished(self, res):
        assert self.working
        self.working = False

        # check for failure
        if isinstance(res, failure.Failure):
            log.msg("Bonsai poll failed: %s" % res)
        return res

    def _make_url(self):
        args = ["treeid=%s" % self.tree, "module=%s" % self.module,
                "branch=%s" % self.branch, "branchtype=match",
                "sortby=Date", "date=explicit",
                "mindate=%d" % self.lastChange,
                "maxdate=%d" % int(time.time()),
                "cvsroot=%s" % self.cvsroot, "xml=1"]
        # build the bonsai URL
        url = self.bonsaiURL
        url += "/cvsquery.cgi?"
        url += "&".join(args)

        return url

    def _get_changes(self):
        url = self._make_url()
        log.msg("Polling Bonsai tree at %s" % url)

        self.lastPoll = time.time()
        # get the page, in XML format
        return defer.maybeDeferred(urlopen, url)

    def _process_changes(self, query):
        files = []
        try:
            bp = BonsaiParser(query)
            result = bp.getData()
        except InvalidResultError, e:
            log.msg("Could not process Bonsai query: " + e.value)
            return
        except EmptyResult:
            return

        for cinode in result.nodes:
            for file in cinode.files:
                files.append(file.filename+' (revision '+file.revision+')')
            c = changes.Change(who = cinode.who,
                               files = files,
                               comments = cinode.log,
                               when = cinode.date,
                               branch = self.branch)
            self.parent.addChange(c)
            self.lastChange = self.lastPoll
