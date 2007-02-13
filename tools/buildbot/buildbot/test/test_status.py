# -*- test-case-name: buildbot.test.test_status -*-

import email, os

from twisted.internet import defer, reactor
from twisted.trial import unittest

from buildbot import interfaces
from buildbot.sourcestamp import SourceStamp
from buildbot.process.base import BuildRequest
from buildbot.twcompat import implements, providedBy, maybeWait
from buildbot.status import builder, base

mail = None
try:
    from buildbot.status import mail
except ImportError:
    pass
from buildbot.status import progress, client # NEEDS COVERAGE
from buildbot.test.runutils import RunMixin

class MyStep:
    build = None
    def getName(self):
        return "step"

class MyLogFileProducer(builder.LogFileProducer):
    # The reactor.callLater(0) in LogFileProducer.resumeProducing is a bit of
    # a nuisance from a testing point of view. This subclass adds a Deferred
    # to that call so we can find out when it is complete.
    def resumeProducing(self):
        d = defer.Deferred()
        reactor.callLater(0, self._resumeProducing, d)
        return d
    def _resumeProducing(self, d):
        builder.LogFileProducer._resumeProducing(self)
        reactor.callLater(0, d.callback, None)

class MyLog(builder.LogFile):
    def __init__(self, basedir, name, text=None, step=None):
        self.fakeBuilderBasedir = basedir
        if not step:
            step = MyStep()
        builder.LogFile.__init__(self, step, name, name)
        if text:
            self.addStdout(text)
            self.finish()
    def getFilename(self):
        return os.path.join(self.fakeBuilderBasedir, self.name)

    def subscribeConsumer(self, consumer):
        p = MyLogFileProducer(self, consumer)
        d = p.resumeProducing()
        return d

class MyHTMLLog(builder.HTMLLogFile):
    def __init__(self, basedir, name, html):
        step = MyStep()
        builder.HTMLLogFile.__init__(self, step, name, name, html)

class MyLogSubscriber:
    def __init__(self):
        self.chunks = []
    def logChunk(self, build, step, log, channel, text):
        self.chunks.append((channel, text))

class MyLogConsumer:
    def __init__(self, limit=None):
        self.chunks = []
        self.finished = False
        self.limit = limit
    def registerProducer(self, producer, streaming):
        self.producer = producer
        self.streaming = streaming
    def unregisterProducer(self):
        self.producer = None
    def writeChunk(self, chunk):
        self.chunks.append(chunk)
        if self.limit:
            self.limit -= 1
            if self.limit == 0:
                self.producer.pauseProducing()
    def finish(self):
        self.finished = True

if mail:
    class MyMailer(mail.MailNotifier):
        def sendMessage(self, m, recipients):
            self.parent.messages.append((m, recipients))

class MyStatus:
    def getBuildbotURL(self):
        return self.url
    def getURLForThing(self, thing):
        return None

class MyBuilder(builder.BuilderStatus):
    nextBuildNumber = 0

class MyBuild(builder.BuildStatus):
    testlogs = []
    def __init__(self, parent, number, results):
        builder.BuildStatus.__init__(self, parent, number)
        self.results = results
        self.source = SourceStamp(revision="1.14")
        self.reason = "build triggered by changes"
        self.finished = True
    def getLogs(self):
        return self.testlogs

class MyLookup:
    if implements:
        implements(interfaces.IEmailLookup)
    else:
        __implements__ = interfaces.IEmailLookup,

    def getAddress(self, user):
        d = defer.Deferred()
        # With me now is Mr Thomas Walters of West Hartlepool who is totally
        # invisible.
        if user == "Thomas_Walters":
            d.callback(None)
        else:
            d.callback(user + "@" + "dev.com")
        return d

