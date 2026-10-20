# SPDX-License-Identifier: MPL-2.0
import copy
import gzip
import hashlib
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import sdk_source as sdk
from qa_core import Failure


# Independently recorded normal API origins, not loader-derived expectations.
KNOWN_SDK_ORIGINS = {
    12182302735: {'artifact': 12182302735, 'run': 39244172146,
                  'producerJob': 118032017226,
                  'producerName': 'Mac build [PGO] / download-macos-sdk'},
    12225585543: {'artifact': 12225585543, 'run': 39332628358,
                  'producerJob': 118334374715,
                  'producerName': 'Mac build / download-macos-sdk'},
}


class SDKSourceTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='floorp-sdk-source-')
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.plan_path = self.root/'plan.json'
        self.source_path = self.root/'sdk_source.json'
        self.plan_raw = Path(sdk.__file__).with_name('plan.json').read_bytes()
        self.source_raw = Path(sdk.__file__).with_name('sdk_source.json').read_bytes()
        self.plan_path.write_bytes(self.plan_raw)
        self.source_path.write_bytes(self.source_raw)
        self.plan = json.loads(self.plan_raw)
        self.config = json.loads(self.source_raw)
        self.expected_origin = KNOWN_SDK_ORIGINS[self.config['pin']['artifact']]
        self.assertEqual({'artifact': self.config['pin']['artifact'],
                          'run': self.config['pin']['run'],
                          'producerJob': self.config['producerJob'],
                          'producerName': sdk.PRODUCER_NAME}, self.expected_origin)

    def alternate(self):
        return sdk.load_sdk_source(self.plan_path, self.source_path)

    def source(self, value):
        self.source_path.write_text(json.dumps(value))

    def assert_fixed_failure(self, code, operation):
        with self.assertRaises(Failure) as caught:
            operation()
        self.assertEqual(str(caught.exception), 'sdk-source-'+code)
        self.assertNotIn('PRIVATE-SENTINEL', str(caught.exception))

    def compressed_fixture(self, body=b'private synthetic TAR bytes\n', **changes):
        path = self.root/'PRIVATE-SENTINEL.tar.gz'
        path.write_bytes(gzip.compress(body))
        spec = self.alternate()
        spec['uncompressedTar'] = {'bytes':len(body), 'sha256':hashlib.sha256(body).hexdigest()}
        spec['uncompressedTar'].update(changes)
        return path, spec

    def test_default_uses_exact_existing_plan_pin_and_no_implicit_source_file(self):
        spec = sdk.load_sdk_source(self.plan_path)
        self.assertEqual(set(spec), {'pin','runtimeRef','producerJob','producerName','uncompressedTar','sourceSHA256',
                                     'baselinePlanSHA256','qualification'})
        self.assertEqual(spec['pin'], self.plan['sdk'])
        self.assertEqual(spec['runtimeRef'], self.plan['runtime']['ref'])
        self.assertEqual(spec['baselinePlanSHA256'], hashlib.sha256(self.plan_raw).hexdigest())
        self.assertEqual(spec['qualification'], sdk.DEFAULT_QUALIFICATION)
        self.assertIsNone(spec['sourceSHA256'])
        self.assertIsNone(spec['producerJob'])
        self.assertIsNone(spec['producerName'])
        self.assertIsNone(spec['uncompressedTar'])
        self.source_path.write_text('PRIVATE-SENTINEL malformed source')
        self.assertEqual(sdk.load_sdk_source(self.plan_path), spec)

    def test_default_does_not_add_alternate_guards_to_other_legacy_plans(self):
        plan = copy.deepcopy(self.plan)
        plan['sdk'].update(artifact=1, tarSHA256='legacy-unmodified', private={'PRIVATE-SENTINEL':1})
        plan['runtime']['ref'] = 'legacy-ref'
        self.plan_path.write_text(json.dumps(plan))
        spec = sdk.load_sdk_source(self.plan_path)
        self.assertEqual(spec['pin'], plan['sdk'])
        self.assertEqual(spec['runtimeRef'], 'legacy-ref')
        self.assertIsNone(sdk.verify_uncompressed_tar('/PRIVATE-SENTINEL/missing', spec))
        spec['pin']['private']['PRIVATE-SENTINEL'] = 2
        self.assertEqual(plan['sdk']['private']['PRIVATE-SENTINEL'],1)

    def test_plan_read_failures_have_fixed_codes_without_path_or_json_text(self):
        self.plan_path.unlink()
        self.assert_fixed_failure('plan-read',lambda:sdk.load_sdk_source(self.plan_path))
        for raw in (b'PRIVATE-SENTINEL',b'\xff',b'{}'):
            self.plan_path.write_bytes(raw)
            self.assert_fixed_failure('plan-read',lambda:sdk.load_sdk_source(self.plan_path))

    def test_exact_alternate_is_explicit_preserves_baseline_and_overwrites_only_allowlist(self):
        spec = self.alternate()
        expected = copy.deepcopy(self.plan['sdk'])
        expected.update(self.config['pin'])
        self.assertEqual(spec['pin'], expected)
        self.assertEqual(spec['runtimeRef'], sdk.RUNTIME_REF)
        self.assertEqual(spec['producerJob'], self.expected_origin['producerJob'])
        self.assertEqual(spec['producerName'], self.expected_origin['producerName'])
        self.assertEqual(spec['sourceSHA256'],hashlib.sha256(self.source_raw).hexdigest())
        self.assertEqual(spec['baselinePlanSHA256'],sdk.BASELINE_PLAN_SHA256)
        self.assertEqual(spec['qualification'],sdk.QUALIFICATION)
        self.assertEqual(spec['uncompressedTar'],sdk.UNCOMPRESSED_TAR)
        self.assertEqual(spec['pin']['treeSHA256'],self.plan['sdk']['treeSHA256'])
        self.assertEqual(spec['pin']['draftReaderFormat'],self.plan['sdk']['draftReaderFormat'])
        self.assertEqual(self.plan_path.read_bytes(), self.plan_raw)
        spec['pin']['run'] = 1
        spec['uncompressedTar']['bytes'] = 1
        self.assertEqual(self.alternate()['pin']['run'], self.expected_origin['run'])
        self.assertEqual(sdk.UNCOMPRESSED_TAR['bytes'],760518144)

    def test_source_schema_key_allowlist_and_bool_schema_are_strict(self):
        for value in ([], {}, {**self.config,'schema':True}, {**self.config,'private':'PRIVATE-SENTINEL'}):
            self.source(value)
            self.assert_fixed_failure('shape',self.alternate)
        for key in self.config:
            value=copy.deepcopy(self.config);value.pop(key);self.source(value)
            self.assert_fixed_failure('shape',self.alternate)

    def test_producer_name_is_internal_trusted_spec_and_never_a_json_override(self):
        self.assertEqual(sdk.PRODUCER_NAME, self.expected_origin['producerName'])
        for name in ('Mac build [PGO] / download-macos-sdk', 'Mac build / download-macos-sdk', True, None):
            self.source({**self.config, 'producerName': name})
            self.assert_fixed_failure('shape', self.alternate)

    def test_source_read_json_utf8_duplicate_nonfinite_and_size_failures_are_private(self):
        self.source_path.unlink()
        self.assert_fixed_failure('read',self.alternate)
        for raw,code in ((b'PRIVATE-SENTINEL','read'),(b'\xff','read'),
                         (b'{"schema":1,"schema":1}','json'),(b'{"schema":NaN}','json'),
                         (b' '*16385,'size')):
            self.source_path.write_bytes(raw)
            self.assert_fixed_failure(code,self.alternate)

    def test_baseline_plan_hash_binds_exact_file_bytes_including_whitespace(self):
        self.plan_path.write_bytes(self.plan_raw+b'\n')
        self.assert_fixed_failure('plan-binding',self.alternate)
        self.plan_path.write_bytes(self.plan_raw)
        self.source({**self.config,'baselinePlanSHA256':'0'*64})
        self.assert_fixed_failure('plan-binding',self.alternate)

    def test_baseline_tar_runtime_producer_and_qualification_are_fixed(self):
        for key,value,code in [('baselineTarSHA256','0'*64,'baseline-tar'),
                               ('runtimeRef','0'*40,'runtime'),('producerJob',True,'producer'),
                               ('producerJob',117779018336,'producer'),
                               ('qualification','PRIVATE-SENTINEL','qualification')]:
            self.source({**self.config,key:value})
            self.assert_fixed_failure(code,self.alternate)

    def test_all_content_origin_and_wire_pin_fields_are_fixed_typed_and_complete(self):
        for key,old in self.config['pin'].items():
            value=copy.deepcopy(self.config)
            value['pin'][key]=True if type(old) is int else 'PRIVATE-SENTINEL'
            self.source(value)
            self.assert_fixed_failure('pin',self.alternate)
            value=copy.deepcopy(self.config);value['pin'].pop(key);self.source(value)
            self.assert_fixed_failure('pin',self.alternate)
        value=copy.deepcopy(self.config);value['pin']['url']='PRIVATE-SENTINEL';self.source(value)
        self.assert_fixed_failure('pin',self.alternate)

    def test_well_formed_but_foreign_zip_and_tar_hashes_are_rejected(self):
        for key in ('zipSHA256','tarSHA256','draftReaderTreeSHA256'):
            value=copy.deepcopy(self.config);value['pin'][key]='0'*64;self.source(value)
            self.assert_fixed_failure('pin',self.alternate)

    def test_uncompressed_anchor_is_complete_typed_and_fixed(self):
        for anchor in (None,{}, {'bytes':True,'sha256':sdk.UNCOMPRESSED_TAR['sha256']},
                       {**sdk.UNCOMPRESSED_TAR,'bytes':760518145},
                       {**sdk.UNCOMPRESSED_TAR,'sha256':'0'*64},
                       {**sdk.UNCOMPRESSED_TAR,'private':'PRIVATE-SENTINEL'}):
            self.source({**self.config,'uncompressedTar':anchor})
            self.assert_fixed_failure('uncompressed-anchor',self.alternate)

    def test_content_binding_still_rejects_changed_baseline_content_after_hash_authorization(self):
        plan=copy.deepcopy(self.plan);plan['sdk']['version']='26.6'
        self.plan_path.write_text(json.dumps(plan))
        checksum=hashlib.sha256(self.plan_path.read_bytes()).hexdigest()
        self.source({**self.config,'baselinePlanSHA256':checksum})
        with mock.patch.object(sdk,'BASELINE_PLAN_SHA256',checksum):
            self.assert_fixed_failure('content-binding',self.alternate)

    def test_uncompressed_default_returns_none_without_opening_or_hashing_any_path(self):
        spec=sdk.load_sdk_source(self.plan_path)
        with mock.patch.object(sdk.os,'open') as opened:
            self.assertIsNone(sdk.verify_uncompressed_tar('/PRIVATE-SENTINEL/missing',spec))
        opened.assert_not_called()

    def test_complete_gzip_stream_returns_exact_anchor_without_extracting(self):
        path,spec=self.compressed_fixture()
        result=sdk.verify_uncompressed_tar(path,spec)
        self.assertEqual(result,spec['uncompressedTar'])
        self.assertIsNot(result,spec['uncompressedTar'])

    def test_uncompressed_short_bytes_wrong_sha_and_overlong_stream_fail_closed(self):
        for changes,code in (({'bytes':1000},'uncompressed-bytes'),({'sha256':'0'*64},'uncompressed-sha256'),
                              ({'bytes':1},'uncompressed-limit')):
            path,spec=self.compressed_fixture(**changes)
            self.assert_fixed_failure(code,lambda:sdk.verify_uncompressed_tar(path,spec))

    def test_concatenated_extra_gzip_member_exceeds_exact_limit(self):
        path,spec=self.compressed_fixture()
        with path.open('ab') as output:output.write(gzip.compress(b'private extra bytes'))
        self.assert_fixed_failure('uncompressed-limit',lambda:sdk.verify_uncompressed_tar(path,spec))

    def test_uncompressed_limits_and_anchor_types_are_strict(self):
        path,spec=self.compressed_fixture()
        for anchor in ({'bytes':True,'sha256':'a'*64},{'bytes':0,'sha256':'a'*64},
                       {'bytes':sdk.MAX_UNCOMPRESSED_BYTES+1,'sha256':'a'*64},
                       {'bytes':1,'sha256':'PRIVATE-SENTINEL'},{'bytes':1,'sha256':'a'*64,'extra':1}):
            value={**spec,'uncompressedTar':anchor}
            self.assert_fixed_failure('uncompressed-spec',lambda:sdk.verify_uncompressed_tar(path,value))
        self.assert_fixed_failure('spec',lambda:sdk.verify_uncompressed_tar(path,{**spec,'sourceSHA256':False}))

    def test_malformed_truncated_crc_corrupt_and_invalid_deflate_gzip_are_fixed_failures(self):
        path,spec=self.compressed_fixture()
        good=path.read_bytes()
        bad_crc=bytearray(good);bad_crc[-8]^=1
        for raw in (b'PRIVATE-SENTINEL',good[:-4],bytes(bad_crc),b'\x1f\x8b\x08\x00'+b'\x00'*6+b'PRIVATE-SENTINEL'):
            path.write_bytes(raw)
            self.assert_fixed_failure('uncompressed-read',lambda:sdk.verify_uncompressed_tar(path,spec))

    def test_symlink_directory_foreign_owner_and_compressed_size_cap_reject(self):
        path,spec=self.compressed_fixture()
        link=self.root/'PRIVATE-SENTINEL-link';link.symlink_to(path)
        self.assert_fixed_failure('uncompressed-file',lambda:sdk.verify_uncompressed_tar(link,spec))
        self.assert_fixed_failure('uncompressed-file',lambda:sdk.verify_uncompressed_tar(self.root,spec))
        with mock.patch.object(sdk.os,'geteuid',return_value=os.geteuid()+1):
            self.assert_fixed_failure('uncompressed-file',lambda:sdk.verify_uncompressed_tar(path,spec))
        with mock.patch.object(sdk,'MAX_COMPRESSED_BYTES',1):
            self.assert_fixed_failure('uncompressed-file',lambda:sdk.verify_uncompressed_tar(path,spec))

    def test_stream_file_change_is_rejected(self):
        path,spec=self.compressed_fixture()
        before=path.lstat()
        fields=('st_dev','st_ino','st_mode','st_uid','st_size','st_mtime_ns','st_ctime_ns')
        changed=SimpleNamespace(**{key:getattr(before,key) for key in fields})
        changed.st_mtime_ns+=1
        with mock.patch.object(Path,'lstat',side_effect=[before,changed]):
            self.assert_fixed_failure('uncompressed-file-changed',lambda:sdk.verify_uncompressed_tar(path,spec))

    def test_default_acquisition_receipt_is_exact_legacy_eight_fields(self):
        spec=sdk.load_sdk_source(self.plan_path);pin=self.plan['sdk']
        expected={key:pin[key] for key in ('artifact','run','attempt','zipSHA256','tarSHA256')}
        expected.update(bytes=pin['tarBytes'],crcVerified=True,signedURLPublished=False)
        self.assertEqual(sdk.acquisition_receipt(spec),expected)
        self.assertEqual(len(expected),8)
        self.assertEqual(sdk.validate_acquisition_receipt(expected,spec),expected)

    def test_alternate_acquisition_receipt_has_exact_source_and_uncompressed_anchors(self):
        spec=self.alternate();value=sdk.acquisition_receipt(spec)
        self.assertEqual(set(value),{'artifact','run','attempt','zipSHA256','tarSHA256','bytes','crcVerified',
                                    'signedURLPublished','sourceSHA256','baselinePlanSHA256','qualification','uncompressedTar'})
        self.assertEqual(value['sourceSHA256'],hashlib.sha256(self.source_raw).hexdigest())
        self.assertEqual(value['uncompressedTar'],sdk.UNCOMPRESSED_TAR)
        self.assertEqual(sdk.validate_acquisition_receipt(value,spec),value)

    def test_receipt_foreign_missing_extra_url_and_bool_as_int_values_are_rejected(self):
        for spec in (sdk.load_sdk_source(self.plan_path),self.alternate()):
            expected=sdk.acquisition_receipt(spec)
            for key in expected:
                value=copy.deepcopy(expected);value.pop(key)
                self.assert_fixed_failure('acquisition-receipt',lambda:sdk.validate_acquisition_receipt(value,spec))
            for key in ('artifact','run','attempt','bytes'):
                value={**expected,key:True}
                self.assert_fixed_failure('acquisition-receipt',lambda:sdk.validate_acquisition_receipt(value,spec))
            for key,value in (('zipSHA256','0'*64),('tarSHA256','0'*64),('crcVerified',1),('signedURLPublished',0)):
                wrong={**expected,key:value}
                self.assert_fixed_failure('acquisition-receipt',lambda:sdk.validate_acquisition_receipt(wrong,spec))
            for value in (None,[],{**expected,'url':'PRIVATE-SENTINEL'}):
                self.assert_fixed_failure('acquisition-receipt',lambda:sdk.validate_acquisition_receipt(value,spec))
        baseline=sdk.acquisition_receipt(sdk.load_sdk_source(self.plan_path))
        self.assert_fixed_failure('acquisition-receipt',lambda:sdk.validate_acquisition_receipt(baseline,self.alternate()))

    def test_receipt_nested_anchors_and_qualification_are_not_caller_authority(self):
        spec=self.alternate();expected=sdk.acquisition_receipt(spec)
        for changes in ({'uncompressedTar':{'bytes':True,'sha256':sdk.UNCOMPRESSED_TAR['sha256']}},
                        {'uncompressedTar':{**sdk.UNCOMPRESSED_TAR,'private':'PRIVATE-SENTINEL'}},
                        {'sourceSHA256':'0'*64},{'baselinePlanSHA256':'0'*64},{'qualification':'PRIVATE-SENTINEL'}):
            self.assert_fixed_failure('acquisition-receipt',lambda:sdk.validate_acquisition_receipt({**expected,**changes},spec))

    def test_receipt_validation_returns_fresh_trusted_data_and_never_mutates_input_or_spec(self):
        spec=self.alternate();value=sdk.acquisition_receipt(spec)
        snapshot=copy.deepcopy((spec,value,dict(os.environ)))
        result=sdk.validate_acquisition_receipt(value,spec)
        self.assertIsNot(result,value)
        result['uncompressedTar']['bytes']=1
        self.assertEqual((spec,value,dict(os.environ)),snapshot)

    def test_optional_acquisition_deadline_rejects_invalid_values_before_open(self):
        path,spec=self.compressed_fixture()
        for value in (True,'PRIVATE-SENTINEL',float('nan'),float('inf'),0):
            with self.subTest(kind=type(value).__name__), mock.patch.object(sdk.os,'open') as opened:
                with self.assertRaisesRegex(Failure,'^download-deadline$'):
                    sdk.verify_uncompressed_tar(path,spec,deadline=value)
                opened.assert_not_called()

    def test_acquisition_deadline_checks_after_complete_raw_stream(self):
        path,spec=self.compressed_fixture()
        with mock.patch.object(sdk.time,'monotonic',side_effect=[0,0,10]):
            with self.assertRaisesRegex(Failure,'^download-deadline$'):
                sdk.verify_uncompressed_tar(path,spec,deadline=10)

    def test_local_verification_has_no_acquisition_or_expiry_clock(self):
        path,spec=self.compressed_fixture()
        with mock.patch.object(sdk.time,'monotonic',side_effect=AssertionError('unexpected-acquisition-clock')):
            self.assertEqual(sdk.verify_uncompressed_tar(path,spec),spec['uncompressedTar'])


if __name__=='__main__':
    unittest.main()
