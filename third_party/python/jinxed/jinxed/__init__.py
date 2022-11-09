# -*- coding: utf-8 -*-
# Copyright 2019 - 2021 Avram Lubkin, All Rights Reserved

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Jinxed Terminal Library

Jinxed is an implementation of a subset of the Python curses library for Windows

Other libraries implement the full curses stack. Jinxed is intended primarily for libraries
that need to access terminfo functions such as tigetstr() and tparm().
"""

# flake8: noqa: F401

from jinxed._keys import *
from jinxed._tparm import tparm
from jinxed._terminal import setupterm, tigetflag, tigetnum, tigetstr
from jinxed._util import error, IS_WINDOWS

if IS_WINDOWS:  # pragma: no branch
    from jinxed.win32 import get_term  # pragma: no cover
else:
    from jinxed._util import get_term


__version__ = '1.2.0'

COLOR_BLACK = 0
COLOR_RED = 1
COLOR_GREEN = 2
COLOR_YELLOW = 3
COLOR_BLUE = 4
COLOR_MAGENTA = 5
COLOR_CYAN = 6
COLOR_WHITE = 7
