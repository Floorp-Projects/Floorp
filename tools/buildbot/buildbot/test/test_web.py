# -*- test-case-name: buildbot.test.test_web -*-

import os, time, shutil
from twisted.python import components

from twisted.trial import unittest
from buildbot.test.runutils import RunMixin

from twisted.internet import reactor, defer, protocol
from twisted.internet.interfaces import IReactorUNIX
from twisted.web import client

from buildbot import master, interfaces, sourcestamp
from buildbot.twcompat import providedBy, maybeWait
from buildbot.status import html, builder
from buildbot.changes.changes import Change
from buildbot.process import base
from buildbot.process.buildstep import BuildStep
from buildbot.test.runutils import setupBuildStepStatus

class ConfiguredMaster(master.BuildMaster):
    """This BuildMaster variant has a static config file, provided as a
    string when it is created."""

    def __init__(self, basedir, config):
        self.config = config
        master.BuildMaster.__init__(self, basedir)
        
    def loadTheConfigFile(self):
        self.loadConfig(self.config)

components.registerAdapter(master.Control, ConfiguredMaster,
                           interfaces.IControl)


base_config = """
from buildbot.status import html
BuildmasterConfig = c = {
    'bots': [],
    'sources': [],
    'schedulers': [],
    'builders': [],
    'slavePortnum': 0,
    }
"""



class DistribUNIX:
    def __init__(self, unixpath):
        from twisted.web import server, resource, distrib
        root = resource.Resource()
        self.r = r = distrib.ResourceSubscription("unix", unixpath)
        root.putChild('remote', r)
        self.p = p = reactor.listenTCP(0, server.Site(root))
        self.portnum = p.getHost().port
    def shutdown(self):
        d = defer.maybeDeferred(self.p.stopListening)
        return d

class DistribTCP:
    def __init__(self, port):
        from twisted.web import server, resource, distrib
        root = resource.Resource()
        self.r = r = distrib.ResourceSubscription("localhost", port)
        root.putChild('remote', r)
        self.p = p = reactor.listenTCP(0, server.Site(root))
        self.portnum = p.getHost().port
    def shutdown(self):
        d = defer.maybeDeferred(self.p.stopListening)
        d.addCallback(self._shutdown_1)
        return d
    def _shutdown_1(self, res):
        return self.r.publisher.broker.transport.loseConnection()

class SlowReader(protocol.Protocol):
    didPause = False
    count = 0
    data = ""
    def __init__(self, req):
        self.req = req
        self.d = defer.Deferred()
    def connectionMade(self):
        self.transport.write(self.req)
    def dataReceived(self, data):
        self.data += data
        self.count += len(data)
        if not self.didPause and self.count > 10*1000:
            self.didPause = True
            self.transport.pauseProducing()
            reactor.callLater(2, self.resume)
    def resume(self):
        self.transport.resumeProducing()
    def connectionLost(self, why):
        self.d.callback(None)

class CFactory(protocol.ClientFactory):
    def __init__(self, p):
        self.p = p
    def buildProtocol(self, addr):
        self.p.factory = self
        return self.p

def stopHTTPLog():
    # grr.
    try:
        from twisted.web import http # Twisted-2.0
    except ImportError:
        from twisted.protocols import http # Twisted-1.3
    http._logDateTimeStop()

class BaseWeb:
    master = None

    def failUnlessIn(self, substr, string):
        self.failUnless(string.find(substr) != -1)

    def tearDown(self):
        stopHTTPLog()
        if self.master:
            d = self.master.stopService()
            return maybeWait(d)

    def find_waterfall(self, master):
        return filter(lambda child: isinstance(child, html.Waterfall),
                      list(master))

