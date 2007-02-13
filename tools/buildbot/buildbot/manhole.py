
import os.path
import binascii, base64
from twisted.python import log
from twisted.application import service, strports
from twisted.cred import checkers, portal
from twisted.conch import manhole, telnet, manhole_ssh, checkers as conchc
from twisted.conch.insults import insults
from twisted.internet import protocol

from buildbot.util import ComparableMixin
from zope.interface import implements # requires Twisted-2.0 or later

# makeTelnetProtocol and _TelnetRealm are for the TelnetManhole

class makeTelnetProtocol:
    # this curries the 'portal' argument into a later call to
    # TelnetTransport()
    def __init__(self, portal):
        self.portal = portal

    def __call__(self):
        auth = telnet.AuthenticatingTelnetProtocol
        return telnet.TelnetTransport(auth, self.portal)

class _TelnetRealm:
    implements(portal.IRealm)

    def __init__(self, namespace_maker):
        self.namespace_maker = namespace_maker

    def requestAvatar(self, avatarId, *interfaces):
        if telnet.ITelnetProtocol in interfaces:
            namespace = self.namespace_maker()
            p = telnet.TelnetBootstrapProtocol(insults.ServerProtocol,
                                               manhole.ColoredManhole,
                                               namespace)
            return (telnet.ITelnetProtocol, p, lambda: None)
        raise NotImplementedError()


class chainedProtocolFactory:
    # this curries the 'namespace' argument into a later call to
    # chainedProtocolFactory()
    def __init__(self, namespace):
        self.namespace = namespace
    
    def __call__(self):
        return insults.ServerProtocol(manhole.ColoredManhole, self.namespace)

class AuthorizedKeysChecker(conchc.SSHPublicKeyDatabase):
    """Accept connections using SSH keys from a given file.

    SSHPublicKeyDatabase takes the username that the prospective client has
    requested and attempts to get a ~/.ssh/authorized_keys file for that
    username. This requires root access, so it isn't as useful as you'd
    like.

    Instead, this subclass looks for keys in a single file, given as an
    argument. This file is typically kept in the buildmaster's basedir. The
    file should have 'ssh-dss ....' lines in it, just like authorized_keys.
    """

    def __init__(self, authorized_keys_file):
        self.authorized_keys_file = os.path.expanduser(authorized_keys_file)

    def checkKey(self, credentials):
        f = open(self.authorized_keys_file)
        for l in f.readlines():
            l2 = l.split()
            if len(l2) < 2:
                continue
            try:
                if base64.decodestring(l2[1]) == credentials.blob:
                    return 1
            except binascii.Error:
                continue
        return 0


class _BaseManhole(service.MultiService):
    """This provides remote access to a python interpreter (a read/exec/print
    loop) embedded in the buildmaster via an internal SSH server. This allows
    detailed inspection of the buildmaster state. It is of most use to
    buildbot developers. Connect to this by running an ssh client.
    """

    def __init__(self, port, checker, using_ssh=True):
        """
        @type port: string or int
        @param port: what port should the Manhole listen on? This is a
        strports specification string, like 'tcp:12345' or
        'tcp:12345:interface=127.0.0.1'. Bare integers are treated as a
        simple tcp port.

        @type checker: an object providing the
        L{twisted.cred.checkers.ICredentialsChecker} interface
        @param checker: if provided, this checker is used to authenticate the
        client instead of using the username/password scheme. You must either
        provide a username/password or a Checker. Some useful values are::
            import twisted.cred.checkers as credc
            import twisted.conch.checkers as conchc
            c = credc.AllowAnonymousAccess # completely open
            c = credc.FilePasswordDB(passwd_filename) # file of name:passwd
            c = conchc.UNIXPasswordDatabase # getpwnam() (probably /etc/passwd)

        @type using_ssh: bool
        @param using_ssh: If True, accept SSH connections. If False, accept
                          regular unencrypted telnet connections.
        """

        # unfortunately, these don't work unless we're running as root
        #c = credc.PluggableAuthenticationModulesChecker: PAM
        #c = conchc.SSHPublicKeyDatabase() # ~/.ssh/authorized_keys
        # and I can't get UNIXPasswordDatabase to work

        service.MultiService.__init__(self)
        if type(port) is int:
            port = "tcp:%d" % port
        self.port = port # for comparison later
        self.checker = checker # to maybe compare later

        def makeNamespace():
            # close over 'self' so we can get access to .parent later
            master = self.parent
            namespace = {
                'master': master,
                'status': master.getStatus(),
                }
            return namespace

        def makeProtocol():
            namespace = makeNamespace()
            p = insults.ServerProtocol(manhole.ColoredManhole, namespace)
            return p

        self.using_ssh = using_ssh
        if using_ssh:
            r = manhole_ssh.TerminalRealm()
            r.chainedProtocolFactory = makeProtocol
            p = portal.Portal(r, [self.checker])
            f = manhole_ssh.ConchFactory(p)
        else:
            r = _TelnetRealm(makeNamespace)
            p = portal.Portal(r, [self.checker])
            f = protocol.ServerFactory()
            f.protocol = makeTelnetProtocol(p)
        s = strports.service(self.port, f)
        s.setServiceParent(self)


    def startService(self):
        service.MultiService.startService(self)
        if self.using_ssh:
            via = "via SSH"
        else:
            via = "via telnet"
        log.msg("Manhole listening %s on port %s" % (via, self.port))


