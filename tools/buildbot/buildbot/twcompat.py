
if 0:
    print "hey python-mode, stop thinking I want 8-char indentation"

"""
utilities to be compatible with both Twisted-1.3 and 2.0

implements. Use this like the following.

from buildbot.twcompat import implements
class Foo:
    if implements:
        implements(IFoo)
    else:
        __implements__ = IFoo,
        
Interface:
    from buildbot.tcompat import Interface
    class IFoo(Interface)

providedBy:
    from buildbot.tcompat import providedBy
    assert providedBy(obj, IFoo)
"""

import os

from twisted.copyright import version
from twisted.python import components

# does our Twisted use zope.interface?
implements = None
if hasattr(components, "interface"):
    # yes
    from zope.interface import implements
    from zope.interface import Interface
    def providedBy(obj, iface):
        return iface.providedBy(obj)
else:
    # nope
    from twisted.python.components import Interface
    providedBy = components.implements

# are we using a version of Trial that allows setUp/testFoo/tearDown to
# return Deferreds?
oldtrial = version.startswith("1.3")

# use this at the end of setUp/testFoo/tearDown methods
def maybeWait(d, timeout="none"):
    from twisted.python import failure
    from twisted.trial import unittest
    if oldtrial:
        # this is required for oldtrial (twisted-1.3.0) compatibility. When we
        # move to retrial (twisted-2.0.0), replace these with a simple 'return
        # d'.
        try:
            if timeout == "none":
                unittest.deferredResult(d)
            else:
                unittest.deferredResult(d, timeout)
        except failure.Failure, f:
            if f.check(unittest.SkipTest):
                raise f.value
            raise
        return None
    return d

# waitForDeferred and getProcessOutputAndValue are twisted-2.0 things. If
# we're running under 1.3, patch them into place. These versions are copied
# from twisted somewhat after 2.0.1 .

from twisted.internet import defer
if not hasattr(defer, 'waitForDeferred'):
    Deferred = defer.Deferred
    class waitForDeferred:
        """
        API Stability: semi-stable

        Maintainer: U{Christopher Armstrong<mailto:radix@twistedmatrix.com>}

        waitForDeferred and deferredGenerator help you write
        Deferred-using code that looks like it's blocking (but isn't
        really), with the help of generators.

        There are two important functions involved: waitForDeferred, and
        deferredGenerator.

            def thingummy():
                thing = waitForDeferred(makeSomeRequestResultingInDeferred())
                yield thing
                thing = thing.getResult()
                print thing #the result! hoorj!
            thingummy = deferredGenerator(thingummy)

        waitForDeferred returns something that you should immediately yield;
        when your generator is resumed, calling thing.getResult() will either
        give you the result of the Deferred if it was a success, or raise an
        exception if it was a failure.

        deferredGenerator takes one of these waitForDeferred-using
        generator functions and converts it into a function that returns a
        Deferred. The result of the Deferred will be the last
        value that your generator yielded (remember that 'return result' won't
        work; use 'yield result; return' in place of that).

        Note that not yielding anything from your generator will make the
        Deferred result in None. Yielding a Deferred from your generator
        is also an error condition; always yield waitForDeferred(d)
        instead.

        The Deferred returned from your deferred generator may also
        errback if your generator raised an exception.

            def thingummy():
                thing = waitForDeferred(makeSomeRequestResultingInDeferred())
                yield thing
                thing = thing.getResult()
                if thing == 'I love Twisted':
                    # will become the result of the Deferred
                    yield 'TWISTED IS GREAT!'
                    return
                else:
                    # will trigger an errback
                    raise Exception('DESTROY ALL LIFE')
            thingummy = deferredGenerator(thingummy)

        Put succinctly, these functions connect deferred-using code with this
        'fake blocking' style in both directions: waitForDeferred converts from
        a Deferred to the 'blocking' style, and deferredGenerator converts from
        the 'blocking' style to a Deferred.
        """
        def __init__(self, d):
            if not isinstance(d, Deferred):
                raise TypeError("You must give waitForDeferred a Deferred. You gave it %r." % (d,))
            self.d = d

        def getResult(self):
            if hasattr(self, 'failure'):
                self.failure.raiseException()
            return self.result

    def _deferGenerator(g, deferred=None, result=None):
        """
        See L{waitForDeferred}.
        """
        while 1:
            if deferred is None:
                deferred = defer.Deferred()
            try:
                result = g.next()
            except StopIteration:
                deferred.callback(result)
                return deferred
            except:
                deferred.errback()
                return deferred

            # Deferred.callback(Deferred) raises an error; we catch this case
            # early here and give a nicer error message to the user in case
            # they yield a Deferred. Perhaps eventually these semantics may
            # change.
            if isinstance(result, defer.Deferred):
                return defer.fail(TypeError("Yield waitForDeferred(d), not d!"))

            if isinstance(result, waitForDeferred):
                waiting=[True, None]
                # Pass vars in so they don't get changed going around the loop
                def gotResult(r, waiting=waiting, result=result):
                    result.result = r
                    if waiting[0]:
                        waiting[0] = False
                        waiting[1] = r
                    else:
                        _deferGenerator(g, deferred, r)
                def gotError(f, waiting=waiting, result=result):
                    result.failure = f
                    if waiting[0]:
                        waiting[0] = False
                        waiting[1] = f
                    else:
                        _deferGenerator(g, deferred, f)
                result.d.addCallbacks(gotResult, gotError)
                if waiting[0]:
                    # Haven't called back yet, set flag so that we get reinvoked
                    # and return from the loop
                    waiting[0] = False
                    return deferred
                else:
                    result = waiting[1]

    def func_metamerge(f, g):
        """
        Merge function metadata from f -> g and return g
        """
        try:
            g.__doc__ = f.__doc__
            g.__dict__.update(f.__dict__)
            g.__name__ = f.__name__
        except (TypeError, AttributeError):
            pass
        return g

    def deferredGenerator(f):
        """
        See L{waitForDeferred}.
        """
        def unwindGenerator(*args, **kwargs):
            return _deferGenerator(f(*args, **kwargs))
        return func_metamerge(f, unwindGenerator)

    defer.waitForDeferred = waitForDeferred
    defer.deferredGenerator = deferredGenerator

