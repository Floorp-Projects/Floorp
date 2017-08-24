# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals, absolute_import

import os

from mozpack import path as mozpath
from mozpack.files import FileFinder


class FilterPath(object):
    """Helper class to make comparing and matching file paths easier."""
    def __init__(self, path, exclude=None):
        self.path = os.path.normpath(path)
        self._finder = None
        self.exclude = exclude

    @property
    def finder(self):
        if self._finder:
            return self._finder
        self._finder = FileFinder(
            self.path, ignore=self.exclude)
        return self._finder

    @property
    def ext(self):
        return os.path.splitext(self.path)[1].strip('.')

    @property
    def exists(self):
        return os.path.exists(self.path)

    @property
    def isfile(self):
        return os.path.isfile(self.path)

    @property
    def isdir(self):
        return os.path.isdir(self.path)

    def join(self, *args):
        return FilterPath(os.path.join(self, *args))

    def match(self, patterns):
        return any(mozpath.match(self.path, pattern.path) for pattern in patterns)

    def contains(self, other):
        """Return True if other is a subdirectory of self or equals self."""
        if isinstance(other, FilterPath):
            other = other.path
        a = os.path.abspath(self.path)
        b = os.path.normpath(os.path.abspath(other))

        if b.startswith(a):
            return True
        return False

    def __repr__(self):
        return repr(self.path)


def filterpaths(paths, linter, **lintargs):
    """Filters a list of paths.

    Given a list of paths, and a linter definition plus extra
    arguments, return the set of paths that should be linted.

    :param paths: A starting list of paths to possibly lint.
    :param linter: A linter definition.
    :param lintargs: Extra arguments passed to the linter.
    :returns: A list of file paths to lint.
    """
    include = linter.get('include', [])
    exclude = lintargs.get('exclude', [])
    exclude.extend(linter.get('exclude', []))
    root = lintargs['root']

    if not lintargs.get('use_filters', True) or (not include and not exclude):
        return paths

    def normalize(path):
        if not os.path.isabs(path):
            path = os.path.join(root, path)
        return FilterPath(path)

    include = map(normalize, include)
    exclude = map(normalize, exclude)

    # Paths with and without globs will be handled separately,
    # pull them apart now.
    includepaths = [p for p in include if p.exists]
    excludepaths = [p for p in exclude if p.exists]

    includeglobs = [p for p in include if not p.exists]
    excludeglobs = [p for p in exclude if not p.exists]

    extensions = linter.get('extensions')
    keep = set()
    discard = set()
    for path in map(FilterPath, paths):
        # Exclude bad file extensions
        if extensions and path.isfile and path.ext not in extensions:
            continue

        if path.match(excludeglobs):
            continue

        # First handle include/exclude directives
        # that exist (i.e don't have globs)
        for inc in includepaths:
            # Only excludes that are subdirectories of the include
            # path matter.
            excs = [e for e in excludepaths if inc.contains(e)]

            if path.contains(inc):
                # If specified path is an ancestor of include path,
                # then lint the include path.
                keep.add(inc)

                # We can't apply these exclude paths without explicitly
                # including every sibling file. Rather than do that,
                # just return them and hope the underlying linter will
                # deal with them.
                discard.update(excs)

            elif inc.contains(path):
                # If the include path is an ancestor of the specified
                # path, then add the specified path only if there are
                # no exclude paths in-between them.
                if not any(e.contains(path) for e in excs):
                    keep.add(path)

        # Next handle include/exclude directives that
        # contain globs.
        if path.isfile:
            # If the specified path is a file it must be both
            # matched by an include directive and not matched
            # by an exclude directive.
            if not path.match(includeglobs):
                continue

            keep.add(path)
        elif path.isdir:
            # If the specified path is a directory, use a
            # FileFinder to resolve all relevant globs.
            path.exclude = [e.path for e in excludeglobs]
            for pattern in includeglobs:
                for p, f in path.finder.find(pattern.path):
                    keep.add(path.join(p))

    # Only pass paths we couldn't exclude here to the underlying linter
    lintargs['exclude'] = [f.path for f in discard]
    return [f.path for f in keep]


def findobject(path):
    """
    Find a Python object given a path of the form <modulepath>:<objectpath>.
    Conceptually equivalent to

        def find_object(modulepath, objectpath):
            import <modulepath> as mod
            return mod.<objectpath>
    """
    if path.count(':') != 1:
        raise ValueError(
            'python path {!r} does not have the form "module:object"'.format(path))

    modulepath, objectpath = path.split(':')
    obj = __import__(modulepath)
    for a in modulepath.split('.')[1:]:
        obj = getattr(obj, a)
    for a in objectpath.split('.'):
        obj = getattr(obj, a)
    return obj
