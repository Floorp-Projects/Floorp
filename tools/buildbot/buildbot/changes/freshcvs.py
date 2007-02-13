#! /usr/bin/python

import os.path

from twisted.cred import credentials
from twisted.spread import pb
from twisted.application.internet import TCPClient
from twisted.python import log

import cvstoys.common # to make sure VersionedPatch gets registered

from buildbot.twcompat import implements
from buildbot.interfaces import IChangeSource
from buildbot.pbutil import ReconnectingPBClientFactory
from buildbot.changes.changes import Change
from buildbot import util

class FreshCVSListener(pb.Referenceable):
    def remote_notify(self, root, files, message, user):
        try:
            self.source.notify(root, files, message, user)
        except Exception, e:
            print "notify failed"
            log.err()

    def remote_goodbye(self, message):
        pass

class FreshCVSConnectionFactory(ReconnectingPBClientFactory):

    def gotPerspective(self, perspective):
        log.msg("connected to FreshCVS daemon")
        ReconnectingPBClientFactory.gotPerspective(self, perspective)
        self.source.connected = True
        # TODO: freshcvs-1.0.10 doesn't handle setFilter correctly, it will
        # be fixed in the upcoming 1.0.11 . I haven't been able to test it
        # to make sure the failure mode is survivable, so I'll just leave
        # this out for now.
        return
        if self.source.prefix is not None:
            pathfilter = "^%s" % self.source.prefix
            d = perspective.callRemote("setFilter",
                                       None, pathfilter, None)
            # ignore failures, setFilter didn't work in 1.0.10 and this is
            # just an optimization anyway
            d.addErrback(lambda f: None)

    def clientConnectionLost(self, connector, reason):
        ReconnectingPBClientFactory.clientConnectionLost(self, connector,
                                                         reason)
        self.source.connected = False

class FreshCVSSourceNewcred(TCPClient, util.ComparableMixin):
    """This source will connect to a FreshCVS server associated with one or
    more CVS repositories. Each time a change is committed to a repository,
    the server will send us a message describing the change. This message is
    used to build a Change object, which is then submitted to the
    ChangeMaster.

    This class handles freshcvs daemons which use newcred. CVSToys-1.0.9
    does not, later versions might.
    """

    if implements:
        implements(IChangeSource)
    else:
        __implements__ = IChangeSource, TCPClient.__implements__
    compare_attrs = ["host", "port", "username", "password", "prefix"]

    changemaster = None # filled in when we're added
    connected = False

    def __init__(self, host, port, user, passwd, prefix=None):
        self.host = host
        self.port = port
        self.username = user
        self.password = passwd
        if prefix is not None and not prefix.endswith("/"):
            log.msg("WARNING: prefix '%s' should probably end with a slash" \
                    % prefix)
        self.prefix = prefix
        self.listener = l = FreshCVSListener()
        l.source = self
        self.factory = f = FreshCVSConnectionFactory()
        f.source = self
        self.creds = credentials.UsernamePassword(user, passwd)
        f.startLogin(self.creds, client=l)
        TCPClient.__init__(self, host, port, f)

    def __repr__(self):
        return "<FreshCVSSource where=%s, prefix=%s>" % \
               ((self.host, self.port), self.prefix)

    def describe(self):
        online = ""
        if not self.connected:
            online = " [OFFLINE]"
        return "freshcvs %s:%s%s" % (self.host, self.port, online)

    def notify(self, root, files, message, user):
        pathnames = []
        isdir = 0
        for f in files:
            if not isinstance(f, (cvstoys.common.VersionedPatch,
                                  cvstoys.common.Directory)):
                continue
            pathname, filename = f.pathname, f.filename
            #r1, r2 = getattr(f, 'r1', None), getattr(f, 'r2', None)
            if isinstance(f, cvstoys.common.Directory):
                isdir = 1
            path = os.path.join(pathname, filename)
            log.msg("FreshCVS notify '%s'" % path)
            if self.prefix:
                if path.startswith(self.prefix):
                    path = path[len(self.prefix):]
                else:
                    continue
            pathnames.append(path)
        if pathnames:
            # now() is close enough: FreshCVS *is* realtime, after all
            when=util.now()
            c = Change(user, pathnames, message, isdir, when=when)
            self.parent.addChange(c)

class FreshCVSSourceOldcred(FreshCVSSourceNewcred):
    """This is for older freshcvs daemons (from CVSToys-1.0.9 and earlier).
    """

    def __init__(self, host, port, user, passwd,
                 serviceName="cvstoys.notify", prefix=None):
        self.host = host
        self.port = port
        self.prefix = prefix
        self.listener = l = FreshCVSListener()
        l.source = self
        self.factory = f = FreshCVSConnectionFactory()
        f.source = self
        f.startGettingPerspective(user, passwd, serviceName, client=l)
        TCPClient.__init__(self, host, port, f)

    def __repr__(self):
        return "<FreshCVSSourceOldcred where=%s, prefix=%s>" % \
               ((self.host, self.port), self.prefix)

# this is suitable for CVSToys-1.0.10 and later. If you run CVSToys-1.0.9 or
# earlier, use FreshCVSSourceOldcred instead.
FreshCVSSource = FreshCVSSourceNewcred