class Mail(unittest.TestCase):

    def setUp(self):
        self.builder = MyBuilder("builder1")

    def stall(self, res, timeout):
        d = defer.Deferred()
        reactor.callLater(timeout, d.callback, res)
        return d

    def makeBuild(self, number, results):
        return MyBuild(self.builder, number, results)

    def failUnlessIn(self, substring, string):
        self.failUnless(string.find(substring) != -1)

    def getBuildbotURL(self):
        return "BUILDBOT_URL"

    def getURLForThing(self, thing):
        return None

    def testBuild1(self):
        mailer = MyMailer(fromaddr="buildbot@example.com",
                          extraRecipients=["recip@example.com",
                                           "recip2@example.com"],
                          lookup=mail.Domain("dev.com"))
        mailer.parent = self
        mailer.status = self
        self.messages = []

        b1 = self.makeBuild(3, builder.SUCCESS)
        b1.blamelist = ["bob"]

        mailer.buildFinished("builder1", b1, b1.results)
        self.failUnless(len(self.messages) == 1)
        m,r = self.messages.pop()
        t = m.as_string()
        self.failUnlessIn("To: bob@dev.com, recip2@example.com, "
                          "recip@example.com\n", t)
        self.failUnlessIn("From: buildbot@example.com\n", t)
        self.failUnlessIn("Subject: buildbot success in builder1\n", t)
        self.failUnlessIn("Date: ", t)
        self.failUnlessIn("Build succeeded!\n", t)
        self.failUnlessIn("Buildbot URL: BUILDBOT_URL\n", t)

    def testBuild2(self):
        mailer = MyMailer(fromaddr="buildbot@example.com",
                          extraRecipients=["recip@example.com",
                                           "recip2@example.com"],
                          lookup="dev.com",
                          sendToInterestedUsers=False)
        mailer.parent = self
        mailer.status = self
        self.messages = []

        b1 = self.makeBuild(3, builder.SUCCESS)
        b1.blamelist = ["bob"]

        mailer.buildFinished("builder1", b1, b1.results)
        self.failUnless(len(self.messages) == 1)
        m,r = self.messages.pop()
        t = m.as_string()
        self.failUnlessIn("To: recip2@example.com, "
                          "recip@example.com\n", t)
        self.failUnlessIn("From: buildbot@example.com\n", t)
        self.failUnlessIn("Subject: buildbot success in builder1\n", t)
        self.failUnlessIn("Build succeeded!\n", t)
        self.failUnlessIn("Buildbot URL: BUILDBOT_URL\n", t)

    def testBuildStatusCategory(self):
        # a status client only interested in a category should only receive
        # from that category
        mailer = MyMailer(fromaddr="buildbot@example.com",
                          extraRecipients=["recip@example.com",
                                           "recip2@example.com"],
                          lookup="dev.com",
                          sendToInterestedUsers=False,
                          categories=["debug"])

        mailer.parent = self
        mailer.status = self
        self.messages = []

        b1 = self.makeBuild(3, builder.SUCCESS)
        b1.blamelist = ["bob"]

        mailer.buildFinished("builder1", b1, b1.results)
        self.failIf(self.messages)

    def testBuilderCategory(self):
        # a builder in a certain category should notify status clients that
        # did not list categories, or categories including this one
        mailer1 = MyMailer(fromaddr="buildbot@example.com",
                           extraRecipients=["recip@example.com",
                                            "recip2@example.com"],
                           lookup="dev.com",
                           sendToInterestedUsers=False)
        mailer2 = MyMailer(fromaddr="buildbot@example.com",
                           extraRecipients=["recip@example.com",
                                            "recip2@example.com"],
                           lookup="dev.com",
                           sendToInterestedUsers=False,
                           categories=["active"])
        mailer3 = MyMailer(fromaddr="buildbot@example.com",
                           extraRecipients=["recip@example.com",
                                            "recip2@example.com"],
                           lookup="dev.com",
                           sendToInterestedUsers=False,
                           categories=["active", "debug"])

        builderd = MyBuilder("builder2", "debug")

        mailer1.parent = self
        mailer1.status = self
        mailer2.parent = self
        mailer2.status = self
        mailer3.parent = self
        mailer3.status = self
        self.messages = []

        t = mailer1.builderAdded("builder2", builderd)
        self.assertEqual(len(mailer1.watched), 1)
        self.assertEqual(t, mailer1)
        t = mailer2.builderAdded("builder2", builderd)
        self.assertEqual(len(mailer2.watched), 0)
        self.assertEqual(t, None)
        t = mailer3.builderAdded("builder2", builderd)
        self.assertEqual(len(mailer3.watched), 1)
        self.assertEqual(t, mailer3)

        b2 = MyBuild(builderd, 3, builder.SUCCESS)
        b2.blamelist = ["bob"]

        mailer1.buildFinished("builder2", b2, b2.results)
        self.failUnlessEqual(len(self.messages), 1)
        self.messages = []
        mailer2.buildFinished("builder2", b2, b2.results)
        self.failUnlessEqual(len(self.messages), 0)
        self.messages = []
        mailer3.buildFinished("builder2", b2, b2.results)
        self.failUnlessEqual(len(self.messages), 1)

    def testFailure(self):
        mailer = MyMailer(fromaddr="buildbot@example.com", mode="problem",
                          extraRecipients=["recip@example.com",
                                           "recip2@example.com"],
                          lookup=MyLookup())
        mailer.parent = self
        mailer.status = self
        self.messages = []

        b1 = self.makeBuild(3, builder.SUCCESS)
        b1.blamelist = ["dev1", "dev2"]
        b2 = self.makeBuild(4, builder.FAILURE)
        b2.setText(["snarkleack", "polarization", "failed"])
        b2.blamelist = ["dev3", "dev3", "dev3", "dev4",
                        "Thomas_Walters"]
        mailer.buildFinished("builder1", b1, b1.results)
        self.failIf(self.messages)
        mailer.buildFinished("builder1", b2, b2.results)
        self.failUnless(len(self.messages) == 1)
        m,r = self.messages.pop()
        t = m.as_string()
        self.failUnlessIn("To: dev3@dev.com, dev4@dev.com, "
                          "recip2@example.com, recip@example.com\n", t)
        self.failUnlessIn("From: buildbot@example.com\n", t)
        self.failUnlessIn("Subject: buildbot failure in builder1\n", t)
        self.failUnlessIn("The Buildbot has detected a new failure", t)
        self.failUnlessIn("BUILD FAILED: snarkleack polarization failed\n", t)
        self.failUnlessEqual(r, ["dev3@dev.com", "dev4@dev.com",
                                 "recip2@example.com", "recip@example.com"])

    def testLogs(self):
        basedir = "test_status_logs"
        os.mkdir(basedir)
        mailer = MyMailer(fromaddr="buildbot@example.com", addLogs=True,
                          extraRecipients=["recip@example.com",
                                           "recip2@example.com"])
        mailer.parent = self
        mailer.status = self
        self.messages = []

        b1 = self.makeBuild(3, builder.WARNINGS)
        b1.testlogs = [MyLog(basedir, 'compile', "Compile log here\n"),
                       MyLog(basedir,
                             'test', "Test log here\nTest 4 failed\n"),
                   ]
        b1.text = ["unusual", "gnarzzler", "output"]
        mailer.buildFinished("builder1", b1, b1.results)
        self.failUnless(len(self.messages) == 1)
        m,r = self.messages.pop()
        t = m.as_string()
        self.failUnlessIn("Subject: buildbot warnings in builder1\n", t)
        m2 = email.message_from_string(t)
        p = m2.get_payload()
        self.failUnlessEqual(len(p), 3)

        self.failUnlessIn("Build Had Warnings: unusual gnarzzler output\n",
                          p[0].get_payload())

        self.failUnlessEqual(p[1].get_filename(), "step.compile")
        self.failUnlessEqual(p[1].get_payload(), "Compile log here\n")
        
        self.failUnlessEqual(p[2].get_filename(), "step.test")
        self.failUnlessIn("Test log here\n", p[2].get_payload())

    def testMail(self):
        basedir = "test_status_mail"
        os.mkdir(basedir)
        dest = os.environ.get("BUILDBOT_TEST_MAIL")
        if not dest:
            raise unittest.SkipTest("define BUILDBOT_TEST_MAIL=dest to run this")
        mailer = mail.MailNotifier(fromaddr="buildbot@example.com",
                                   addLogs=True,
                                   extraRecipients=[dest])
        s = MyStatus()
        s.url = "project URL"
        mailer.status = s

        b1 = self.makeBuild(3, builder.SUCCESS)
        b1.testlogs = [MyLog(basedir, 'compile', "Compile log here\n"),
                       MyLog(basedir,
                             'test', "Test log here\nTest 4 failed\n"),
                   ]

        print "sending mail to", dest
        d = mailer.buildFinished("builder1", b1, b1.results)
        # When this fires, the mail has been sent, but the SMTP connection is
        # still up (because smtp.sendmail relies upon the server to hang up).
        # Spin for a moment to avoid the "unclean reactor" warning that Trial
        # gives us if we finish before the socket is disconnected. Really,
        # sendmail() ought to hang up the connection once it is finished:
        # otherwise a malicious SMTP server could make us consume lots of
        # memory.
        d.addCallback(self.stall, 0.1)
        return maybeWait(d)