class TelnetManhole(_BaseManhole, ComparableMixin):
    """This Manhole accepts unencrypted (telnet) connections, and requires a
    username and password authorize access. You are encouraged to use the
    encrypted ssh-based manhole classes instead."""

    compare_attrs = ["port", "username", "password"]

    def __init__(self, port, username, password):
        """
        @type port: string or int
        @param port: what port should the Manhole listen on? This is a
        strports specification string, like 'tcp:12345' or
        'tcp:12345:interface=127.0.0.1'. Bare integers are treated as a
        simple tcp port.

        @param username:
        @param password: username= and password= form a pair of strings to
                         use when authenticating the remote user.
        """

        self.username = username
        self.password = password

        c = checkers.InMemoryUsernamePasswordDatabaseDontUse()
        c.addUser(username, password)

        _BaseManhole.__init__(self, port, c, using_ssh=False)

class PasswordManhole(_BaseManhole, ComparableMixin):
    """This Manhole accepts encrypted (ssh) connections, and requires a
    username and password to authorize access.
    """

    compare_attrs = ["port", "username", "password"]

    def __init__(self, port, username, password):
        """
        @type port: string or int
        @param port: what port should the Manhole listen on? This is a
        strports specification string, like 'tcp:12345' or
        'tcp:12345:interface=127.0.0.1'. Bare integers are treated as a
        simple tcp port.

        @param username:
        @param password: username= and password= form a pair of strings to
                         use when authenticating the remote user.
        """

        self.username = username
        self.password = password

        c = checkers.InMemoryUsernamePasswordDatabaseDontUse()
        c.addUser(username, password)

        _BaseManhole.__init__(self, port, c)

class AuthorizedKeysManhole(_BaseManhole, ComparableMixin):
    """This Manhole accepts ssh connections, and requires that the
    prospective client have an ssh private key that matches one of the public
    keys in our authorized_keys file. It is created with the name of a file
    that contains the public keys that we will accept."""

    compare_attrs = ["port", "keyfile"]

    def __init__(self, port, keyfile):
        """
        @type port: string or int
        @param port: what port should the Manhole listen on? This is a
        strports specification string, like 'tcp:12345' or
        'tcp:12345:interface=127.0.0.1'. Bare integers are treated as a
        simple tcp port.

        @param keyfile: the name of a file (relative to the buildmaster's
                        basedir) that contains SSH public keys of authorized
                        users, one per line. This is the exact same format
                        as used by sshd in ~/.ssh/authorized_keys .
        """

        # TODO: expanduser this, and make it relative to the buildmaster's
        # basedir
        self.keyfile = keyfile
        c = AuthorizedKeysChecker(keyfile)
        _BaseManhole.__init__(self, port, c)

class ArbitraryCheckerManhole(_BaseManhole, ComparableMixin):
    """This Manhole accepts ssh connections, but uses an arbitrary
    user-supplied 'checker' object to perform authentication."""

    compare_attrs = ["port", "checker"]

    def __init__(self, port, checker):
        """
        @type port: string or int
        @param port: what port should the Manhole listen on? This is a
        strports specification string, like 'tcp:12345' or
        'tcp:12345:interface=127.0.0.1'. Bare integers are treated as a
        simple tcp port.

        @param checker: an instance of a twisted.cred 'checker' which will
                        perform authentication
        """

        _BaseManhole.__init__(self, port, checker)


