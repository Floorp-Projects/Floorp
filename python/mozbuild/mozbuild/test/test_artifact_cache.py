# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import mozunit
import time
import unittest
from tempfile import mkdtemp
from shutil import rmtree

from mozbuild.artifact_cache import ArtifactCache
from mozbuild import artifact_cache


CONTENTS = {
    'http://server/foo': b'foo',
    'http://server/bar': b'bar' * 400,
    'http://server/qux': b'qux' * 400,
    'http://server/fuga': b'fuga' * 300,
    'http://server/hoge': b'hoge' * 300,
    'http://server/larger': b'larger' * 3000,
}

class FakeResponse(object):
    def __init__(self, content):
        self._content = content

    @property
    def headers(self):
        return {
            'Content-length': str(len(self._content))
        }

    def iter_content(self, chunk_size):
        content = memoryview(self._content)
        while content:
            yield content[:chunk_size]
            content = content[chunk_size:]

    def raise_for_status(self):
        pass

    def close(self):
        pass


class FakeSession(object):
    def get(self, url, stream=True):
        assert stream is True
        return FakeResponse(CONTENTS[url])


class TestArtifactCache(unittest.TestCase):
    def setUp(self):
        self.min_cached_artifacts = artifact_cache.MIN_CACHED_ARTIFACTS
        self.max_cached_artifacts_size = artifact_cache.MAX_CACHED_ARTIFACTS_SIZE
        artifact_cache.MIN_CACHED_ARTIFACTS = 2
        artifact_cache.MAX_CACHED_ARTIFACTS_SIZE = 4096

        self._real_utime = os.utime
        os.utime = self.utime
        self.timestamp = time.time() - 86400

        self.tmpdir = mkdtemp()

    def tearDown(self):
        rmtree(self.tmpdir)
        artifact_cache.MIN_CACHED_ARTIFACTS = self.min_cached_artifacts
        artifact_cache.MAX_CACHED_ARTIFACTS_SIZE = self.max_cached_artifacts_size
        os.utime = self._real_utime

    def utime(self, path, times):
        if times is None:
            # Ensure all downloaded files have a different timestamp
            times = (self.timestamp, self.timestamp)
            self.timestamp += 2
        self._real_utime(path, times)

    def listtmpdir(self):
        return [p for p in os.listdir(self.tmpdir)
                if p != '.metadata_never_index']

    def test_artifact_cache_persistence(self):
        cache = ArtifactCache(self.tmpdir)
        cache._download_manager.session = FakeSession()

        path = cache.fetch('http://server/foo')
        expected = [os.path.basename(path)]
        self.assertEqual(self.listtmpdir(), expected)

        path = cache.fetch('http://server/bar')
        expected.append(os.path.basename(path))
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))

        # We're downloading more than the cache allows us, but since it's all
        # in the same session, no purge happens.
        path = cache.fetch('http://server/qux')
        expected.append(os.path.basename(path))
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))

        path = cache.fetch('http://server/fuga')
        expected.append(os.path.basename(path))
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))

        cache = ArtifactCache(self.tmpdir)
        cache._download_manager.session = FakeSession()

        # Downloading a new file in a new session purges the oldest files in
        # the cache.
        path = cache.fetch('http://server/hoge')
        expected.append(os.path.basename(path))
        expected = expected[2:]
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))

        # Downloading a file already in the cache leaves the cache untouched
        cache = ArtifactCache(self.tmpdir)
        cache._download_manager.session = FakeSession()

        path = cache.fetch('http://server/qux')
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))

        # bar was purged earlier, re-downloading it should purge the oldest
        # downloaded file, which at this point would be qux, but we also
        # re-downloaded it in the mean time, so the next one (fuga) should be
        # the purged one.
        cache = ArtifactCache(self.tmpdir)
        cache._download_manager.session = FakeSession()

        path = cache.fetch('http://server/bar')
        expected.append(os.path.basename(path))
        expected = [p for p in expected if 'fuga' not in p]
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))

        # Downloading one file larger than the cache size should still leave
        # MIN_CACHED_ARTIFACTS files.
        cache = ArtifactCache(self.tmpdir)
        cache._download_manager.session = FakeSession()

        path = cache.fetch('http://server/larger')
        expected.append(os.path.basename(path))
        expected = expected[-2:]
        self.assertEqual(sorted(self.listtmpdir()), sorted(expected))


if __name__ == '__main__':
    mozunit.main()
