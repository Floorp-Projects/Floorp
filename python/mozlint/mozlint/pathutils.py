# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozpack import path as mozpath
from mozpack.files import FileFinder


class FilterPath(object):
    """Helper class to make comparing and matching file paths easier."""

    def __init__(self, path):
        self.path = os.path.normpath(path)
        self._finder = None

    @property
    def finder(self):
        if self._finder:
            return self._finder
        self._finder = FileFinder(mozpath.normsep(self.path))
        return self._finder

    @property
    def ext(self):
        return os.path.splitext(self.path)[1].strip(".")

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
        return FilterPath(os.path.join(self.path, *args))

    def match(self, patterns):
        a = mozpath.normsep(self.path)
        for p in patterns:
            if isinstance(p, FilterPath):
                p = p.path
            p = mozpath.normsep(p)
            if mozpath.match(a, p):
                return True
        return False

    def contains(self, other):
        """Return True if other is a subdirectory of self or equals self."""
        if isinstance(other, FilterPath):
            other = other.path
        a = os.path.abspath(self.path)
        b = os.path.normpath(os.path.abspath(other))

        parts_a = a.split(os.sep)
        parts_b = b.split(os.sep)

        if len(parts_a) > len(parts_b):
            return False

        for i, part in enumerate(parts_a):
            if part != parts_b[i]:
                return False
        return True

    def __repr__(self):
        return repr(self.path)


def collapse(paths, base=None, dotfiles=False):
    """Given an iterable of paths, collapse them into the smallest possible set
    of paths that contain the original set (without containing any extra paths).

    For example, if directory 'a' contains two files b.txt and c.txt, calling:

        collapse(['a/b.txt', 'a/c.txt'])

    returns ['a']. But if a third file d.txt also exists, then it will return
    ['a/b.txt', 'a/c.txt'] since ['a'] would also include that extra file.

    :param paths: An iterable of paths (files and directories) to collapse.
    :returns: The smallest set of paths (files and directories) that contain
              the original set of paths and only the original set.
    """
    if not paths:
        if not base:
            return []

        # Need to test whether directory chain is empty. If it is then bubble
        # the base back up so that it counts as 'covered'.
        for _, _, names in os.walk(base):
            if names:
                return []
        return [base]

    if not base:
        paths = list(map(mozpath.abspath, paths))
        base = mozpath.commonprefix(paths).rstrip("/")

        # Make sure `commonprefix` factors in sibling directories that have the
        # same prefix in their basenames.
        parent = mozpath.dirname(base)
        same_prefix = [
            p for p in os.listdir(parent) if p.startswith(mozpath.basename(base))
        ]
        if not os.path.isdir(base) or len(same_prefix) > 1:
            base = parent

    if base in paths:
        return [base]

    covered = set()
    full = set()
    for name in os.listdir(base):
        if not dotfiles and name[0] == ".":
            continue

        path = mozpath.join(base, name)
        full.add(path)

        if path in paths:
            # This path was explicitly specified, so just bubble it back up
            # without recursing down into it (if it was a directory).
            covered.add(path)
        elif os.path.isdir(path):
            new_paths = [p for p in paths if p.startswith(path)]
            covered.update(collapse(new_paths, base=path, dotfiles=dotfiles))

    if full == covered:
        # Every file under this base was covered, so we can collapse them all
        # up into the base path.
        return [base]
    return list(covered)


def filterpaths(root, paths, include, exclude=None, extensions=None):
    """Filters a list of paths.

    Given a list of paths and some filtering rules, return the set of paths
    that should be linted.

    :param paths: A starting list of paths to possibly lint.
    :param include: A list of paths that should be included (required).
    :param exclude: A list of paths that should be excluded (optional).
    :param extensions: A list of file extensions which should be considered (optional).
    :returns: A tuple containing a list of file paths to lint and a list of
              paths to exclude.
    """

    def normalize(path):
        if "*" not in path and not os.path.isabs(path):
            path = os.path.join(root, path)
        return FilterPath(path)

    # Includes are always paths and should always exist.
    include = list(map(normalize, include))

    # Exclude paths with and without globs will be handled separately,
    # pull them apart now.
    exclude = list(map(normalize, exclude or []))
    excludepaths = [p for p in exclude if p.exists]
    excludeglobs = [p.path for p in exclude if not p.exists]

    keep = set()
    discard = set()
    for path in list(map(normalize, paths)):
        # Exclude bad file extensions
        if extensions and path.isfile and path.ext not in extensions:
            continue

        if path.match(excludeglobs):
            continue

        # First handle include/exclude directives
        # that exist (i.e don't have globs)
        for inc in include:
            if inc.isfile:
                keep.add(inc)

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
                    discard.update([e for e in excs if path.contains(e)])

        # Next expand excludes with globs in them so we can add them to
        # the set of files to discard.
        for pattern in excludeglobs:
            for p, f in path.finder.find(pattern):
                discard.add(path.join(p))

    return (
        [f.path for f in keep if f.exists],
        collapse([f.path for f in discard if f.exists]),
    )


def findobject(path):
    """
    Find a Python object given a path of the form <modulepath>:<objectpath>.
    Conceptually equivalent to

        def find_object(modulepath, objectpath):
            import <modulepath> as mod
            return mod.<objectpath>
    """
    if path.count(":") != 1:
        raise ValueError(
            'python path {!r} does not have the form "module:object"'.format(path)
        )

    modulepath, objectpath = path.split(":")
    obj = __import__(modulepath)
    for a in modulepath.split(".")[1:]:
        obj = getattr(obj, a)
    for a in objectpath.split("."):
        obj = getattr(obj, a)
    return obj


def ancestors(path):
    while path:
        yield path
        (path, child) = os.path.split(path)
        if child == "":
            break


def get_ancestors_by_name(name, path, root):
    """Returns a list of files called `name` in `path`'s ancestors,
    sorted from closest->furthest. This can be useful for finding
    relevant configuration files.
    """
    configs = []
    for path in ancestors(path):
        config = os.path.join(path, name)
        if os.path.isfile(config):
            configs.append(config)
        if path == root:
            break
    return configs


def expand_exclusions(paths, config, root):
    """Returns all files that match patterns and aren't excluded.

    This is used by some external linters who receive 'batch' files (e.g dirs)
    but aren't capable of applying their own exclusions. There is an argument
    to be made that this step should just apply to all linters no matter what.

    Args:
        paths (list): List of candidate paths to lint.
        config (dict): Linter's config object.
        root (str): Root of the repository.

    Returns:
        Generator which generates list of paths that weren't excluded.
    """
    extensions = [e.lstrip(".") for e in config.get("extensions", [])]
    find_dotfiles = config.get("find-dotfiles", False)

    def normalize(path):
        path = mozpath.normpath(path)
        if os.path.isabs(path):
            return path
        return mozpath.join(root, path)

    exclude = list(map(normalize, config.get("exclude", [])))
    for path in paths:
        path = mozpath.normsep(path)
        if os.path.isfile(path):
            if any(path.startswith(e) for e in exclude if "*" not in e):
                continue

            if any(mozpath.match(path, e) for e in exclude if "*" in e):
                continue

            yield path
            continue

        ignore = [
            e[len(path) :].lstrip("/")
            for e in exclude
            if mozpath.commonprefix((path, e)) == path
        ]
        finder = FileFinder(path, ignore=ignore, find_dotfiles=find_dotfiles)

        _, ext = os.path.splitext(path)
        ext.lstrip(".")

        for ext in extensions:
            for p, f in finder.find("**/*.{}".format(ext)):
                yield os.path.join(path, p)
