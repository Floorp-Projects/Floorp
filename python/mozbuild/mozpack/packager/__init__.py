# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mozbuild.preprocessor import Preprocessor
import re
import os
from mozpack.errors import errors
from mozpack.chrome.manifest import (
    Manifest,
    ManifestChrome,
    ManifestInterfaces,
    is_manifest,
    parse_manifest,
)
import mozpack.path as mozpath
from collections import deque


class Component(object):
    '''
    Class that represents a component in a package manifest.
    '''
    def __init__(self, name, destdir=''):
        if name.find(' ') > 0:
            errors.fatal('Malformed manifest: space in component name "%s"'
                         % component)
        self._name = name
        self._destdir = destdir

    def __repr__(self):
        s = self.name
        if self.destdir:
            s += ' destdir="%s"' % self.destdir
        return s

    @property
    def name(self):
        return self._name

    @property
    def destdir(self):
        return self._destdir

    @staticmethod
    def _triples(lst):
        '''
        Split [1, 2, 3, 4, 5, 6, 7] into [(1, 2, 3), (4, 5, 6)].
        '''
        return zip(*[iter(lst)] * 3)

    KEY_VALUE_RE = re.compile(r'''
        \s*                 # optional whitespace.
        ([a-zA-Z0-9_]+)     # key.
        \s*=\s*             # optional space around =.
        "([^"]*)"           # value without surrounding quotes.
        (?:\s+|$)
        ''', re.VERBOSE)

    @staticmethod
    def _split_options(string):
        '''
        Split 'key1="value1" key2="value2"' into
        {'key1':'value1', 'key2':'value2'}.

        Returned keys and values are all strings.

        Throws ValueError if the input is malformed.
        '''
        options = {}
        splits = Component.KEY_VALUE_RE.split(string)
        if len(splits) % 3 != 1:
            # This should never happen -- we expect to always split
            # into ['', ('key', 'val', '')*].
            raise ValueError("Bad input")
        if splits[0]:
            raise ValueError('Unrecognized input ' + splits[0])
        for key, val, no_match in Component._triples(splits[1:]):
            if no_match:
                raise ValueError('Unrecognized input ' + no_match)
            options[key] = val
        return options

    @staticmethod
    def _split_component_and_options(string):
        '''
        Split 'name key1="value1" key2="value2"' into
        ('name', {'key1':'value1', 'key2':'value2'}).

        Returned name, keys and values are all strings.

        Raises ValueError if the input is malformed.
        '''
        splits = string.strip().split(None, 1)
        if not splits:
            raise ValueError('No component found')
        component = splits[0].strip()
        if not component:
            raise ValueError('No component found')
        if not re.match('[a-zA-Z0-9_\-]+$', component):
            raise ValueError('Bad component name ' + component)
        options = Component._split_options(splits[1]) if len(splits) > 1 else {}
        return component, options

    @staticmethod
    def from_string(string):
        '''
        Create a component from a string.
        '''
        try:
            name, options = Component._split_component_and_options(string)
        except ValueError as e:
            errors.fatal('Malformed manifest: %s' % e)
            return
        destdir = options.pop('destdir', '')
        if options:
            errors.fatal('Malformed manifest: options %s not recognized'
                         % options.keys())
        return Component(name, destdir=destdir)


class PackageManifestParser(object):
    '''
    Class for parsing of a package manifest, after preprocessing.

    A package manifest is a list of file paths, with some syntaxic sugar:
        [] designates a toplevel component. Example: [xpcom]
        - in front of a file specifies it to be removed
        * wildcard support
        ** expands to all files and zero or more directories
        ; file comment

    The parser takes input from the preprocessor line by line, and pushes
    parsed information to a sink object.

    The add and remove methods of the sink object are called with the
    current Component instance and a path.
    '''
    def __init__(self, sink):
        '''
        Initialize the package manifest parser with the given sink.
        '''
        self._component = Component('')
        self._sink = sink

    def handle_line(self, str):
        '''
        Handle a line of input and push the parsed information to the sink
        object.
        '''
        # Remove comments.
        str = str.strip()
        if not str or str.startswith(';'):
            return
        if str.startswith('[') and str.endswith(']'):
            self._component = Component.from_string(str[1:-1])
        elif str.startswith('-'):
            str = str[1:]
            self._sink.remove(self._component, str)
        elif ',' in str:
            errors.fatal('Incompatible syntax')
        else:
            self._sink.add(self._component, str)


class PreprocessorOutputWrapper(object):
    '''
    File-like helper to handle the preprocessor output and send it to a parser.
    The parser's handle_line method is called in the relevant errors.context.
    '''
    def __init__(self, preprocessor, parser):
        self._parser = parser
        self._pp = preprocessor

    def write(self, str):
        file = os.path.normpath(os.path.abspath(self._pp.context['FILE']))
        with errors.context(file, self._pp.context['LINE']):
            self._parser.handle_line(str)


def preprocess(input, parser, defines={}):
    '''
    Preprocess the file-like input with the given defines, and send the
    preprocessed output line by line to the given parser.
    '''
    pp = Preprocessor()
    pp.context.update(defines)
    pp.do_filter('substitution')
    pp.out = PreprocessorOutputWrapper(pp, parser)
    pp.do_include(input)


def preprocess_manifest(sink, manifest, defines={}):
    '''
    Preprocess the given file-like manifest with the given defines, and push
    the parsed information to a sink. See PackageManifestParser documentation
    for more details on the sink.
    '''
    preprocess(manifest, PackageManifestParser(sink), defines)


class CallDeque(deque):
    '''
    Queue of function calls to make.
    '''
    def append(self, function, *args):
        deque.append(self, (errors.get_context(), function, args))

    def execute(self):
        while True:
            try:
                context, function, args = self.popleft()
            except IndexError:
                return
            if context:
                with errors.context(context[0], context[1]):
                    function(*args)
            else:
                function(*args)


