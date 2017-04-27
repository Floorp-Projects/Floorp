# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Fetch build artifacts from a Firefox tree.

This provides an (at-the-moment special purpose) interface to download Android
artifacts from Mozilla's Task Cluster.

This module performs the following steps:

* find a candidate hg parent revision.  At one time we used the local pushlog,
  which required the mozext hg extension.  This isn't feasible with git, and it
  is only mildly less efficient to not use the pushlog, so we don't use it even
  when querying hg.

* map the candidate parent to candidate Task Cluster tasks and artifact
  locations.  Pushlog entries might not correspond to tasks (yet), and those
  tasks might not produce the desired class of artifacts.

* fetch fresh Task Cluster artifacts and purge old artifacts, using a simple
  Least Recently Used cache.

* post-process fresh artifacts, to speed future installation.  In particular,
  extract relevant files from Mac OS X DMG files into a friendly archive format
  so we don't have to mount DMG files frequently.

The bulk of the complexity is in managing and persisting several caches.  If
we found a Python LRU cache that pickled cleanly, we could remove a lot of
this code!  Sadly, I found no such candidate implementations, so we pickle
pylru caches manually.

None of the instances (or the underlying caches) are safe for concurrent use.
A future need, perhaps.

This module requires certain modules be importable from the ambient Python
environment.  |mach artifact| ensures these modules are available, but other
consumers will need to arrange this themselves.
'''


from __future__ import absolute_import, print_function, unicode_literals

import binascii
import collections
import functools
import glob
import hashlib
import logging
import operator
import os
import pickle
import re
import requests
import shutil
import stat
import subprocess
import tarfile
import tempfile
import urlparse
import zipfile

import pylru
from taskgraph.util.taskcluster import (
    find_task_id,
    get_artifact_url,
    list_artifacts,
)

from mozbuild.util import (
    ensureParentDir,
    FileAvoidWrite,
    mkdir,
)
import mozinstall
from mozpack.files import (
    JarFinder,
    TarFinder,
)
from mozpack.mozjar import (
    JarReader,
    JarWriter,
)
from mozpack.packager.unpack import UnpackFinder
import mozpack.path as mozpath
from dlmanager import (
    DownloadManager,
    PersistLimit,
)

NUM_PUSHHEADS_TO_QUERY_PER_PARENT = 50  # Number of candidate pushheads to cache per parent changeset.

# Number of parent changesets to consider as possible pushheads.
# There isn't really such a thing as a reasonable default here, because we don't
# know how many pushheads we'll need to look at to find a build with our artifacts,
# and we don't know how many changesets will be in each push. For now we assume
# we'll find a build in the last 50 pushes, assuming each push contains 10 changesets.
NUM_REVISIONS_TO_QUERY = 500

MAX_CACHED_TASKS = 400  # Number of pushheads to cache Task Cluster task data for.

# Minimum number of downloaded artifacts to keep. Each artifact can be very large,
# so don't make this to large!
MIN_CACHED_ARTIFACTS = 6

# Maximum size of the downloaded artifacts to keep in cache, in bytes (1GiB).
MAX_CACHED_ARTIFACTS_SIZE = 1024 * 1024 * 1024

# Downloaded artifacts are cached, and a subset of their contents extracted for
# easy installation.  This is most noticeable on Mac OS X: since mounting and
# copying from DMG files is very slow, we extract the desired binaries to a
# separate archive for fast re-installation.
PROCESSED_SUFFIX = '.processed.jar'

CANDIDATE_TREES = (
    'mozilla-central',
    'integration/mozilla-inbound',
    'releases/mozilla-beta'
)

class ArtifactJob(object):
    # These are a subset of TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
    # Each item is a pair of (pattern, (src_prefix, dest_prefix), where src_prefix
    # is the prefix of the pattern relevant to its location in the archive, and
    # dest_prefix is the prefix to be added that will yield the final path relative
    # to dist/.
    test_artifact_patterns = {
        ('bin/BadCertServer', ('bin', 'bin')),
        ('bin/GenerateOCSPResponse', ('bin', 'bin')),
        ('bin/OCSPStaplingServer', ('bin', 'bin')),
        ('bin/certutil', ('bin', 'bin')),
        ('bin/fileid', ('bin', 'bin')),
        ('bin/pk12util', ('bin', 'bin')),
        ('bin/screentopng', ('bin', 'bin')),
        ('bin/ssltunnel', ('bin', 'bin')),
        ('bin/xpcshell', ('bin', 'bin')),
        ('bin/plugins/gmp-*/*/*', ('bin/plugins', 'bin')),
        ('bin/plugins/*', ('bin/plugins', 'plugins')),
        ('bin/components/*', ('bin/components', 'bin/components')),
    }

    # We can tell our input is a test archive by this suffix, which happens to
    # be the same across platforms.
    _test_archive_suffix = '.common.tests.zip'

    def __init__(self, package_re, tests_re, log=None, download_symbols=False, substs=None):
        self._package_re = re.compile(package_re)
        self._tests_re = None
        if tests_re:
            self._tests_re = re.compile(tests_re)
        self._log = log
        self._substs = substs
        self._symbols_archive_suffix = None
        if download_symbols:
            self._symbols_archive_suffix = 'crashreporter-symbols.zip'

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def find_candidate_artifacts(self, artifacts):
        # TODO: Handle multiple artifacts, taking the latest one.
        tests_artifact = None
        for artifact in artifacts:
            name = artifact['name']
            if self._package_re and self._package_re.match(name):
                yield name
            elif self._tests_re and self._tests_re.match(name):
                tests_artifact = name
                yield name
            elif self._symbols_archive_suffix and name.endswith(self._symbols_archive_suffix):
                yield name
            else:
                self.log(logging.DEBUG, 'artifact',
                         {'name': name},
                         'Not yielding artifact named {name} as a candidate artifact')
        if self._tests_re and not tests_artifact:
            raise ValueError('Expected tests archive matching "{re}", but '
                             'found none!'.format(re=self._tests_re))

    def process_artifact(self, filename, processed_filename):
        if filename.endswith(ArtifactJob._test_archive_suffix) and self._tests_re:
            return self.process_tests_artifact(filename, processed_filename)
        if self._symbols_archive_suffix and filename.endswith(self._symbols_archive_suffix):
            return self.process_symbols_archive(filename, processed_filename)
        return self.process_package_artifact(filename, processed_filename)

    def process_package_artifact(self, filename, processed_filename):
        raise NotImplementedError("Subclasses must specialize process_package_artifact!")

    def process_tests_artifact(self, filename, processed_filename):
        from mozbuild.action.test_archive import OBJDIR_TEST_FILES
        added_entry = False

        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            reader = JarReader(filename)
            for filename, entry in reader.entries.iteritems():
                for pattern, (src_prefix, dest_prefix) in self.test_artifact_patterns:
                    if not mozpath.match(filename, pattern):
                        continue
                    destpath = mozpath.relpath(filename, src_prefix)
                    destpath = mozpath.join(dest_prefix, destpath)
                    self.log(logging.INFO, 'artifact',
                             {'destpath': destpath},
                             'Adding {destpath} to processed archive')
                    mode = entry['external_attr'] >> 16
                    writer.add(destpath.encode('utf-8'), reader[filename], mode=mode)
                    added_entry = True
                    break
                for files_entry in OBJDIR_TEST_FILES.values():
                    origin_pattern = files_entry['pattern']
                    leaf_filename = filename
                    if 'dest' in files_entry:
                        dest = files_entry['dest']
                        origin_pattern = mozpath.join(dest, origin_pattern)
                        leaf_filename = filename[len(dest) + 1:]
                    if mozpath.match(filename, origin_pattern):
                        destpath = mozpath.join('..', files_entry['base'], leaf_filename)
                        mode = entry['external_attr'] >> 16
                        writer.add(destpath.encode('utf-8'), reader[filename], mode=mode)

        if not added_entry:
            raise ValueError('Archive format changed! No pattern from "{patterns}"'
                             'matched an archive path.'.format(
                                 patterns=LinuxArtifactJob.test_artifact_patterns))

    def process_symbols_archive(self, filename, processed_filename):
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            reader = JarReader(filename)
            for filename in reader.entries:
                destpath = mozpath.join('crashreporter-symbols', filename)
                self.log(logging.INFO, 'artifact',
                         {'destpath': destpath},
                         'Adding {destpath} to processed archive')
                writer.add(destpath.encode('utf-8'), reader[filename])

class AndroidArtifactJob(ArtifactJob):

    product = 'mobile'

    package_artifact_patterns = {
        'application.ini',
        'platform.ini',
        '**/*.so',
        '**/interfaces.xpt',
    }

    def process_package_artifact(self, filename, processed_filename):
        # Extract all .so files into the root, which will get copied into dist/bin.
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            for p, f in UnpackFinder(JarFinder(filename, JarReader(filename))):
                if not any(mozpath.match(p, pat) for pat in self.package_artifact_patterns):
                    continue

                dirname, basename = os.path.split(p)
                self.log(logging.INFO, 'artifact',
                    {'basename': basename},
                   'Adding {basename} to processed archive')

                basedir = 'bin'
                if not basename.endswith('.so'):
                    basedir = mozpath.join('bin', dirname.lstrip('assets/'))
                basename = mozpath.join(basedir, basename)
                writer.add(basename.encode('utf-8'), f.open())


class LinuxArtifactJob(ArtifactJob):

    product = 'firefox'

    package_artifact_patterns = {
        'firefox/application.ini',
        'firefox/crashreporter',
        'firefox/dependentlibs.list',
        'firefox/firefox',
        'firefox/firefox-bin',
        'firefox/minidump-analyzer',
        'firefox/pingsender',
        'firefox/platform.ini',
        'firefox/plugin-container',
        'firefox/updater',
        'firefox/**/*.so',
        'firefox/**/interfaces.xpt',
    }

    def process_package_artifact(self, filename, processed_filename):
        added_entry = False

        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            with tarfile.open(filename) as reader:
                for p, f in UnpackFinder(TarFinder(filename, reader)):
                    if not any(mozpath.match(p, pat) for pat in self.package_artifact_patterns):
                        continue

                    # We strip off the relative "firefox/" bit from the path,
                    # but otherwise preserve it.
                    destpath = mozpath.join('bin',
                                            mozpath.relpath(p, "firefox"))
                    self.log(logging.INFO, 'artifact',
                             {'destpath': destpath},
                             'Adding {destpath} to processed archive')
                    writer.add(destpath.encode('utf-8'), f.open(), mode=f.mode)
                    added_entry = True

        if not added_entry:
            raise ValueError('Archive format changed! No pattern from "{patterns}" '
                             'matched an archive path.'.format(
                                 patterns=LinuxArtifactJob.package_artifact_patterns))


class MacArtifactJob(ArtifactJob):

    product = 'firefox'

    def process_package_artifact(self, filename, processed_filename):
        tempdir = tempfile.mkdtemp()
        oldcwd = os.getcwd()
        try:
            self.log(logging.INFO, 'artifact',
                {'tempdir': tempdir},
                'Unpacking DMG into {tempdir}')
            if self._substs['HOST_OS_ARCH'] == 'Linux':
                # This is a cross build, use hfsplus and dmg tools to extract the dmg.
                os.chdir(tempdir)
                with open(os.devnull, 'wb') as devnull:
                    subprocess.check_call([
                        self._substs['DMG_TOOL'],
                        'extract',
                        filename,
                        'extracted_img',
                    ], stdout=devnull)
                    subprocess.check_call([
                        self._substs['HFS_TOOL'],
                        'extracted_img',
                        'extractall'
                    ], stdout=devnull)
            else:
                mozinstall.install(filename, tempdir)

            bundle_dirs = glob.glob(mozpath.join(tempdir, '*.app'))
            if len(bundle_dirs) != 1:
                raise ValueError('Expected one source bundle, found: {}'.format(bundle_dirs))
            [source] = bundle_dirs

            # These get copied into dist/bin without the path, so "root/a/b/c" -> "dist/bin/c".
            paths_no_keep_path = ('Contents/MacOS', [
                'crashreporter.app/Contents/MacOS/crashreporter',
                'crashreporter.app/Contents/MacOS/minidump-analyzer',
                'firefox',
                'firefox-bin',
                'libfreebl3.dylib',
                'liblgpllibs.dylib',
                # 'liblogalloc.dylib',
                'libmozglue.dylib',
                'libnss3.dylib',
                'libnssckbi.dylib',
                'libnssdbm3.dylib',
                'libplugin_child_interpose.dylib',
                # 'libreplace_jemalloc.dylib',
                # 'libreplace_malloc.dylib',
                'libmozavutil.dylib',
                'libmozavcodec.dylib',
                'libsoftokn3.dylib',
                'pingsender',
                'plugin-container.app/Contents/MacOS/plugin-container',
                'updater.app/Contents/MacOS/org.mozilla.updater',
                # 'xpcshell',
                'XUL',
            ])

            # These get copied into dist/bin with the path, so "root/a/b/c" -> "dist/bin/a/b/c".
            paths_keep_path = ('Contents/Resources', [
                'browser/components/libbrowsercomps.dylib',
                'dependentlibs.list',
                # 'firefox',
                'gmp-clearkey/0.1/libclearkey.dylib',
                # 'gmp-fake/1.0/libfake.dylib',
                # 'gmp-fakeopenh264/1.0/libfakeopenh264.dylib',
                '**/interfaces.xpt',
            ])

            with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
                root, paths = paths_no_keep_path
                finder = UnpackFinder(mozpath.join(source, root))
                for path in paths:
                    for p, f in finder.find(path):
                        self.log(logging.INFO, 'artifact',
                            {'path': p},
                            'Adding {path} to processed archive')
                        destpath = mozpath.join('bin', os.path.basename(p))
                        writer.add(destpath.encode('utf-8'), f, mode=f.mode)

                root, paths = paths_keep_path
                finder = UnpackFinder(mozpath.join(source, root))
                for path in paths:
                    for p, f in finder.find(path):
                        self.log(logging.INFO, 'artifact',
                            {'path': p},
                            'Adding {path} to processed archive')
                        destpath = mozpath.join('bin', p)
                        writer.add(destpath.encode('utf-8'), f.open(), mode=f.mode)

        finally:
            os.chdir(oldcwd)
            try:
                shutil.rmtree(tempdir)
            except (OSError, IOError):
                self.log(logging.WARN, 'artifact',
                    {'tempdir': tempdir},
                    'Unable to delete {tempdir}')
                pass


class WinArtifactJob(ArtifactJob):
    package_artifact_patterns = {
        'firefox/dependentlibs.list',
        'firefox/platform.ini',
        'firefox/application.ini',
        'firefox/**/*.dll',
        'firefox/*.exe',
        'firefox/**/interfaces.xpt',
    }

    product = 'firefox'

    # These are a subset of TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
    test_artifact_patterns = {
        ('bin/BadCertServer.exe', ('bin', 'bin')),
        ('bin/GenerateOCSPResponse.exe', ('bin', 'bin')),
        ('bin/OCSPStaplingServer.exe', ('bin', 'bin')),
        ('bin/certutil.exe', ('bin', 'bin')),
        ('bin/fileid.exe', ('bin', 'bin')),
        ('bin/pk12util.exe', ('bin', 'bin')),
        ('bin/screenshot.exe', ('bin', 'bin')),
        ('bin/ssltunnel.exe', ('bin', 'bin')),
        ('bin/xpcshell.exe', ('bin', 'bin')),
        ('bin/plugins/gmp-*/*/*', ('bin/plugins', 'bin')),
        ('bin/plugins/*', ('bin/plugins', 'plugins')),
        ('bin/components/*', ('bin/components', 'bin/components')),
    }

    def process_package_artifact(self, filename, processed_filename):
        added_entry = False
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            for p, f in UnpackFinder(JarFinder(filename, JarReader(filename))):
                if not any(mozpath.match(p, pat) for pat in self.package_artifact_patterns):
                    continue

                # strip off the relative "firefox/" bit from the path:
                basename = mozpath.relpath(p, "firefox")
                basename = mozpath.join('bin', basename)
                self.log(logging.INFO, 'artifact',
                    {'basename': basename},
                    'Adding {basename} to processed archive')
                writer.add(basename.encode('utf-8'), f.open(), mode=f.mode)
                added_entry = True

        if not added_entry:
            raise ValueError('Archive format changed! No pattern from "{patterns}"'
                             'matched an archive path.'.format(
                                 patterns=self.artifact_patterns))

# Keep the keys of this map in sync with the |mach artifact| --job
# options.  The keys of this map correspond to entries at
# https://tools.taskcluster.net/index/artifacts/#gecko.v2.mozilla-central.latest/gecko.v2.mozilla-central.latest
# The values correpsond to a pair of (<package regex>, <test archive regex>).
JOB_DETAILS = {
    'android-api-15-opt': (AndroidArtifactJob, (r'(public/build/fennec-(.*)\.android-arm.apk|public/build/target\.apk)',
                                                r'public/build/fennec-(.*)\.common\.tests\.zip|public/build/target\.common\.tests\.zip')),
    'android-api-15-debug': (AndroidArtifactJob, (r'public/build/target\.apk',
                                                  r'public/build/target\.common\.tests\.zip')),
    'android-x86-opt': (AndroidArtifactJob, (r'public/build/target\.apk',
                                             r'public/build/target\.common\.tests\.zip')),
    'linux-opt': (LinuxArtifactJob, (r'public/build/target\.tar\.bz2',
                                     r'public/build/target\.common\.tests\.zip')),
    'linux-debug': (LinuxArtifactJob, (r'public/build/target\.tar\.bz2',
                                       r'public/build/target\.common\.tests\.zip')),
    'linux64-opt': (LinuxArtifactJob, (r'public/build/target\.tar\.bz2',
                                       r'public/build/target\.common\.tests\.zip')),
    'linux64-debug': (LinuxArtifactJob, (r'public/build/target\.tar\.bz2',
                                         r'public/build/target\.common\.tests\.zip')),
    'macosx64-opt': (MacArtifactJob, (r'public/build/firefox-(.*)\.mac\.dmg',
                                      r'public/build/firefox-(.*)\.common\.tests\.zip')),
    'macosx64-debug': (MacArtifactJob, (r'public/build/firefox-(.*)\.mac\.dmg',
                                        r'public/build/firefox-(.*)\.common\.tests\.zip')),
    'win32-opt': (WinArtifactJob, (r'public/build/firefox-(.*)\.win32.zip',
                                   r'public/build/firefox-(.*)\.common\.tests\.zip')),
    'win32-debug': (WinArtifactJob, (r'public/build/firefox-(.*)\.win32.zip',
                                     r'public/build/firefox-(.*)\.common\.tests\.zip')),
    'win64-opt': (WinArtifactJob, (r'public/build/firefox-(.*)\.win64.zip',
                                   r'public/build/firefox-(.*)\.common\.tests\.zip')),
    'win64-debug': (WinArtifactJob, (r'public/build/firefox-(.*)\.win64.zip',
                                     r'public/build/firefox-(.*)\.common\.tests\.zip')),
}



def get_job_details(job, log=None, download_symbols=False, substs=None):
    cls, (package_re, tests_re) = JOB_DETAILS[job]
    return cls(package_re, tests_re, log=log, download_symbols=download_symbols,
               substs=substs)

def cachedmethod(cachefunc):
    '''Decorator to wrap a class or instance method with a memoizing callable that
    saves results in a (possibly shared) cache.
    '''
    def decorator(method):
        def wrapper(self, *args, **kwargs):
            mapping = cachefunc(self)
            if mapping is None:
                return method(self, *args, **kwargs)
            key = (method.__name__, args, tuple(sorted(kwargs.items())))
            try:
                value = mapping[key]
                return value
            except KeyError:
                pass
            result = method(self, *args, **kwargs)
            mapping[key] = result
            return result
        return functools.update_wrapper(wrapper, method)
    return decorator


class CacheManager(object):
    '''Maintain an LRU cache.  Provide simple persistence, including support for
    loading and saving the state using a "with" block.  Allow clearing the cache
    and printing the cache for debugging.

    Provide simple logging.
    '''

    def __init__(self, cache_dir, cache_name, cache_size, cache_callback=None, log=None, skip_cache=False):
        self._skip_cache = skip_cache
        self._cache = pylru.lrucache(cache_size, callback=cache_callback)
        self._cache_filename = mozpath.join(cache_dir, cache_name + '-cache.pickle')
        self._log = log
        mkdir(cache_dir, not_indexed=True)

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def load_cache(self):
        if self._skip_cache:
            self.log(logging.DEBUG, 'artifact',
                {},
                'Skipping cache: ignoring load_cache!')
            return

        try:
            items = pickle.load(open(self._cache_filename, 'rb'))
            for key, value in items:
                self._cache[key] = value
        except Exception as e:
            # Corrupt cache, perhaps?  Sadly, pickle raises many different
            # exceptions, so it's not worth trying to be fine grained here.
            # We ignore any exception, so the cache is effectively dropped.
            self.log(logging.INFO, 'artifact',
                {'filename': self._cache_filename, 'exception': repr(e)},
                'Ignoring exception unpickling cache file {filename}: {exception}')
            pass

    def dump_cache(self):
        if self._skip_cache:
            self.log(logging.DEBUG, 'artifact',
                {},
                'Skipping cache: ignoring dump_cache!')
            return

        ensureParentDir(self._cache_filename)
        pickle.dump(list(reversed(list(self._cache.items()))), open(self._cache_filename, 'wb'), -1)

    def clear_cache(self):
        if self._skip_cache:
            self.log(logging.DEBUG, 'artifact',
                {},
                'Skipping cache: ignoring clear_cache!')
            return

        with self:
            self._cache.clear()

    def __enter__(self):
        self.load_cache()
        return self

    def __exit__(self, type, value, traceback):
        self.dump_cache()

class PushheadCache(CacheManager):
    '''Helps map tree/revision pairs to parent pushheads according to the pushlog.'''

    def __init__(self, cache_dir, log=None, skip_cache=False):
        CacheManager.__init__(self, cache_dir, 'pushhead_cache', MAX_CACHED_TASKS, log=log, skip_cache=skip_cache)

    @cachedmethod(operator.attrgetter('_cache'))
    def parent_pushhead_id(self, tree, revision):
        cset_url_tmpl = ('https://hg.mozilla.org/{tree}/json-pushes?'
                         'changeset={changeset}&version=2&tipsonly=1')
        req = requests.get(cset_url_tmpl.format(tree=tree, changeset=revision),
                           headers={'Accept': 'application/json'})
        if req.status_code not in range(200, 300):
            raise ValueError
        result = req.json()
        [found_pushid] = result['pushes'].keys()
        return int(found_pushid)

    @cachedmethod(operator.attrgetter('_cache'))
    def pushid_range(self, tree, start, end):
        pushid_url_tmpl = ('https://hg.mozilla.org/{tree}/json-pushes?'
                           'startID={start}&endID={end}&version=2&tipsonly=1')

        req = requests.get(pushid_url_tmpl.format(tree=tree, start=start,
                                                  end=end),
                           headers={'Accept': 'application/json'})
        result = req.json()
        return [
            p['changesets'][-1] for p in result['pushes'].values()
        ]

class TaskCache(CacheManager):
    '''Map candidate pushheads to Task Cluster task IDs and artifact URLs.'''

    def __init__(self, cache_dir, log=None, skip_cache=False):
        CacheManager.__init__(self, cache_dir, 'artifact_url', MAX_CACHED_TASKS, log=log, skip_cache=skip_cache)

    @cachedmethod(operator.attrgetter('_cache'))
    def artifact_urls(self, tree, job, rev, download_symbols):
        try:
            artifact_job = get_job_details(job, log=self._log, download_symbols=download_symbols)
        except KeyError:
            self.log(logging.INFO, 'artifact',
                {'job': job},
                'Unknown job {job}')
            raise KeyError("Unknown job")

        # Grab the second part of the repo name, which is generally how things
        # are indexed. Eg: 'integration/mozilla-inbound' is indexed as
        # 'mozilla-inbound'
        tree = tree.split('/')[1] if '/' in tree else tree

        namespace = 'gecko.v2.{tree}.revision.{rev}.{product}.{job}'.format(
            rev=rev,
            tree=tree,
            product=artifact_job.product,
            job=job,
        )
        self.log(logging.DEBUG, 'artifact',
                 {'namespace': namespace},
                 'Searching Taskcluster index with namespace: {namespace}')
        try:
            taskId = find_task_id(namespace)
        except Exception:
            # Not all revisions correspond to pushes that produce the job we
            # care about; and even those that do may not have completed yet.
            raise ValueError('Task for {namespace} does not exist (yet)!'.format(namespace=namespace))

        artifacts = list_artifacts(taskId)

        urls = []
        for artifact_name in artifact_job.find_candidate_artifacts(artifacts):
            # We can easily extract the task ID from the URL.  We can't easily
            # extract the build ID; we use the .ini files embedded in the
            # downloaded artifact for this.  We could also use the uploaded
            # public/build/buildprops.json for this purpose.
            url = get_artifact_url(taskId, artifact_name)
            urls.append(url)
        if not urls:
            raise ValueError('Task for {namespace} existed, but no artifacts found!'.format(namespace=namespace))
        return urls


class ArtifactPersistLimit(PersistLimit):
    '''Handle persistence for artifacts cache

    When instantiating a DownloadManager, it starts by filling the
    PersistLimit instance it's given with register_dir_content.
    In practice, this registers all the files already in the cache directory.
    After a download finishes, the newly downloaded file is registered, and the
    oldest files registered to the PersistLimit instance are removed depending
    on the size and file limits it's configured for.
    This is all good, but there are a few tweaks we want here:
    - We have pickle files in the cache directory that we don't want purged.
    - Files that were just downloaded in the same session shouldn't be purged.
      (if for some reason we end up downloading more than the default max size,
       we don't want the files to be purged)
    To achieve this, this subclass of PersistLimit inhibits the register_file
    method for pickle files and tracks what files were downloaded in the same
    session to avoid removing them.

    The register_file method may be used to register cache matches too, so that
    later sessions know they were freshly used.
    '''

    def __init__(self, log=None):
        super(ArtifactPersistLimit, self).__init__(
            size_limit=MAX_CACHED_ARTIFACTS_SIZE,
            file_limit=MIN_CACHED_ARTIFACTS)
        self._log = log
        self._registering_dir = False
        self._downloaded_now = set()

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def register_file(self, path):
        if path.endswith('.pickle') or \
                os.path.basename(path) == '.metadata_never_index':
            return
        if not self._registering_dir:
            # Touch the file so that subsequent calls to a mach artifact
            # command know it was recently used. While remove_old_files
            # is based on access time, in various cases, the access time is not
            # updated when just reading the file, so we force an update.
            try:
                os.utime(path, None)
            except OSError:
                pass
            self._downloaded_now.add(path)
        super(ArtifactPersistLimit, self).register_file(path)

    def register_dir_content(self, directory, pattern="*"):
        self._registering_dir = True
        super(ArtifactPersistLimit, self).register_dir_content(
            directory, pattern)
        self._registering_dir = False

    def remove_old_files(self):
        from dlmanager import fs
        files = sorted(self.files, key=lambda f: f.stat.st_atime)
        kept = []
        while len(files) > self.file_limit and \
                self._files_size >= self.size_limit:
            f = files.pop(0)
            if f.path in self._downloaded_now:
                kept.append(f)
                continue
            fs.remove(f.path)
            self.log(logging.INFO, 'artifact',
                {'filename': f.path},
                'Purged artifact {filename}')
            self._files_size -= f.stat.st_size
        self.files = files + kept

    def remove_all(self):
        from dlmanager import fs
        for f in self.files:
            fs.remove(f.path)
        self._files_size = 0
        self.files = []


class ArtifactCache(object):
    '''Fetch Task Cluster artifact URLs and purge least recently used artifacts from disk.'''

    def __init__(self, cache_dir, log=None, skip_cache=False):
        mkdir(cache_dir, not_indexed=True)
        self._cache_dir = cache_dir
        self._log = log
        self._skip_cache = skip_cache
        self._persist_limit = ArtifactPersistLimit(log)
        self._download_manager = DownloadManager(
            self._cache_dir, persist_limit=self._persist_limit)
        self._last_dl_update = -1

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def fetch(self, url, force=False):
        fname = os.path.basename(url)
        try:
            # Use the file name from the url if it looks like a hash digest.
            if len(fname) not in (32, 40, 56, 64, 96, 128):
                raise TypeError()
            binascii.unhexlify(fname)
        except TypeError:
            # We download to a temporary name like HASH[:16]-basename to
            # differentiate among URLs with the same basenames.  We used to then
            # extract the build ID from the downloaded artifact and use it to make a
            # human readable unique name, but extracting build IDs is time consuming
            # (especially on Mac OS X, where we must mount a large DMG file).
            hash = hashlib.sha256(url).hexdigest()[:16]
            fname = hash + '-' + os.path.basename(url)

        path = os.path.abspath(mozpath.join(self._cache_dir, fname))
        if self._skip_cache and os.path.exists(path):
            self.log(logging.DEBUG, 'artifact',
                {'path': path},
                'Skipping cache: removing cached downloaded artifact {path}')
            os.remove(path)

        self.log(logging.INFO, 'artifact',
            {'path': path},
            'Downloading to temporary location {path}')
        try:
            dl = self._download_manager.download(url, fname)

            def download_progress(dl, bytes_so_far, total_size):
                if not total_size:
                    return
                percent = (float(bytes_so_far) / total_size) * 100
                now = int(percent / 5)
                if now == self._last_dl_update:
                    return
                self._last_dl_update = now
                self.log(logging.INFO, 'artifact',
                         {'bytes_so_far': bytes_so_far, 'total_size': total_size, 'percent': percent},
                         'Downloading... {percent:02.1f} %')

            if dl:
                dl.set_progress(download_progress)
                dl.wait()
            else:
                # Avoid the file being removed if it was in the cache already.
                path = os.path.join(self._cache_dir, fname)
                self._persist_limit.register_file(path)

            self.log(logging.INFO, 'artifact',
                {'path': os.path.abspath(mozpath.join(self._cache_dir, fname))},
                'Downloaded artifact to {path}')
            return os.path.abspath(mozpath.join(self._cache_dir, fname))
        finally:
            # Cancel any background downloads in progress.
            self._download_manager.cancel()

    def clear_cache(self):
        if self._skip_cache:
            self.log(logging.DEBUG, 'artifact',
                {},
                'Skipping cache: ignoring clear_cache!')
            return

        self._persist_limit.remove_all()


class Artifacts(object):
    '''Maintain state to efficiently fetch build artifacts from a Firefox tree.'''

    def __init__(self, tree, substs, defines, job=None, log=None,
                 cache_dir='.', hg=None, git=None, skip_cache=False,
                 topsrcdir=None):
        if (hg and git) or (not hg and not git):
            raise ValueError("Must provide path to exactly one of hg and git")

        self._substs = substs
        self._download_symbols = self._substs.get('MOZ_ARTIFACT_BUILD_SYMBOLS', False)
        self._defines = defines
        self._tree = tree
        self._job = job or self._guess_artifact_job()
        self._log = log
        self._hg = hg
        self._git = git
        self._cache_dir = cache_dir
        self._skip_cache = skip_cache
        self._topsrcdir = topsrcdir

        try:
            self._artifact_job = get_job_details(self._job, log=self._log,
                                                 download_symbols=self._download_symbols,
                                                 substs=self._substs)
        except KeyError:
            self.log(logging.INFO, 'artifact',
                {'job': self._job},
                'Unknown job {job}')
            raise KeyError("Unknown job")

        self._task_cache = TaskCache(self._cache_dir, log=self._log, skip_cache=self._skip_cache)
        self._artifact_cache = ArtifactCache(self._cache_dir, log=self._log, skip_cache=self._skip_cache)
        self._pushhead_cache = PushheadCache(self._cache_dir, log=self._log, skip_cache=self._skip_cache)

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def _guess_artifact_job(self):
        # Add the "-debug" suffix to the guessed artifact job name
        # if MOZ_DEBUG is enabled.
        if self._substs.get('MOZ_DEBUG'):
            target_suffix = '-debug'
        else:
            target_suffix = '-opt'

        if self._substs.get('MOZ_BUILD_APP', '') == 'mobile/android':
            if self._substs['ANDROID_CPU_ARCH'] == 'x86':
                return 'android-x86-opt'
            return 'android-api-15' + target_suffix

        target_64bit = False
        if self._substs['target_cpu'] == 'x86_64':
            target_64bit = True

        if self._defines.get('XP_LINUX', False):
            return ('linux64' if target_64bit else 'linux') + target_suffix
        if self._defines.get('XP_WIN', False):
            return ('win64' if target_64bit else 'win32') + target_suffix
        if self._defines.get('XP_MACOSX', False):
            # We only produce unified builds in automation, so the target_cpu
            # check is not relevant.
            return 'macosx64' + target_suffix
        raise Exception('Cannot determine default job for |mach artifact|!')

    def _pushheads_from_rev(self, rev, count):
        """Queries hg.mozilla.org's json-pushlog for pushheads that are nearby
        ancestors or `rev`. Multiple trees are queried, as the `rev` may
        already have been pushed to multiple repositories. For each repository
        containing `rev`, the pushhead introducing `rev` and the previous
        `count` pushheads from that point are included in the output.
        """

        with self._pushhead_cache as pushhead_cache:
            found_pushids = {}
            for tree in CANDIDATE_TREES:
                self.log(logging.INFO, 'artifact',
                         {'tree': tree,
                          'rev': rev},
                         'Attempting to find a pushhead containing {rev} on {tree}.')
                try:
                    pushid = pushhead_cache.parent_pushhead_id(tree, rev)
                    found_pushids[tree] = pushid
                except ValueError:
                    continue

            candidate_pushheads = collections.defaultdict(list)

            for tree, pushid in found_pushids.iteritems():
                end = pushid
                start = pushid - NUM_PUSHHEADS_TO_QUERY_PER_PARENT

                self.log(logging.INFO, 'artifact',
                         {'tree': tree,
                          'pushid': pushid,
                          'num': NUM_PUSHHEADS_TO_QUERY_PER_PARENT},
                         'Retrieving the last {num} pushheads starting with id {pushid} on {tree}')
                for pushhead in pushhead_cache.pushid_range(tree, start, end):
                    candidate_pushheads[pushhead].append(tree)

        return candidate_pushheads

    def _get_hg_revisions_from_git(self):
        rev_list = subprocess.check_output([
            self._git, 'rev-list', '--topo-order',
            '--max-count={num}'.format(num=NUM_REVISIONS_TO_QUERY),
            'HEAD',
        ], cwd=self._topsrcdir)

        hg_hash_list = subprocess.check_output([
            self._git, 'cinnabar', 'git2hg'
        ] + rev_list.splitlines(), cwd=self._topsrcdir)

        zeroes = "0" * 40

        hashes = []
        for hg_hash in hg_hash_list.splitlines():
            hg_hash = hg_hash.strip()
            if not hg_hash or hg_hash == zeroes:
                continue
            hashes.append(hg_hash)
        return hashes

    def _get_recent_public_revisions(self):
        """Returns recent ancestors of the working parent that are likely to
        to be known to Mozilla automation.

        If we're using git, retrieves hg revisions from git-cinnabar.
        """
        if self._git:
            return self._get_hg_revisions_from_git()

        return subprocess.check_output([
            self._hg, 'log',
            '--template', '{node}\n',
            '-r', 'last(public() and ::., {num})'.format(
                num=NUM_REVISIONS_TO_QUERY)
        ], cwd=self._topsrcdir).splitlines()

    def _find_pushheads(self):
        """Returns an iterator of recent pushhead revisions, starting with the
        working parent.
        """

        last_revs = self._get_recent_public_revisions()
        candidate_pushheads = self._pushheads_from_rev(last_revs[0].rstrip(),
                                                       NUM_PUSHHEADS_TO_QUERY_PER_PARENT)
        count = 0
        for rev in last_revs:
            rev = rev.rstrip()
            if not rev:
                continue
            if rev not in candidate_pushheads:
                continue
            count += 1
            yield candidate_pushheads[rev], rev

        if not count:
            raise Exception('Could not find any candidate pushheads in the last {num} revisions.\n'
                            'Search started with {rev}, which must be known to Mozilla automation.\n\n'
                            'see https://developer.mozilla.org/en-US/docs/Artifact_builds'.format(
                                rev=last_revs[0], num=NUM_PUSHHEADS_TO_QUERY_PER_PARENT))

    def find_pushhead_artifacts(self, task_cache, job, tree, pushhead):
        try:
            urls = task_cache.artifact_urls(tree, job, pushhead, self._download_symbols)
        except ValueError:
            return None
        if urls:
            self.log(logging.INFO, 'artifact',
                     {'pushhead': pushhead,
                      'tree': tree},
                     'Installing from remote pushhead {pushhead} on {tree}')
            return urls
        return None

    def install_from_file(self, filename, distdir):
        self.log(logging.INFO, 'artifact',
            {'filename': filename},
            'Installing from {filename}')

        # Do we need to post-process?
        processed_filename = filename + PROCESSED_SUFFIX

        if self._skip_cache and os.path.exists(processed_filename):
            self.log(logging.DEBUG, 'artifact',
                {'path': processed_filename},
                'Skipping cache: removing cached processed artifact {path}')
            os.remove(processed_filename)

        if not os.path.exists(processed_filename):
            self.log(logging.INFO, 'artifact',
                {'filename': filename},
                'Processing contents of {filename}')
            self.log(logging.INFO, 'artifact',
                {'processed_filename': processed_filename},
                'Writing processed {processed_filename}')
            self._artifact_job.process_artifact(filename, processed_filename)

        self._artifact_cache._persist_limit.register_file(processed_filename)

        self.log(logging.INFO, 'artifact',
            {'processed_filename': processed_filename},
            'Installing from processed {processed_filename}')

        # Copy all .so files, avoiding modification where possible.
        ensureParentDir(mozpath.join(distdir, '.dummy'))

        with zipfile.ZipFile(processed_filename) as zf:
            for info in zf.infolist():
                if info.filename.endswith('.ini'):
                    continue
                n = mozpath.join(distdir, info.filename)
                fh = FileAvoidWrite(n, mode='rb')
                shutil.copyfileobj(zf.open(info), fh)
                file_existed, file_updated = fh.close()
                self.log(logging.INFO, 'artifact',
                    {'updating': 'Updating' if file_updated else 'Not updating', 'filename': n},
                    '{updating} {filename}')
                if not file_existed or file_updated:
                    # Libraries and binaries may need to be marked executable,
                    # depending on platform.
                    perms = info.external_attr >> 16 # See http://stackoverflow.com/a/434689.
                    perms |= stat.S_IWUSR | stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH # u+w, a+r.
                    os.chmod(n, perms)
        return 0

    def install_from_url(self, url, distdir):
        self.log(logging.INFO, 'artifact',
            {'url': url},
            'Installing from {url}')
        filename = self._artifact_cache.fetch(url)
        return self.install_from_file(filename, distdir)

    def _install_from_hg_pushheads(self, hg_pushheads, distdir):
        """Iterate pairs (hg_hash, {tree-set}) associating hg revision hashes
        and tree-sets they are known to be in, trying to download and
        install from each.
        """

        urls = None
        count = 0
        # with blocks handle handle persistence.
        with self._task_cache as task_cache:
            for trees, hg_hash in hg_pushheads:
                for tree in trees:
                    count += 1
                    self.log(logging.DEBUG, 'artifact',
                             {'hg_hash': hg_hash,
                              'tree': tree},
                             'Trying to find artifacts for hg revision {hg_hash} on tree {tree}.')
                    urls = self.find_pushhead_artifacts(task_cache, self._job, tree, hg_hash)
                    if urls:
                        for url in urls:
                            if self.install_from_url(url, distdir):
                                return 1
                        return 0

        self.log(logging.ERROR, 'artifact',
                 {'count': count},
                 'Tried {count} pushheads, no built artifacts found.')
        return 1

    def install_from_recent(self, distdir):
        hg_pushheads = self._find_pushheads()
        return self._install_from_hg_pushheads(hg_pushheads, distdir)

    def install_from_revset(self, revset, distdir):
        if self._hg:
            revision = subprocess.check_output([self._hg, 'log', '--template', '{node}\n',
                                                '-r', revset], cwd=self._topsrcdir).strip()
            if len(revision.split('\n')) != 1:
                raise ValueError('hg revision specification must resolve to exactly one commit')
        else:
            revision = subprocess.check_output([self._git, 'rev-parse', revset], cwd=self._topsrcdir).strip()
            revision = subprocess.check_output([self._git, 'cinnabar', 'git2hg', revision], cwd=self._topsrcdir).strip()
            if len(revision.split('\n')) != 1:
                raise ValueError('hg revision specification must resolve to exactly one commit')
            if revision == "0" * 40:
                raise ValueError('git revision specification must resolve to a commit known to hg')

        self.log(logging.INFO, 'artifact',
                 {'revset': revset,
                  'revision': revision},
                 'Will only accept artifacts from a pushhead at {revision} '
                 '(matched revset "{revset}").')
        # Include try in our search to allow pulling from a specific push.
        pushheads = [(list(CANDIDATE_TREES) + ['try'], revision)]
        return self._install_from_hg_pushheads(pushheads, distdir)

    def install_from(self, source, distdir):
        """Install artifacts from a ``source`` into the given ``distdir``.
        """
        if source and os.path.isfile(source):
            return self.install_from_file(source, distdir)
        elif source and urlparse.urlparse(source).scheme:
            return self.install_from_url(source, distdir)
        else:
            if source is None and 'MOZ_ARTIFACT_REVISION' in os.environ:
                source = os.environ['MOZ_ARTIFACT_REVISION']

            if source:
                return self.install_from_revset(source, distdir)

            return self.install_from_recent(distdir)


    def clear_cache(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Deleting cached artifacts and caches.')
        self._task_cache.clear_cache()
        self._artifact_cache.clear_cache()
        self._pushhead_cache.clear_cache()
