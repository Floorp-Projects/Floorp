# SPDX-License-Identifier: MPL-2.0
import copy
import datetime
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import plistlib
import sys
import tarfile
import tempfile
import unittest
import urllib.error
import zipfile
from unittest import mock

import fetch_sdk
import materialize
import census_experiment
import sdk_source
from sdk_source import load_sdk_source, acquisition_receipt
from support import Failure, digest, scan_tar


# Independently recorded normal API origins, not loader-derived expectations.
KNOWN_SDK_ORIGINS = {
    12182302735: {'artifact': 12182302735, 'run': 39244172146,
                  'producerJob': 118032017226,
                  'producerName': 'Mac build [PGO] / download-macos-sdk'},
    12225585543: {'artifact': 12225585543, 'run': 39332628358,
                  'producerJob': 118334374715,
                  'producerName': 'Mac build / download-macos-sdk'},
}


class SDKSourceIntegration(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        raw = io.BytesIO()
        with tarfile.open(fileobj=raw, mode='w') as archive:
            data = plistlib.dumps({'Version': self.plan['sdk']['version']})
            member = tarfile.TarInfo('MacOSX.sdk/SDKSettings.plist')
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        self.raw = raw.getvalue()
        old_tar, self.new_tar = gzip.compress(self.raw, mtime=1), gzip.compress(self.raw, mtime=2)
        self.assertNotEqual(old_tar, self.new_tar)
        pin = self.plan['sdk']
        tar_path = self.root / pin['entry']
        tar_path.write_bytes(old_tar)
        def zipped(content):
            target = io.BytesIO()
            with zipfile.ZipFile(target, 'w') as archive:
                archive.writestr(pin['entry'], content)
            return target.getvalue()
        old_zip, self.new_zip = zipped(old_tar), zipped(self.new_tar)
        sha = lambda data: hashlib.sha256(data).hexdigest()
        pin.update(tarBytes=len(old_tar), tarSHA256=sha(old_tar), zipBytes=len(old_zip),
                   zipSHA256=sha(old_zip), draftReaderTreeSHA256=scan_tar(tar_path)['canonicalSHA256'])
        tar_path.unlink()
        self.plan_path = self.root / 'plan.json'
        self.plan_path.write_text(json.dumps(self.plan))
        self.config = json.loads(Path(__file__).with_name('sdk_source.json').read_text())
        self.expected_origin = KNOWN_SDK_ORIGINS[self.config['pin']['artifact']]
        self.assertEqual({'artifact': self.config['pin']['artifact'],
                          'run': self.config['pin']['run'],
                          'producerJob': self.config['producerJob'],
                          'producerName': sdk_source.PRODUCER_NAME}, self.expected_origin)
        self.config.update(baselinePlanSHA256=digest(self.plan_path), baselineTarSHA256=pin['tarSHA256'])
        self.config['pin'].update({key: pin[key] for key in
            ('name', 'entry', 'tarBytes', 'version', 'draftReaderTreeSHA256')})
        self.config['pin'].update(zipBytes=len(self.new_zip), zipSHA256=sha(self.new_zip),
                                  tarSHA256=sha(self.new_tar))
        self.config['uncompressedTar'] = {'bytes': len(self.raw), 'sha256': sha(self.raw)}
        self.source_path = self.root / 'source.json'
        self.save_source()
        anchors = mock.patch.multiple(sdk_source, BASELINE_PLAN_SHA256=digest(self.plan_path),
            BASELINE_TAR_SHA256=pin['tarSHA256'], PIN=copy.deepcopy(self.config['pin']),
            UNCOMPRESSED_TAR=copy.deepcopy(self.config['uncompressedTar']))
        anchors.start()
        self.addCleanup(anchors.stop)
        self.now = datetime.datetime(2026, 10, 19, 12, tzinfo=datetime.timezone.utc)
        current = self.config['pin']
        self.artifact = {'id': current['artifact'], 'name': current['name'],
                         'size_in_bytes': current['zipBytes'], 'digest': 'sha256:' + current['zipSHA256'],
                         'expired': False, 'workflow_run': {'id': current['run']},
                         'expires_at': (self.now + datetime.timedelta(hours=1)).isoformat()}
        self.run = {'run_attempt': current['attempt'], 'head_sha': self.plan['runtime']['ref'],
                    'status': 'completed', 'conclusion': 'failure'}
        self.producer = {'id': self.config['producerJob'], 'run_id': current['run'],
                         'run_attempt': current['attempt'], 'name': self.expected_origin['producerName'],
                         'status': 'completed', 'conclusion': 'success'}
        self.api_opener, self.download_opener = mock.Mock(), mock.Mock()
        self.api_opener.open.side_effect = self.api
        self.download_opener.open.side_effect = lambda *_a, **_k: io.BytesIO(self.new_zip)

    def save_source(self):
        self.source_path.write_text(json.dumps(self.config))

    def api(self, request, **_kwargs):
        url = request.full_url
        self.assertTrue(url.startswith('https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/actions/'))
        if url.endswith('/zip'):
            raise urllib.error.HTTPError(url, 302, 'redirect', {'Location': 'https://example.invalid/sdk.zip'}, None)
        value = self.producer if '/jobs/' in url else self.run if '/runs/' in url else self.artifact
        return io.BytesIO(json.dumps(value).encode())

    def fetch(self, *, alternate=True):
        argv = ['fetch_sdk.py', '--plan', str(self.plan_path), '--output', str(self.root / 'sdk-download')]
        if alternate:
            argv += ['--sdk-source', str(self.source_path)]
        case = self
        class Clock(datetime.datetime):
            @classmethod
            def now(cls, _tz=None):
                return case.now
        with mock.patch.dict(os.environ, {'SDK_READ_TOKEN': 'test-only-placeholder'}), \
                mock.patch.object(sys, 'argv', argv), mock.patch('fetch_sdk.datetime.datetime', Clock), \
                mock.patch('fetch_sdk.urllib.request.build_opener',
                           side_effect=[self.api_opener, self.download_opener]):
            return fetch_sdk.main()

    def materialize(self):
        with mock.patch('materialize.__file__', str(self.root / 'materialize.py')), \
                mock.patch.object(sys, 'argv', ['materialize.py', 'sdk', '--task', str(self.root),
                                              '--sdk-source', str(self.source_path)]), \
                mock.patch('urllib.request.build_opener', side_effect=AssertionError('unexpected network')):
            return materialize.main()

    def test_explicit_source_acquires_new_wire_bytes_and_retains_actual_provenance(self):
        self.assertEqual(self.fetch(), 0)
        acquired = json.loads((self.root / 'sdk-download/receipt.json').read_text())
        self.assertEqual(acquired, acquisition_receipt(load_sdk_source(self.plan_path, self.source_path)))
        self.assertEqual(acquired['artifact'], self.config['pin']['artifact'])
        self.assertNotEqual(acquired['artifact'], self.plan['sdk']['artifact'])
        self.assertNotEqual(acquired['tarSHA256'], self.plan['sdk']['tarSHA256'])
        self.assertEqual(acquired['uncompressedTar'], self.config['uncompressedTar'])
        self.assertEqual(self.api_opener.open.call_count, 4)
        self.assertEqual(self.run['conclusion'], 'failure')
        self.assertEqual(self.producer['conclusion'], 'success')

    def configure_current_origin(self):
        self.config.update(schema=2,producerRef=sdk_source.CURRENT_PRODUCER_REF,
                           producerJob=sdk_source.CURRENT_PRODUCER_JOB)
        self.config['pin'].update(artifact=12624490543,run=40259426572,
                                  expiresObserved='2026-10-27T02:31:18Z')
        self.save_source()
        current=self.config['pin']
        self.artifact.update(id=current['artifact'],workflow_run={'id':current['run']})
        self.run['head_sha']=self.config['producerRef']
        self.producer.update(id=self.config['producerJob'],run_id=current['run'],
                            name='Mac build [PGO] / download-macos-sdk')
        return mock.patch.object(sdk_source,'CURRENT_PIN',copy.deepcopy(current))

    def test_current_sdk_uses_producer_head_without_changing_compilation_source(self):
        with self.configure_current_origin():
            self.assertEqual(self.fetch(),0)
            spec=load_sdk_source(self.plan_path,self.source_path)
            self.assertEqual(spec['runtimeRef'],self.plan['runtime']['ref'])
            self.assertNotEqual(spec['runtimeRef'],spec['producerRef'])
            receipt=json.loads((self.root/'sdk-download/receipt.json').read_text())
            self.assertEqual(receipt,acquisition_receipt(spec))
            self.assertEqual(receipt['producerRef'],'0ac255739cdd64e100cd6913329247421b0ec2ea')
            self.assertEqual(self.materialize(),0)
            self.assertEqual(self.api_opener.open.call_count,4)

    def configure_recovered_origin(self):
        self.config.update(schema=3, producerRef='a2bceca6e076c130cb133e490e0f7c5f50f34c30',
                           producerJob=122231898010)
        self.config['pin'].update(artifact=12739697223, run=40516581212,
                                 expiresObserved='2026-10-29T13:57:37Z')
        self.save_source()
        pin = self.config['pin']
        self.artifact.update(id=pin['artifact'], workflow_run={'id':pin['run']})
        self.run.update(head_sha=self.config['producerRef'], conclusion='success')
        self.producer.update(id=122231898010, run_id=40516581212, name='download-macos-sdk')
        return mock.patch.object(sdk_source, 'RECOVERED_PIN', copy.deepcopy(pin))

    def test_recovered_sdk_download_and_materialization_bind_real_producer_identity(self):
        with self.configure_recovered_origin():
            self.assertEqual(self.fetch(), 0)
            spec = load_sdk_source(self.plan_path, self.source_path)
            self.assertEqual(spec['runtimeRef'], self.plan['runtime']['ref'])
            self.assertNotEqual(spec['runtimeRef'], spec['producerRef'])
            receipt = json.loads((self.root/'sdk-download/receipt.json').read_text())
            self.assertEqual(receipt, acquisition_receipt(spec))
            self.assertEqual(receipt['artifact'], 12739697223)
            self.assertEqual(receipt['run'], 40516581212)
            self.assertEqual(receipt['producerRef'], 'a2bceca6e076c130cb133e490e0f7c5f50f34c30')
            self.assertEqual(self.materialize(), 0)
            self.assertEqual(self.api_opener.open.call_count, 4)

    def test_recovered_sdk_rejects_foreign_producer_run_head_and_expiry_before_download(self):
        with self.configure_recovered_origin():
            original_run, original_producer = copy.deepcopy(self.run), copy.deepcopy(self.producer)
            for subject, key, bad, code in (
                    ('run', 'head_sha', self.plan['runtime']['ref'], 'sdk-run-provenance'),
                    ('run', 'run_attempt', 2, 'sdk-run-provenance'),
                    ('producer', 'run_id', 40505081896, 'sdk-producer-provenance'),
                    ('producer', 'name', 'Mac build [PGO] / download-macos-sdk', 'sdk-producer-provenance'),
                    ('producer', 'conclusion', 'failure', 'sdk-producer-provenance')):
                self.run, self.producer = copy.deepcopy(original_run), copy.deepcopy(original_producer)
                getattr(self, subject)[key] = bad
                with self.subTest(subject=subject, key=key), self.assertRaisesRegex(Failure, code): self.fetch()
                self.download_opener.open.assert_not_called()
                self.assertFalse((self.root/'sdk-download').exists())
            self.run, self.producer = original_run, original_producer
            self.artifact['expires_at'] = (self.now-datetime.timedelta(seconds=1)).isoformat()
            with self.assertRaisesRegex(Failure, 'sdk-expired'): self.fetch()
            self.download_opener.open.assert_not_called()

    def test_current_sdk_rejects_wrong_run_head_attempt_producer_and_expiry_before_download(self):
        with self.configure_current_origin():
            current_run=copy.deepcopy(self.run);current_producer=copy.deepcopy(self.producer)
            variants=[('run','head_sha',self.plan['runtime']['ref'],'sdk-run-provenance'),
                      ('run','run_attempt',True,'sdk-run-provenance'),
                      ('producer','id',True,'sdk-producer-provenance'),
                      ('producer','name','Mac build / download-macos-sdk','sdk-producer-provenance'),
                      ('producer','conclusion','failure','sdk-producer-provenance')]
            for subject,key,bad,code in variants:
                self.run=copy.deepcopy(current_run);self.producer=copy.deepcopy(current_producer)
                getattr(self,subject)[key]=bad
                with self.subTest(subject=subject,key=key),self.assertRaisesRegex(Failure,code):self.fetch()
                self.download_opener.open.assert_not_called()
                self.assertFalse((self.root/'sdk-download').exists())
            self.run=current_run;self.producer=current_producer
            self.artifact['expires_at']=(self.now-datetime.timedelta(seconds=1)).isoformat()
            with self.assertRaisesRegex(Failure,'sdk-expired'):self.fetch()
            self.download_opener.open.assert_not_called()

    def test_no_opt_in_keeps_original_artifact_guard(self):
        with self.assertRaisesRegex(Failure, 'sdk-artifact-provenance'):
            self.fetch(alternate=False)
        self.download_opener.open.assert_not_called()

    def test_current_official_api_exact_producer_name_acquires_through_mocked_download(self):
        # Both fixed origins use tiny synthetic archives. This exercises a future
        # pin transition; it does not qualify the new real SDK payload or TAR hash.
        original_root = self.root
        original = copy.deepcopy((self.config, self.artifact, self.producer, self.run))
        try:
            for expected in KNOWN_SDK_ORIGINS.values():
                with self.subTest(artifact=expected['artifact']):
                    self.root = original_root / str(expected['artifact'])
                    self.root.mkdir()
                    self.config = copy.deepcopy(original[0])
                    self.config['pin'].update(artifact=expected['artifact'], run=expected['run'])
                    self.config['producerJob'] = expected['producerJob']
                    self.save_source()
                    self.artifact = {**original[1], 'id': expected['artifact'],
                                     'workflow_run': {'id': expected['run']}}
                    self.producer = {**original[2], 'id': expected['producerJob'],
                                     'run_id': expected['run'], 'name': expected['producerName']}
                    self.run = {**original[3], 'conclusion': 'success'}
                    self.api_opener.open.reset_mock()
                    self.download_opener.open.reset_mock()
                    with mock.patch.multiple(sdk_source, PIN=copy.deepcopy(self.config['pin']),
                            PRODUCER_JOB=expected['producerJob'], PRODUCER_NAME=expected['producerName']):
                        trusted = load_sdk_source(self.plan_path, self.source_path)
                        self.assertEqual({'artifact': trusted['pin']['artifact'], 'run': trusted['pin']['run'],
                            'producerJob': trusted['producerJob'], 'producerName': trusted['producerName']}, expected)
                        self.assertEqual(self.fetch(), 0)
                        self.download_opener.open.assert_called_once()
                        receipt = json.loads((self.root / 'sdk-download/receipt.json').read_text())
                        self.assertEqual(receipt['tarSHA256'], hashlib.sha256(self.new_tar).hexdigest())
                        self.assertEqual(receipt['artifact'], expected['artifact'])
                        other_names = [row['producerName'] for row in KNOWN_SDK_ORIGINS.values()
                                       if row['artifact'] != expected['artifact']]
                        for name in (*other_names, 'Foreign / download-macos-sdk',
                                     expected['producerName'] + ' extra', True, None):
                            self.producer['name'] = name
                            with self.assertRaisesRegex(Failure, 'sdk-producer-provenance'):
                                self.fetch()
                        self.producer.pop('name')
                        with self.assertRaisesRegex(Failure, 'sdk-producer-provenance'):
                            self.fetch()
                        self.download_opener.open.assert_called_once()
        finally:
            self.root = original_root
            self.config, self.artifact, self.producer, self.run = original
            self.save_source()

    def test_old_foreign_boolean_and_missing_producer_names_stop_before_download(self):
        other_names = [row['producerName'] for row in KNOWN_SDK_ORIGINS.values()
                       if row['artifact'] != self.expected_origin['artifact']]
        for name in (*other_names, 'Foreign / download-macos-sdk',
                     self.expected_origin['producerName'] + ' extra', True, None):
            with self.subTest(name=name):
                self.producer['name'] = name
                with self.assertRaisesRegex(Failure, 'sdk-producer-provenance'):
                    self.fetch()
        self.producer.pop('name')
        with self.assertRaisesRegex(Failure, 'sdk-producer-provenance'):
            self.fetch()
        self.download_opener.open.assert_not_called()

    def test_producer_name_binding_does_not_relax_runtime_head_or_run_attempt(self):
        for change in ({'head_sha': '0' * 40}, {'run_attempt': 2}):
            original = copy.deepcopy(self.run)
            self.run.update(change)
            with self.subTest(change=change), self.assertRaisesRegex(Failure, 'sdk-run-provenance'):
                self.fetch()
            self.run = original
        self.download_opener.open.assert_not_called()

    def test_new_source_expiry_is_checked_before_producer_or_download(self):
        self.artifact.update(expires_at=self.now.isoformat())
        with self.assertRaisesRegex(Failure, 'sdk-expired'):
            self.fetch()
        self.assertEqual(self.api_opener.open.call_count, 2)
        self.download_opener.open.assert_not_called()

    def test_failed_producer_stops_before_download(self):
        self.producer['conclusion'] = 'failure'
        with self.assertRaisesRegex(Failure, 'sdk-producer-provenance'):
            self.fetch()
        self.download_opener.open.assert_not_called()

    def test_expiry_during_producer_check_stops_before_zip_request(self):
        normal_api = self.api
        def advanced(request, **kwargs):
            value = normal_api(request, **kwargs)
            if '/jobs/' in request.full_url:
                self.now += datetime.timedelta(hours=1)
            return value
        self.api_opener.open.side_effect = advanced
        with self.assertRaisesRegex(Failure, 'sdk-expired'):
            self.fetch()
        self.assertEqual(self.api_opener.open.call_count, 3)
        self.download_opener.open.assert_not_called()
        self.assertFalse((self.root / 'sdk-download').exists())

    def test_foreign_producer_run_or_boolean_identity_stops_download(self):
        for change in ({'run_id': 1}, {'run_attempt': True}, {'id': True}, {'status': 'in_progress'}):
            original = copy.deepcopy(self.producer)
            self.producer.update(change)
            with self.subTest(change=change), self.assertRaisesRegex(Failure, 'sdk-producer-provenance'):
                self.fetch()
            self.producer = original
        self.download_opener.open.assert_not_called()

    def test_producer_http_error_never_falls_back(self):
        normal_api = self.api
        def failed(request, **kwargs):
            if '/jobs/' in request.full_url:
                raise urllib.error.HTTPError('https://api.github.com/', 403, 'denied', {}, None)
            return normal_api(request, **kwargs)
        self.api_opener.open.side_effect = failed
        with self.assertRaises(urllib.error.HTTPError):
            self.fetch()
        self.download_opener.open.assert_not_called()

    def test_new_wire_zip_hash_is_enforced(self):
        self.download_opener.open.side_effect = lambda *_a, **_k: io.BytesIO(b'x' * len(self.new_zip))
        with self.assertRaisesRegex(Failure, 'download-hash-mismatch'):
            self.fetch()
        self.assertFalse((self.root / 'sdk-download/receipt.json').exists())

    def test_new_compressed_tar_hash_is_enforced(self):
        self.config['pin']['tarSHA256'] = '0' * 64
        self.save_source()
        with mock.patch.dict(sdk_source.PIN, {'tarSHA256': '0' * 64}), \
                self.assertRaisesRegex(Failure, 'sdk-entry-integrity'):
            self.fetch()
        self.assertFalse((self.root / 'sdk-download/receipt.json').exists())

    def test_uncompressed_anchor_failure_never_writes_success_receipt(self):
        self.config['uncompressedTar']['sha256'] = '0' * 64
        self.save_source()
        with mock.patch.dict(sdk_source.UNCOMPRESSED_TAR, {'sha256': '0' * 64}), \
                self.assertRaisesRegex(Failure, 'sdk-source-uncompressed-sha256'):
            self.fetch()
        self.assertFalse((self.root / 'sdk-download/receipt.json').exists())

    def test_verified_local_new_source_materializes_after_service_expiry(self):
        self.assertEqual(self.fetch(), 0)
        self.artifact.update(expired=True, expires_at=(self.now-datetime.timedelta(hours=1)).isoformat())
        self.assertEqual(self.materialize(), 0)
        proof = json.loads((self.root / 'sdk-materialized.json').read_text())
        self.assertEqual(proof['canonicalSHA256'], self.plan['sdk']['draftReaderTreeSHA256'])
        self.assertEqual(plistlib.loads((Path(proof['path'])/'SDKSettings.plist').read_bytes())['Version'], '26.5')

    def test_materialize_rejects_receipt_from_baseline_source(self):
        self.assertEqual(self.fetch(), 0)
        path = self.root / 'sdk-download/receipt.json'
        path.write_text(json.dumps(acquisition_receipt(load_sdk_source(self.plan_path))))
        with self.assertRaises(Failure):
            self.materialize()
        self.assertFalse((self.root / 'sdk-extracted').exists())

    def test_materialize_rechecks_new_wire_hash_before_extraction(self):
        self.assertEqual(self.fetch(), 0)
        (self.root / 'sdk-download' / self.plan['sdk']['entry']).write_bytes(b'changed')
        with self.assertRaisesRegex(Failure, 'sdk-file-hash'):
            self.materialize()
        self.assertFalse((self.root / 'sdk-extracted').exists())

    def test_census_uses_same_source_and_reports_actual_sdk(self):
        self.assertEqual(self.fetch(), 0)
        runner = mock.Mock()
        def materialize_stage(stage, command, *_args, **_kwargs):
            self.assertEqual(stage, 'sdk-materialize')
            self.assertEqual(command[-2:], ['--sdk-source', str(self.source_path.resolve())])
            self.assertEqual(self.materialize(), 0)
        runner.run.side_effect = materialize_stage
        result = {'sdkAcquired': None}
        with mock.patch.object(census_experiment, 'HERE', self.root):
            census_experiment.verify_sdk_acquisition(self.root, runner, {}, result, self.source_path)
        self.assertIs(result['sdkAcquired'], True)
        self.assertEqual(result['sdkVerification']['artifact'], self.config['pin']['artifact'])
        self.assertEqual(result['sdkVerification']['tarSHA256'], self.config['pin']['tarSHA256'])
        self.assertEqual(result['sdkVerification']['acquisitionSource']['uncompressedTar'], self.config['uncompressedTar'])

    def test_census_rejects_foreign_receipt_before_materialize(self):
        self.assertEqual(self.fetch(), 0)
        target = self.root / 'sdk-download/receipt.json'
        target.write_text(json.dumps(acquisition_receipt(load_sdk_source(self.plan_path))))
        runner, result = mock.Mock(), {'sdkAcquired': None}
        with mock.patch.object(census_experiment, 'HERE', self.root), self.assertRaises(Failure):
            census_experiment.verify_sdk_acquisition(self.root, runner, {}, result, self.source_path)
        runner.run.assert_not_called()
        self.assertIsNone(result['sdkAcquired'])

    def test_census_source_requires_explicit_sdk_verification_mode(self):
        with mock.patch.object(sys, 'argv', ['census_experiment.py', '--task', str(self.root),
            '--expected-tests', '1', '--sdk-source', str(self.source_path)]), \
                self.assertRaisesRegex(Failure, 'sdk-source-mode'):
            census_experiment.main()

    def test_metadata_time_is_inside_the_same_600_second_acquisition_budget(self):
        normal_api=self.api
        def delayed(request,**kwargs):
            response=normal_api(request,**kwargs)
            clock['now']=600
            return response
        clock={'now':0}
        self.api_opener.open.side_effect=delayed
        with mock.patch.object(fetch_sdk.time,'monotonic',side_effect=lambda:clock['now']):
            with self.assertRaisesRegex(Failure,'^download-deadline$'):self.fetch()
        self.download_opener.open.assert_not_called()
        self.assertFalse((self.root/'sdk-download').exists())

    def test_payload_does_not_restart_the_budget_after_metadata(self):
        clock={'now':0};normal_api=self.api
        def delayed(request,**kwargs):
            try:return normal_api(request,**kwargs)
            finally:clock['now']+=100
        class Payload(io.BytesIO):
            def read(stream,size=-1):
                data=super().read(size);clock['now']+=201;return data
        self.api_opener.open.side_effect=delayed
        self.download_opener.open.side_effect=lambda *_a,**_k:Payload(self.new_zip)
        with mock.patch.object(fetch_sdk.time,'monotonic',side_effect=lambda:clock['now']):
            with self.assertRaisesRegex(Failure,'^download-deadline$'):self.fetch()
        self.assertEqual(self.api_opener.open.call_count,4)
        self.assertFalse((self.root/'sdk-download/receipt.json').exists())

    def test_payload_eof_after_deadline_is_not_success(self):
        clock={'now':0}
        class Payload(io.BytesIO):
            def read(stream,size=-1):
                data=super().read(size);clock['now']=599 if data else 601;return data
        self.download_opener.open.side_effect=lambda *_a,**_k:Payload(self.new_zip)
        with mock.patch.object(fetch_sdk.time,'monotonic',side_effect=lambda:clock['now']):
            with self.assertRaisesRegex(Failure,'^download-deadline$'):self.fetch()
        self.assertFalse((self.root/'sdk-download/receipt.json').exists())

    def test_redirect_does_not_permit_acquisition_to_start_after_expiry(self):
        normal_api=self.api
        def expires(request,**kwargs):
            if request.full_url.endswith('/zip'):self.now+=datetime.timedelta(hours=1)
            return normal_api(request,**kwargs)
        self.api_opener.open.side_effect=expires
        with self.assertRaisesRegex(Failure,'^sdk-expired$'):self.fetch()
        self.download_opener.open.assert_not_called()
        self.assertFalse((self.root/'sdk-download').exists())

    def test_unexpected_redirect_http_error_stops_and_closes_body(self):
        normal_api=self.api;body=io.BytesIO(b'PRIVATE')
        def denied(request,**kwargs):
            if request.full_url.endswith('/zip'):
                raise urllib.error.HTTPError(request.full_url,429,'PRIVATE',{},body)
            return normal_api(request,**kwargs)
        self.api_opener.open.side_effect=denied
        with self.assertRaisesRegex(Failure,'^sdk-download-redirect$'):self.fetch()
        self.assertTrue(body.closed)
        self.download_opener.open.assert_not_called()

    def test_http_200_redirect_response_is_closed_and_not_downloaded(self):
        normal_api=self.api;body=io.BytesIO(b'PRIVATE')
        def ordinary(request,**kwargs):
            return body if request.full_url.endswith('/zip') else normal_api(request,**kwargs)
        self.api_opener.open.side_effect=ordinary
        with self.assertRaisesRegex(Failure,'^sdk-expected-download-redirect$'):self.fetch()
        self.assertTrue(body.closed)
        self.download_opener.open.assert_not_called()

    def test_boolean_run_attempt_and_unknown_expiry_flag_fail_closed(self):
        self.run['run_attempt']=True
        with self.assertRaisesRegex(Failure,'^sdk-run-provenance$'):self.fetch()
        self.run['run_attempt']=self.config['pin']['attempt']
        for value in (None,0,'',[]):
            self.artifact['expired']=value
            with self.subTest(kind=type(value).__name__),self.assertRaisesRegex(Failure,'^sdk-artifact-provenance$'):
                self.fetch()
        self.download_opener.open.assert_not_called()

    def test_expired_acquisition_receipt_write_cannot_leave_success_receipt(self):
        clock={'now':0};normal_write=fetch_sdk.write_json
        def late(path,value):
            normal_write(path,value);clock['now']=601
        with mock.patch.object(fetch_sdk.time,'monotonic',side_effect=lambda:clock['now']), \
                mock.patch.object(fetch_sdk,'write_json',side_effect=late):
            with self.assertRaisesRegex(Failure,'^download-deadline$'):self.fetch()
        self.assertFalse((self.root/'sdk-download/receipt.json').exists())

    def test_shared_deadline_arguments_are_typed_and_timeouts_are_bounded(self):
        with mock.patch.object(fetch_sdk.time,'monotonic',return_value=10):
            self.assertEqual(fetch_sdk.acquisition_budget(11),1)
            self.assertEqual(fetch_sdk.acquisition_budget(610),30)
            for value in (True,'PRIVATE',float('nan'),float('inf'),10):
                with self.subTest(kind=type(value).__name__),self.assertRaisesRegex(Failure,'^download-deadline$'):
                    fetch_sdk.acquisition_budget(value)


if __name__ == '__main__':
    unittest.main()