class Ports(BaseWeb, unittest.TestCase):

    def test_webPortnum(self):
        # run a regular web server on a TCP socket
        config = base_config + "c['status'] = [html.Waterfall(http_port=0)]\n"
        os.mkdir("test_web1")
        self.master = m = ConfiguredMaster("test_web1", config)
        m.startService()
        # hack to find out what randomly-assigned port it is listening on
        port = list(self.find_waterfall(m)[0])[0]._port.getHost().port

        d = client.getPage("http://localhost:%d/" % port)
        d.addCallback(self._test_webPortnum_1)
        return maybeWait(d)
    test_webPortnum.timeout = 10
    def _test_webPortnum_1(self, page):
        #print page
        self.failUnless(page)

    def test_webPathname(self):
        # running a t.web.distrib server over a UNIX socket
        if not providedBy(reactor, IReactorUNIX):
            raise unittest.SkipTest("UNIX sockets not supported here")
        config = (base_config +
                  "c['status'] = [html.Waterfall(distrib_port='.web-pb')]\n")
        os.mkdir("test_web2")
        self.master = m = ConfiguredMaster("test_web2", config)
        m.startService()
            
        p = DistribUNIX("test_web2/.web-pb")

        d = client.getPage("http://localhost:%d/remote/" % p.portnum)
        d.addCallback(self._test_webPathname_1, p)
        return maybeWait(d)
    test_webPathname.timeout = 10
    def _test_webPathname_1(self, page, p):
        #print page
        self.failUnless(page)
        return p.shutdown()


    def test_webPathname_port(self):
        # running a t.web.distrib server over TCP
        config = (base_config +
                  "c['status'] = [html.Waterfall(distrib_port=0)]\n")
        os.mkdir("test_web3")
        self.master = m = ConfiguredMaster("test_web3", config)
        m.startService()
        dport = list(self.find_waterfall(m)[0])[0]._port.getHost().port

        p = DistribTCP(dport)

        d = client.getPage("http://localhost:%d/remote/" % p.portnum)
        d.addCallback(self._test_webPathname_port_1, p)
        return maybeWait(d)
    test_webPathname_port.timeout = 10
    def _test_webPathname_port_1(self, page, p):
        self.failUnlessIn("BuildBot", page)
        return p.shutdown()


class Waterfall(BaseWeb, unittest.TestCase):
    def test_waterfall(self):
        os.mkdir("test_web4")
        os.mkdir("my-maildir"); os.mkdir("my-maildir/new")
        self.robots_txt = os.path.abspath(os.path.join("test_web4",
                                                       "robots.txt"))
        self.robots_txt_contents = "User-agent: *\nDisallow: /\n"
        f = open(self.robots_txt, "w")
        f.write(self.robots_txt_contents)
        f.close()
        # this is the right way to configure the Waterfall status
        config1 = base_config + """
from buildbot.changes import mail
c['sources'] = [mail.SyncmailMaildirSource('my-maildir')]
c['status'] = [html.Waterfall(http_port=0, robots_txt=%s)]
""" % repr(self.robots_txt)

        self.master = m = ConfiguredMaster("test_web4", config1)
        m.startService()
        # hack to find out what randomly-assigned port it is listening on
        port = list(self.find_waterfall(m)[0])[0]._port.getHost().port
        self.port = port
        # insert an event
        m.change_svc.addChange(Change("user", ["foo.c"], "comments"))

        d = client.getPage("http://localhost:%d/" % port)
        d.addCallback(self._test_waterfall_1)
        return maybeWait(d)
    test_waterfall.timeout = 10
    def _test_waterfall_1(self, page):
        self.failUnless(page)
        self.failUnlessIn("current activity", page)
        self.failUnlessIn("<html", page)
        TZ = time.tzname[time.daylight]
        self.failUnlessIn("time (%s)" % TZ, page)

        # phase=0 is really for debugging the waterfall layout
        d = client.getPage("http://localhost:%d/?phase=0" % self.port)
        d.addCallback(self._test_waterfall_2)
        return d
    def _test_waterfall_2(self, page):
        self.failUnless(page)
        self.failUnlessIn("<html", page)

        d = client.getPage("http://localhost:%d/favicon.ico" % self.port)
        d.addCallback(self._test_waterfall_3)
        return d
    def _test_waterfall_3(self, icon):
        expected = open(html.buildbot_icon,"rb").read()
        self.failUnless(icon == expected)

        d = client.getPage("http://localhost:%d/changes" % self.port)
        d.addCallback(self._test_waterfall_4)
        return d
    def _test_waterfall_4(self, changes):
        self.failUnlessIn("<li>Syncmail mailing list in maildir " +
                          "my-maildir</li>", changes)

        d = client.getPage("http://localhost:%d/robots.txt" % self.port)
        d.addCallback(self._test_waterfall_5)
        return d
    def _test_waterfall_5(self, robotstxt):
        self.failUnless(robotstxt == self.robots_txt_contents)

class WaterfallSteps(unittest.TestCase):

    # failUnlessSubstring copied from twisted-2.1.0, because this helps us
    # maintain compatibility with python2.2.
    def failUnlessSubstring(self, substring, astring, msg=None):
        """a python2.2 friendly test to assert that substring is found in
        astring parameters follow the semantics of failUnlessIn
        """
        if astring.find(substring) == -1:
            raise self.failureException(msg or "%r not found in %r"
                                        % (substring, astring))
        return substring
    assertSubstring = failUnlessSubstring

    def test_urls(self):
        s = setupBuildStepStatus("test_web.test_urls")
        s.addURL("coverage", "http://coverage.example.org/target")
        s.addURL("icon", "http://coverage.example.org/icon.png")
        box = html.IBox(s).getBox()
        td = box.td()
        e1 = '[<a href="http://coverage.example.org/target" class="BuildStep external">coverage</a>]'
        self.failUnlessSubstring(e1, td)
        e2 = '[<a href="http://coverage.example.org/icon.png" class="BuildStep external">icon</a>]'
        self.failUnlessSubstring(e2, td)



