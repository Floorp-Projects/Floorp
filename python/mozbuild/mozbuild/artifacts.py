# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Fetch build artifacts from a Firefox tree.

This provides an (at-the-moment special purpose) interface to download Android
artifacts from Mozilla's Task Cluster.

This module performs the following steps:

* find a candidate hg parent revision using the local pushlog.  The local
  pushlog is maintained by mozext locally and updated on every pull.

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

import collections
import functools
import glob
import hashlib
import logging
import operator
import os
import pickle
import re
import shutil
import stat
import subprocess
import tarfile
import tempfile
import urlparse
import zipfile

import pylru
import taskcluster

import buildconfig
from mozbuild.util import (
    ensureParentDir,
    FileAvoidWrite,
)
import mozinstall
from mozpack.files import FileFinder
from mozpack.mozjar import (
    JarReader,
    JarWriter,
)
import mozpack.path as mozpath
from mozregression.download_manager import (
    DownloadManager,
)
from mozregression.persist_limit import (
    PersistLimit,
)

NUM_PUSHHEADS_TO_QUERY_PER_PARENT = 50  # Number of candidate pushheads to cache per parent changeset.

MAX_CACHED_TASKS = 400  # Number of pushheads to cache Task Cluster task data for.

# Number of downloaded artifacts to cache.  Each artifact can be very large,
# so don't make this to large!  TODO: make this a size (like 500 megs) rather than an artifact count.
MAX_CACHED_ARTIFACTS = 6

# Downloaded artifacts are cached, and a subset of their contents extracted for
# easy installation.  This is most noticeable on Mac OS X: since mounting and
# copying from DMG files is very slow, we extract the desired binaries to a
# separate archive for fast re-installation.
PROCESSED_SUFFIX = '.processed.jar'

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
        ('bin/ssltunnel', ('bin', 'bin')),
        ('bin/xpcshell', ('bin', 'bin')),
        ('bin/plugins/*', ('bin/plugins', 'plugins'))
    }

    # We can tell our input is a test archive by this suffix, which happens to
    # be the same across platforms.
    _test_archive_suffix = '.common.tests.zip'

    def __init__(self, package_re, tests_re, log=None):
        self._package_re = re.compile(package_re)
        self._tests_re = None
        if tests_re:
            self._tests_re = re.compile(tests_re)
        self._log = log

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
        return self.process_package_artifact(filename, processed_filename)

    def process_package_artifact(self, filename, processed_filename):
        raise NotImplementedError("Subclasses must specialize process_package_artifact!")

    def process_tests_artifact(self, filename, processed_filename):
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

        if not added_entry:
            raise ValueError('Archive format changed! No pattern from "{patterns}"'
                             'matched an archive path.'.format(
                                 patterns=LinuxArtifactJob.test_artifact_patterns))

class AndroidArtifactJob(ArtifactJob):
    def process_artifact(self, filename, processed_filename):
        # Extract all .so files into the root, which will get copied into dist/bin.
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            for f in JarReader(filename):
                if not f.filename.endswith('.so') and \
                   not f.filename in ('platform.ini', 'application.ini'):
                    continue

                basename = os.path.basename(f.filename)
                self.log(logging.INFO, 'artifact',
                    {'basename': basename},
                   'Adding {basename} to processed archive')

                basename = mozpath.join('bin', basename)
                writer.add(basename.encode('utf-8'), f)


class LinuxArtifactJob(ArtifactJob):

    package_artifact_patterns = {
        'firefox/application.ini',
        'firefox/crashreporter',
        'firefox/dependentlibs.list',
        'firefox/firefox',
        'firefox/firefox-bin',
        'firefox/platform.ini',
        'firefox/plugin-container',
        'firefox/updater',
        'firefox/webapprt-stub',
        'firefox/**/*.so',
    }

    def process_package_artifact(self, filename, processed_filename):
        added_entry = False

        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            with tarfile.open(filename) as reader:
                for f in reader:
                    if not f.isfile():
                        continue

                    if not any(mozpath.match(f.name, p) for p in self.package_artifact_patterns):
                        continue

                    # We strip off the relative "firefox/" bit from the path,
                    # but otherwise preserve it.
                    destpath = mozpath.join('bin',
                                            mozpath.relpath(f.name, "firefox"))
                    self.log(logging.INFO, 'artifact',
                             {'destpath': destpath},
                             'Adding {destpath} to processed archive')
                    writer.add(destpath.encode('utf-8'), reader.extractfile(f), mode=f.mode)
                    added_entry = True

        if not added_entry:
            raise ValueError('Archive format changed! No pattern from "{patterns}" '
                             'matched an archive path.'.format(
                                 patterns=LinuxArtifactJob.package_artifact_patterns))