if not mail:
    Mail.skip = "the Twisted Mail package is not installed"

class Progress(unittest.TestCase):
    def testWavg(self):
        bp = progress.BuildProgress([])
        e = progress.Expectations(bp)
        # wavg(old, current)
        self.failUnlessEqual(e.wavg(None, None), None)
        self.failUnlessEqual(e.wavg(None, 3), 3)
        self.failUnlessEqual(e.wavg(3, None), 3)
        self.failUnlessEqual(e.wavg(3, 4), 3.5)
        e.decay = 0.1
        self.failUnlessEqual(e.wavg(3, 4), 3.1)


class Results(unittest.TestCase):

    def testAddResults(self):
        b = builder.BuildStatus(builder.BuilderStatus("test"), 12)
        testname = ("buildbot", "test", "test_status", "Results",
                    "testAddResults")
        r1 = builder.TestResult(name=testname,
                                results=builder.SUCCESS,
                                text=["passed"],
                                logs={'output': ""},
                                )
        b.addTestResult(r1)

        res = b.getTestResults()
        self.failUnlessEqual(res.keys(), [testname])
        t = res[testname]
        self.failUnless(providedBy(t, interfaces.ITestResult))
        self.failUnlessEqual(t.getName(), testname)
        self.failUnlessEqual(t.getResults(), builder.SUCCESS)
        self.failUnlessEqual(t.getText(), ["passed"])
        self.failUnlessEqual(t.getLogs(), {'output': ""})

