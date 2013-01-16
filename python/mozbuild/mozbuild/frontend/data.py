# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

r"""Data structures representing Mozilla's source tree.

The frontend files are parsed into static data structures. These data
structures are defined in this module.

All data structures of interest are children of the TreeMetadata class.

Logic for populating these data structures is not defined in this class.
Instead, what we have here are dumb container classes. The emitter module
contains the code for converting executed mozbuild files into these data
structures.
"""

from __future__ import unicode_literals

from collections import OrderedDict


class TreeMetadata(object):
    """Base class for all data being captured."""


class SandboxDerived(TreeMetadata):
    """Build object derived from a single MozbuildSandbox instance.

    It holds fields common to all sandboxes. This class is likely never
    instantiated directly but is instead derived from.
    """

    __slots__ = (
        'objdir',
        'relativedir',
        'srcdir',
        'topobjdir',
        'topsrcdir',
    )

    def __init__(self, sandbox):
        self.topsrcdir = sandbox['TOPSRCDIR']
        self.topobjdir = sandbox['TOPOBJDIR']

        self.relativedir = sandbox['RELATIVEDIR']
        self.srcdir = sandbox['SRCDIR']
        self.objdir = sandbox['OBJDIR']


class DirectoryTraversal(SandboxDerived):
    """Describes how directory traversal for building should work.

    This build object is likely only of interest to the recursive make backend.
    Other build backends should (ideally) not attempt to mimic the behavior of
    the recursive make backend. The only reason this exists is to support the
    existing recursive make backend while the transition to mozbuild frontend
    files is complete and we move to a more optimal build backend.

    Fields in this class correspond to similarly named variables in the
    frontend files.
    """
    __slots__ = (
        'dirs',
        'parallel_dirs',
        'tool_dirs',
        'test_dirs',
        'test_tool_dirs',
        'tier_dirs',
        'tier_static_dirs',
    )

    def __init__(self, sandbox):
        SandboxDerived.__init__(self, sandbox)

        self.dirs = []
        self.parallel_dirs = []
        self.tool_dirs = []
        self.test_dirs = []
        self.test_tool_dirs = []
        self.tier_dirs = OrderedDict()
        self.tier_static_dirs = OrderedDict()
