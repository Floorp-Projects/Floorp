# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules provides functionality for dealing with compiler warnings.

import json
import os
import re

from mozbuild.util import hash_file


# Regular expression to strip ANSI color sequences from a string. This is
# needed to properly analyze Clang compiler output, which may be colorized.
# It assumes ANSI escape sequences.
RE_STRIP_COLORS = re.compile(r'\x1b\[[\d;]+m')

# This captures Clang diagnostics with the standard formatting.
RE_CLANG_WARNING = re.compile(r"""
    (?P<file>[^:]+)
    :
    (?P<line>\d+)
    :
    (?P<column>\d+)
    :
    \swarning:\s
    (?P<message>[^\[]+)
    \[(?P<flag>[^\]]+)
    """, re.X)

# This captures Visual Studio's warning format.
RE_MSVC_WARNING = re.compile(r"""
    (?P<file>.*)
    \((?P<line>\d+)\)
    \s:\swarning\s
    (?P<flag>[^:]+)
    :\s
    (?P<message>.*)
    """, re.X)

IN_FILE_INCLUDED_FROM = 'In file included from '


class CompilerWarning(dict):
    """Represents an individual compiler warning."""

    def __init__(self):
        dict.__init__(self)

        self['filename'] = None
        self['line'] = None
        self['column'] = None
        self['message'] = None
        self['flag'] = None

    def __eq__(self, other):
        if not isinstance(other, CompilerWarning):
            return False

        return self['filename'] == other['filename'] \
            and self['line'] == other['line'] \
            and self['column'] == other['column']

    def __neq__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        """Define so this can exist inside a set, etc."""
        return hash(tuple(sorted(self.items())))


class WarningsDatabase(object):
    """Holds a collection of warnings.

    The warnings database is a semi-intelligent container that holds warnings
    encountered during builds.

    The warnings database is backed by a JSON file. But, that is transparent
    to consumers.

    Under most circumstances, the warnings database is insert only. When a
    warning is encountered, the caller simply blindly inserts it into the
    database. The database figures out whether it is a dupe, etc.

    During the course of development, it is common for warnings to change
    slightly as source code changes. For example, line numbers will disagree.
    The WarningsDatabase handles this by storing the hash of a file a warning
    occurred in. At warning insert time, if the hash of the file does not match
    what is stored in the database, the existing warnings for that file are
    purged from the database.

    Callers should periodically prune old, invalid warnings from the database
    by calling prune(). A good time to do this is at the end of a build.
    """
    def __init__(self):
        """Create an empty database."""
        self._files = {}

    def __len__(self):
        i = 0
        for value in self._files.values():
            i += len(value['warnings'])

        return i

    def __iter__(self):
        for value in self._files.values():
            for warning in value['warnings']:
                yield warning

    def __contains__(self, item):
        for value in self._files.values():
            for warning in value['warnings']:
                if warning == item:
                    return True

        return False

    @property
    def warnings(self):
        """All the CompilerWarning instances in this database."""
        for value in self._files.values():
            for w in value['warnings']:
                yield w

    @property
    def type_counts(self):
        """Returns a mapping of warning types to their counts."""

        types = {}
        for value in self._files.values():
            for warning in value['warnings']:
                count = types.get(warning['flag'], 0)
                count += 1

                types[warning['flag']] = count

        return types

    def has_file(self, filename):
        """Whether we have any warnings for the specified file."""
        return filename in self._files

    def warnings_for_file(self, filename):
        """Obtain the warnings for the specified file."""
        f = self._files.get(filename, {'warnings': []})

        for warning in f['warnings']:
            yield warning

    def insert(self, warning, compute_hash=True):
        assert isinstance(warning, CompilerWarning)

        filename = warning['filename']

        new_hash = None

        if compute_hash:
            new_hash = hash_file(filename)

        if filename in self._files:
            if new_hash != self._files[filename]['hash']:
                del self._files[filename]

        value = self._files.get(filename, {
            'hash': new_hash,
            'warnings': set(),
        })

        value['warnings'].add(warning)

        self._files[filename] = value

    def prune(self):
        """Prune the contents of the database.

        This removes warnings that are no longer valid. A warning is no longer
        valid if the file it was in no longer exists or if the content has
        changed.

        The check for changed content catches the case where a file previously
        contained warnings but no longer does.
        """

        # Need to calculate up front since we are mutating original object.
        filenames = self._files.keys()
        for filename in filenames:
            if not os.path.exists(filename):
                del self._files[filename]
                continue

            if self._files[filename]['hash'] is None:
                continue

            current_hash = hash_file(filename)
            if current_hash != self._files[filename]['hash']:
                del self._files[filename]
                continue

    def serialize(self, fh):
        """Serialize the database to an open file handle."""
        obj = {'files': {}}

        # All this hackery because JSON can't handle sets.
        for k, v in self._files.iteritems():
            obj['files'][k] = {}

            for k2, v2 in v.iteritems():
                normalized = v2

                if k2 == 'warnings':
                    normalized = [w for w in v2]

                obj['files'][k][k2] = normalized

        json.dump(obj, fh, indent=2)

    def deserialize(self, fh):
        """Load serialized content from a handle into the current instance."""
        obj = json.load(fh)

        self._files = obj['files']

        # Normalize data types.
        for filename, value in self._files.iteritems():
            for k, v in value.iteritems():
                if k != 'warnings':
                    continue

                normalized = set()
                for d in v:
                    w = CompilerWarning()
                    w.update(d)
                    normalized.add(w)

                self._files[filename]['warnings'] = normalized

    def load_from_file(self, filename):
        """Load the database from a file."""
        with open(filename, 'rb') as fh:
            self.deserialize(fh)

    def save_to_file(self, filename):
        """Save the database to a file."""
        with open(filename, 'wb') as fh:
            self.serialize(fh)


