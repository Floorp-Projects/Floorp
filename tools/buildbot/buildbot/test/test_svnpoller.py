# -*- test-case-name: buildbot.test.test_svnpoller -*-

import time
from twisted.internet import defer
from twisted.trial import unittest
from buildbot.changes.svnpoller import SVNPoller

# this is the output of "svn info --xml
# svn+ssh://svn.twistedmatrix.com/svn/Twisted/trunk"
prefix_output = """\
<?xml version="1.0"?>
<info>
<entry
   kind="dir"
   path="trunk"
   revision="18354">
<url>svn+ssh://svn.twistedmatrix.com/svn/Twisted/trunk</url>
<repository>
<root>svn+ssh://svn.twistedmatrix.com/svn/Twisted</root>
<uuid>bbbe8e31-12d6-0310-92fd-ac37d47ddeeb</uuid>
</repository>
<commit
   revision="18352">
<author>jml</author>
<date>2006-10-01T02:37:34.063255Z</date>
</commit>
</entry>
</info>
"""

# and this is "svn info --xml svn://svn.twistedmatrix.com/svn/Twisted". I
# think this is kind of a degenerate case.. it might even be a form of error.
prefix_output_2 = """\
<?xml version="1.0"?>
<info>
</info>
"""

# this is the svn info output for a local repository, svn info --xml
# file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository
prefix_output_3 = """\
<?xml version="1.0"?>
<info>
<entry
   kind="dir"
   path="SVN-Repository"
   revision="3">
<url>file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository</url>
<repository>
<root>file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository</root>
<uuid>c0f47ff4-ba1e-0410-96b5-d44cc5c79e7f</uuid>
</repository>
<commit
   revision="3">
<author>warner</author>
<date>2006-10-01T07:37:04.182499Z</date>
</commit>
</entry>
</info>
"""

# % svn info --xml file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample/trunk

prefix_output_4 = """\
<?xml version="1.0"?>
<info>
<entry
   kind="dir"
   path="trunk"
   revision="3">
<url>file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample/trunk</url>
<repository>
<root>file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository</root>
<uuid>c0f47ff4-ba1e-0410-96b5-d44cc5c79e7f</uuid>
</repository>
<commit
   revision="1">
<author>warner</author>
<date>2006-10-01T07:37:02.286440Z</date>
</commit>
</entry>
</info>
"""



class ComputePrefix(unittest.TestCase):
    def test1(self):
        base = "svn+ssh://svn.twistedmatrix.com/svn/Twisted/trunk"
        s = SVNPoller(base + "/")
        self.failUnlessEqual(s.svnurl, base) # certify slash-stripping
        prefix = s.determine_prefix(prefix_output)
        self.failUnlessEqual(prefix, "trunk")
        self.failUnlessEqual(s._prefix, prefix)

    def test2(self):
        base = "svn+ssh://svn.twistedmatrix.com/svn/Twisted"
        s = SVNPoller(base)
        self.failUnlessEqual(s.svnurl, base)
        prefix = s.determine_prefix(prefix_output_2)
        self.failUnlessEqual(prefix, "")

    def test3(self):
        base = "file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository"
        s = SVNPoller(base)
        self.failUnlessEqual(s.svnurl, base)
        prefix = s.determine_prefix(prefix_output_3)
        self.failUnlessEqual(prefix, "")

    def test4(self):
        base = "file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample/trunk"
        s = SVNPoller(base)
        self.failUnlessEqual(s.svnurl, base)
        prefix = s.determine_prefix(prefix_output_4)
        self.failUnlessEqual(prefix, "sample/trunk")

# output from svn log on .../SVN-Repository/sample
# (so it includes trunk and branches)
sample_base = "file:///usr/home/warner/stuff/Projects/BuildBot/trees/misc/_trial_temp/test_vc/repositories/SVN-Repository/sample"
sample_logentries = [None] * 4

sample_logentries[3] = """\
<logentry
   revision="4">
<author>warner</author>
<date>2006-10-01T19:35:16.165664Z</date>
<paths>
<path
   action="M">/sample/trunk/version.c</path>
</paths>
<msg>revised_to_2</msg>
</logentry>
"""

sample_logentries[2] = """\
<logentry
   revision="3">
<author>warner</author>
<date>2006-10-01T19:35:10.215692Z</date>
<paths>
<path
   action="M">/sample/branch/main.c</path>
</paths>
<msg>commit_on_branch</msg>
</logentry>
"""

sample_logentries[1] = """\
<logentry
   revision="2">
<author>warner</author>
<date>2006-10-01T19:35:09.154973Z</date>
<paths>
<path
   copyfrom-path="/sample/trunk"
   copyfrom-rev="1"
   action="A">/sample/branch</path>
</paths>
<msg>make_branch</msg>
</logentry>
"""