geturl_config = """
from buildbot.status import html
from buildbot.changes import mail
from buildbot.process import factory
from buildbot.steps import dummy
from buildbot.scheduler import Scheduler
from buildbot.changes.base import ChangeSource
s = factory.s

class DiscardScheduler(Scheduler):
    def addChange(self, change):
        pass
class DummyChangeSource(ChangeSource):
    pass

BuildmasterConfig = c = {}
c['bots'] = [('bot1', 'sekrit'), ('bot2', 'sekrit')]
c['sources'] = [DummyChangeSource()]
c['schedulers'] = [DiscardScheduler('discard', None, 60, ['b1'])]
c['slavePortnum'] = 0
c['status'] = [html.Waterfall(http_port=0)]

f = factory.BuildFactory([s(dummy.RemoteDummy, timeout=1)])

c['builders'] = [
    {'name': 'b1', 'slavenames': ['bot1','bot2'],
     'builddir': 'b1', 'factory': f},
    ]
c['buildbotURL'] = 'http://dummy.example.org:8010/'

"""

class GetURL(RunMixin, unittest.TestCase):

    def setUp(self):
        RunMixin.setUp(self)
        self.master.loadConfig(geturl_config)
        self.master.startService()
        d = self.connectSlave(["b1"])
        return maybeWait(d)

    def tearDown(self):
        stopHTTPLog()
        return RunMixin.tearDown(self)

    def doBuild(self, buildername):
        br = base.BuildRequest("forced", sourcestamp.SourceStamp())
        d = br.waitUntilFinished()
        self.control.getBuilder(buildername).requestBuild(br)
        return d

    def assertNoURL(self, target):
        self.failUnlessIdentical(self.status.getURLForThing(target), None)

    def assertURLEqual(self, target, expected):
        got = self.status.getURLForThing(target)
        full_expected = "http://dummy.example.org:8010/" + expected
        self.failUnlessEqual(got, full_expected)

    def testMissingBase(self):
        noweb_config1 = geturl_config + "del c['buildbotURL']\n"
        d = self.master.loadConfig(noweb_config1)
        d.addCallback(self._testMissingBase_1)
        return maybeWait(d)
    def _testMissingBase_1(self, res):
        s = self.status
        self.assertNoURL(s)
        builder = s.getBuilder("b1")
        self.assertNoURL(builder)

    def testBase(self):
        s = self.status
        self.assertURLEqual(s, "")
        builder = s.getBuilder("b1")
        self.assertURLEqual(builder, "b1")

    def testChange(self):
        s = self.status
        c = Change("user", ["foo.c"], "comments")
        self.master.change_svc.addChange(c)
        # TODO: something more like s.getChanges(), requires IChange and
        # an accessor in IStatus. The HTML page exists already, though
        self.assertURLEqual(c, "changes/1")

    def testBuild(self):
        # first we do some stuff so we'll have things to look at.
        s = self.status
        d = self.doBuild("b1")
        # maybe check IBuildSetStatus here?
        d.addCallback(self._testBuild_1)
        return maybeWait(d)

    def _testBuild_1(self, res):
        s = self.status
        builder = s.getBuilder("b1")
        build = builder.getLastFinishedBuild()
        self.assertURLEqual(build, "b1/builds/0")
        # no page for builder.getEvent(-1)
        step = build.getSteps()[0]
        self.assertURLEqual(step, "b1/builds/0/step-remote%20dummy")
        # maybe page for build.getTestResults?
        self.assertURLEqual(step.getLogs()[0],
                            "b1/builds/0/step-remote%20dummy/0")



