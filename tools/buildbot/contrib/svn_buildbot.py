#!/usr/bin/python

# this requires python >=2.3 for the 'sets' module.

# The sets.py from python-2.3 appears to work fine under python2.2 . To
# install this script on a host with only python2.2, copy
# /usr/lib/python2.3/sets.py from a newer python into somewhere on your
# PYTHONPATH, then edit the #! line above to invoke python2.2

# python2.1 is right out

# If you run this program as part of your SVN post-commit hooks, it will
# deliver Change notices to a buildmaster that is running a PBChangeSource
# instance.

# edit your svn-repository/hooks/post-commit file, and add lines that look
# like this:

'''
# set up PYTHONPATH to contain Twisted/buildbot perhaps, if not already
# installed site-wide
. ~/.environment

/path/to/svn_buildbot.py --repository "$REPOS" --revision "$REV" --bbserver localhost --bbport 9989
'''

import commands, sys, os
import re
import sets

# We have hackish "-d" handling here rather than in the Options
# subclass below because a common error will be to not have twisted in
# PYTHONPATH; we want to be able to print that error to the log if
# debug mode is on, so we set it up before the imports.

DEBUG = None

if '-d' in sys.argv:
    i = sys.argv.index('-d')
    DEBUG = sys.argv[i+1]
    del sys.argv[i]
    del sys.argv[i]

if DEBUG:
    f = open(DEBUG, 'a')
    sys.stderr = f
    sys.stdout = f

from twisted.internet import defer, reactor
from twisted.python import usage
from twisted.spread import pb
from twisted.cred import credentials

class Options(usage.Options):
    optParameters = [
        ['repository', 'r', None,
         "The repository that was changed."],
        ['revision', 'v', None,
         "The revision that we want to examine (default: latest)"],
        ['bbserver', 's', 'localhost',
         "The hostname of the server that buildbot is running on"],
        ['bbport', 'p', 8007,
         "The port that buildbot is listening on"],
        ['include', 'f', None,
         '''\
Search the list of changed files for this regular expression, and if there is
at least one match notify buildbot; otherwise buildbot will not do a build.
You may provide more than one -f argument to try multiple
patterns.  If no filter is given, buildbot will always be notified.'''],
        ['filter', 'f', None, "Same as --include.  (Deprecated)"],
        ['exclude', 'F', None,
         '''\
The inverse of --filter.  Changed files matching this expression will never  
be considered for a build.  
You may provide more than one -F argument to try multiple
patterns.  Excludes override includes, that is, patterns that match both an
include and an exclude will be excluded.'''],
        ]
    optFlags = [
        ['dryrun', 'n', "Do not actually send changes"],
        ]

    def __init__(self):
        usage.Options.__init__(self)
        self._includes = []
        self._excludes = []
        self['includes'] = None
        self['excludes'] = None

    def opt_include(self, arg):
        self._includes.append('.*%s.*' % (arg,))
    opt_filter = opt_include

    def opt_exclude(self, arg):
        self._excludes.append('.*%s.*' % (arg,))

    def postOptions(self):
        if self['repository'] is None:
            raise usage.error("You must pass --repository")
        if self._includes:
            self['includes'] = '(%s)' % ('|'.join(self._includes),)
        if self._excludes:
            self['excludes'] = '(%s)' % ('|'.join(self._excludes),)

def split_file_dummy(changed_file):
    """Split the repository-relative filename into a tuple of (branchname,
    branch_relative_filename). If you have no branches, this should just
    return (None, changed_file).
    """
    return (None, changed_file)