class MacArtifactJob(ArtifactJob):
    def process_package_artifact(self, filename, processed_filename):
        tempdir = tempfile.mkdtemp()
        try:
            self.log(logging.INFO, 'artifact',
                {'tempdir': tempdir},
                'Unpacking DMG into {tempdir}')
            mozinstall.install(filename, tempdir) # Doesn't handle already mounted DMG files nicely:

            # InstallError: Failed to install "/Users/nalexander/.mozbuild/package-frontend/b38eeeb54cdcf744-firefox-44.0a1.en-US.mac.dmg (local variable 'appDir' referenced before assignment)"

            #   File "/Users/nalexander/Mozilla/gecko/mobile/android/mach_commands.py", line 250, in artifact_install
            #     return artifacts.install_from(source, self.distdir)
            #   File "/Users/nalexander/Mozilla/gecko/python/mozbuild/mozbuild/artifacts.py", line 457, in install_from
            #     return self.install_from_hg(source, distdir)
            #   File "/Users/nalexander/Mozilla/gecko/python/mozbuild/mozbuild/artifacts.py", line 445, in install_from_hg
            #     return self.install_from_url(url, distdir)
            #   File "/Users/nalexander/Mozilla/gecko/python/mozbuild/mozbuild/artifacts.py", line 418, in install_from_url
            #     return self.install_from_file(filename, distdir)
            #   File "/Users/nalexander/Mozilla/gecko/python/mozbuild/mozbuild/artifacts.py", line 336, in install_from_file
            #     mozinstall.install(filename, tempdir)
            #   File "/Users/nalexander/Mozilla/gecko/objdir-dce/_virtualenv/lib/python2.7/site-packages/mozinstall/mozinstall.py", line 117, in install
            #     install_dir = _install_dmg(src, dest)
            #   File "/Users/nalexander/Mozilla/gecko/objdir-dce/_virtualenv/lib/python2.7/site-packages/mozinstall/mozinstall.py", line 261, in _install_dmg
            #     subprocess.call('hdiutil detach %s -quiet' % appDir,

            bundle_dirs = glob.glob(mozpath.join(tempdir, '*.app'))
            if len(bundle_dirs) != 1:
                raise ValueError('Expected one source bundle, found: {}'.format(bundle_dirs))
            [source] = bundle_dirs

            # These get copied into dist/bin without the path, so "root/a/b/c" -> "dist/bin/c".
            paths_no_keep_path = ('Contents/MacOS', [
                'crashreporter.app/Contents/MacOS/crashreporter',
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
                'libsoftokn3.dylib',
                'plugin-container.app/Contents/MacOS/plugin-container',
                'updater.app/Contents/MacOS/updater',
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
                'webapprt-stub',
            ])

            with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
                root, paths = paths_no_keep_path
                finder = FileFinder(mozpath.join(source, root))
                for path in paths:
                    for p, f in finder.find(path):
                        self.log(logging.INFO, 'artifact',
                            {'path': path},
                            'Adding {path} to processed archive')
                        destpath = mozpath.join('bin', os.path.basename(p))
                        writer.add(destpath.encode('utf-8'), f, mode=os.stat(mozpath.join(finder.base, p)).st_mode)

                root, paths = paths_keep_path
                finder = FileFinder(mozpath.join(source, root))
                for path in paths:
                    for p, f in finder.find(path):
                        self.log(logging.INFO, 'artifact',
                            {'path': path},
                            'Adding {path} to processed archive')
                        destpath = mozpath.join('bin', p)
                        writer.add(destpath.encode('utf-8'), f, mode=os.stat(mozpath.join(finder.base, p)).st_mode)

        finally:
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
    }
    # These are a subset of TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
    test_artifact_patterns = {
        ('bin/BadCertServer.exe', ('bin', 'bin')),
        ('bin/GenerateOCSPResponse.exe', ('bin', 'bin')),
        ('bin/OCSPStaplingServer.exe', ('bin', 'bin')),
        ('bin/certutil.exe', ('bin', 'bin')),
        ('bin/fileid.exe', ('bin', 'bin')),
        ('bin/pk12util.exe', ('bin', 'bin')),
        ('bin/ssltunnel.exe', ('bin', 'bin')),
        ('bin/xpcshell.exe', ('bin', 'bin')),
        ('bin/plugins/*', ('bin/plugins', 'plugins'))
    }

    def process_package_artifact(self, filename, processed_filename):
        added_entry = False
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            for f in JarReader(filename):
                if not any(mozpath.match(f.filename, p) for p in self.package_artifact_patterns):
                    continue

                # strip off the relative "firefox/" bit from the path:
                basename = mozpath.relpath(f.filename, "firefox")
                basename = mozpath.join('bin', basename)
                self.log(logging.INFO, 'artifact',
                    {'basename': basename},
                    'Adding {basename} to processed archive')
                writer.add(basename.encode('utf-8'), f)
                added_entry = True

        if not added_entry:
            raise ValueError('Archive format changed! No pattern from "{patterns}"'
                             'matched an archive path.'.format(
                                 patterns=self.artifact_patterns))

