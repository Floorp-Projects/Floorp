#! /usr/bin/python

# this script is meant to run as an Arch post-commit hook (and also as a
# pre-commit hook), using the "arch-meta-hook" framework. See
# http://wiki.gnuarch.org/NdimMetaHook for details. The pre-commit hook
# creates a list of files (and log comments), while the post-commit hook
# actually notifies the buildmaster.

# this script doesn't handle partial commits quite right: it will tell the
# buildmaster that everything changed, not just the filenames you give to
# 'tla commit'.

import os, commands, cStringIO
from buildbot.scripts import runner

# Just modify the appropriate values below and then put this file in two
# places: ~/.arch-params/hooks/ARCHIVE/=precommit/90buildbot.py and
# ~/.arch-params/hooks/ARCHIVE/=commit/10buildbot.py

master = "localhost:9989"
username = "myloginname"

# Remember that for this to work, your buildmaster's master.cfg needs to have
# a c['sources'] list which includes a pb.PBChangeSource instance.

os.chdir(os.getenv("ARCH_TREE_ROOT"))
filelist = ",,bb-files"
commentfile = ",,bb-comments"

if os.getenv("ARCH_HOOK_ACTION") == "precommit":
    files = []
    out = commands.getoutput("tla changes")
    for line in cStringIO.StringIO(out).readlines():
        if line[0] in "AMD": # add, modify, delete
            files.append(line[3:])
    if files:
        f = open(filelist, "w")
        f.write("".join(files))
        f.close()
    # comments
    logfiles = [f for f in os.listdir(".") if f.startswith("++log.")]
    if len(logfiles) > 1:
        print ("Warning, multiple ++log.* files found, getting comments "
               "from the first one")
    if logfiles:
        open(commentfile, "w").write(open(logfiles[0], "r").read())

elif os.getenv("ARCH_HOOK_ACTION") == "commit":
    revision = os.getenv("ARCH_REVISION")

    files = []
    if os.path.exists(filelist):
        f = open(filelist, "r")
        for line in f.readlines():
            files.append(line.rstrip())
    if not files:
        # buildbot insists upon having at least one modified file (otherwise
        # the prefix-stripping mechanism will ignore the change)
        files = ["dummy"]

    if os.path.exists(commentfile):
        comments = open(commentfile, "r").read()
    else:
        comments = "commit from arch"

    c = {'master': master, 'username': username,
         'revision': revision, 'comments': comments, 'files': files}
    runner.sendchange(c, True)

    if os.path.exists(filelist):
        os.unlink(filelist)
    if os.path.exists(commentfile):
        os.unlink(commentfile)
