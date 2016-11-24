# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from StringIO import StringIO
import json
import fnmatch
import os
import shutil
import sys
import types

from .ini import read_ini
from .filters import (
    DEFAULT_FILTERS,
    enabled,
    exists as _exists,
    filterlist,
)

__all__ = ['ManifestParser', 'TestManifest', 'convert']

relpath = os.path.relpath
string = (basestring,)


# path normalization

def normalize_path(path):
    """normalize a relative path"""
    if sys.platform.startswith('win'):
        return path.replace('/', os.path.sep)
    return path


def denormalize_path(path):
    """denormalize a relative path"""
    if sys.platform.startswith('win'):
        return path.replace(os.path.sep, '/')
    return path


# objects for parsing manifests

class ManifestParser(object):
    """read .ini manifests"""

    def __init__(self, manifests=(), defaults=None, strict=True, rootdir=None,
                 finder=None, handle_defaults=True):
        """Creates a ManifestParser from the given manifest files.

        :param manifests: An iterable of file paths or file objects corresponding
                          to manifests. If a file path refers to a manifest file that
                          does not exist, an IOError is raised.
        :param defaults: Variables to pre-define in the environment for evaluating
                         expressions in manifests.
        :param strict: If False, the provided manifests may contain references to
                       listed (test) files that do not exist without raising an
                       IOError during reading, and certain errors in manifests
                       are not considered fatal. Those errors include duplicate
                       section names, redefining variables, and defining empty
                       variables.
        :param rootdir: The directory used as the basis for conversion to and from
                        relative paths during manifest reading.
        :param finder: If provided, this finder object will be used for filesystem
                       interactions. Finder objects are part of the mozpack package,
                       documented at
                       http://gecko.readthedocs.org/en/latest/python/mozpack.html#module-mozpack.files
        :param handle_defaults: If not set, do not propagate manifest defaults to individual
                                test objects. Callers are expected to manage per-manifest
                                defaults themselves via the manifest_defaults member
                                variable in this case.
        """
        self._defaults = defaults or {}
        self._ancestor_defaults = {}
        self.tests = []
        self.manifest_defaults = {}
        self.source_files = set()
        self.strict = strict
        self.rootdir = rootdir
        self.relativeRoot = None
        self.finder = finder
        self._handle_defaults = handle_defaults
        if manifests:
            self.read(*manifests)

    def path_exists(self, path):
        if self.finder:
            return self.finder.get(path) is not None
        return os.path.exists(path)

    # methods for reading manifests

    def _read(self, root, filename, defaults, defaults_only=False, parentmanifest=None):
        """
        Internal recursive method for reading and parsing manifests.
        Stores all found tests in self.tests
        :param root: The base path
        :param filename: File object or string path for the base manifest file
        :param defaults: Options that apply to all items
        :param defaults_only: If True will only gather options, not include
                              tests. Used for upstream parent includes
                              (default False)
        :param parentmanifest: Filename of the parent manifest (default None)
        """
        def read_file(type):
            include_file = section.split(type, 1)[-1]
            include_file = normalize_path(include_file)
            if not os.path.isabs(include_file):
                include_file = os.path.join(here, include_file)
            if not self.path_exists(include_file):
                message = "Included file '%s' does not exist" % include_file
                if self.strict:
                    raise IOError(message)
                else:
                    sys.stderr.write("%s\n" % message)
                    return
            return include_file

        # get directory of this file if not file-like object
        if isinstance(filename, string):
            # If we're using mercurial as our filesystem via a finder
            # during manifest reading, the getcwd() calls that happen
            # with abspath calls will not be meaningful, so absolute
            # paths are required.
            if self.finder:
                assert os.path.isabs(filename)
            filename = os.path.abspath(filename)
            self.source_files.add(filename)
            if self.finder:
                fp = self.finder.get(filename)
            else:
                fp = open(filename)
            here = os.path.dirname(filename)
        else:
            fp = filename
            filename = here = None
        defaults['here'] = here

        # Rootdir is needed for relative path calculation. Precompute it for
        # the microoptimization used below.
        if self.rootdir is None:
            rootdir = ""
        else:
            assert os.path.isabs(self.rootdir)
            rootdir = self.rootdir + os.path.sep

        # read the configuration
        sections = read_ini(fp=fp, variables=defaults, strict=self.strict,
                            handle_defaults=self._handle_defaults)
        self.manifest_defaults[filename] = defaults

        parent_section_found = False

        # get the tests
        for section, data in sections:
            # In case of defaults only, no other section than parent: has to
            # be processed.
            if defaults_only and not section.startswith('parent:'):
                continue

            # read the parent manifest if specified
            if section.startswith('parent:'):
                parent_section_found = True

                include_file = read_file('parent:')
                if include_file:
                    self._read(root, include_file, {}, True)
                continue

            # a file to include
            # TODO: keep track of included file structure:
            # self.manifests = {'manifest.ini': 'relative/path.ini'}
            if section.startswith('include:'):
                include_file = read_file('include:')
                if include_file:
                    include_defaults = data.copy()
                    self._read(root, include_file, include_defaults, parentmanifest=filename)
                continue

            # otherwise an item
            # apply ancestor defaults, while maintaining current file priority
            data = dict(self._ancestor_defaults.items() + data.items())

            test = data
            test['name'] = section

            # Will be None if the manifest being read is a file-like object.
            test['manifest'] = filename

            # determine the path
            path = test.get('path', section)
            _relpath = path
            if '://' not in path:  # don't futz with URLs
                path = normalize_path(path)
                if here and not os.path.isabs(path):
                    # Profiling indicates 25% of manifest parsing is spent
                    # in this call to normpath, but almost all calls return
                    # their argument unmodified, so we avoid the call if
                    # '..' if not present in the path.
                    path = os.path.join(here, path)
                    if '..' in path:
                        path = os.path.normpath(path)

                # Microoptimization, because relpath is quite expensive.
                # We know that rootdir is an absolute path or empty. If path
                # starts with rootdir, then path is also absolute and the tail
                # of the path is the relative path (possibly non-normalized,
                # when here is unknown).
                # For this to work rootdir needs to be terminated with a path
                # separator, so that references to sibling directories with
                # a common prefix don't get misscomputed (e.g. /root and
                # /rootbeer/file).
                # When the rootdir is unknown, the relpath needs to be left
                # unchanged. We use an empty string as rootdir in that case,
                # which leaves relpath unchanged after slicing.
                if path.startswith(rootdir):
                    _relpath = path[len(rootdir):]
                else:
                    _relpath = relpath(path, rootdir)

            test['path'] = path
            test['relpath'] = _relpath

            if parentmanifest is not None:
                # If a test was included by a parent manifest we may need to
                # indicate that in the test object for the sake of identifying
                # a test, particularly in the case a test file is included by
                # multiple manifests.
                test['ancestor-manifest'] = parentmanifest

            # append the item
            self.tests.append(test)

        # if no parent: section was found for defaults-only, only read the
        # defaults section of the manifest without interpreting variables
        if defaults_only and not parent_section_found:
            sections = read_ini(fp=fp, variables=defaults, defaults_only=True,
                                strict=self.strict)
            (section, self._ancestor_defaults) = sections[0]

    def read(self, *filenames, **defaults):
        """
        read and add manifests from file paths or file-like objects

        filenames -- file paths or file-like objects to read as manifests
        defaults -- default variables
        """

        # ensure all files exist
        missing = [filename for filename in filenames
                   if isinstance(filename, string) and not self.path_exists(filename)]
        if missing:
            raise IOError('Missing files: %s' % ', '.join(missing))

        # default variables
        _defaults = defaults.copy() or self._defaults.copy()
        _defaults.setdefault('here', None)

        # process each file
        for filename in filenames:
            # set the per file defaults
            defaults = _defaults.copy()
            here = None
            if isinstance(filename, string):
                here = os.path.dirname(os.path.abspath(filename))
                defaults['here'] = here  # directory of master .ini file

            if self.rootdir is None:
                # set the root directory
                # == the directory of the first manifest given
                self.rootdir = here

            self._read(here, filename, defaults)

    # methods for querying manifests

    def query(self, *checks, **kw):
        """
        general query function for tests
        - checks : callable conditions to test if the test fulfills the query
        """
        tests = kw.get('tests', None)
        if tests is None:
            tests = self.tests
        retval = []
        for test in tests:
            for check in checks:
                if not check(test):
                    break
            else:
                retval.append(test)
        return retval

    def get(self, _key=None, inverse=False, tags=None, tests=None, **kwargs):
        # TODO: pass a dict instead of kwargs since you might hav
        # e.g. 'inverse' as a key in the dict

        # TODO: tags should just be part of kwargs with None values
        # (None == any is kinda weird, but probably still better)

        # fix up tags
        if tags:
            tags = set(tags)
        else:
            tags = set()

        # make some check functions
        if inverse:
            def has_tags(test):
                return not tags.intersection(test.keys())

            def dict_query(test):
                for key, value in kwargs.items():
                    if test.get(key) == value:
                        return False
                return True
        else:
            def has_tags(test):
                return tags.issubset(test.keys())

            def dict_query(test):
                for key, value in kwargs.items():
                    if test.get(key) != value:
                        return False
                return True

        # query the tests
        tests = self.query(has_tags, dict_query, tests=tests)

        # if a key is given, return only a list of that key
        # useful for keys like 'name' or 'path'
        if _key:
            return [test[_key] for test in tests]

        # return the tests
        return tests

    def manifests(self, tests=None):
        """
        return manifests in order in which they appear in the tests
        """
        if tests is None:
            # Make sure to return all the manifests, even ones without tests.
            return self.manifest_defaults.keys()

        manifests = []
        for test in tests:
            manifest = test.get('manifest')
            if not manifest:
                continue
            if manifest not in manifests:
                manifests.append(manifest)
        return manifests

    def paths(self):
        return [i['path'] for i in self.tests]

    # methods for auditing

    def missing(self, tests=None):
        """
        return list of tests that do not exist on the filesystem
        """
        if tests is None:
            tests = self.tests
        existing = list(_exists(tests, {}))
        return [t for t in tests if t not in existing]

    def check_missing(self, tests=None):
        missing = self.missing(tests=tests)
        if missing:
            missing_paths = [test['path'] for test in missing]
            if self.strict:
                raise IOError("Strict mode enabled, test paths must exist. "
                              "The following test(s) are missing: %s" %
                              json.dumps(missing_paths, indent=2))
            print >> sys.stderr, "Warning: The following test(s) are missing: %s" % \
                json.dumps(missing_paths, indent=2)
        return missing

    def verifyDirectory(self, directories, pattern=None, extensions=None):
        """
        checks what is on the filesystem vs what is in a manifest
        returns a 2-tuple of sets:
        (missing_from_filesystem, missing_from_manifest)
        """

        files = set([])
        if isinstance(directories, basestring):
            directories = [directories]

        # get files in directories
        for directory in directories:
            for dirpath, dirnames, filenames in os.walk(directory, topdown=True):

                # only add files that match a pattern
                if pattern:
                    filenames = fnmatch.filter(filenames, pattern)

                # only add files that have one of the extensions
                if extensions:
                    filenames = [filename for filename in filenames
                                 if os.path.splitext(filename)[-1] in extensions]

                files.update([os.path.join(dirpath, filename) for filename in filenames])

        paths = set(self.paths())
        missing_from_filesystem = paths.difference(files)
        missing_from_manifest = files.difference(paths)
        return (missing_from_filesystem, missing_from_manifest)

    # methods for output

    def write(self, fp=sys.stdout, rootdir=None,
              global_tags=None, global_kwargs=None,
              local_tags=None, local_kwargs=None):
        """
        write a manifest given a query
        global and local options will be munged to do the query
        globals will be written to the top of the file
        locals (if given) will be written per test
        """

        # open file if `fp` given as string
        close = False
        if isinstance(fp, string):
            fp = file(fp, 'w')
            close = True

        # root directory
        if rootdir is None:
            rootdir = self.rootdir

        # sanitize input
        global_tags = global_tags or set()
        local_tags = local_tags or set()
        global_kwargs = global_kwargs or {}
        local_kwargs = local_kwargs or {}

        # create the query
        tags = set([])
        tags.update(global_tags)
        tags.update(local_tags)
        kwargs = {}
        kwargs.update(global_kwargs)
        kwargs.update(local_kwargs)

        # get matching tests
        tests = self.get(tags=tags, **kwargs)

        # print the .ini manifest
        if global_tags or global_kwargs:
            print >> fp, '[DEFAULT]'
            for tag in global_tags:
                print >> fp, '%s =' % tag
            for key, value in global_kwargs.items():
                print >> fp, '%s = %s' % (key, value)
            print >> fp

        for test in tests:
            test = test.copy()  # don't overwrite

            path = test['name']
            if not os.path.isabs(path):
                path = test['path']
                if self.rootdir:
                    path = relpath(test['path'], self.rootdir)
                path = denormalize_path(path)
            print >> fp, '[%s]' % path

            # reserved keywords:
            reserved = ['path', 'name', 'here', 'manifest', 'relpath', 'ancestor-manifest']
            for key in sorted(test.keys()):
                if key in reserved:
                    continue
                if key in global_kwargs:
                    continue
                if key in global_tags and not test[key]:
                    continue
                print >> fp, '%s = %s' % (key, test[key])
            print >> fp

        if close:
            # close the created file
            fp.close()

    def __str__(self):
        fp = StringIO()
        self.write(fp=fp)
        value = fp.getvalue()
        return value

    def copy(self, directory, rootdir=None, *tags, **kwargs):
        """
        copy the manifests and associated tests
        - directory : directory to copy to
        - rootdir : root directory to copy to (if not given from manifests)
        - tags : keywords the tests must have
        - kwargs : key, values the tests must match
        """
        # XXX note that copy does *not* filter the tests out of the
        # resulting manifest; it just stupidly copies them over.
        # ideally, it would reread the manifests and filter out the
        # tests that don't match *tags and **kwargs

        # destination
        if not os.path.exists(directory):
            os.path.makedirs(directory)
        else:
            # sanity check
            assert os.path.isdir(directory)

        # tests to copy
        tests = self.get(tags=tags, **kwargs)
        if not tests:
            return  # nothing to do!

        # root directory
        if rootdir is None:
            rootdir = self.rootdir

        # copy the manifests + tests
        manifests = [relpath(manifest, rootdir) for manifest in self.manifests()]
        for manifest in manifests:
            destination = os.path.join(directory, manifest)
            dirname = os.path.dirname(destination)
            if not os.path.exists(dirname):
                os.makedirs(dirname)
            else:
                # sanity check
                assert os.path.isdir(dirname)
            shutil.copy(os.path.join(rootdir, manifest), destination)

        missing = self.check_missing(tests)
        tests = [test for test in tests if test not in missing]
        for test in tests:
            if os.path.isabs(test['name']):
                continue
            source = test['path']
            destination = os.path.join(directory, relpath(test['path'], rootdir))
            shutil.copy(source, destination)
            # TODO: ensure that all of the tests are below the from_dir

    def update(self, from_dir, rootdir=None, *tags, **kwargs):
        """
        update the tests as listed in a manifest from a directory
        - from_dir : directory where the tests live
        - rootdir : root directory to copy to (if not given from manifests)
        - tags : keys the tests must have
        - kwargs : key, values the tests must match
        """

        # get the tests
        tests = self.get(tags=tags, **kwargs)

        # get the root directory
        if not rootdir:
            rootdir = self.rootdir

        # copy them!
        for test in tests:
            if not os.path.isabs(test['name']):
                _relpath = relpath(test['path'], rootdir)
                source = os.path.join(from_dir, _relpath)
                if not os.path.exists(source):
                    message = "Missing test: '%s' does not exist!"
                    if self.strict:
                        raise IOError(message)
                    print >> sys.stderr, message + " Skipping."
                    continue
                destination = os.path.join(rootdir, _relpath)
                shutil.copy(source, destination)

    # directory importers

    @classmethod
    def _walk_directories(cls, directories, callback, pattern=None, ignore=()):
        """
        internal function to import directories
        """

        if isinstance(pattern, basestring):
            patterns = [pattern]
        else:
            patterns = pattern
        ignore = set(ignore)

        if not patterns:
            def accept_filename(filename):
                return True
        else:
            def accept_filename(filename):
                for pattern in patterns:
                    if fnmatch.fnmatch(filename, pattern):
                        return True

        if not ignore:
            def accept_dirname(dirname):
                return True
        else:
            def accept_dirname(dirname):
                return dirname not in ignore

        rootdirectories = directories[:]
        seen_directories = set()
        for rootdirectory in rootdirectories:
            # let's recurse directories using list
            directories = [os.path.realpath(rootdirectory)]
            while directories:
                directory = directories.pop(0)
                if directory in seen_directories:
                    # eliminate possible infinite recursion due to
                    # symbolic links
                    continue
                seen_directories.add(directory)

                files = []
                subdirs = []
                for name in sorted(os.listdir(directory)):
                    path = os.path.join(directory, name)
                    if os.path.isfile(path):
                        # os.path.isfile follow symbolic links, we don't
                        # need to handle them here.
                        if accept_filename(name):
                            files.append(name)
                        continue
                    elif os.path.islink(path):
                        # eliminate symbolic links
                        path = os.path.realpath(path)

                    # we must have a directory here
                    if accept_dirname(name):
                        subdirs.append(name)
                        # this subdir is added for recursion
                        directories.insert(0, path)

                # here we got all subdirs and files filtered, we can
                # call the callback function if directory is not empty
                if subdirs or files:
                    callback(rootdirectory, directory, subdirs, files)

    @classmethod
    def populate_directory_manifests(cls, directories, filename, pattern=None, ignore=(),
                                     overwrite=False):
        """
        walks directories and writes manifests of name `filename` in-place;
        returns `cls` instance populated with the given manifests

        filename -- filename of manifests to write
        pattern -- shell pattern (glob) or patterns of filenames to match
        ignore -- directory names to ignore
        overwrite -- whether to overwrite existing files of given name
        """

        manifest_dict = {}

        if os.path.basename(filename) != filename:
            raise IOError("filename should not include directory name")

        # no need to hit directories more than once
        _directories = directories
        directories = []
        for directory in _directories:
            if directory not in directories:
                directories.append(directory)

        def callback(directory, dirpath, dirnames, filenames):
            """write a manifest for each directory"""

            manifest_path = os.path.join(dirpath, filename)
            if (dirnames or filenames) and not (os.path.exists(manifest_path) and overwrite):
                with file(manifest_path, 'w') as manifest:
                    for dirname in dirnames:
                        print >> manifest, '[include:%s]' % os.path.join(dirname, filename)
                    for _filename in filenames:
                        print >> manifest, '[%s]' % _filename

                # add to list of manifests
                manifest_dict.setdefault(directory, manifest_path)

        # walk the directories to gather files
        cls._walk_directories(directories, callback, pattern=pattern, ignore=ignore)
        # get manifests
        manifests = [manifest_dict[directory] for directory in _directories]

        # create a `cls` instance with the manifests
        return cls(manifests=manifests)

    @classmethod
    def from_directories(cls, directories, pattern=None, ignore=(), write=None, relative_to=None):
        """
        convert directories to a simple manifest; returns ManifestParser instance

        pattern -- shell pattern (glob) or patterns of filenames to match
        ignore -- directory names to ignore
        write -- filename or file-like object of manifests to write;
                 if `None` then a StringIO instance will be created
        relative_to -- write paths relative to this path;
                       if false then the paths are absolute
        """

        # determine output
        opened_manifest_file = None  # name of opened manifest file
        absolute = not relative_to  # whether to output absolute path names as names
        if isinstance(write, string):
            opened_manifest_file = write
            write = file(write, 'w')
        if write is None:
            write = StringIO()

        # walk the directories, generating manifests
        def callback(directory, dirpath, dirnames, filenames):

            # absolute paths
            filenames = [os.path.join(dirpath, filename)
                         for filename in filenames]
            # ensure new manifest isn't added
            filenames = [filename for filename in filenames
                         if filename != opened_manifest_file]
            # normalize paths
            if not absolute and relative_to:
                filenames = [relpath(filename, relative_to)
                             for filename in filenames]

            # write to manifest
            print >> write, '\n'.join(['[%s]' % denormalize_path(filename)
                                       for filename in filenames])

        cls._walk_directories(directories, callback, pattern=pattern, ignore=ignore)

        if opened_manifest_file:
            # close file
            write.close()
            manifests = [opened_manifest_file]
        else:
            # manifests/write is a file-like object;
            # rewind buffer
            write.flush()
            write.seek(0)
            manifests = [write]

        # make a ManifestParser instance
        return cls(manifests=manifests)

