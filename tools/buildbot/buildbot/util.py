# -*- test-case-name: buildbot.test.test_util -*-

from twisted.internet.defer import Deferred
from twisted.spread import pb
import time

def now():
    #return int(time.time())
    return time.time()

def earlier(old, new):
    # minimum of two things, but "None" counts as +infinity
    if old:
        if new < old:
            return new
        return old
    return new

def later(old, new):
    # maximum of two things, but "None" counts as -infinity
    if old:
        if new > old:
            return new
        return old
    return new

class CancelableDeferred(Deferred):
    """I am a version of Deferred that can be canceled by calling my
    .cancel() method. After being canceled, no callbacks or errbacks will be
    executed.
    """
    def __init__(self):
        Deferred.__init__(self)
        self.canceled = 0
    def cancel(self):
        self.canceled = 1
    def _runCallbacks(self):
        if self.canceled:
            self.callbacks = []
            return
        Deferred._runCallbacks(self)

def ignoreStaleRefs(failure):
    """d.addErrback(util.ignoreStaleRefs)"""
    r = failure.trap(pb.DeadReferenceError, pb.PBConnectionLost)
    return None

class _None:
    pass

class ComparableMixin:
    """Specify a list of attributes that are 'important'. These will be used
    for all comparison operations."""

    compare_attrs = []

    def __hash__(self):
        alist = [self.__class__] + \
                [getattr(self, name, _None) for name in self.compare_attrs]
        return hash(tuple(alist))

    def __cmp__(self, them):
        if cmp(type(self), type(them)):
            return cmp(type(self), type(them))
        if cmp(self.__class__, them.__class__):
            return cmp(self.__class__, them.__class__)
        assert self.compare_attrs == them.compare_attrs
        self_list= [getattr(self, name, _None) for name in self.compare_attrs]
        them_list= [getattr(them, name, _None) for name in self.compare_attrs]
        return cmp(self_list, them_list)