# Keep the keys of this map in sync with the |mach artifact| --job
# options.  The keys of this map correspond to entries at
# https://tools.taskcluster.net/index/artifacts/#buildbot.branches.mozilla-central/buildbot.branches.mozilla-central.
# The values correpsond to a pair of (<package regex>, <test archive regex>).
JOB_DETAILS = {
    # 'android-api-9': (AndroidArtifactJob, 'public/build/fennec-(.*)\.android-arm\.apk'),
    'android-api-15': (AndroidArtifactJob, ('public/build/fennec-(.*)\.android-arm\.apk',
                                            None)),
    'android-x86': (AndroidArtifactJob, ('public/build/fennec-(.*)\.android-i386\.apk',
                                         None)),
    'linux': (LinuxArtifactJob, ('public/build/firefox-(.*)\.linux-i686\.tar\.bz2',
                                 'public/build/firefox-(.*)\.common\.tests\.zip')),
    'linux64': (LinuxArtifactJob, ('public/build/firefox-(.*)\.linux-x86_64\.tar\.bz2',
                                   'public/build/firefox-(.*)\.common\.tests\.zip')),
    'macosx64': (MacArtifactJob, ('public/build/firefox-(.*)\.mac\.dmg',
                                  'public/build/firefox-(.*)\.common\.tests\.zip')),
    'win32': (WinArtifactJob, ('public/build/firefox-(.*)\.win32.zip',
                               'public/build/firefox-(.*)\.common\.tests\.zip')),
    'win64': (WinArtifactJob, ('public/build/firefox-(.*)\.win64.zip',
                               'public/build/firefox-(.*)\.common\.tests\.zip')),
}



def get_job_details(job, log=None):
    cls, (package_re, tests_re) = JOB_DETAILS[job]
    return cls(package_re, tests_re, log=log)

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

    def __init__(self, cache_dir, cache_name, cache_size, cache_callback=None, log=None):
        self._cache = pylru.lrucache(cache_size, callback=cache_callback)
        self._cache_filename = mozpath.join(cache_dir, cache_name + '-cache.pickle')
        self._log = log

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def load_cache(self):
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
        ensureParentDir(self._cache_filename)
        pickle.dump(list(reversed(list(self._cache.items()))), open(self._cache_filename, 'wb'), -1)

    def clear_cache(self):
        with self:
            self._cache.clear()

    def print_cache(self):
        with self:
            for item in self._cache.items():
                self.log(logging.INFO, 'artifact',
                    {'item': item},
                    '{item}')

    def print_last_item(self, args, sorted_kwargs, result):
        # By default, show nothing.
        pass

    def print_last(self):
        # We use the persisted LRU caches to our advantage.  The first item is
        # most recent.
        with self:
            item = next(self._cache.items(), None)
            if item is not None:
                (name, args, sorted_kwargs), result = item
                self.print_last_item(args, sorted_kwargs, result)
            else:
                self.log(logging.WARN, 'artifact',
                    {},
                    'No last cached item found.')

    def __enter__(self):
        self.load_cache()
        return self

    def __exit__(self, type, value, traceback):
        self.dump_cache()