class Log(unittest.TestCase):
    def setUpClass(self):
        self.basedir = "status_log_add"
        os.mkdir(self.basedir)

    def testAdd(self):
        l = MyLog(self.basedir, "compile", step=13)
        self.failUnlessEqual(l.getName(), "compile")
        self.failUnlessEqual(l.getStep(), 13)
        l.addHeader("HEADER\n")
        l.addStdout("Some text\n")
        l.addStderr("Some error\n")
        l.addStdout("Some more text\n")
        self.failIf(l.isFinished())
        l.finish()
        self.failUnless(l.isFinished())
        self.failUnlessEqual(l.getText(),
                             "Some text\nSome error\nSome more text\n")
        self.failUnlessEqual(l.getTextWithHeaders(),
                             "HEADER\n" +
                             "Some text\nSome error\nSome more text\n")
        self.failUnlessEqual(len(list(l.getChunks())), 4)

        self.failUnless(l.hasContents())
        os.unlink(l.getFilename())
        self.failIf(l.hasContents())

    def TODO_testDuplicate(self):
        # create multiple logs for the same step with the same logname, make
        # sure their on-disk filenames are suitably uniquified. This
        # functionality actually lives in BuildStepStatus and BuildStatus, so
        # this test must involve more than just the MyLog class.

        # naieve approach, doesn't work
        l1 = MyLog(self.basedir, "duplicate")
        l1.addStdout("Some text\n")
        l1.finish()
        l2 = MyLog(self.basedir, "duplicate")
        l2.addStdout("Some more text\n")
        l2.finish()
        self.failIfEqual(l1.getFilename(), l2.getFilename())

    def testMerge1(self):
        l = MyLog(self.basedir, "merge1")
        l.addHeader("HEADER\n")
        l.addStdout("Some text\n")
        l.addStdout("Some more text\n")
        l.addStdout("more\n")
        l.finish()
        self.failUnlessEqual(l.getText(),
                             "Some text\nSome more text\nmore\n")
        self.failUnlessEqual(l.getTextWithHeaders(),
                             "HEADER\n" +
                             "Some text\nSome more text\nmore\n")
        self.failUnlessEqual(len(list(l.getChunks())), 2)

    def testMerge2(self):
        l = MyLog(self.basedir, "merge2")
        l.addHeader("HEADER\n")
        for i in xrange(1000):
            l.addStdout("aaaa")
        for i in xrange(30):
            l.addStderr("bbbb")
        for i in xrange(10):
            l.addStdout("cc")
        target = 1000*"aaaa" + 30 * "bbbb" + 10 * "cc"
        self.failUnlessEqual(len(l.getText()), len(target))
        self.failUnlessEqual(l.getText(), target)
        l.finish()
        self.failUnlessEqual(len(l.getText()), len(target))
        self.failUnlessEqual(l.getText(), target)
        self.failUnlessEqual(len(list(l.getChunks())), 4)

    def testMerge3(self):
        l = MyLog(self.basedir, "merge3")
        l.chunkSize = 100
        l.addHeader("HEADER\n")
        for i in xrange(8):
            l.addStdout(10*"a")
        for i in xrange(8):
            l.addStdout(10*"a")
        self.failUnlessEqual(list(l.getChunks()),
                             [(builder.HEADER, "HEADER\n"),
                              (builder.STDOUT, 100*"a"),
                              (builder.STDOUT, 60*"a")])
        l.finish()
        self.failUnlessEqual(l.getText(), 160*"a")

    def testReadlines(self):
        l = MyLog(self.basedir, "chunks")
        l.addHeader("HEADER\n") # should be ignored
        l.addStdout("Some text\n")
        l.addStdout("Some More Text\nAnd Some More\n")
        l.addStderr("Some Stderr\n")
        l.addStdout("Last line\n")
        l.finish()
        alllines = list(l.readlines())
        self.failUnlessEqual(len(alllines), 4)
        self.failUnlessEqual(alllines[0], "Some text\n")
        self.failUnlessEqual(alllines[2], "And Some More\n")
        self.failUnlessEqual(alllines[3], "Last line\n")
        stderr = list(l.readlines(interfaces.LOG_CHANNEL_STDERR))
        self.failUnlessEqual(len(stderr), 1)
        self.failUnlessEqual(stderr[0], "Some Stderr\n")
        lines = l.readlines()
        if False: # TODO: l.readlines() is not yet an iterator
            # verify that it really is an iterator
            line0 = lines.next()
            self.failUnlessEqual(line0, "Some text\n")
            line1 = lines.next()
            line2 = lines.next()
            self.failUnlessEqual(line2, "And Some More\n")


    def testChunks(self):
        l = MyLog(self.basedir, "chunks")
        c1 = l.getChunks()
        l.addHeader("HEADER\n")
        l.addStdout("Some text\n")
        self.failUnlessEqual("".join(l.getChunks(onlyText=True)),
                             "HEADER\nSome text\n")
        c2 = l.getChunks()

        l.addStdout("Some more text\n")
        self.failUnlessEqual("".join(l.getChunks(onlyText=True)),
                             "HEADER\nSome text\nSome more text\n")
        c3 = l.getChunks()
        
        l.addStdout("more\n")
        l.finish()

        self.failUnlessEqual(list(c1), [])
        self.failUnlessEqual(list(c2), [(builder.HEADER, "HEADER\n"),
                                        (builder.STDOUT, "Some text\n")])
        self.failUnlessEqual(list(c3), [(builder.HEADER, "HEADER\n"),
                                        (builder.STDOUT,
                                         "Some text\nSome more text\n")])
        
        self.failUnlessEqual(l.getText(),
                             "Some text\nSome more text\nmore\n")
        self.failUnlessEqual(l.getTextWithHeaders(),
                             "HEADER\n" +
                             "Some text\nSome more text\nmore\n")
        self.failUnlessEqual(len(list(l.getChunks())), 2)

    def testUpgrade(self):
        l = MyLog(self.basedir, "upgrade")
        l.addHeader("HEADER\n")
        l.addStdout("Some text\n")
        l.addStdout("Some more text\n")
        l.addStdout("more\n")
        l.finish()
        self.failUnless(l.hasContents())
        # now doctor it to look like a 0.6.4-era non-upgraded logfile
        l.entries = list(l.getChunks())
        del l.filename
        os.unlink(l.getFilename())
        # now make sure we can upgrade it
        l.upgrade("upgrade")
        self.failUnlessEqual(l.getText(),
                             "Some text\nSome more text\nmore\n")
        self.failUnlessEqual(len(list(l.getChunks())), 2)
        self.failIf(l.entries)

        # now, do it again, but make it look like an upgraded 0.6.4 logfile
        # (i.e. l.filename is missing, but the contents are there on disk)
        l.entries = list(l.getChunks())
        del l.filename
        l.upgrade("upgrade")
        self.failUnlessEqual(l.getText(),
                             "Some text\nSome more text\nmore\n")
        self.failUnlessEqual(len(list(l.getChunks())), 2)
        self.failIf(l.entries)
        self.failUnless(l.hasContents())

    def testHTMLUpgrade(self):
        l = MyHTMLLog(self.basedir, "upgrade", "log contents")
        l.upgrade("filename")

    def testSubscribe(self):
        l1 = MyLog(self.basedir, "subscribe1")
        l1.finish()
        self.failUnless(l1.isFinished())

        s = MyLogSubscriber()
        l1.subscribe(s, True)
        l1.unsubscribe(s)
        self.failIf(s.chunks)

        s = MyLogSubscriber()
        l1.subscribe(s, False)
        l1.unsubscribe(s)
        self.failIf(s.chunks)

        finished = []
        l2 = MyLog(self.basedir, "subscribe2")
        l2.waitUntilFinished().addCallback(finished.append)
        l2.addHeader("HEADER\n")
        s1 = MyLogSubscriber()
        l2.subscribe(s1, True)
        s2 = MyLogSubscriber()
        l2.subscribe(s2, False)
        self.failUnlessEqual(s1.chunks, [(builder.HEADER, "HEADER\n")])
        self.failUnlessEqual(s2.chunks, [])

        l2.addStdout("Some text\n")
        self.failUnlessEqual(s1.chunks, [(builder.HEADER, "HEADER\n"),
                                         (builder.STDOUT, "Some text\n")])
        self.failUnlessEqual(s2.chunks, [(builder.STDOUT, "Some text\n")])
        l2.unsubscribe(s1)
        
        l2.addStdout("Some more text\n")
        self.failUnlessEqual(s1.chunks, [(builder.HEADER, "HEADER\n"),
                                         (builder.STDOUT, "Some text\n")])
        self.failUnlessEqual(s2.chunks, [(builder.STDOUT, "Some text\n"),
                                         (builder.STDOUT, "Some more text\n"),
                                         ])
        self.failIf(finished)
        l2.finish()
        self.failUnlessEqual(finished, [l2])

    def testConsumer(self):
        l1 = MyLog(self.basedir, "consumer1")
        l1.finish()
        self.failUnless(l1.isFinished())

        s = MyLogConsumer()
        d = l1.subscribeConsumer(s)
        d.addCallback(self._testConsumer_1, s)
        return maybeWait(d, 5)
    def _testConsumer_1(self, res, s):
        self.failIf(s.chunks)
        self.failUnless(s.finished)
        self.failIf(s.producer) # producer should be registered and removed

        l2 = MyLog(self.basedir, "consumer2")
        l2.addHeader("HEADER\n")
        l2.finish()
        self.failUnless(l2.isFinished())

        s = MyLogConsumer()
        d = l2.subscribeConsumer(s)
        d.addCallback(self._testConsumer_2, s)
        return d
    def _testConsumer_2(self, res, s):
        self.failUnlessEqual(s.chunks, [(builder.HEADER, "HEADER\n")])
        self.failUnless(s.finished)
        self.failIf(s.producer) # producer should be registered and removed


        l2 = MyLog(self.basedir, "consumer3")
        l2.chunkSize = 1000
        l2.addHeader("HEADER\n")
        l2.addStdout(800*"a")
        l2.addStdout(800*"a") # should now have two chunks on disk, 1000+600
        l2.addStdout(800*"b") # HEADER,1000+600*a on disk, 800*a in memory
        l2.addStdout(800*"b") # HEADER,1000+600*a,1000+600*b on disk
        l2.addStdout(200*"c") # HEADER,1000+600*a,1000+600*b on disk,
                              # 200*c in memory
        
        s = MyLogConsumer(limit=1)
        d = l2.subscribeConsumer(s)
        d.addCallback(self._testConsumer_3, l2, s)
        return d
    def _testConsumer_3(self, res, l2, s):
        self.failUnless(s.streaming)
        self.failUnlessEqual(s.chunks, [(builder.HEADER, "HEADER\n")])
        s.limit = 1
        d = s.producer.resumeProducing()
        d.addCallback(self._testConsumer_4, l2, s)
        return d
    def _testConsumer_4(self, res, l2, s):
        self.failUnlessEqual(s.chunks, [(builder.HEADER, "HEADER\n"),
                                        (builder.STDOUT, 1000*"a"),
                                        ])
        s.limit = None
        d = s.producer.resumeProducing()
        d.addCallback(self._testConsumer_5, l2, s)
        return d
    def _testConsumer_5(self, res, l2, s):
        self.failUnlessEqual(s.chunks, [(builder.HEADER, "HEADER\n"),
                                        (builder.STDOUT, 1000*"a"),
                                        (builder.STDOUT, 600*"a"),
                                        (builder.STDOUT, 1000*"b"),
                                        (builder.STDOUT, 600*"b"),
                                        (builder.STDOUT, 200*"c")])
        l2.addStdout(1000*"c") # HEADER,1600*a,1600*b,1200*c on disk
        self.failUnlessEqual(s.chunks, [(builder.HEADER, "HEADER\n"),
                                        (builder.STDOUT, 1000*"a"),
                                        (builder.STDOUT, 600*"a"),
                                        (builder.STDOUT, 1000*"b"),
                                        (builder.STDOUT, 600*"b"),
                                        (builder.STDOUT, 200*"c"),
                                        (builder.STDOUT, 1000*"c")])
        l2.finish()
        self.failUnlessEqual(s.chunks, [(builder.HEADER, "HEADER\n"),
                                        (builder.STDOUT, 1000*"a"),
                                        (builder.STDOUT, 600*"a"),
                                        (builder.STDOUT, 1000*"b"),
                                        (builder.STDOUT, 600*"b"),
                                        (builder.STDOUT, 200*"c"),
                                        (builder.STDOUT, 1000*"c")])
        self.failIf(s.producer)
        self.failUnless(s.finished)

    def testLargeSummary(self):
        bigtext = "a" * 200000 # exceed the NetstringReceiver 100KB limit
        l = MyLog(self.basedir, "large", bigtext)
        s = MyLogConsumer()
        d = l.subscribeConsumer(s)
        def _check(res):
            for ctype,chunk in s.chunks:
                self.failUnless(len(chunk) < 100000)
            merged = "".join([c[1] for c in s.chunks])
            self.failUnless(merged == bigtext)
        d.addCallback(_check)
        # when this fails, it fails with a timeout, and there is an exception
        # sent to log.err(). This AttributeError exception is in
        # NetstringReceiver.dataReceived where it does
        # self.transport.loseConnection() because of the NetstringParseError,
        # however self.transport is None
        return maybeWait(d, 5)
    testLargeSummary.timeout = 5

