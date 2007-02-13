#! /usr/bin/python

# This is a script which delivers Change events from Mercurial to the
# buildmaster each time a changeset is pushed into a repository. Add it to
# the 'incoming' commit hook on your canonical "central" repository, by
# putting something like the following in the .hg/hgrc file of that
# repository:
#
#  [hooks]
#  incoming.buildbot = /PATH/TO/hg_buildbot.py BUILDMASTER:PORT
#
# Note that both Buildbot and Mercurial must be installed on the repository
# machine.

import os, sys, commands
from StringIO import StringIO
from buildbot.scripts import runner

MASTER = sys.argv[1]

CHANGESET_ID = os.environ["HG_NODE"]

# TODO: consider doing 'import mercurial.hg' and extract this information
# using the native python
out = commands.getoutput("hg -v log -r %s" % CHANGESET_ID)
# TODO: or maybe use --template instead of trying hard to parse everything
#out = commands.getoutput("hg --template SOMETHING log -r %s" % CHANGESET_ID)

s = StringIO(out)
while True:
    line = s.readline()
    if not line:
        break
    if line.startswith("user:"):
        user = line[line.find(":")+1:].strip()
    elif line.startswith("files:"):
        files = line[line.find(":")+1:].strip().split()
    elif line.startswith("description:"):
        comments = "".join(s.readlines())
        if comments[-1] == "\n":
            # this removes the additional newline that hg emits
            comments = comments[:-1]
        break

change = {
    'master': MASTER,
    # note: this is more likely to be a full email address, which would make
    # the left-hand "Changes" column kind of wide. The buildmaster should
    # probably be improved to display an abbreviation of the username.
    'username': user,
    'revision': CHANGESET_ID,
    'comments': comments,
    'files': files,
    }

runner.sendchange(c, True)