class WarningsCollector(object):
    """Collects warnings from text data.

    Instances of this class receive data (usually the output of compiler
    invocations) and parse it into warnings and add these warnings to a
    database.

    The collector works by incrementally receiving data, usually line-by-line
    output from the compiler. Therefore, it can maintain state to parse
    multi-line warning messages.
    """
    def __init__(self, database=None, objdir=None, resolve_files=True):
        self.database = database
        self.objdir = objdir
        self.resolve_files = resolve_files
        self.included_from = []

        if database is None:
            self.database = WarningsDatabase()

    def process_line(self, line):
        """Take a line of text and process it for a warning."""

        filtered = RE_STRIP_COLORS.sub('', line)

        # Clang warnings in files included from the one(s) being compiled will
        # start with "In file included from /path/to/file:line:". Here, we
        # record those.
        if filtered.startswith(IN_FILE_INCLUDED_FROM):
            included_from = filtered[len(IN_FILE_INCLUDED_FROM):]

            parts = included_from.split(':')

            self.included_from.append(parts[0])

            return

        warning = CompilerWarning()
        filename = None

        # TODO make more efficient so we run minimal regexp matches.
        match_clang = RE_CLANG_WARNING.match(filtered)
        match_msvc = RE_MSVC_WARNING.match(filtered)
        if match_clang:
            d = match_clang.groupdict()

            filename = d['file']
            warning['line'] = int(d['line'])
            warning['column'] = int(d['column'])
            warning['flag'] = d['flag']
            warning['message'] = d['message'].rstrip()

        elif match_msvc:
            d = match_msvc.groupdict()

            filename = d['file']
            warning['line'] = int(d['line'])
            warning['flag'] = d['flag']
            warning['message'] = d['message'].rstrip()
        else:
            self.included_from = []
            return None

        filename = os.path.normpath(filename)

        # Sometimes we get relative includes. These typically point to files in
        # the object directory. We try to resolve the relative path.
        if not os.path.isabs(filename):
            filename = self._normalize_relative_path(filename)

        if not os.path.exists(filename) and self.resolve_files:
            raise Exception('Could not find file containing warning: %s' %
                    filename)

        warning['filename'] = filename

        self.database.insert(warning, compute_hash=self.resolve_files)

        return warning

    def _normalize_relative_path(self, filename):
        # Special case files in dist/include.
        idx = filename.find('/dist/include')
        if idx != -1:
            return self.objdir + filename[idx:]

        for included_from in self.included_from:
            source_dir = os.path.dirname(included_from)

            candidate = os.path.normpath(os.path.join(source_dir, filename))

            if os.path.exists(candidate):
                return candidate

        return filename