config_base = """
from buildbot.process import factory
from buildbot.steps import dummy
s = factory.s

f1 = factory.QuickBuildFactory('fakerep', 'cvsmodule', configure=None)

f2 = factory.BuildFactory([
    s(dummy.Dummy, timeout=1),
    s(dummy.RemoteDummy, timeout=2),
    ])

BuildmasterConfig = c = {}
c['bots'] = [['bot1', 'sekrit']]
c['sources'] = []
c['schedulers'] = []
c['builders'] = []
c['builders'].append({'name':'quick', 'slavename':'bot1',
                      'builddir': 'quickdir', 'factory': f1})
c['slavePortnum'] = 0
"""

config_2 = config_base + """
c['builders'] = [{'name': 'dummy', 'slavename': 'bot1',
                  'builddir': 'dummy1', 'factory': f2},
                 {'name': 'testdummy', 'slavename': 'bot1',
                  'builddir': 'dummy2', 'factory': f2, 'category': 'test'}]
"""

class STarget(base.StatusReceiver):
    debug = False

    def __init__(self, mode):
        self.mode = mode
        self.events = []
    def announce(self):
        if self.debug:
            print self.events[-1]

    def builderAdded(self, name, builder):
        self.events.append(("builderAdded", name, builder))
        self.announce()
        if "builder" in self.mode:
            return self
    def builderChangedState(self, name, state):
        self.events.append(("builderChangedState", name, state))
        self.announce()
    def buildStarted(self, name, build):
        self.events.append(("buildStarted", name, build))
        self.announce()
        if "eta" in self.mode:
            self.eta_build = build.getETA()
        if "build" in self.mode:
            return self
    def buildETAUpdate(self, build, ETA):
        self.events.append(("buildETAUpdate", build, ETA))
        self.announce()
    def stepStarted(self, build, step):
        self.events.append(("stepStarted", build, step))
        self.announce()
        if 0 and "eta" in self.mode:
            print "TIMES", step.getTimes()
            print "ETA", step.getETA()
            print "EXP", step.getExpectations()
        if "step" in self.mode:
            return self
    def stepETAUpdate(self, build, step, ETA, expectations):
        self.events.append(("stepETAUpdate", build, step, ETA, expectations))
        self.announce()
    def logStarted(self, build, step, log):
        self.events.append(("logStarted", build, step, log))
        self.announce()
    def logFinished(self, build, step, log):
        self.events.append(("logFinished", build, step, log))
        self.announce()
    def stepFinished(self, build, step, results):
        self.events.append(("stepFinished", build, step, results))
        if 0 and "eta" in self.mode:
            print "post-EXP", step.getExpectations()
        self.announce()
    def buildFinished(self, name, build, results):
        self.events.append(("buildFinished", name, build, results))
        self.announce()
    def builderRemoved(self, name):
        self.events.append(("builderRemoved", name))
        self.announce()

