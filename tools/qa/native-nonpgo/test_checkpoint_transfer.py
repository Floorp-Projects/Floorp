# SPDX-License-Identifier: MPL-2.0
import contextlib
import datetime
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest import mock
import urllib.error

import checkpoint_transfer as transfer
from support import Failure


class TransferGuards(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.task = Path(self.temporary.name)
        (self.task / 'started-monotonic').write_text(str(time.monotonic()))
        self.data = gzip.compress(b'raw checkpoint fixture')
        self.sha = hashlib.sha256(self.data).hexdigest()
        self.env = mock.patch.dict(os.environ, {'GITHUB_REPOSITORY': 'Floorp-Projects/Floorp-Runtime',
            'GITHUB_RUN_ID': '1234', 'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_EVENT_NAME': 'push',
            'GITHUB_SHA': 'a' * 40, 'CHECKPOINT_READ_TOKEN': 'TEST-ONLY',
            'CHECKPOINT_ARTIFACT_ID': '4567', 'CHECKPOINT_ARCHIVE_SHA256': self.sha,
            'CHECKPOINT_MANIFEST_SHA256': 'b' * 64, 'CHECKPOINT_ARCHIVE_BYTES': str(len(self.data))})
        self.env.start()
        self.addCleanup(self.env.stop)
        self.run = {'head_sha': 'a' * 40, 'run_attempt': 1, 'event': 'push'}
        self.jobs = {'total_count': 2, 'jobs': [{'id': 1, 'name': 'checkpoint-producer',
                                               'conclusion': 'success'}]}
        self.artifact = {'id': 4567, 'name': 'checkpoint.tar.gz', 'expired': False,
                         'workflow_run': {'id': 1234}, 'size_in_bytes': len(self.data),
                         'digest': 'sha256:' + self.sha}
        self.requests = []
        self.free = 200 * 1024**3

    def metadata(self, request, timeout):
        url = request.full_url
        self.requests.append(url)
        if url.endswith('/zip'):
            raise urllib.error.HTTPError(url, 302, 'redirect', {'Location': 'https://fixture.invalid/raw'}, None)
        if url.endswith('/runs/1234'):
            value = self.run
        elif url.endswith('/runs/1234/jobs'):
            value = self.jobs
        elif url.endswith('/artifacts/4567'):
            value = self.artifact
        else:
            raise AssertionError('unexpected metadata path')
        return io.BytesIO(json.dumps(value).encode())

    def invoke(self):
        metadata = mock.Mock()
        metadata.open.side_effect = self.metadata
        raw = mock.Mock()
        raw.open.return_value = io.BytesIO(self.data)
        with mock.patch('sys.argv', ['checkpoint_transfer.py', 'fetch', '--role', 'consumer',
                                     '--task', str(self.task)]), \
                mock.patch('urllib.request.build_opener', side_effect=[metadata, raw]), \
                mock.patch('checkpoint_transfer.shutil.disk_usage', return_value=mock.Mock(free=self.free)), \
                contextlib.redirect_stdout(io.StringIO()):
            transfer.main()
        return raw

    def test_raw_rest_artifact_is_bound_to_expected_digest_and_successful_producer(self):
        raw = self.invoke()
        self.assertEqual((self.task / 'checkpoint.tar.gz').read_bytes(), self.data)
        receipt = json.loads((self.task / 'checkpoint-download.json').read_text())
        self.assertEqual(receipt['sha256'], self.sha)
        self.assertEqual(receipt['producerJob'], 1)
        raw.open.assert_called_once_with('https://fixture.invalid/raw', timeout=30)

    def test_expired_wrong_run_oversized_or_digest_mismatch_never_downloads(self):
        original = dict(self.artifact)
        for changes in ({'expired': True}, {'workflow_run': {'id': 9999}},
                        {'size_in_bytes': 10 * 1024**3 + 1}, {'digest': 'sha256:' + '0' * 64}):
            self.artifact = {**original, **changes}
            self.requests.clear()
            with self.subTest(changes=changes), self.assertRaises(Failure):
                self.invoke()
            self.assertFalse(any(url.endswith('/zip') for url in self.requests))
            self.assertFalse((self.task / 'checkpoint.tar.gz').exists())

    def test_failed_producer_prevents_download(self):
        self.jobs['jobs'][0]['conclusion'] = 'failure'
        with self.assertRaisesRegex(Failure, 'checkpoint-producer-job'):
            self.invoke()
        self.assertFalse(any(url.endswith('/zip') for url in self.requests))

    def test_disk_reserve_prevents_download(self):
        self.free = 8 * 1024**3
        with self.assertRaisesRegex(Failure, 'checkpoint-download-disk'):
            self.invoke()
        self.assertFalse(any(url.endswith('/zip') for url in self.requests))
        self.assertFalse((self.task / 'checkpoint.tar.gz').exists())

    def test_http_error_is_not_coerced_to_empty_artifact(self):
        with mock.patch.object(self, 'metadata', side_effect=urllib.error.HTTPError(
                'https://api.github.com/fixture', 403, 'forbidden', {}, None)):
            with self.assertRaises(urllib.error.HTTPError):
                self.invoke()
        self.assertFalse((self.task / 'checkpoint-download.json').exists())


if __name__ == '__main__':
    unittest.main()