sample_logentries[0] = """\
<logentry
   revision="1">
<author>warner</author>
<date>2006-10-01T19:35:08.642045Z</date>
<paths>
<path
   action="A">/sample</path>
<path
   action="A">/sample/trunk</path>
<path
   action="A">/sample/trunk/subdir/subdir.c</path>
<path
   action="A">/sample/trunk/main.c</path>
<path
   action="A">/sample/trunk/version.c</path>
<path
   action="A">/sample/trunk/subdir</path>
</paths>
<msg>sample_project_files</msg>
</logentry>
"""

sample_info_output = """\
<?xml version="1.0"?>
<info>
<entry
   kind="dir"
   path="sample"
   revision="4">
<url>file:///usr/home/warner/stuff/Projects/BuildBot/trees/misc/_trial_temp/test_vc/repositories/SVN-Repository/sample</url>
<repository>
<root>file:///usr/home/warner/stuff/Projects/BuildBot/trees/misc/_trial_temp/test_vc/repositories/SVN-Repository</root>
<uuid>4f94adfc-c41e-0410-92d5-fbf86b7c7689</uuid>
</repository>
<commit
   revision="4">
<author>warner</author>
<date>2006-10-01T19:35:16.165664Z</date>
</commit>
</entry>
</info>
"""


changes_output_template = """\
<?xml version="1.0"?>
<log>
%s</log>
"""

def make_changes_output(maxrevision):
    # return what 'svn log' would have just after the given revision was
    # committed
    logs = sample_logentries[0:maxrevision]
    assert len(logs) == maxrevision
    logs.reverse()
    output = changes_output_template % ("".join(logs))
    return output

def split_file(path):
    pieces = path.split("/")
    if pieces[0] == "branch":
        return "branch", "/".join(pieces[1:])
    if pieces[0] == "trunk":
        return None, "/".join(pieces[1:])
    raise RuntimeError("there shouldn't be any files like %s" % path)

class MySVNPoller(SVNPoller):
    def __init__(self, *args, **kwargs):
        SVNPoller.__init__(self, *args, **kwargs)
        self.pending_commands = []
        self.finished_changes = []

    def getProcessOutput(self, args):
        d = defer.Deferred()
        self.pending_commands.append((args, d))
        return d

    def submit_changes(self, changes):
        self.finished_changes.extend(changes)

class ComputeChanges(unittest.TestCase):
    def test1(self):
        base = "file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample"
        s = SVNPoller(base)
        s._prefix = "sample"
        output = make_changes_output(4)
        doc = s.parse_logs(output)

        newlast, logentries = s._filter_new_logentries(doc, 4)
        self.failUnlessEqual(newlast, 4)
        self.failUnlessEqual(len(logentries), 0)

        newlast, logentries = s._filter_new_logentries(doc, 3)
        self.failUnlessEqual(newlast, 4)
        self.failUnlessEqual(len(logentries), 1)

        newlast, logentries = s._filter_new_logentries(doc, 1)
        self.failUnlessEqual(newlast, 4)
        self.failUnlessEqual(len(logentries), 3)

        newlast, logentries = s._filter_new_logentries(doc, None)
        self.failUnlessEqual(newlast, 4)
        self.failUnlessEqual(len(logentries), 0)

    def testChanges(self):
        base = "file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample"
        s = SVNPoller(base, split_file=split_file)
        s._prefix = "sample"
        doc = s.parse_logs(make_changes_output(3))
        newlast, logentries = s._filter_new_logentries(doc, 1)
        # so we see revisions 2 and 3 as being new
        self.failUnlessEqual(newlast, 3)
        changes = s.create_changes(logentries)
        self.failUnlessEqual(len(changes), 2)
        self.failUnlessEqual(changes[0].branch, "branch")
        self.failUnlessEqual(changes[0].revision, 2)
        self.failUnlessEqual(changes[1].branch, "branch")
        self.failUnlessEqual(changes[1].files, ["main.c"])
        self.failUnlessEqual(changes[1].revision, 3)

        # and now pull in r4
        doc = s.parse_logs(make_changes_output(4))
        newlast, logentries = s._filter_new_logentries(doc, newlast)
        self.failUnlessEqual(newlast, 4)
        # so we see revision 4 as being new
        changes = s.create_changes(logentries)
        self.failUnlessEqual(len(changes), 1)
        self.failUnlessEqual(changes[0].branch, None)
        self.failUnlessEqual(changes[0].revision, 4)
        self.failUnlessEqual(changes[0].files, ["version.c"])

    def testFirstTime(self):
        base = "file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample"
        s = SVNPoller(base, split_file=split_file)
        s._prefix = "sample"
        doc = s.parse_logs(make_changes_output(4))
        logentries = s.get_new_logentries(doc)
        # SVNPoller ignores all changes that happened before it was started
        self.failUnlessEqual(len(logentries), 0)
        self.failUnlessEqual(s.last_change, 4)