class Subscription(RunMixin, unittest.TestCase):
    # verify that StatusTargets can subscribe/unsubscribe properly

    def testSlave(self):
        m = self.master
        s = m.getStatus()
        self.t1 = t1 = STarget(["builder"])
        #t1.debug = True; print
        s.subscribe(t1)
        self.failUnlessEqual(len(t1.events), 0)

        self.t3 = t3 = STarget(["builder", "build", "step"])
        s.subscribe(t3)

        m.loadConfig(config_2)
        m.readConfig = True
        m.startService()

        self.failUnlessEqual(len(t1.events), 4)
        self.failUnlessEqual(t1.events[0][0:2], ("builderAdded", "dummy"))
        self.failUnlessEqual(t1.events[1],
                             ("builderChangedState", "dummy", "offline"))
        self.failUnlessEqual(t1.events[2][0:2], ("builderAdded", "testdummy"))
        self.failUnlessEqual(t1.events[3],
                             ("builderChangedState", "testdummy", "offline"))
        t1.events = []

        self.failUnlessEqual(s.getBuilderNames(), ["dummy", "testdummy"])
        self.failUnlessEqual(s.getBuilderNames(categories=['test']),
                             ["testdummy"])
        self.s1 = s1 = s.getBuilder("dummy")
        self.failUnlessEqual(s1.getName(), "dummy")
        self.failUnlessEqual(s1.getState(), ("offline", []))
        self.failUnlessEqual(s1.getCurrentBuilds(), [])
        self.failUnlessEqual(s1.getLastFinishedBuild(), None)
        self.failUnlessEqual(s1.getBuild(-1), None)
        #self.failUnlessEqual(s1.getEvent(-1), foo("created"))

        # status targets should, upon being subscribed, immediately get a
        # list of all current builders matching their category
        self.t2 = t2 = STarget([])
        s.subscribe(t2)
        self.failUnlessEqual(len(t2.events), 2)
        self.failUnlessEqual(t2.events[0][0:2], ("builderAdded", "dummy"))
        self.failUnlessEqual(t2.events[1][0:2], ("builderAdded", "testdummy"))

        d = self.connectSlave(builders=["dummy", "testdummy"])
        d.addCallback(self._testSlave_1, t1)
        return maybeWait(d)

    def _testSlave_1(self, res, t1):
        self.failUnlessEqual(len(t1.events), 2)
        self.failUnlessEqual(t1.events[0],
                             ("builderChangedState", "dummy", "idle"))
        self.failUnlessEqual(t1.events[1],
                             ("builderChangedState", "testdummy", "idle"))
        t1.events = []

        c = interfaces.IControl(self.master)
        req = BuildRequest("forced build for testing", SourceStamp())
        c.getBuilder("dummy").requestBuild(req)
        d = req.waitUntilFinished()
        d2 = self.master.botmaster.waitUntilBuilderIdle("dummy")
        dl = defer.DeferredList([d, d2])
        dl.addCallback(self._testSlave_2)
        return dl

    def _testSlave_2(self, res):
        # t1 subscribes to builds, but not anything lower-level
        ev = self.t1.events
        self.failUnlessEqual(len(ev), 4)
        self.failUnlessEqual(ev[0][0:3],
                             ("builderChangedState", "dummy", "building"))
        self.failUnlessEqual(ev[1][0], "buildStarted")
        self.failUnlessEqual(ev[2][0:2]+ev[2][3:4],
                             ("buildFinished", "dummy", builder.SUCCESS))
        self.failUnlessEqual(ev[3][0:3],
                             ("builderChangedState", "dummy", "idle"))

        self.failUnlessEqual([ev[0] for ev in self.t3.events],
                             ["builderAdded",
                              "builderChangedState", # offline
                              "builderAdded",
                              "builderChangedState", # idle
                              "builderChangedState", # offline
                              "builderChangedState", # idle
                              "builderChangedState", # building
                              "buildStarted",
                              "stepStarted", "stepETAUpdate", "stepFinished",
                              "stepStarted", "stepETAUpdate",
                              "logStarted", "logFinished", "stepFinished",
                              "buildFinished",
                              "builderChangedState", # idle
                              ])

        b = self.s1.getLastFinishedBuild()
        self.failUnless(b)
        self.failUnlessEqual(b.getBuilder().getName(), "dummy")
        self.failUnlessEqual(b.getNumber(), 0)
        self.failUnlessEqual(b.getSourceStamp(), (None, None, None))
        self.failUnlessEqual(b.getReason(), "forced build for testing")
        self.failUnlessEqual(b.getChanges(), [])
        self.failUnlessEqual(b.getResponsibleUsers(), [])
        self.failUnless(b.isFinished())
        self.failUnlessEqual(b.getText(), ['build', 'successful'])
        self.failUnlessEqual(b.getColor(), "green")
        self.failUnlessEqual(b.getResults(), builder.SUCCESS)

        steps = b.getSteps()
        self.failUnlessEqual(len(steps), 2)

        eta = 0
        st1 = steps[0]
        self.failUnlessEqual(st1.getName(), "dummy")
        self.failUnless(st1.isFinished())
        self.failUnlessEqual(st1.getText(), ["delay", "1 secs"])
        start,finish = st1.getTimes()
        self.failUnless(0.5 < (finish-start) < 10)
        self.failUnlessEqual(st1.getExpectations(), [])
        self.failUnlessEqual(st1.getLogs(), [])
        eta += finish-start

        st2 = steps[1]
        self.failUnlessEqual(st2.getName(), "remote dummy")
        self.failUnless(st2.isFinished())
        self.failUnlessEqual(st2.getText(),
                             ["remote", "delay", "2 secs"])
        start,finish = st2.getTimes()
        self.failUnless(1.5 < (finish-start) < 10)
        eta += finish-start
        self.failUnlessEqual(st2.getExpectations(), [('output', 38, None)])
        logs = st2.getLogs()
        self.failUnlessEqual(len(logs), 1)
        self.failUnlessEqual(logs[0].getName(), "stdio")
        self.failUnlessEqual(logs[0].getText(), "data")

        self.eta = eta
        # now we run it a second time, and we should have an ETA

        self.t4 = t4 = STarget(["builder", "build", "eta"])
        self.master.getStatus().subscribe(t4)
        c = interfaces.IControl(self.master)
        req = BuildRequest("forced build for testing", SourceStamp())
        c.getBuilder("dummy").requestBuild(req)
        d = req.waitUntilFinished()
        d2 = self.master.botmaster.waitUntilBuilderIdle("dummy")
        dl = defer.DeferredList([d, d2])
        dl.addCallback(self._testSlave_3)
        return dl

    def _testSlave_3(self, res):
        t4 = self.t4
        eta = self.eta
        self.failUnless(eta-1 < t4.eta_build < eta+1, # should be 3 seconds
                        "t4.eta_build was %g, not in (%g,%g)"
                        % (t4.eta_build, eta-1, eta+1))
    

class Client(unittest.TestCase):
    def testAdaptation(self):
        b = builder.BuilderStatus("bname")
        b2 = client.makeRemote(b)
        self.failUnless(isinstance(b2, client.RemoteBuilder))
        b3 = client.makeRemote(None)
        self.failUnless(b3 is None)
