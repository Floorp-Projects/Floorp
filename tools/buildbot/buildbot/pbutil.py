
"""Base classes handy for use with PB clients.
"""

from twisted.spread import pb

from twisted.spread.pb import PBClientFactory
from twisted.internet import protocol
from twisted.python import log

class NewCredPerspective(pb.Avatar):
    def attached(self, mind):
        return self
    def detached(self, mind):
        pass

class ReconnectingPBClientFactory(PBClientFactory,
                                  protocol.ReconnectingClientFactory):
    """Reconnecting client factory for PB brokers.

    Like PBClientFactory, but if the connection fails or is lost, the factory
    will attempt to reconnect.

    Instead of using f.getRootObject (which gives a Deferred that can only
    be fired once), override the gotRootObject method.

    Instead of using the newcred f.login (which is also one-shot), call
    f.startLogin() with the credentials and client, and override the
    gotPerspective method.

    Instead of using the oldcred f.getPerspective (also one-shot), call
    f.startGettingPerspective() with the same arguments, and override
    gotPerspective.

    gotRootObject and gotPerspective will be called each time the object is
    received (once per successful connection attempt). You will probably want
    to use obj.notifyOnDisconnect to find out when the connection is lost.

    If an authorization error occurs, failedToGetPerspective() will be
    invoked.

    To use me, subclass, then hand an instance to a connector (like
    TCPClient).
    """

    def __init__(self):
        PBClientFactory.__init__(self)
        self._doingLogin = False
        self._doingGetPerspective = False

    def clientConnectionFailed(self, connector, reason):
        PBClientFactory.clientConnectionFailed(self, connector, reason)
        # Twisted-1.3 erroneously abandons the connection on non-UserErrors.
        # To avoid this bug, don't upcall, and implement the correct version
        # of the method here.
        if self.continueTrying:
            self.connector = connector
            self.retry()

    def clientConnectionLost(self, connector, reason):
        PBClientFactory.clientConnectionLost(self, connector, reason,
                                             reconnecting=True)
        RCF = protocol.ReconnectingClientFactory
        RCF.clientConnectionLost(self, connector, reason)

    def clientConnectionMade(self, broker):
        self.resetDelay()
        PBClientFactory.clientConnectionMade(self, broker)
        if self._doingLogin:
            self.doLogin(self._root)
        if self._doingGetPerspective:
            self.doGetPerspective(self._root)
        self.gotRootObject(self._root)

    def __getstate__(self):
        # this should get folded into ReconnectingClientFactory
        d = self.__dict__.copy()
        d['connector'] = None
        d['_callID'] = None
        return d

    # oldcred methods

    def getPerspective(self, *args):
        raise RuntimeError, "getPerspective is one-shot: use startGettingPerspective instead"

    def startGettingPerspective(self, username, password, serviceName,
                                perspectiveName=None, client=None):
        self._doingGetPerspective = True
        if perspectiveName == None:
            perspectiveName = username
        self._oldcredArgs = (username, password, serviceName,
                             perspectiveName, client)

    def doGetPerspective(self, root):
        # oldcred getPerspective()
        (username, password,
         serviceName, perspectiveName, client) = self._oldcredArgs
        d = self._cbAuthIdentity(root, username, password)
        d.addCallback(self._cbGetPerspective,
                      serviceName, perspectiveName, client)
        d.addCallbacks(self.gotPerspective, self.failedToGetPerspective)


    # newcred methods

    def login(self, *args):
        raise RuntimeError, "login is one-shot: use startLogin instead"

    def startLogin(self, credentials, client=None):
        self._credentials = credentials
        self._client = client
        self._doingLogin = True

    def doLogin(self, root):
        # newcred login()
        d = self._cbSendUsername(root, self._credentials.username,
                                 self._credentials.password, self._client)
        d.addCallbacks(self.gotPerspective, self.failedToGetPerspective)


    # methods to override

    def gotPerspective(self, perspective):
        """The remote avatar or perspective (obtained each time this factory
        connects) is now available."""
        pass

    def gotRootObject(self, root):
        """The remote root object (obtained each time this factory connects)
        is now available. This method will be called each time the connection
        is established and the object reference is retrieved."""
        pass

    def failedToGetPerspective(self, why):
        """The login process failed, most likely because of an authorization
        failure (bad password), but it is also possible that we lost the new
        connection before we managed to send our credentials.
        """
        log.msg("ReconnectingPBClientFactory.failedToGetPerspective")
        if why.check(pb.PBConnectionLost):
            log.msg("we lost the brand-new connection")
            # retrying might help here, let clientConnectionLost decide
            return
        # probably authorization
        self.stopTrying() # logging in harder won't help
        log.err(why)