# this version handles repository layouts that look like:
#  trunk/files..                  -> trunk
#  branches/branch1/files..       -> branches/branch1
#  branches/branch2/files..       -> branches/branch2
#
def split_file_branches(changed_file):
    pieces = changed_file.split(os.sep)
    if pieces[0] == 'branches':
        return (os.path.join(*pieces[:2]),
                os.path.join(*pieces[2:]))
    if pieces[0] == 'trunk':
        return (pieces[0], os.path.join(*pieces[1:]))
    ## there are other sibilings of 'trunk' and 'branches'. Pretend they are
    ## all just funny-named branches, and let the Schedulers ignore them.
    #return (pieces[0], os.path.join(*pieces[1:]))

    raise RuntimeError("cannot determine branch for '%s'" % changed_file)

split_file = split_file_dummy


class ChangeSender:

    def getChanges(self, opts):
        """Generate and stash a list of Change dictionaries, ready to be sent
        to the buildmaster's PBChangeSource."""

        # first we extract information about the files that were changed
        repo = opts['repository']
        print "Repo:", repo
        rev_arg = ''
        if opts['revision']:
            rev_arg = '-r %s' % (opts['revision'],)
        changed = commands.getoutput('svnlook changed %s "%s"' % (rev_arg,
                                                                  repo)
                                     ).split('\n')
        # the first 4 columns can contain status information
        changed = [x[4:] for x in changed]

        message = commands.getoutput('svnlook log %s "%s"' % (rev_arg, repo))
        who = commands.getoutput('svnlook author %s "%s"' % (rev_arg, repo))
        revision = opts.get('revision')
        if revision is not None:
            revision = int(revision)

        # see if we even need to notify buildbot by looking at filters first
        changestring = '\n'.join(changed)
        fltpat = opts['includes']
        if fltpat:
            included = sets.Set(re.findall(fltpat, changestring))
        else:
            included = sets.Set(changed)

        expat = opts['excludes']
        if expat:
            excluded = sets.Set(re.findall(expat, changestring))
        else:
            excluded = sets.Set([])
        if len(included.difference(excluded)) == 0:
            print changestring
            print """\
    Buildbot was not interested, no changes matched any of these filters:\n %s
    or all the changes matched these exclusions:\n %s\
    """ % (fltpat, expat)
            sys.exit(0)

        # now see which branches are involved
        files_per_branch = {}
        for f in changed:
            branch, filename = split_file(f)
            if files_per_branch.has_key(branch):
                files_per_branch[branch].append(filename)
            else:
                files_per_branch[branch] = [filename]

        # now create the Change dictionaries
        changes = []
        for branch in files_per_branch.keys():
            d = {'who': who,
                 'branch': branch,
                 'files': files_per_branch[branch],
                 'comments': message,
                 'revision': revision}
            changes.append(d)

        return changes

    def sendChanges(self, opts, changes):
        pbcf = pb.PBClientFactory()
        reactor.connectTCP(opts['bbserver'], int(opts['bbport']), pbcf)
        d = pbcf.login(credentials.UsernamePassword('change', 'changepw'))
        d.addCallback(self.sendAllChanges, changes)
        return d

    def sendAllChanges(self, remote, changes):
        dl = [remote.callRemote('addChange', change)
              for change in changes]
        return defer.DeferredList(dl)

    def run(self):
        opts = Options()
        try:
            opts.parseOptions()
        except usage.error, ue:
            print opts
            print "%s: %s" % (sys.argv[0], ue)
            sys.exit()

        changes = self.getChanges(opts)
        if opts['dryrun']:
            for i,c in enumerate(changes):
                print "CHANGE #%d" % (i+1)
                keys = c.keys()
                keys.sort()
                for k in keys:
                    print "[%10s]: %s" % (k, c[k])
            print "*NOT* sending any changes"
            return

        d = self.sendChanges(opts, changes)

        def quit(*why):
            print "quitting! because", why
            reactor.stop()

        def failed(f):
            print "FAILURE"
            print f
            reactor.stop()

        d.addCallback(quit, "SUCCESS")
        d.addErrback(failed)
        reactor.callLater(60, quit, "TIMEOUT")
        reactor.run()

if __name__ == '__main__':
    s = ChangeSender()
    s.run()