class TreeCache(CacheManager):
    '''Map pushhead revisions to trees with tasks/artifacts known to taskcluster.'''

    def __init__(self, cache_dir, log=None):
        CacheManager.__init__(self, cache_dir, 'artifact_tree', MAX_CACHED_TASKS, log=log)

        self._index = taskcluster.Index()

    @cachedmethod(operator.attrgetter('_cache'))
    def artifact_trees(self, rev, trees):
        # The "trees" argument is intentionally ignored. If this value
        # changes over time it means a changeset we care about has become
        # a pushhead on another tree, and our cache may no longer be
        # valid.
        rev_ns = 'buildbot.revisions.{rev}'.format(rev=rev)
        try:
            result = self._index.listNamespaces(rev_ns, {"limit": 10})
        except Exception:
            return []
        return [ns['name'] for ns in result['namespaces']]

    def print_last_item(self, args, sorted_kwargs, result):
        rev, trees = args
        self.log(logging.INFO, 'artifact',
            {'rev': rev},
            'Last fetched trees for pushhead revision {rev}')

class TaskCache(CacheManager):
    '''Map candidate pushheads to Task Cluster task IDs and artifact URLs.'''

    def __init__(self, cache_dir, log=None):
        CacheManager.__init__(self, cache_dir, 'artifact_url', MAX_CACHED_TASKS, log=log)
        self._index = taskcluster.Index()
        self._queue = taskcluster.Queue()

    @cachedmethod(operator.attrgetter('_cache'))
    def artifact_urls(self, tree, job, rev):
        try:
            artifact_job = get_job_details(job, log=self._log)
        except KeyError:
            self.log(logging.INFO, 'artifact',
                {'job': job},
                'Unknown job {job}')
            raise KeyError("Unknown job")

        key = '{rev}.{tree}.{job}'.format(rev=rev, tree=tree, job=job)
        try:
            namespace = 'buildbot.revisions.{key}'.format(key=key)
            task = self._index.findTask(namespace)
        except Exception:
            # Not all revisions correspond to pushes that produce the job we
            # care about; and even those that do may not have completed yet.
            raise ValueError('Task for {key} does not exist (yet)!'.format(key=key))
        taskId = task['taskId']

        artifacts = self._queue.listLatestArtifacts(taskId)['artifacts']

        urls = []
        for artifact_name in artifact_job.find_candidate_artifacts(artifacts):
            # We can easily extract the task ID from the URL.  We can't easily
            # extract the build ID; we use the .ini files embedded in the
            # downloaded artifact for this.  We could also use the uploaded
            # public/build/buildprops.json for this purpose.
            url = self._queue.buildUrl('getLatestArtifact', taskId, artifact_name)
            urls.append(url)
        if not urls:
            raise ValueError('Task for {key} existed, but no artifacts found!'.format(key=key))
        return urls

    def print_last_item(self, args, sorted_kwargs, result):
        tree, job, rev = args
        self.log(logging.INFO, 'artifact',
            {'rev': rev},
            'Last installed binaries from hg parent revision {rev}')


