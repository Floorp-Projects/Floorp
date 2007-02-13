# -*- test-case-name: buildbot.test.test_status -*-

# the email.MIMEMultipart module is only available in python-2.2.2 and later

from email.Message import Message
from email.Utils import formatdate
from email.MIMEText import MIMEText
try:
    from email.MIMEMultipart import MIMEMultipart
    canDoAttachments = True
except ImportError:
    canDoAttachments = False
import urllib

from twisted.internet import defer
try:
    from twisted.mail.smtp import sendmail # Twisted-2.0
except ImportError:
    from twisted.protocols.smtp import sendmail # Twisted-1.3
from twisted.python import log

from buildbot import interfaces, util
from buildbot.twcompat import implements, providedBy
from buildbot.status import base
from buildbot.status.builder import FAILURE, SUCCESS, WARNINGS


class Domain(util.ComparableMixin):
    if implements:
        implements(interfaces.IEmailLookup)
    else:
        __implements__ = interfaces.IEmailLookup
    compare_attrs = ["domain"]

    def __init__(self, domain):
        assert "@" not in domain
        self.domain = domain

    def getAddress(self, name):
        return name + "@" + self.domain


class MailNotifier(base.StatusReceiverMultiService):
    """This is a status notifier which sends email to a list of recipients
    upon the completion of each build. It can be configured to only send out
    mail for certain builds, and only send messages when the build fails, or
    when it transitions from success to failure. It can also be configured to
    include various build logs in each message.

    By default, the message will be sent to the Interested Users list, which
    includes all developers who made changes in the build. You can add
    additional recipients with the extraRecipients argument.

    To get a simple one-message-per-build (say, for a mailing list), use
    sendToInterestedUsers=False, extraRecipients=['listaddr@example.org']

    Each MailNotifier sends mail to a single set of recipients. To send
    different kinds of mail to different recipients, use multiple
    MailNotifiers.
    """

    if implements:
        implements(interfaces.IEmailSender)
    else:
        __implements__ = (interfaces.IEmailSender,
                          base.StatusReceiverMultiService.__implements__)

    compare_attrs = ["extraRecipients", "lookup", "fromaddr", "mode",
                     "categories", "builders", "addLogs", "relayhost",
                     "subject", "sendToInterestedUsers"]

    def __init__(self, fromaddr, mode="all", categories=None, builders=None,
                 addLogs=False, relayhost="localhost",
                 subject="buildbot %(result)s in %(builder)s",
                 lookup=None, extraRecipients=[],
                 sendToInterestedUsers=True):
        """
        @type  fromaddr: string
        @param fromaddr: the email address to be used in the 'From' header.
        @type  sendToInterestedUsers: boolean
        @param sendToInterestedUsers: if True (the default), send mail to all 
                                      of the Interested Users. If False, only
                                      send mail to the extraRecipients list.

        @type  extraRecipients: tuple of string
        @param extraRecipients: a list of email addresses to which messages
                                should be sent (in addition to the
                                InterestedUsers list, which includes any
                                developers who made Changes that went into this
                                build). It is a good idea to create a small
                                mailing list and deliver to that, then let
                                subscribers come and go as they please.

        @type  subject: string
        @param subject: a string to be used as the subject line of the message.
                        %(builder)s will be replaced with the name of the
                        builder which provoked the message.

        @type  mode: string (defaults to all)
        @param mode: one of:
                     - 'all': send mail about all builds, passing and failing
                     - 'failing': only send mail about builds which fail
                     - 'problem': only send mail about a build which failed
                     when the previous build passed

        @type  builders: list of strings
        @param builders: a list of builder names for which mail should be
                         sent. Defaults to None (send mail for all builds).
                         Use either builders or categories, but not both.

        @type  categories: list of strings
        @param categories: a list of category names to serve status
                           information for. Defaults to None (all
                           categories). Use either builders or categories,
                           but not both.

        @type  addLogs: boolean.
        @param addLogs: if True, include all build logs as attachments to the
                        messages.  These can be quite large. This can also be
                        set to a list of log names, to send a subset of the
                        logs. Defaults to False.

        @type  relayhost: string
        @param relayhost: the host to which the outbound SMTP connection
                          should be made. Defaults to 'localhost'

        @type  lookup:    implementor of {IEmailLookup}
        @param lookup:    object which provides IEmailLookup, which is
                          responsible for mapping User names (which come from
                          the VC system) into valid email addresses. If not
                          provided, the notifier will only be able to send mail
                          to the addresses in the extraRecipients list. Most of
                          the time you can use a simple Domain instance. As a
                          shortcut, you can pass as string: this will be
                          treated as if you had provided Domain(str). For
                          example, lookup='twistedmatrix.com' will allow mail
                          to be sent to all developers whose SVN usernames
                          match their twistedmatrix.com account names.
        """

        base.StatusReceiverMultiService.__init__(self)
        assert isinstance(extraRecipients, (list, tuple))
        for r in extraRecipients:
            assert isinstance(r, str)
            assert "@" in r # require full email addresses, not User names
        self.extraRecipients = extraRecipients
        self.sendToInterestedUsers = sendToInterestedUsers
        self.fromaddr = fromaddr
        self.mode = mode
        self.categories = categories
        self.builders = builders
        self.addLogs = addLogs
        self.relayhost = relayhost
        self.subject = subject
        if lookup is not None:
            if type(lookup) is str:
                lookup = Domain(lookup)
            assert providedBy(lookup, interfaces.IEmailLookup)
        self.lookup = lookup
        self.watched = []
        self.status = None

        # you should either limit on builders or categories, not both
        if self.builders != None and self.categories != None:
            log.err("Please specify only builders to ignore or categories to include")
            raise # FIXME: the asserts above do not raise some Exception either

    def setServiceParent(self, parent):
        """
        @type  parent: L{buildbot.master.BuildMaster}
        """
        base.StatusReceiverMultiService.setServiceParent(self, parent)
        self.setup()

    def setup(self):
        self.status = self.parent.getStatus()
        self.status.subscribe(self)

    def disownServiceParent(self):
        self.status.unsubscribe(self)
        for w in self.watched:
            w.unsubscribe(self)
        return base.StatusReceiverMultiService.disownServiceParent(self)

    def builderAdded(self, name, builder):
        # only subscribe to builders we are interested in
        if self.categories != None and builder.category not in self.categories:
            return None

        self.watched.append(builder)
        return self # subscribe to this builder

    def builderRemoved(self, name):
        pass

    def builderChangedState(self, name, state):
        pass
    def buildStarted(self, name, build):
        pass
    def buildFinished(self, name, build, results):
        # here is where we actually do something.
        builder = build.getBuilder()
        if self.builders is not None and name not in self.builders:
            return # ignore this build
        if self.categories is not None and \
               builder.category not in self.categories:
            return # ignore this build

        if self.mode == "failing" and results != FAILURE:
            return
        if self.mode == "problem":
            if results != FAILURE:
                return
            prev = build.getPreviousBuild()
            if prev and prev.getResults() == FAILURE:
                return
        # for testing purposes, buildMessage returns a Deferred that fires
        # when the mail has been sent. To help unit tests, we return that
        # Deferred here even though the normal IStatusReceiver.buildFinished
        # signature doesn't do anything with it. If that changes (if
        # .buildFinished's return value becomes significant), we need to
        # rearrange this.
        return self.buildMessage(name, build, results)

    def buildMessage(self, name, build, results):
        text = ""
        if self.mode == "all":
            text += "The Buildbot has finished a build of %s.\n" % name
        elif self.mode == "failing":
            text += "The Buildbot has detected a failed build of %s.\n" % name
        else:
            text += "The Buildbot has detected a new failure of %s.\n" % name
        buildurl = self.status.getURLForThing(build)
        if buildurl:
            text += "Full details are available at:\n %s\n" % buildurl
        text += "\n"

        url = self.status.getBuildbotURL()
        if url:
            text += "Buildbot URL: %s\n\n" % urllib.quote(url, '/:')

        text += "Buildslave for this Build: %s\n\n" % build.getSlavename()
        text += "Build Reason: %s\n" % build.getReason()

        patch = None
        ss = build.getSourceStamp()
        if ss is None:
            source = "unavailable"
        else:
            branch, revision, patch = ss
            source = ""
            if branch:
                source += "[branch %s] " % branch
            if revision:
                source += revision
            else:
                source += "HEAD"
            if patch is not None:
                source += " (plus patch)"
        text += "Build Source Stamp: %s\n" % source

        text += "Blamelist: %s\n" % ",".join(build.getResponsibleUsers())

        # TODO: maybe display changes here? or in an attachment?
        text += "\n"

        t = build.getText()
        if t:
            t = ": " + " ".join(t)
        else:
            t = ""

        if results == SUCCESS:
            text += "Build succeeded!\n"
            res = "success"
        elif results == WARNINGS:
            text += "Build Had Warnings%s\n" % t
            res = "warnings"
        else:
            text += "BUILD FAILED%s\n" % t
            res = "failure"

        if self.addLogs and build.getLogs():
            text += "Logs are attached.\n"

        # TODO: it would be nice to provide a URL for the specific build
        # here. That involves some coordination with html.Waterfall .
        # Ideally we could do:
        #  helper = self.parent.getServiceNamed("html")
        #  if helper:
        #      url = helper.getURLForBuild(build)

        text += "\n"
        text += "sincerely,\n"
        text += " -The Buildbot\n"
        text += "\n"

        haveAttachments = False
        if patch or self.addLogs:
            haveAttachments = True
            if not canDoAttachments:
                log.msg("warning: I want to send mail with attachments, "
                        "but this python is too old to have "
                        "email.MIMEMultipart . Please upgrade to python-2.3 "
                        "or newer to enable addLogs=True")

        if haveAttachments and canDoAttachments:
            m = MIMEMultipart()
            m.attach(MIMEText(text))
        else:
            m = Message()
            m.set_payload(text)

        m['Date'] = formatdate(localtime=True)
        m['Subject'] = self.subject % { 'result': res,
                                        'builder': name,
                                        }
        m['From'] = self.fromaddr
        # m['To'] is added later

        if patch:
            a = MIMEText(patch)
            a.add_header('Content-Disposition', "attachment",
                         filename="source patch")
            m.attach(a)
        if self.addLogs:
            for log in build.getLogs():
                name = "%s.%s" % (log.getStep().getName(),
                                  log.getName())
                a = MIMEText(log.getText())
                a.add_header('Content-Disposition', "attachment",
                             filename=name)
                m.attach(a)

        # now, who is this message going to?
        dl = []
        recipients = self.extraRecipients[:]
        if self.sendToInterestedUsers and self.lookup:
            for u in build.getInterestedUsers():
                d = defer.maybeDeferred(self.lookup.getAddress, u)
                d.addCallback(recipients.append)
                dl.append(d)
        d = defer.DeferredList(dl)
        d.addCallback(self._gotRecipients, recipients, m)
        return d

    def _gotRecipients(self, res, rlist, m):
        recipients = []
        for r in rlist:
            if r is not None and r not in recipients:
                recipients.append(r)
        recipients.sort()
        m['To'] = ", ".join(recipients)
        return self.sendMessage(m, recipients)

    def sendMessage(self, m, recipients):
        s = m.as_string()
        ds = []
        log.msg("sending mail (%d bytes) to" % len(s), recipients)
        for recip in recipients:
            ds.append(sendmail(self.relayhost, self.fromaddr, recip, s))
        return defer.DeferredList(ds)