class Misc(unittest.TestCase):
    def testAlreadyWorking(self):
        base = "file:///home/warner/stuff/Projects/BuildBot/trees/svnpoller/_trial_temp/test_vc/repositories/SVN-Repository/sample"
        s = MySVNPoller(base)
        d = s.checksvn()
        # the SVNPoller is now waiting for its getProcessOutput to finish
        self.failUnlessEqual(s.overrun_counter, 0)
        d2 = s.checksvn()
        self.failUnlessEqual(s.overrun_counter, 1)
        self.failUnlessEqual(len(s.pending_commands), 1)

    def testGetRoot(self):
        base = "svn+ssh://svn.twistedmatrix.com/svn/Twisted/trunk"
        s = MySVNPoller(base)
        d = s.checksvn()
        # the SVNPoller is now waiting for its getProcessOutput to finish
        self.failUnlessEqual(len(s.pending_commands), 1)
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["info", "--xml", "--non-interactive", base])

def makeTime(timestring):
    datefmt = '%Y/%m/%d %H:%M:%S'
    when = time.mktime(time.strptime(timestring, datefmt))
    return when


class Everything(unittest.TestCase):
    def test1(self):
        s = MySVNPoller(sample_base, split_file=split_file)
        d = s.checksvn()
        # the SVNPoller is now waiting for its getProcessOutput to finish
        self.failUnlessEqual(len(s.pending_commands), 1)
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["info", "--xml", "--non-interactive",
                              sample_base])
        d = s.pending_commands[0][1]
        s.pending_commands.pop(0)
        d.callback(sample_info_output)
        # now it should be waiting for the 'svn log' command
        self.failUnlessEqual(len(s.pending_commands), 1)
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["log", "--xml", "--verbose", "--non-interactive",
                              "--limit=100", sample_base])
        d = s.pending_commands[0][1]
        s.pending_commands.pop(0)
        d.callback(make_changes_output(1))
        # the command ignores the first batch of changes
        self.failUnlessEqual(len(s.finished_changes), 0)
        self.failUnlessEqual(s.last_change, 1)

        # now fire it again, nothing changing
        d = s.checksvn()
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["log", "--xml", "--verbose", "--non-interactive",
                              "--limit=100", sample_base])
        d = s.pending_commands[0][1]
        s.pending_commands.pop(0)
        d.callback(make_changes_output(1))
        # nothing has changed
        self.failUnlessEqual(len(s.finished_changes), 0)
        self.failUnlessEqual(s.last_change, 1)

        # and again, with r2 this time
        d = s.checksvn()
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["log", "--xml", "--verbose", "--non-interactive",
                              "--limit=100", sample_base])
        d = s.pending_commands[0][1]
        s.pending_commands.pop(0)
        d.callback(make_changes_output(2))
        # r2 should appear
        self.failUnlessEqual(len(s.finished_changes), 1)
        self.failUnlessEqual(s.last_change, 2)

        c = s.finished_changes[0]
        self.failUnlessEqual(c.branch, "branch")
        self.failUnlessEqual(c.revision, 2)
        self.failUnlessEqual(c.files, [''])
        # TODO: this is what creating the branch looks like: a Change with a
        # zero-length file. We should decide if we want filenames like this
        # in the Change (and make sure nobody else gets confused by it) or if
        # we want to strip them out.
        self.failUnlessEqual(c.comments, "make_branch")

        # and again at r2, so nothing should change
        d = s.checksvn()
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["log", "--xml", "--verbose", "--non-interactive",
                              "--limit=100", sample_base])
        d = s.pending_commands[0][1]
        s.pending_commands.pop(0)
        d.callback(make_changes_output(2))
        # nothing has changed
        self.failUnlessEqual(len(s.finished_changes), 1)
        self.failUnlessEqual(s.last_change, 2)

        # and again with both r3 and r4 appearing together
        d = s.checksvn()
        self.failUnlessEqual(s.pending_commands[0][0],
                             ["log", "--xml", "--verbose", "--non-interactive",
                              "--limit=100", sample_base])
        d = s.pending_commands[0][1]
        s.pending_commands.pop(0)
        d.callback(make_changes_output(4))
        self.failUnlessEqual(len(s.finished_changes), 3)
        self.failUnlessEqual(s.last_change, 4)

        c3 = s.finished_changes[1]
        self.failUnlessEqual(c3.branch, "branch")
        self.failUnlessEqual(c3.revision, 3)
        self.failUnlessEqual(c3.files, ["main.c"])
        self.failUnlessEqual(c3.comments, "commit_on_branch")

        c4 = s.finished_changes[2]
        self.failUnlessEqual(c4.branch, None)
        self.failUnlessEqual(c4.revision, 4)
        self.failUnlessEqual(c4.files, ["version.c"])
        self.failUnlessEqual(c4.comments, "revised_to_2")
        self.failUnless(abs(c4.when - time.time()) < 60)


# TODO:
#  get coverage of split_file returning None
#  point at a live SVN server for a little while