class SimplePackager(object):
    '''
    Helper used to translate and buffer instructions from the
    SimpleManifestSink to a formatter. Formatters expect some information to be
    given first that the simple manifest contents can't guarantee before the
    end of the input.
    '''
    def __init__(self, formatter):
        self.formatter = formatter
        # Queue for formatter.add_interfaces()/add_manifest() calls.
        self._queue = CallDeque()
        # Queue for formatter.add_manifest() calls for ManifestChrome.
        self._chrome_queue = CallDeque()
        # Queue for formatter.add() calls.
        self._file_queue = CallDeque()
        # All paths containing addons.
        self._addons = set()
        # All manifest paths imported.
        self._manifests = set()
        # All manifest paths included from some other manifest.
        self._included_manifests = {}
        self._closed = False

    def add(self, path, file):
        '''
        Add the given BaseFile instance with the given path.
        '''
        assert not self._closed
        if is_manifest(path):
            self._add_manifest_file(path, file)
        elif path.endswith('.xpt'):
            self._queue.append(self.formatter.add_interfaces, path, file)
        else:
            self._file_queue.append(self.formatter.add, path, file)
            if mozpath.basename(path) == 'install.rdf':
                self._addons.add(mozpath.dirname(path))

    def _add_manifest_file(self, path, file):
        '''
        Add the given BaseFile with manifest file contents with the given path.
        '''
        self._manifests.add(path)
        base = ''
        if hasattr(file, 'path'):
            # Find the directory the given path is relative to.
            b = mozpath.normsep(file.path)
            if b.endswith('/' + path) or b == path:
                base = os.path.normpath(b[:-len(path)])
        for e in parse_manifest(base, path, file.open()):
            # ManifestResources need to be given after ManifestChrome, so just
            # put all ManifestChrome in a separate queue to make them first.
            if isinstance(e, ManifestChrome):
                # e.move(e.base) just returns a clone of the entry.
                self._chrome_queue.append(self.formatter.add_manifest,
                                          e.move(e.base))
            elif not isinstance(e, (Manifest, ManifestInterfaces)):
                self._queue.append(self.formatter.add_manifest, e.move(e.base))
            if isinstance(e, Manifest):
                if e.flags:
                    errors.fatal('Flags are not supported on ' +
                                 '"manifest" entries')
                self._included_manifests[e.path] = path

    def get_bases(self, addons=True):
        '''
        Return all paths under which root manifests have been found. Root
        manifests are manifests that are included in no other manifest.
        `addons` indicates whether to include addon bases as well.
        '''
        all_bases = set(mozpath.dirname(m)
                        for m in self._manifests
                                 - set(self._included_manifests))
        if not addons:
            all_bases -= self._addons
        else:
            # If for some reason some detected addon doesn't have a
            # non-included manifest.
            all_bases |= self._addons
        return all_bases

    def close(self):
        '''
        Push all instructions to the formatter.
        '''
        self._closed = True

        bases = self.get_bases()
        broken_bases = sorted(
            m for m, includer in self._included_manifests.iteritems()
            if mozpath.basedir(m, bases) != mozpath.basedir(includer, bases))
        for m in broken_bases:
            errors.fatal('"%s" is included from "%s", which is outside "%s"' %
                         (m, self._included_manifests[m],
                          mozpath.basedir(m, bases)))
        for base in bases:
            self.formatter.add_base(base, base in self._addons)
        self._chrome_queue.execute()
        self._queue.execute()
        self._file_queue.execute()


class SimpleManifestSink(object):
    '''
    Parser sink for "simple" package manifests. Simple package manifests use
    the format described in the PackageManifestParser documentation, but don't
    support file removals, and require manifests, interfaces and chrome data to
    be explicitely listed.
    Entries starting with bin/ are searched under bin/ in the FileFinder, but
    are packaged without the bin/ prefix.
    '''
    def __init__(self, finder, formatter):
        '''
        Initialize the SimpleManifestSink. The given FileFinder is used to
        get files matching the patterns given in the manifest. The given
        formatter does the packaging job.
        '''
        self._finder = finder
        self.packager = SimplePackager(formatter)
        self._closed = False
        self._manifests = set()

    @staticmethod
    def normalize_path(path):
        '''
        Remove any bin/ prefix.
        '''
        if mozpath.basedir(path, ['bin']) == 'bin':
            return mozpath.relpath(path, 'bin')
        return path

    def add(self, component, pattern):
        '''
        Add files with the given pattern in the given component.
        '''
        assert not self._closed
        added = False
        for p, f in self._finder.find(pattern):
            added = True
            if is_manifest(p):
                self._manifests.add(p)
            dest = mozpath.join(component.destdir, SimpleManifestSink.normalize_path(p))
            self.packager.add(dest, f)
        if not added:
            errors.error('Missing file(s): %s' % pattern)

    def remove(self, component, pattern):
        '''
        Remove files with the given pattern in the given component.
        '''
        assert not self._closed
        errors.fatal('Removal is unsupported')

    def close(self, auto_root_manifest=True):
        '''
        Add possibly missing bits and push all instructions to the formatter.
        '''
        if auto_root_manifest:
            # Simple package manifests don't contain the root manifests, so
            # find and add them.
            paths = [mozpath.dirname(m) for m in self._manifests]
            path = mozpath.dirname(mozpath.commonprefix(paths))
            for p, f in self._finder.find(mozpath.join(path,
                                          'chrome.manifest')):
                if not p in self._manifests:
                    self.packager.add(SimpleManifestSink.normalize_path(p), f)
        self.packager.close()
