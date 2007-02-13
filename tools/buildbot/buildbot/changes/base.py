#! /usr/bin/python

from twisted.application import service

from buildbot.twcompat import implements
from buildbot.interfaces import IChangeSource
from buildbot import util

class ChangeSource(service.Service, util.ComparableMixin):
    if implements:
        implements(IChangeSource)
    else:
        __implements__ = IChangeSource, service.Service.__implements__