class ArtifactCache(CacheManager):
    '''Fetch Task Cluster artifact URLs and purge least recently used artifacts from disk.'''

    def __init__(self, cache_dir, log=None):
        # TODO: instead of storing N artifact packages, store M megabytes.
        CacheManager.__init__(self, cache_dir, 'fetch', MAX_CACHED_ARTIFACTS, cache_callback=self.delete_file, log=log)
        self._cache_dir = cache_dir
        size_limit = 1024 * 1024 * 1024 # 1Gb in bytes.
        file_limit = 4 # But always keep at least 4 old artifacts around.
        persist_limit = PersistLimit(size_limit, file_limit)
        self._download_manager = DownloadManager(self._cache_dir, persist_limit=persist_limit)

    def delete_file(self, key, value):
        try:
            os.remove(value)
            self.log(logging.INFO, 'artifact',
                {'filename': value},
                'Purged artifact {filename}')
        except (OSError, IOError):
            pass

        try:
            os.remove(value + PROCESSED_SUFFIX)
            self.log(logging.INFO, 'artifact',
                {'filename': value + PROCESSED_SUFFIX},
                'Purged processed artifact {filename}')
        except (OSError, IOError):
            pass

    @cachedmethod(operator.attrgetter('_cache'))
    def fetch(self, url, force=False):
        # We download to a temporary name like HASH[:16]-basename to
        # differentiate among URLs with the same basenames.  We used to then
        # extract the build ID from the downloaded artifact and use it to make a
        # human readable unique name, but extracting build IDs is time consuming
        # (especially on Mac OS X, where we must mount a large DMG file).
        hash = hashlib.sha256(url).hexdigest()[:16]
        fname = hash + '-' + os.path.basename(url)
        self.log(logging.INFO, 'artifact',
            {'path': os.path.abspath(mozpath.join(self._cache_dir, fname))},
            'Downloading to temporary location {path}')
        try:
            dl = self._download_manager.download(url, fname)
            if dl:
                dl.wait()
            self.log(logging.INFO, 'artifact',
                {'path': os.path.abspath(mozpath.join(self._cache_dir, fname))},
                'Downloaded artifact to {path}')
            return os.path.abspath(mozpath.join(self._cache_dir, fname))
        finally:
            # Cancel any background downloads in progress.
            self._download_manager.cancel()

    def print_last_item(self, args, sorted_kwargs, result):
        url, = args
        self.log(logging.INFO, 'artifact',
            {'url': url},
            'Last installed binaries from url {url}')
        self.log(logging.INFO, 'artifact',
            {'filename': result},
            'Last installed binaries from local file {filename}')
        self.log(logging.INFO, 'artifact',
            {'filename': result + PROCESSED_SUFFIX},
            'Last installed binaries from local processed file {filename}')


