
from email.Message import Message
from email.Utils import formatdate

from twisted.internet import defer

from buildbot import interfaces
from buildbot.twcompat import implements
from buildbot.status import base, mail
from buildbot.status.builder import SUCCESS, WARNINGS

import zlib, bz2, base64

# TODO: docs, maybe a test of some sort just to make sure it actually imports
# and can format email without raising an exception.

class TinderboxMailNotifier(mail.MailNotifier):
    """This is a Tinderbox status notifier. It can send e-mail to a number of
    different tinderboxes or people. E-mails are sent at the beginning and
    upon completion of each build. It can be configured to send out e-mails
    for only certain builds.

    The most basic usage is as follows::
        TinderboxMailNotifier(fromaddr="buildbot@localhost",
                              tree="MyTinderboxTree",
                              extraRecipients=["tinderboxdaemon@host.org"])

    The builder name (as specified in master.cfg) is used as the "build"
    tinderbox option.

    """
    if implements:
        implements(interfaces.IEmailSender)
    else:
        __implements__ = (interfaces.IEmailSender,
                          base.StatusReceiverMultiService.__implements__)

    compare_attrs = ["extraRecipients", "fromaddr", "categories", "builders",
                     "addLogs", "relayhost", "subject", "binaryURL", "tree",
                     "logCompression"]

    def __init__(self, fromaddr, tree, extraRecipients,
                 categories=None, builders=None, relayhost="localhost",
                 subject="buildbot %(result)s in %(builder)s", binaryURL="",
                 logCompression=""):
        """
        @type  fromaddr: string
        @param fromaddr: the email address to be used in the 'From' header.

        @type  tree: string
        @param tree: The Tinderbox tree to post to.

        @type  extraRecipients: tuple of string
        @param extraRecipients: E-mail addresses of recipients. This should at
                                least include the tinderbox daemon.

        @type  categories: list of strings
        @param categories: a list of category names to serve status
                           information for. Defaults to None (all
                           categories). Use either builders or categories,
                           but not both.

        @type  builders: list of strings
        @param builders: a list of builder names for which mail should be
                         sent. Defaults to None (send mail for all builds).
                         Use either builders or categories, but not both.

        @type  relayhost: string
        @param relayhost: the host to which the outbound SMTP connection
                          should be made. Defaults to 'localhost'

        @type  subject: string
        @param subject: a string to be used as the subject line of the message.
                        %(builder)s will be replaced with the name of the
                        %builder which provoked the message.
                        This parameter is not significant for the tinderbox
                        daemon.

        @type  binaryURL: string
        @param binaryURL: If specified, this should be the location where final
                          binary for a build is located.
                          (ie. http://www.myproject.org/nightly/08-08-2006.tgz)
                          It will be posted to the Tinderbox.

        @type  logCompression: string
        @param logCompression: The type of compression to use on the log.
                               Valid options are"bzip2" and "gzip". gzip is
                               only known to work on Python 2.4 and above.
        """

        mail.MailNotifier.__init__(self, fromaddr, categories=categories,
                                   builders=builders, relayhost=relayhost,
                                   subject=subject,
                                   extraRecipients=extraRecipients,
                                   sendToInterestedUsers=False)
        self.tree = tree
        self.binaryURL = binaryURL
        self.logCompression = logCompression

    def buildStarted(self, name, build):
        self.buildMessage(name, build, "building")

    def buildMessage(self, name, build, results):
        text = ""
        res = ""
        # shortform
        t = "tinderbox:"

        text += "%s tree: %s\n" % (t, self.tree)
        # the start time
        # getTimes() returns a fractioned time that tinderbox doesn't understand
        text += "%s builddate: %s\n" % (t, int(build.getTimes()[0]))
        text += "%s status: " % t

        if results == "building":
            res = "building"
            text += res
        elif results == SUCCESS:
            res = "success"
            text += res
        elif results == WARNINGS:
            res = "testfailed"
            text += res
        else:
            res += "busted"
            text += res

        text += "\n";

        text += "%s build: %s\n" % (t, name)
        text += "%s errorparser: unix\n" % t # always use the unix errorparser

        # if the build just started...
        if results == "building":
            text += "%s END\n" % t
        # if the build finished...
        else:
            text += "%s binaryurl: %s\n" % (t, self.binaryURL)
            text += "%s logcompression: %s\n" % (t, self.logCompression)

            # logs will always be appended
            tinderboxLogs = ""
            for log in build.getLogs():
                l = ""
                logEncoding = ""
                if self.logCompression == "bzip2":
                    compressedLog = bz2.compress(log.getText())
                    l = base64.encodestring(compressedLog)
                    logEncoding = "base64";
                elif self.logCompression == "gzip":
                    compressedLog = zlib.compress(log.getText())
                    l = base64.encodestring(compressedLog)
                    logEncoding = "base64";
                else:
                    l = log.getText()
                tinderboxLogs += l

            text += "%s logencoding: %s\n" % (t, logEncoding)
            text += "%s END\n\n" % t
            text += tinderboxLogs
            text += "\n"

        m = Message()
        m.set_payload(text)

        m['Date'] = formatdate(localtime=True)
        m['Subject'] = self.subject % { 'result': res,
                                        'builder': name,
                                        }
        m['From'] = self.fromaddr
        # m['To'] is added later

        d = defer.DeferredList([])
        d.addCallback(self._gotRecipients, self.extraRecipients, m)
        return d

