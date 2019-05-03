# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Fetch and cache artifacts from URLs.

This module manages fetching artifacts from URLS and purging old
artifacts using a simple Least Recently Used cache.

This module requires certain modules be importable from the ambient Python
environment.  Consumers will need to arrange this themselves.

The bulk of the complexity is in managing and persisting several caches.  If
we found a Python LRU cache that pickled cleanly, we could remove a lot of
this code!  Sadly, I found no such candidate implementations, so we pickle
pylru caches manually.

None of the instances (or the underlying caches) are safe for concurrent use.
A future need, perhaps.
'''


from __future__ import absolute_import, print_function, unicode_literals

import binascii
import hashlib
import logging
import os
import urlparse

from mozbuild.util import (
    mkdir,
)
import mozpack.path as mozpath
from dlmanager import (
    DownloadManager,
    PersistLimit,
)


# Minimum number of downloaded artifacts to keep. Each artifact can be very large,
# so don't make this to large!
MIN_CACHED_ARTIFACTS = 6

# Maximum size of the downloaded artifacts to keep in cache, in bytes (1GiB).
MAX_CACHED_ARTIFACTS_SIZE = 1024 * 1024 * 1024


class ArtifactPersistLimit(PersistLimit):
    '''Handle persistence for a cache of artifacts.

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
                path.endswith('.checksum') or \
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
            try:
                fs.remove(f.path)
            except WindowsError:
                # For some reason, on automation, we can't remove those files.
                # So for now, ignore the error.
                kept.append(f)
                continue
            self.log(
                logging.INFO,
                'artifact',
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
    '''Fetch artifacts from URLS and purge least recently used artifacts from disk.'''

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
            # Strip query string and fragments.
            basename = os.path.basename(urlparse.urlparse(url).path)
            fname = hash + '-' + basename

        path = os.path.abspath(mozpath.join(self._cache_dir, fname))
        if self._skip_cache and os.path.exists(path):
            self.log(
                logging.INFO,
                'artifact',
                {'path': path},
                'Skipping cache: removing cached downloaded artifact {path}')
            os.remove(path)

        self.log(
            logging.INFO,
            'artifact',
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
                         {'bytes_so_far': bytes_so_far,
                          'total_size': total_size,
                          'percent': percent},
                         'Downloading... {percent:02.1f} %')

            if dl:
                dl.set_progress(download_progress)
                dl.wait()
            else:
                # Avoid the file being removed if it was in the cache already.
                path = os.path.join(self._cache_dir, fname)
                self._persist_limit.register_file(path)

            self.log(
                logging.INFO,
                'artifact',
                {'path': os.path.abspath(mozpath.join(self._cache_dir, fname))},
                'Downloaded artifact to {path}')
            return os.path.abspath(mozpath.join(self._cache_dir, fname))
        finally:
            # Cancel any background downloads in progress.
            self._download_manager.cancel()

    def clear_cache(self):
        if self._skip_cache:
            self.log(
                logging.INFO,
                'artifact',
                {},
                'Skipping cache: ignoring clear_cache!')
            return

        self._persist_limit.remove_all()