class Artifacts(object):
    '''Maintain state to efficiently fetch build artifacts from a Firefox tree.'''

    def __init__(self, tree, job=None, log=None, cache_dir='.', hg='hg'):
        self._tree = tree
        self._job = job or self._guess_artifact_job()
        self._log = log
        self._hg = hg
        self._cache_dir = cache_dir

        try:
            self._artifact_job = get_job_details(self._job, log=self._log)
        except KeyError:
            self.log(logging.INFO, 'artifact',
                {'job': self._job},
                'Unknown job {job}')
            raise KeyError("Unknown job")

        self._task_cache = TaskCache(self._cache_dir, log=self._log)
        self._artifact_cache = ArtifactCache(self._cache_dir, log=self._log)
        self._tree_cache = TreeCache(self._cache_dir, log=self._log)
        # A "tree" according to mozext and an integration branch isn't always
        # an exact match. For example, pushhead("central") refers to pushheads
        # with artifacts under the taskcluster namespace "mozilla-central".
        self._tree_replacements = {
            'inbound': 'mozilla-inbound',
            'central': 'mozilla-central',
        }


    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def _guess_artifact_job(self):
        if buildconfig.substs.get('MOZ_BUILD_APP', '') == 'mobile/android':
            if buildconfig.substs['ANDROID_CPU_ARCH'] == 'x86':
                return 'android-x86'
            return 'android-api-15'

        target_64bit = False
        if buildconfig.substs['target_cpu'] == 'x86_64':
            target_64bit = True

        if buildconfig.defines.get('XP_LINUX', False):
            return 'linux64' if target_64bit else 'linux'
        if buildconfig.defines.get('XP_WIN', False):
            return 'win64' if target_64bit else 'win32'
        if buildconfig.defines.get('XP_MACOSX', False):
            # We only produce unified builds in automation, so the target_cpu
            # check is not relevant.
            return 'macosx64'
        raise Exception('Cannot determine default job for |mach artifact|!')

    def _find_pushheads(self, parent):
        # Return an ordered dict associating revisions that are pushheads with
        # trees they are known to be in (starting with the first tree they're
        # known to be in).

        try:
            output = subprocess.check_output([
                self._hg, 'log',
                '--template', '{node},{join(trees, ",")}\n',
                '-r', 'last(pushhead({tree}) and ::{parent}, {num})'.format(
                    tree=self._tree or '', parent=parent, num=NUM_PUSHHEADS_TO_QUERY_PER_PARENT)
            ])
        except subprocess.CalledProcessError:
            # We probably don't have the mozext extension installed.
            ret = subprocess.call([self._hg, 'showconfig', 'extensions.mozext'])
            if ret:
                raise Exception('Could not find pushheads for recent revisions.\n\n'
                                'You need to enable the "mozext" hg extension: '
                                'see https://developer.mozilla.org/en-US/docs/Artifact_builds')
            raise

        rev_trees = collections.OrderedDict()
        for line in output.splitlines():
            if not line:
                continue
            rev_info = line.split(',')
            if len(rev_info) == 1:
                # If pushhead() is true, it would seem "trees" should be
                # non-empty, but this is defensive.
                continue
            rev_trees[rev_info[0]] = tuple(rev_info[1:])

        if not rev_trees:
            raise Exception('Could not find any candidate pushheads in the last {num} revisions.\n\n'
                            'Try running |hg pushlogsync|;\n'
                            'see https://developer.mozilla.org/en-US/docs/Artifact_builds'.format(
                                num=NUM_PUSHHEADS_TO_QUERY_PER_PARENT))

        return rev_trees

    def find_pushhead_artifacts(self, task_cache, tree_cache, job, pushhead, trees):
        known_trees = set(tree_cache.artifact_trees(pushhead, trees))
        if not known_trees:
            return None
        # If we ever find a rev that's a pushhead on multiple trees, we want
        # the most recent one.
        for tree in reversed(trees):
            tree = self._tree_replacements.get(tree) or tree
            if tree not in known_trees:
                continue
            try:
                urls = task_cache.artifact_urls(tree, job, pushhead)
            except ValueError:
                continue
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
        if not os.path.exists(processed_filename):
            self.log(logging.INFO, 'artifact',
                {'filename': filename},
                'Processing contents of {filename}')
            self.log(logging.INFO, 'artifact',
                {'processed_filename': processed_filename},
                'Writing processed {processed_filename}')
            self._artifact_job.process_artifact(filename, processed_filename)

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
        with self._artifact_cache as artifact_cache:  # The with block handles persistence.
            filename = artifact_cache.fetch(url)
        return self.install_from_file(filename, distdir)

    def install_from_hg(self, revset, distdir):
        if not revset:
            revset = '.'
        rev_pushheads = self._find_pushheads(revset)
        urls = None
        # with blocks handle handle persistence.
        with self._task_cache as task_cache, self._tree_cache as tree_cache:
            while rev_pushheads:
                rev, trees = rev_pushheads.popitem(last=False)
                self.log(logging.DEBUG, 'artifact',
                    {'rev': rev},
                    'Trying to find artifacts for pushhead {rev}.')
                urls = self.find_pushhead_artifacts(task_cache, tree_cache,
                                                    self._job, rev, trees)
                if urls:
                    for url in urls:
                        if self.install_from_url(url, distdir):
                            return 1
                    return 0
        self.log(logging.ERROR, 'artifact',
                 {'revset': revset},
                 'No built artifacts for {revset} found.')
        return 1

    def install_from(self, source, distdir):
        """Install artifacts from a ``source`` into the given ``distdir``.
        """
        if source and os.path.isfile(source):
            return self.install_from_file(source, distdir)
        elif source and urlparse.urlparse(source).scheme:
            return self.install_from_url(source, distdir)
        else:
            return self.install_from_hg(source, distdir)

    def print_last(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Printing last used artifact details.')
        self._pushhead_cache.print_last()
        self._task_cache.print_last()
        self._artifact_cache.print_last()

    def clear_cache(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Deleting cached artifacts and caches.')
        self._pushhead_cache.clear_cache()
        self._task_cache.clear_cache()
        self._artifact_cache.clear_cache()

    def print_cache(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Printing cached artifacts and caches.')
        self._tree_cache.print_cache()
        self._task_cache.print_cache()
        self._artifact_cache.print_cache()
