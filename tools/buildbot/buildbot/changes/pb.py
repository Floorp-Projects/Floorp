# -*- test-case-name: buildbot.test.test_changes -*-

from twisted.python import log

from buildbot.pbutil import NewCredPerspective
from buildbot.changes import base, changes

class ChangePerspective(NewCredPerspective):

    def __init__(self, changemaster, prefix):
        self.changemaster = changemaster
        self.prefix = prefix

    def attached(self, mind):
        return self
    def detached(self, mind):
        pass

    def perspective_addChange(self, changedict):
        log.msg("perspective_addChange called")
        pathnames = []
        prefixpaths = None
        for path in changedict['files']:
            if self.prefix:
                if not path.startswith(self.prefix):
                    # this file does not start with the prefix, so ignore it
                    continue
                path = path[len(self.prefix):]
            pathnames.append(path)

        if pathnames:
            change = changes.Change(changedict['who'],
                                    pathnames,
                                    changedict['comments'],
                                    branch=changedict.get('branch'),
                                    revision=changedict.get('revision'),
                                    )
            self.changemaster.addChange(change)

class PBChangeSource(base.ChangeSource):
    compare_attrs = ["user", "passwd", "port", "prefix"]

    def __init__(self, user="change", passwd="changepw", port=None,
                 prefix=None, sep=None):
        """I listen on a TCP port for Changes from 'buildbot sendchange'.

        I am a ChangeSource which will accept Changes from a remote source. I
        share a TCP listening port with the buildslaves.

        Both the 'buildbot sendchange' command and the
        contrib/svn_buildbot.py tool know how to send changes to me.

        @type prefix: string (or None)
        @param prefix: if set, I will ignore any filenames that do not start
                       with this string. Moreover I will remove this string
                       from all filenames before creating the Change object
                       and delivering it to the Schedulers. This is useful
                       for changes coming from version control systems that
                       represent branches as parent directories within the
                       repository (like SVN and Perforce). Use a prefix of
                       'trunk/' or 'project/branches/foobranch/' to only
                       follow one branch and to get correct tree-relative
                       filenames.

        @param sep: DEPRECATED (with an axe). sep= was removed in
                    buildbot-0.7.4 . Instead of using it, you should use
                    prefix= with a trailing directory separator. This
                    docstring (and the better-than-nothing error message
                    which occurs when you use it) will be removed in 0.7.5 .
        """

        # sep= was removed in 0.7.4 . This more-helpful-than-nothing error
        # message will be removed in 0.7.5 .
        assert sep is None, "prefix= is now a complete string, do not use sep="
        # TODO: current limitations
        assert user == "change"
        assert passwd == "changepw"
        assert port == None
        self.user = user
        self.passwd = passwd
        self.port = port
        self.prefix = prefix

    def describe(self):
        # TODO: when the dispatcher is fixed, report the specific port
        #d = "PB listener on port %d" % self.port
        d = "PBChangeSource listener on all-purpose slaveport"
        if self.prefix is not None:
            d += " (prefix '%s')" % self.prefix
        return d

    def startService(self):
        base.ChangeSource.startService(self)
        # our parent is the ChangeMaster object
        # find the master's Dispatch object and register our username
        # TODO: the passwd should be registered here too
        master = self.parent.parent
        master.dispatcher.register(self.user, self)

    def stopService(self):
        base.ChangeSource.stopService(self)
        # unregister our username
        master = self.parent.parent
        master.dispatcher.unregister(self.user)

    def getPerspective(self):
        return ChangePerspective(self.parent, self.prefix)

