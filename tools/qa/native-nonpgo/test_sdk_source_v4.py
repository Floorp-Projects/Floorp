# SPDX-License-Identifier: MPL-2.0
import copy
import datetime
import gzip
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import sdk_source as sdk
import test_sdk_source_integration as legacy
from qa_core import Failure


class ExactSDKSourceTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.source = Path(directory.name) / 'source.json'
        self.plan = Path(sdk.__file__).with_name('plan.json')
        self.value = json.loads(Path(sdk.__file__).with_name('sdk_source-20261031.json').read_text())
        self.source.write_text(json.dumps(self.value))

    def test_fixed_artifact_keeps_compilation_revision_separate(self):
        spec = sdk.load_sdk_source(self.plan, self.source)
        self.assertEqual(spec['pin']['artifact'], 13011079529)
        self.assertEqual(spec['pin']['run'], 41128679337)
        self.assertEqual(spec['producerJob'], 124313694921)
        self.assertEqual(spec['runtimeRef'], '7dd619742b25567e479eeda97ae071d10542ad0b')
        self.assertEqual(spec['producerRef'], '032085d9c0176b68d405fb7d3297629df6c1a971')
        self.assertEqual(spec['qualification'], 'official-artifact-exact-profile-v2')
        self.assertEqual(spec['uncompressedTar']['sha256'],
                         'f0d4937d3793e5bd0d17ec899df993fb3b68267dab88ddaa9bdc14ccab7c4a58')
        self.assertNotEqual(spec['uncompressedTar'], sdk.UNCOMPRESSED_TAR)

    def test_every_source_field_is_bound_and_typed(self):
        def leaves(value, prefix=()):
            if type(value) is dict:
                for key, item in value.items():
                    yield from leaves(item, prefix + (key,))
            else:
                yield prefix, value
        for path, value in leaves(self.value):
            for bad in (None, True, str(value) + '-foreign'):
                changed = copy.deepcopy(self.value)
                parent = changed
                for key in path[:-1]:
                    parent = parent[key]
                parent[path[-1]] = bad
                self.source.write_text(json.dumps(changed))
                with self.subTest(path=path, bad=bad), self.assertRaises(Failure):
                    sdk.load_sdk_source(self.plan, self.source)

    def test_receipt_and_provenance_retain_exact_profile(self):
        spec = sdk.load_sdk_source(self.plan, self.source)
        receipt = sdk.acquisition_receipt(spec)
        self.assertEqual(receipt['exactProfile'], self.value['exactProfile'])
        self.assertEqual(sdk.source_provenance(spec)['exactProfile'], self.value['exactProfile'])
        self.assertEqual(sdk.validate_acquisition_receipt(receipt, spec), receipt)
        receipt['exactProfile']['producerContext']['ImageVersion'] = 'foreign'
        with self.assertRaisesRegex(Failure, 'sdk-source-acquisition-receipt'):
            sdk.validate_acquisition_receipt(receipt, spec)
        self.assertEqual(spec['exactProfile'], self.value['exactProfile'])

    def test_changed_plan_is_rejected_and_no_source_preserves_default(self):
        changed = self.source.with_name('plan.json')
        changed.write_bytes(self.plan.read_bytes() + b'\n')
        with self.assertRaisesRegex(Failure, 'sdk-source-plan-binding'):
            sdk.load_sdk_source(changed, self.source)
        self.assertEqual(sdk.load_sdk_source(self.plan)['qualification'], sdk.DEFAULT_QUALIFICATION)