from twisted.internet import utils
if not hasattr(utils, "getProcessOutputAndValue"):
    from twisted.internet import reactor, protocol
    _callProtocolWithDeferred = utils._callProtocolWithDeferred
    try:
        import cStringIO
        StringIO = cStringIO
    except ImportError:
        import StringIO

    class _EverythingGetter(protocol.ProcessProtocol):

        def __init__(self, deferred):
            self.deferred = deferred
            self.outBuf = StringIO.StringIO()
            self.errBuf = StringIO.StringIO()
            self.outReceived = self.outBuf.write
            self.errReceived = self.errBuf.write

        def processEnded(self, reason):
            out = self.outBuf.getvalue()
            err = self.errBuf.getvalue()
            e = reason.value
            code = e.exitCode
            if e.signal:
                self.deferred.errback((out, err, e.signal))
            else:
                self.deferred.callback((out, err, code))

    def getProcessOutputAndValue(executable, args=(), env={}, path='.', 
                                 reactor=reactor):
        """Spawn a process and returns a Deferred that will be called back
        with its output (from stdout and stderr) and it's exit code as (out,
        err, code) If a signal is raised, the Deferred will errback with the
        stdout and stderr up to that point, along with the signal, as (out,
        err, signalNum)
        """
        return _callProtocolWithDeferred(_EverythingGetter,
                                         executable, args, env, path,
                                         reactor)
    utils.getProcessOutputAndValue = getProcessOutputAndValue


# copied from Twisted circa 2.2.0
def _which(name, flags=os.X_OK):
    """Search PATH for executable files with the given name.

    @type name: C{str}
    @param name: The name for which to search.

    @type flags: C{int}
    @param flags: Arguments to L{os.access}.

    @rtype: C{list}
    @return: A list of the full paths to files found, in the
    order in which they were found.
    """
    result = []
    exts = filter(None, os.environ.get('PATHEXT', '').split(os.pathsep))
    for p in os.environ['PATH'].split(os.pathsep):
        p = os.path.join(p, name)
        if os.access(p, flags):
            result.append(p)
        for e in exts:
            pext = p + e
            if os.access(pext, flags):
                result.append(pext)
    return result

which = _which
try:
    from twisted.python.procutils import which
except ImportError:
    pass
