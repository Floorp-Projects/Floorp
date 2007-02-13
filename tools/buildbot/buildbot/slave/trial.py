# -*- test-case-name: buildbot.test.test_trial.TestRemoteReporter -*-

import types, time
import zope.interface as zi

from twisted.spread import pb
from twisted.internet import reactor, defer
from twisted.python import reflect, failure, log, usage, util
from twisted.trial import registerAdapter, adaptWithDefault, reporter, runner
from twisted.trial.interfaces import ITestMethod, ITestSuite, ITestRunner, \
     IJellied, IUnjellied, IRemoteReporter
from twisted.application import strports


class RemoteTestAny(object, util.FancyStrMixin):
    def __init__(self, original):
        self.original = original

    def __getattr__(self, attr):
        if attr not in self.original:
            raise AttributeError, "%s has no attribute %s" % (self.__str__(), attr)
        return self.original[attr]


class RemoteTestMethod(RemoteTestAny):
    zi.implements(ITestMethod)

class RemoteTestSuite(RemoteTestAny):
    zi.implements(ITestSuite)


class RemoteReporter(reporter.Reporter):
    zi.implements(IRemoteReporter)
    pbroot = None

    def __init__(self, stream=None, tbformat=None, args=None):
        super(RemoteReporter, self).__init__(stream, tbformat, args)

    def setUpReporter(self):
        factory = pb.PBClientFactory()
        
        self.pbcnx = reactor.connectTCP("localhost", self.args, factory)
        assert self.pbcnx is not None

        def _cb(root):
            self.pbroot = root
            return root

        return factory.getRootObject().addCallback(_cb
                                     ).addErrback(log.err)
        
    def tearDownReporter(self):
        def _disconnected(passthru):
            log.msg(sekritHQ='_disconnected, passthru: %r' % (passthru,))
            return passthru

        d = defer.Deferred().addCallback(_disconnected
                           ).addErrback(log.err)

        self.pbroot.notifyOnDisconnect(d.callback)
        self.pbcnx.transport.loseConnection()
        return d

    def reportImportError(self, name, fail):
        pass

    def startTest(self, method):
        return self.pbroot.callRemote('startTest', IJellied(method))

    def endTest(self, method):
        return self.pbroot.callRemote('endTest', IJellied(method))

    def startSuite(self, arg):
        return self.pbroot.callRemote('startSuite', IJellied(arg))

    def endSuite(self, suite):
        return self.pbroot.callRemote('endSuite', IJellied(suite))


# -- Adapters --

def jellyList(L):
    return [IJellied(i) for i in L]
    
def jellyTuple(T):
    return tuple(IJellied(list(T)))
    
def jellyDict(D):
    def _clean(*a):
       return tuple(map(lambda x: adaptWithDefault(IJellied, x, None), a))
    return dict([_clean(k, v) for k, v in D.iteritems()]) 

def jellyTimingInfo(d, timed):
    for attr in ('startTime', 'endTime'):
        d[attr] = getattr(timed, attr, 0.0)
    return d

def _logFormatter(eventDict):
    #XXX: this is pretty weak, it's basically the guts of
    # t.p.log.FileLogObserver.emit, but then again, that's been pretty
    # stable over the past few releases....
    edm = eventDict['message']
    if not edm:
        if eventDict['isError'] and eventDict.has_key('failure'):
            text = eventDict['failure'].getTraceback()
        elif eventDict.has_key('format'):
            try:
                text = eventDict['format'] % eventDict
            except:
                try:
                    text = ('Invalid format string in log message: %s'
                            % eventDict)
                except:
                    text = 'UNFORMATTABLE OBJECT WRITTEN TO LOG, MESSAGE LOST'
        else:
            # we don't know how to log this
            return
    else:
        text = ' '.join(map(str, edm))

    timeStr = time.strftime("%Y/%m/%d %H:%M %Z", time.localtime(eventDict['time']))
    fmtDict = {'system': eventDict['system'], 'text': text.replace("\n", "\n\t")}
    msgStr = " [%(system)s] %(text)s\n" % fmtDict
    return "%s%s" % (timeStr, msgStr)

def jellyTestMethod(testMethod):
    """@param testMethod: an object that implements L{twisted.trial.interfaces.ITestMethod}"""
    d = {}
    for attr in ('status', 'todo', 'skip', 'stdout', 'stderr',
                 'name', 'fullName', 'runs', 'errors', 'failures', 'module'):
        d[attr] = getattr(testMethod, attr)

    q = None
    try:
        q = reflect.qual(testMethod.klass)
    except TypeError:
        # XXX: This may be incorrect somehow
        q = "%s.%s" % (testMethod.module, testMethod.klass.__name__)
    d['klass'] = q

    d['logevents'] = [_logFormatter(event) for event in testMethod.logevents]
            
    jellyTimingInfo(d, testMethod)
    
    return d
    
def jellyTestRunner(testRunner):
    """@param testRunner: an object that implements L{twisted.trial.interfaces.ITestRunner}"""
    d = dict(testMethods=[IJellied(m) for m in testRunner.testMethods])
    jellyTimingInfo(d, testRunner)
    return d

def jellyTestSuite(testSuite):
    d = {}
    for attr in ('tests', 'runners', 'couldNotImport'):
        d[attr] = IJellied(getattr(testSuite, attr))

    jellyTimingInfo(d, testSuite)
    return d



for a, o, i in [(jellyTuple, types.TupleType, IJellied),
                (jellyTestMethod, ITestMethod, IJellied),
                (jellyList, types.ListType, IJellied),
                (jellyTestSuite, ITestSuite, IJellied),
                (jellyTestRunner, ITestRunner, IJellied),
                (jellyDict, types.DictType, IJellied),
                (RemoteTestMethod, types.DictType, ITestMethod),
                (RemoteTestSuite, types.DictType, ITestSuite)]:
    registerAdapter(a, o, i)

for t in [types.StringType, types.IntType, types.FloatType, failure.Failure]:
    zi.classImplements(t, IJellied)