class ExactSDKIntegrationTests(unittest.TestCase):
    save_source = legacy.SDKSourceIntegration.save_source
    api = legacy.SDKSourceIntegration.api
    fetch = legacy.SDKSourceIntegration.fetch
    materialize = legacy.SDKSourceIntegration.materialize

    def setUp(self):
        legacy.SDKSourceIntegration.setUp(self)
        self.now = datetime.datetime(2026, 10, 31, 3, tzinfo=datetime.timezone.utc)
        self.config.update(schema=4, qualification=sdk.EXACT_QUALIFICATION,
            producerRef='032085d9c0176b68d405fb7d3297629df6c1a971', producerJob=124313694921,
            exactProfile=copy.deepcopy(sdk.EXACT_PROFILE))
        self.config['pin'].update(artifact=13011079529, run=41128679337,
            name='macos-sdk-single-image-v4', expiresObserved='2026-11-13T21:08:48Z')
        self.save_source()
        self.exact_pin = mock.patch.object(sdk, 'EXACT_PIN', copy.deepcopy(self.config['pin']))
        self.exact_raw = mock.patch.object(sdk, 'EXACT_UNCOMPRESSED_TAR', copy.deepcopy(self.config['uncompressedTar']))
        for patch in (self.exact_pin, self.exact_raw):
            patch.start()
            self.addCleanup(patch.stop)
        pin = self.config['pin']
        self.artifact.update(id=13011079529, name=pin['name'], expires_at=pin['expiresObserved'],
            workflow_run={'id':41128679337, 'repository_id':914860957, 'head_repository_id':914860957,
                'head_branch':'b/qa-handoff-20261026', 'head_sha':self.config['producerRef']})
        self.run.update(id=41128679337, head_sha=self.config['producerRef'], conclusion='success',
            head_branch='b/qa-handoff-20261026', event='push', path=sdk.EXACT_PROFILE['producerWorkflow'],
            repository={'id':914860957, 'full_name':'Floorp-Projects/Floorp-Runtime'})
        self.producer.update(id=124313694921, run_id=41128679337, name='single-image-sdk',
            head_sha=self.config['producerRef'], labels=['macos-26'])

    def test_download_rechecks_api_and_materializes_with_profile_receipt(self):
        self.assertEqual(self.fetch(), 0)
        receipt = json.loads((self.root/'sdk-download/receipt.json').read_text())
        self.assertEqual(receipt['exactProfile'], sdk.EXACT_PROFILE)
        self.assertEqual(receipt['producerRef'], self.config['producerRef'])
        self.assertEqual(self.api_opener.open.call_count, 7)
        self.assertEqual(self.materialize(), 0)

    def test_foreign_and_malformed_lineage_is_rejected_before_download(self):
        original = tuple(copy.deepcopy(value) for value in (self.artifact, self.run, self.producer))
        cases = [('artifact','id',True), ('artifact','expired',True),
            ('artifact','expires_at','2026-11-14T21:08:48Z'), ('artifact','name','macos-sdk'),
            ('artifact','workflow_run',{'id':41128679337}),
            ('run','id',True), ('run','run_attempt',True), ('run','conclusion','failure'),
            ('run','head_sha',sdk.RUNTIME_REF), ('run','event','workflow_dispatch'),
            ('run','path','.github/workflows/other.yml'), ('run','repository',{'id':914860957}),
            ('producer','id',True), ('producer','head_sha',sdk.RUNTIME_REF),
            ('producer','run_id',41128679338), ('producer','labels',['macos-latest']),
            ('producer','name','image-snapshot'), ('producer','conclusion','failure')]
        for subject, key, bad in cases:
            self.artifact, self.run, self.producer = copy.deepcopy(original)
            getattr(self, subject)[key] = bad
            with self.subTest(subject=subject, key=key), self.assertRaises(Failure):
                self.fetch()
            self.download_opener.open.assert_not_called()

    def test_expired_artifact_is_rejected_before_download(self):
        self.now = datetime.datetime(2026, 11, 13, 21, 8, 48, tzinfo=datetime.timezone.utc)
        with self.assertRaisesRegex(Failure, 'sdk-expired'):
            self.fetch()
        self.download_opener.open.assert_not_called()

    def test_artifact_revoked_after_download_leaves_no_receipt(self):
        api, count = self.api, 0
        def changed(request, **kwargs):
            nonlocal count
            count += 1
            if count == 5:
                self.artifact['expired'] = True
            return api(request, **kwargs)
        self.api_opener.open.side_effect = changed
        with self.assertRaisesRegex(Failure, 'sdk-exact-artifact'):
            self.fetch()
        self.assertFalse((self.root/'sdk-download/receipt.json').exists())

    def test_metadata_change_after_download_leaves_no_receipt(self):
        api, count = self.api, 0
        def changed(request, **kwargs):
            nonlocal count
            count += 1
            if count == 5:
                self.artifact['updated_at'] = 'changed'
            return api(request, **kwargs)
        self.api_opener.open.side_effect = changed
        with self.assertRaisesRegex(Failure, 'sdk-exact-metadata-changed'):
            self.fetch()
        self.assertFalse((self.root/'sdk-download/receipt.json').exists())

    def test_expiry_during_download_leaves_no_receipt(self):
        opener = self.download_opener.open.side_effect
        def download(*args, **kwargs):
            self.now = datetime.datetime(2026, 11, 14, tzinfo=datetime.timezone.utc)
            return opener(*args, **kwargs)
        self.download_opener.open.side_effect = download
        with self.assertRaisesRegex(Failure, 'sdk-expired'):
            self.fetch()
        self.assertFalse((self.root/'sdk-download/receipt.json').exists())

    def test_matching_wire_hash_cannot_bypass_raw_tar_identity(self):
        path = self.root/'foreign.tar.gz'
        path.write_bytes(gzip.compress(self.raw.replace(b'Version', b'VERSION'), mtime=2))
        spec = sdk.load_sdk_source(self.plan_path, self.source_path)
        with self.assertRaisesRegex(Failure, 'sdk-source-uncompressed-sha256'):
            sdk.verify_uncompressed_tar(path, spec)


if __name__ == '__main__':
    unittest.main()