class Logfile(BaseWeb, RunMixin, unittest.TestCase):
    def setUp(self):
        config = """
from buildbot.status import html
from buildbot.process.factory import BasicBuildFactory
f1 = BasicBuildFactory('cvsroot', 'cvsmodule')
BuildmasterConfig = {
    'bots': [('bot1', 'passwd1')],
    'sources': [],
    'schedulers': [],
    'builders': [{'name': 'builder1', 'slavename': 'bot1',
                  'builddir':'workdir', 'factory':f1}],
    'slavePortnum': 0,
    'status': [html.Waterfall(http_port=0)],
    }
"""
        if os.path.exists("test_logfile"):
            shutil.rmtree("test_logfile")
        os.mkdir("test_logfile")
        self.master = m = ConfiguredMaster("test_logfile", config)
        m.startService()
        # hack to find out what randomly-assigned port it is listening on
        port = list(self.find_waterfall(m)[0])[0]._port.getHost().port
        self.port = port
        # insert an event

        req = base.BuildRequest("reason", sourcestamp.SourceStamp())
        build1 = base.Build([req])
        bs = m.status.getBuilder("builder1").newBuild()
        bs.setReason("reason")
        bs.buildStarted(build1)

        step1 = BuildStep(build=build1, name="setup")
        bss = bs.addStepWithName("setup")
        step1.setStepStatus(bss)
        bss.stepStarted()

        log1 = step1.addLog("output")
        log1.addStdout("some stdout\n")
        log1.finish()

        log2 = step1.addHTMLLog("error", "<html>ouch</html>")

        log3 = step1.addLog("big")
        log3.addStdout("big log\n")
        for i in range(1000):
            log3.addStdout("a" * 500)
            log3.addStderr("b" * 500)
        log3.finish()

        log4 = step1.addCompleteLog("bigcomplete",
                                    "big2 log\n" + "a" * 1*1000*1000)

        step1.step_status.stepFinished(builder.SUCCESS)
        bs.buildFinished()

    def getLogURL(self, stepname, lognum):
        logurl = "http://localhost:%d/builder1/builds/0/step-%s/%d" \
                 % (self.port, stepname, lognum)
        return logurl

    def test_logfile1(self):
        d = client.getPage("http://localhost:%d/" % self.port)
        d.addCallback(self._test_logfile1_1)
        return maybeWait(d)
    test_logfile1.timeout = 20
    def _test_logfile1_1(self, page):
        self.failUnless(page)

    def test_logfile2(self):
        logurl = self.getLogURL("setup", 0)
        d = client.getPage(logurl)
        d.addCallback(self._test_logfile2_1)
        return maybeWait(d)
    def _test_logfile2_1(self, logbody):
        self.failUnless(logbody)

    def test_logfile3(self):
        logurl = self.getLogURL("setup", 0)
        d = client.getPage(logurl + "/text")
        d.addCallback(self._test_logfile3_1)
        return maybeWait(d)
    def _test_logfile3_1(self, logtext):
        self.failUnlessEqual(logtext, "some stdout\n")

    def test_logfile4(self):
        logurl = self.getLogURL("setup", 1)
        d = client.getPage(logurl)
        d.addCallback(self._test_logfile4_1)
        return maybeWait(d)
    def _test_logfile4_1(self, logbody):
        self.failUnlessEqual(logbody, "<html>ouch</html>")

    def test_logfile5(self):
        # this is log3, which is about 1MB in size, made up of alternating
        # stdout/stderr chunks. buildbot-0.6.6, when run against
        # twisted-1.3.0, fails to resume sending chunks after the client
        # stalls for a few seconds, because of a recursive doWrite() call
        # that was fixed in twisted-2.0.0
        p = SlowReader("GET /builder1/builds/0/step-setup/2 HTTP/1.0\r\n\r\n")
        f = CFactory(p)
        c = reactor.connectTCP("localhost", self.port, f)
        d = p.d
        d.addCallback(self._test_logfile5_1, p)
        return maybeWait(d, 10)
    test_logfile5.timeout = 10
    def _test_logfile5_1(self, res, p):
        self.failUnlessIn("big log", p.data)
        self.failUnlessIn("a"*100, p.data)
        self.failUnless(p.count > 1*1000*1000)

    def test_logfile6(self):
        # this is log4, which is about 1MB in size, one big chunk.
        # buildbot-0.6.6 dies as the NetstringReceiver barfs on the
        # saved logfile, because it was using one big chunk and exceeding
        # NetstringReceiver.MAX_LENGTH
        p = SlowReader("GET /builder1/builds/0/step-setup/3 HTTP/1.0\r\n\r\n")
        f = CFactory(p)
        c = reactor.connectTCP("localhost", self.port, f)
        d = p.d
        d.addCallback(self._test_logfile6_1, p)
        return maybeWait(d, 10)
    test_logfile6.timeout = 10
    def _test_logfile6_1(self, res, p):
        self.failUnlessIn("big2 log", p.data)
        self.failUnlessIn("a"*100, p.data)
        self.failUnless(p.count > 1*1000*1000)