convert = ManifestParser.from_directories


class TestManifest(ManifestParser):
    """
    apply logic to manifests;  this is your integration layer :)
    specific harnesses may subclass from this if they need more logic
    """

    def __init__(self, *args, **kwargs):
        ManifestParser.__init__(self, *args, **kwargs)
        self.filters = filterlist(DEFAULT_FILTERS)
        self.last_used_filters = []

    def active_tests(self, exists=True, disabled=True, filters=None, **values):
        """
        Run all applied filters on the set of tests.

        :param exists: filter out non-existing tests (default True)
        :param disabled: whether to return disabled tests (default True)
        :param values: keys and values to filter on (e.g. `os = linux mac`)
        :param filters: list of filters to apply to the tests
        :returns: list of test objects that were not filtered out
        """
        tests = [i.copy() for i in self.tests]  # shallow copy

        # mark all tests as passing
        for test in tests:
            test['expected'] = test.get('expected', 'pass')

        # make a copy so original doesn't get modified
        fltrs = self.filters[:]
        if exists:
            if self.strict:
                self.check_missing(tests)
            else:
                fltrs.append(_exists)

        if not disabled:
            fltrs.append(enabled)

        if filters:
            fltrs += filters

        self.last_used_filters = fltrs[:]
        for fn in fltrs:
            tests = fn(tests, values)
        return list(tests)

    def test_paths(self):
        return [test['path'] for test in self.active_tests()]

    def fmt_filters(self, filters=None):
        filters = filters or self.last_used_filters
        names = []
        for f in filters:
            if isinstance(f, types.FunctionType):
                names.append(f.__name__)
            else:
                names.append(str(f))
        return ', '.join(names)
