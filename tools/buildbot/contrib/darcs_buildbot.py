#! /usr/bin/python

# This is a script which delivers Change events from Darcs to the buildmaster
# each time a patch is pushed into a repository. Add it to the 'apply' hook
# on your canonical "central" repository, by putting something like the
# following in the _darcs/prefs/defaults file of that repository:
#
#  apply posthook /PATH/TO/darcs_buildbot.py BUILDMASTER:PORT
#  apply run-posthook
#
# (the second command is necessary to avoid the usual "do you really want to
# run this hook" prompt. Note that you cannot have multiple 'apply posthook'
# lines.)
#
# Note that both Buildbot and Darcs must be installed on the repository
# machine. You will also need the Python/XML distribution installed (the
# "python2.3-xml" package under debian).

import os, sys, commands
from StringIO import StringIO
from buildbot.scripts import runner
import xml
from xml.dom import minidom

MASTER = sys.argv[1]

out = commands.getoutput("darcs changes --last=1 --xml-output --summary")
#out = commands.getoutput("darcs changes -p 'project @ 2006-05-21 19:07:27 by warner' --xml-output --summary")
try:
    doc = minidom.parseString(out)
except xml.parsers.expat.ExpatError, e:
    print "failed to parse XML"
    print str(e)
    print "purported XML is:"
    print "--BEGIN--"
    print out
    print "--END--"
    sys.exit(1)

c = doc.getElementsByTagName("changelog")[0]
p = c.getElementsByTagName("patch")[0]

def getText(node):
    return "".join([cn.data
                    for cn in node.childNodes
                    if cn.nodeType == cn.TEXT_NODE])
def getTextFromChild(parent, childtype):
    children = parent.getElementsByTagName(childtype)
    if not children:
        return ""
    return getText(children[0])


author = p.getAttribute("author")
revision = p.getAttribute("hash")
comments = getTextFromChild(p, "name") + "\n" + getTextFromChild(p, "comment")

summary = c.getElementsByTagName("summary")[0]
files = []
for filenode in summary.childNodes:
    if filenode.nodeName in ("add_file", "modify_file", "remove_file"):
        filename = getText(filenode).strip()
        files.append(filename)

# note that these are all unicode. Because PB can't handle unicode, we encode
# them into ascii, which will blow up early if there's anything we can't get
# to the far side. When we move to something that *can* handle unicode (like
# newpb), remove this.
author = author.encode("ascii")
comments = comments.encode("ascii")
files = [f.encode("ascii") for f in files]
revision = revision.encode("ascii")

change = {
    'master': MASTER,
    # note: this is more likely to be a full email address, which would make
    # the left-hand "Changes" column kind of wide. The buildmaster should
    # probably be improved to display an abbreviation of the username.
    'username': author,
    'revision': revision,
    'comments': comments,
    'files': files,
    }

runner.sendchange(change, True)

