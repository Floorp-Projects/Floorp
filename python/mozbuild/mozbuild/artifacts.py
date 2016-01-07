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

import functools
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

MAX_CACHED_PARENTS = 100  # Number of parent changesets to cache candidate pushheads for.
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
    def __init__(self, regexp, log=None):
        self._regexp = re.compile(regexp)
        self._log = log

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def find_candidate_artifacts(self, artifacts):
        # TODO: Handle multiple artifacts, taking the latest one.
        for artifact in artifacts:
            name = artifact['name']
            if self._regexp.match(name):
                yield artifact
            else:
                self.log(logging.DEBUG, 'artifact',
                    {'name': name},
                    'Not yielding artifact named {name} as a candidate artifact')

    def process_artifact(self, filename, processed_filename):
        raise NotImplementedError("Subclasses must specialize process_artifact!")


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

                writer.add(basename.encode('utf-8'), f)


class LinuxArtifactJob(ArtifactJob):
    def process_artifact(self, filename, processed_filename):
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            prefix = 'firefox/'
            whitelist = {
                'application.ini',
                'crashreporter',
                'dependentlibs.list',
                'firefox',
                'firefox-bin',
                'platform.ini',
                'plugin-container',
                'updater',
                'webapprt-stub',
            }
            with tarfile.open(filename) as reader:
                for f in reader:
                    if not f.isfile():
                        continue

                    if not f.name.startswith(prefix):
                        raise ValueError('Archive format changed! Filename should start with "firefox/"; was "{filename}"'.format(filename=f.name))
                    basename = f.name[len(prefix):]

                    if not basename.endswith('.so') and \
                       basename not in whitelist:
                        continue

                    self.log(logging.INFO, 'artifact',
                        {'basename': basename},
                        'Adding {basename} to processed archive')

                    writer.add(basename.encode('utf-8'), reader.extractfile(f), mode=f.mode)


class MacArtifactJob(ArtifactJob):
    def process_artifact(self, filename, processed_filename):
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

            # TODO: Extract 'MOZ_MACBUNDLE_NAME' from buildconfig.
            bundle_name = 'Nightly.app'
            source = mozpath.join(tempdir, bundle_name)

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
                        writer.add(os.path.basename(p).encode('utf-8'), f, mode=os.stat(mozpath.join(finder.base, p)).st_mode)

                root, paths = paths_keep_path
                finder = FileFinder(mozpath.join(source, root))
                for path in paths:
                    for p, f in finder.find(path):
                        self.log(logging.INFO, 'artifact',
                            {'path': path},
                            'Adding {path} to processed archive')
                        writer.add(p.encode('utf-8'), f, mode=os.stat(mozpath.join(finder.base, p)).st_mode)

        finally:
            try:
                shutil.rmtree(tempdir)
            except (OSError, IOError):
                self.log(logging.WARN, 'artifact',
                    {'tempdir': tempdir},
                    'Unable to delete {tempdir}')
                pass


class WinArtifactJob(ArtifactJob):
    def process_artifact(self, filename, processed_filename):
        with JarWriter(file=processed_filename, optimize=False, compress_level=5) as writer:
            prefix = 'firefox/'
            whitelist = {
                'dependentlibs.list',
                'platform.ini',
                'application.ini',
            }
            for f in JarReader(filename):
                if not f.filename.startswith(prefix):
                    raise ValueError('Archive format changed! Filename should start with "firefox/"; was "{filename}"'.format(filename=f.filename))
                basename = f.filename[len(prefix):]

                if not basename.endswith('.dll') and \
                   not basename.endswith('.exe') and \
                   basename not in whitelist:
                    continue

                self.log(logging.INFO, 'artifact',
                    {'basename': basename},
                    'Adding {basename} to processed archive')

                writer.add(basename.encode('utf-8'), f)


# Keep the keys of this map in sync with the |mach artifact| --job
# options.  The keys of this map correspond to entries at
# https://tools.taskcluster.net/index/artifacts/#buildbot.branches.mozilla-central/buildbot.branches.mozilla-central.
JOB_DETAILS = {
    # 'android-api-9': (AndroidArtifactJob, 'public/build/fennec-(.*)\.android-arm\.apk'),
    'android-api-11': (AndroidArtifactJob, 'public/build/fennec-(.*)\.android-arm\.apk'),
    'android-x86': (AndroidArtifactJob, 'public/build/fennec-(.*)\.android-i386\.apk'),
    'linux': (LinuxArtifactJob, 'public/build/firefox-(.*)\.linux-i686\.tar\.bz2'),
    'linux64': (LinuxArtifactJob, 'public/build/firefox-(.*)\.linux-x86_64\.tar\.bz2'),
    'macosx64': (MacArtifactJob, 'public/build/firefox-(.*)\.mac\.dmg'),
    'win32': (WinArtifactJob, 'public/build/firefox-(.*)\.win32.zip'),
    'win64': (WinArtifactJob, 'public/build/firefox-(.*)\.win64.zip'),
}

def get_job_details(job, log=None):
    cls, re = JOB_DETAILS[job]
    return cls(re, log=log)


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


