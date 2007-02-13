#! /usr/bin/python

"""
This is an example of how to use the remote ChangeMaster interface, which is
a port that allows a remote program to inject Changes into the buildmaster.

The buildmaster can either pull changes in from external sources (see
buildbot.changes.changes.ChangeMaster.addSource for an example), or those
changes can be pushed in from outside. This script shows how to do the
pushing.

Changes are just dictionaries with three keys:

 'who': a simple string with a username. Responsibility for this change will
 be assigned to the named user (if something goes wrong with the build, they
 will be blamed for it).

 'files': a list of strings, each with a filename relative to the top of the
 source tree.

 'comments': a (multiline) string with checkin comments.

Each call to .addChange injects a single Change object: each Change
represents multiple files, all changed by the same person, and all with the
same checkin comments.

The port that this script connects to is the same 'slavePort' that the
buildslaves and other debug tools use. The ChangeMaster service will only be
available on that port if 'change' is in the list of services passed to
buildbot.master.makeApp (this service is turned ON by default).
"""

import sys
from twisted.spread import pb
from twisted.cred import credentials
from twisted.internet import reactor
from twisted.python import log
import commands, random, os.path

def done(*args):
    reactor.stop()

users = ('zaphod', 'arthur', 'trillian', 'marvin', 'sbfast')
dirs = ('src', 'doc', 'tests')
sources = ('foo.c', 'bar.c', 'baz.c', 'Makefile')
docs = ('Makefile', 'index.html', 'manual.texinfo')

def makeFilename():
    d = random.choice(dirs)
    if d in ('src', 'tests'):
        f = random.choice(sources)
    else:
        f = random.choice(docs)
    return os.path.join(d, f)
        

def send_change(remote):
    who = random.choice(users)
    if len(sys.argv) > 1:
        files = sys.argv[1:]
    else:
        files = [makeFilename()]
    comments = commands.getoutput("fortune")
    change = {'who': who, 'files': files, 'comments': comments}
    d = remote.callRemote('addChange', change)
    d.addCallback(done)
    print "%s: %s" % (who, " ".join(files))


f = pb.PBClientFactory()
d = f.login(credentials.UsernamePassword("change", "changepw"))
reactor.connectTCP("localhost", 8007, f)
err = lambda f: (log.err(), reactor.stop())
d.addCallback(send_change).addErrback(err)

reactor.run()
