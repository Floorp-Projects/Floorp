# -*- coding: utf-8 -*-
# Copyright 2019 Avram Lubkin, All Rights Reserved

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Provides a terminal class primarily for accessing functions that depend on
a terminal which has been previously setup as well as those functions
"""

import importlib
import io
import platform
import sys

from jinxed.terminfo import BOOL_CAPS, NUM_CAPS
from jinxed._util import BASESTRING, error, raise_from_none

if platform.system() == 'Windows':  # pragma: no branch
    from jinxed.win32 import get_term  # pragma: no cover
else:
    from jinxed._util import get_term


TERM = None


class Terminal(object):
    """
    Persistent terminal object for functions that require a previously configured state
    """

    def __init__(self, term=None, fd=-1):  # pylint: disable=invalid-name

        # Type check for term
        if term is not None and not isinstance(term, BASESTRING):
            raise TypeError('term must be a string or None, not %s' % type(term).__name__)

        # Type check and default handling for fd
        if fd == -1:
            try:
                self.stream_fd = sys.stdout.fileno()
            except (AttributeError, TypeError, io.UnsupportedOperation):
                self.stream_fd = None
        elif not isinstance(fd, int):
            raise TypeError('fd must be an integer, not %s' % type(fd).__name__)
        else:
            self.stream_fd = fd

        # Try to dynamically determine terminal type
        if term is None:
            term = get_term(self.stream_fd)

        try:
            self.terminfo = importlib.import_module('jinxed.terminfo.%s' % term.replace('-', '_'))
        except ImportError:
            raise_from_none(error('Could not find terminal %s' % term))

    def tigetstr(self, capname):
        """
        Reimplementation of curses.tigetstr()
        """

        return self.terminfo.STR_CAPS.get(capname, None)

    def tigetnum(self, capname):
        """
        Reimplementation of curses.tigetnum()
        """

        return self.terminfo.NUM_CAPS.get(capname, -1 if capname in NUM_CAPS else -2)

    def tigetflag(self, capname):
        """
        Reimplementation of curses.tigetflag()
        """

        if capname in self.terminfo.BOOL_CAPS:
            return 1
        if capname in BOOL_CAPS:
            return 0
        return -1


def setupterm(term=None, fd=-1):  # pylint: disable=invalid-name
    """
    Reimplementation of :py:func:`curses.setupterm`
    """

    global TERM  # pylint: disable=global-statement
    TERM = Terminal(term, fd)


def tigetflag(capname):
    """
    Reimplementation of :py:func:`curses.tigetflag`
    """

    if TERM is None:
        raise error('Must call setupterm() first')
    return TERM.tigetflag(capname)


def tigetnum(capname):
    """
    Reimplementation of :py:func:`curses.tigetnum`
    """

    if TERM is None:
        raise error('Must call setupterm() first')
    return TERM.tigetnum(capname)


def tigetstr(capname):
    """
    Reimplementation of :py:func:`curses.tigetstr`
    """

    if TERM is None:
        raise error('Must call setupterm() first')
    return TERM.tigetstr(capname)