class PushHeadCache(CacheManager):
    '''Map parent hg revisions to candidate pushheads.'''

    def __init__(self, hg, cache_dir, log=None):
        # It's not unusual to pull hundreds of changesets at once, and perhaps
        # |hg up| back and forth a few times.
        CacheManager.__init__(self, cache_dir, 'pushheads', MAX_CACHED_PARENTS, log=log)
        self._hg = hg

    @cachedmethod(operator.attrgetter('_cache'))
    def pushheads(self, tree, parent):
        pushheads = subprocess.check_output([self._hg, 'log',
            '--template', '{node}\n',
            '-r', 'last(pushhead("{tree}") and ::"{parent}", {num})'.format(
                tree=tree, parent=parent, num=NUM_PUSHHEADS_TO_QUERY_PER_PARENT)])
        pushheads = pushheads.strip().split('\n')
        return pushheads


class TaskCache(CacheManager):
    '''Map candidate pushheads to Task Cluster task IDs and artifact URLs.'''

    def __init__(self, cache_dir, log=None):
        CacheManager.__init__(self, cache_dir, 'artifact_url', MAX_CACHED_TASKS, log=log)
        self._index = taskcluster.Index()
        self._queue = taskcluster.Queue()

    @cachedmethod(operator.attrgetter('_cache'))
    def artifact_url(self, tree, job, rev):
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

        for artifact in artifact_job.find_candidate_artifacts(artifacts):
            # We can easily extract the task ID from the URL.  We can't easily
            # extract the build ID; we use the .ini files embedded in the
            # downloaded artifact for this.  We could also use the uploaded
            # public/build/buildprops.json for this purpose.
            url = self._queue.buildUrl('getLatestArtifact', taskId, artifact['name'])
            return url
        raise ValueError('Task for {key} existed, but no artifacts found!'.format(key=key))

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

    def __init__(self, tree, job, log=None, cache_dir='.', hg='hg'):
        self._tree = tree
        self._job = job
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

        self._pushhead_cache = PushHeadCache(self._hg, self._cache_dir, log=self._log)
        self._task_cache = TaskCache(self._cache_dir, log=self._log)
        self._artifact_cache = ArtifactCache(self._cache_dir, log=self._log)

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def install_from_file(self, filename, bindir, install_callback=None):
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
        ensureParentDir(mozpath.join(bindir, '.dummy'))

        with zipfile.ZipFile(processed_filename) as zf:
            for info in zf.infolist():
                if info.filename.endswith('.ini'):
                    continue
                n = mozpath.join(bindir, info.filename)
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
                if install_callback:
                    install_callback(info.filename, file_existed, file_updated)
        return 0

    def install_from_url(self, url, bindir, install_callback=None):
        self.log(logging.INFO, 'artifact',
            {'url': url},
            'Installing from {url}')
        with self._artifact_cache as artifact_cache:  # The with block handles persistence.
            filename = artifact_cache.fetch(url)
        return self.install_from_file(filename, bindir, install_callback=install_callback)

    def install_from_hg(self, revset, bindir, install_callback=None):
        if not revset:
            revset = '.'
        if len(revset) != 40:
            revset = subprocess.check_output([self._hg, 'log', '--template', '{node}\n', '-r', revset]).strip()
            if len(revset.split('\n')) != 1:
                raise ValueError('hg revision specification must resolve to exactly one commit')

        self.log(logging.INFO, 'artifact',
            {'revset': revset},
            'Installing from local revision {revset}')

        url = None
        with self._task_cache as task_cache, self._pushhead_cache as pushhead_cache:
            # with blocks handle handle persistence.
            for pushhead in pushhead_cache.pushheads(self._tree, revset):
                self.log(logging.DEBUG, 'artifact',
                    {'pushhead': pushhead},
                    'Trying to find artifacts for pushhead {pushhead}.')
                try:
                    url = task_cache.artifact_url(self._tree, self._job, pushhead)
                    self.log(logging.INFO, 'artifact',
                        {'pushhead': pushhead},
                        'Installing from remote pushhead {pushhead}')
                    break
                except ValueError:
                    pass
        if url:
            return self.install_from_url(url, bindir, install_callback=install_callback)
        self.log(logging.ERROR, 'artifact',
                 {'revset': revset},
                 'No built artifacts for {revset} found.')
        return 1

    def install_from(self, source, bindir, install_callback=None):
        """Install artifacts from a ``source`` into the given ``bindir``.

        If ``callback`` is given, it is called once with arguments ``(path,
        existed, updated)``, where ``path`` is the file path written relative
        to ``bindir``; ``existed`` is a boolean indicating whether the file
        existed; and ``updated`` is a boolean indicating whether the file was
        updated.
        """
        if source and os.path.isfile(source):
            return self.install_from_file(source, bindir, install_callback=install_callback)
        elif source and urlparse.urlparse(source).scheme:
            return self.install_from_url(source, bindir, install_callback=install_callback)
        else:
            return self.install_from_hg(source, bindir, install_callback=install_callback)

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
        self._pushhead_cache.print_cache()
        self._task_cache.print_cache()
        self._artifact_cache.print_cache()
