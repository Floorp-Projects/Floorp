# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Like :py:mod:`os.path`, with a reduced set of functions, and with normalized path
separators (always use forward slashes).
Also contains a few additional utilities not found in :py:mod:`os.path`.
"""

# Imported from
# https://searchfox.org/mozilla-central/rev/c3ebaf6de2d481c262c04bb9657eaf76bf47e2ac/python/mozbuild/mozpack/path.py


import os
import posixpath
import re


def normsep(path):
    """
    Normalize path separators, by using forward slashes instead of whatever
    :py:const:`os.sep` is.
    """
    if os.sep != "/":
        path = path.replace(os.sep, "/")
    if os.altsep and os.altsep != "/":
        path = path.replace(os.altsep, "/")
    return path


def relpath(path, start):
    rel = normsep(os.path.relpath(path, start))
    return "" if rel == "." else rel


def realpath(path):
    return normsep(os.path.realpath(path))


def abspath(path):
    return normsep(os.path.abspath(path))


def join(*paths):
    return normsep(os.path.join(*paths))


def normpath(path):
    return posixpath.normpath(normsep(path))


def dirname(path):
    return posixpath.dirname(normsep(path))


def commonprefix(paths):
    return posixpath.commonprefix([normsep(path) for path in paths])


def basename(path):
    return os.path.basename(path)


def splitext(path):
    return posixpath.splitext(normsep(path))


def split(path):
    """
    Return the normalized path as a list of its components.

        ``split('foo/bar/baz')`` returns ``['foo', 'bar', 'baz']``
    """
    return normsep(path).split("/")


def basedir(path, bases):
    """
    Given a list of directories (`bases`), return which one contains the given
    path. If several matches are found, the deepest base directory is returned.

        ``basedir('foo/bar/baz', ['foo', 'baz', 'foo/bar'])`` returns ``'foo/bar'``
        (`'foo'` and `'foo/bar'` both match, but `'foo/bar'` is the deepest match)
    """
    path = normsep(path)
    bases = [normsep(b) for b in bases]
    if path in bases:
        return path
    for b in sorted(bases, reverse=True):
        if b == "" or path.startswith(b + "/"):
            return b


re_cache = {}
# Python versions < 3.7 return r'\/' for re.escape('/').
if re.escape("/") == "/":
    MATCH_STAR_STAR_RE = re.compile(r"(^|/)\\\*\\\*/")
    MATCH_STAR_STAR_END_RE = re.compile(r"(^|/)\\\*\\\*$")
else:
    MATCH_STAR_STAR_RE = re.compile(r"(^|\\\/)\\\*\\\*\\\/")
    MATCH_STAR_STAR_END_RE = re.compile(r"(^|\\\/)\\\*\\\*$")


def match(path, pattern):
    """
    Return whether the given path matches the given pattern.
    An asterisk can be used to match any string, including the null string, in
    one part of the path:

        ``foo`` matches ``*``, ``f*`` or ``fo*o``

    However, an asterisk matching a subdirectory may not match the null string:

        ``foo/bar`` does *not* match ``foo/*/bar``

    If the pattern matches one of the ancestor directories of the path, the
    patch is considered matching:

        ``foo/bar`` matches ``foo``

    Two adjacent asterisks can be used to match files and zero or more
    directories and subdirectories.

        ``foo/bar`` matches ``foo/**/bar``, or ``**/bar``
    """
    if not pattern:
        return True
    if pattern not in re_cache:
        p = re.escape(pattern)
        p = MATCH_STAR_STAR_RE.sub(r"\1(?:.+/)?", p)
        p = MATCH_STAR_STAR_END_RE.sub(r"(?:\1.+)?", p)
        p = p.replace(r"\*", "[^/]*") + "(?:/.*)?$"
        re_cache[pattern] = re.compile(p)
    return re_cache[pattern].match(path) is not None


def rebase(oldbase, base, relativepath):
    """
    Return `relativepath` relative to `base` instead of `oldbase`.
    """
    if base == oldbase:
        return relativepath
    if len(base) < len(oldbase):
        assert basedir(oldbase, [base]) == base
        relbase = relpath(oldbase, base)
        result = join(relbase, relativepath)
    else:
        assert basedir(base, [oldbase]) == oldbase
        relbase = relpath(base, oldbase)
        result = relpath(relativepath, relbase)
    result = normpath(result)
    if relativepath.endswith("/") and not result.endswith("/"):
        result += "/"
    return result


def ancestors(path):
    """Emit the parent directories of a path.

    Args:
        path (str): Path to emit parents of.

    Yields:
        str: Path of parent directory.
    """
    while path:
        yield path
        newpath = os.path.dirname(path)
        if newpath == path:
            break
        path = newpath
