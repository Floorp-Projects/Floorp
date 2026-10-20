import ast
import copy
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import plistlib
import tempfile
import tarfile
import unittest
from unittest import mock
import urllib.request
import zipfile

import sdk_content_qualification as q
from support import scan_tar


class SDKContentQualification(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.plan = json.loads((q.QA/'plan.json').read_text())
        self.pin = copy.deepcopy(q.sdk.PIN)
        self.raw = self.make_tar()
        self.old_gzip, self.new_gzip = gzip.compress(self.raw, mtime=1), gzip.compress(self.raw, mtime=2)
        self.tar = self.root/'payload.tar.gz'; self.tar.write_bytes(self.old_gzip)
        tree = scan_tar(self.tar)['canonicalSHA256']
        old_zip = self.make_zip(self.old_gzip)
        self.old_zip = self.root/'old.zip'; self.old_zip.write_bytes(old_zip)
        self.new_zip = self.root/'new.zip'; self.new_zip.write_bytes(self.make_zip(self.new_gzip))
        self.pin.update(tarBytes=len(self.old_gzip),tarSHA256=self.sha(self.old_gzip),
                        zipBytes=len(old_zip),zipSHA256=self.sha(old_zip),draftReaderTreeSHA256=tree)
        self.plan['sdk'].update({key:self.pin[key] for key in q.sdk.CONTENT_KEYS})
        self.plan['sdk']['tarSHA256']=self.pin['tarSHA256']
        self.plan_path=self.root/'plan.json';self.plan_path.write_text(json.dumps(self.plan))
        anchor={'bytes':len(self.raw),'sha256':self.sha(self.raw)}
        self.patch(q,'QA',self.root)
        self.patch(q.sdk,'BASELINE_PLAN_SHA256',self.sha(self.plan_path.read_bytes()))
        self.patch(q.sdk,'BASELINE_TAR_SHA256',self.pin['tarSHA256'])
        self.patch(q.sdk,'PIN',self.pin)
        self.patch(q.sdk,'UNCOMPRESSED_TAR',anchor)
        self.patch(q,'NEW_WIRE',{**q.NEW_WIRE,'zipBytes':self.new_zip.stat().st_size,
                                'zipSHA256':self.sha(self.new_zip.read_bytes())})
        self.patch(urllib.request,'urlopen',mock.Mock(side_effect=AssertionError('network-forbidden')))
        self.patch(urllib.request,'build_opener',mock.Mock(side_effect=AssertionError('network-forbidden')))

    def patch(self,owner,name,value):
        patched=mock.patch.object(owner,name,value);patched.start();self.addCleanup(patched.stop)

    @staticmethod
    def sha(value):return hashlib.sha256(value).hexdigest()

    @staticmethod
    def make_tar(*,version='26.5',names=None,link=None):
        stream=io.BytesIO()
        with tarfile.open(fileobj=stream,mode='w') as archive:
            if names is None:names=['MacOSX.sdk/SDKSettings.plist']
            for name in names:
                data=plistlib.dumps({'Version':version})
                member=tarfile.TarInfo(name);member.size=len(data)
                archive.addfile(member,io.BytesIO(data))
            if link:
                member=tarfile.TarInfo('MacOSX.sdk/link');member.type=tarfile.SYMTYPE;member.linkname=link
                archive.addfile(member)
        return stream.getvalue()

    @staticmethod
    def make_zip(payload,entries=None):
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as archive:
            for name,value in entries or [('macos_sdk.tar.gz',payload)]:archive.writestr(name,value)
        return stream.getvalue()

    def replace_candidate(self,payload,*,raw=None,tree=False):
        self.new_zip.write_bytes(self.make_zip(payload))
        q.NEW_WIRE.update(zipBytes=self.new_zip.stat().st_size,zipSHA256=self.sha(self.new_zip.read_bytes()))
        self.pin['tarBytes']=len(payload);self.plan['sdk']['tarBytes']=len(payload)
        if raw is not None:q.sdk.UNCOMPRESSED_TAR={'bytes':len(raw),'sha256':self.sha(raw)}
        if tree:
            path=self.root/'scan.tar.gz';path.write_bytes(payload)
            self.pin['draftReaderTreeSHA256']=scan_tar(path)['canonicalSHA256']
            self.plan['sdk']['draftReaderTreeSHA256']=self.pin['draftReaderTreeSHA256']
        self.plan_path.write_text(json.dumps(self.plan));q.sdk.BASELINE_PLAN_SHA256=self.sha(self.plan_path.read_bytes())

    def rejected(self,code,call):
        with self.assertRaises(q.Failure) as failure:call()
        self.assertEqual(failure.exception.args,(code,))
        self.assertNotIn(str(self.root),str(failure.exception))

    def test_old_containers_full_proof_and_existing_source_shape(self):
        value=q.qualify(self.old_zip,self.tar,target='retained')
        self.assertEqual(value['tar']['sha256'],self.pin['tarSHA256'])
        self.assertEqual(value['uncompressedTar'],q.sdk.UNCOMPRESSED_TAR)
        self.assertEqual(value['tree']['canonicalSHA256'],self.pin['draftReaderTreeSHA256'])
        self.assertEqual(set(value['sourceCandidate']),q.sdk.SOURCE_KEYS)
        self.assertFalse(value['sourceAutomaticallyAdopted'])
        self.assertFalse(value['payloadAcquired'])

    def test_candidate_measures_new_compressed_hash_and_same_raw_anchor(self):
        value=q.qualify(self.new_zip,target='candidate')
        self.assertNotEqual(value['tar']['sha256'],self.pin['tarSHA256'])
        self.assertEqual(value['tar']['sha256'],self.sha(self.new_gzip))
        self.assertEqual(value['uncompressedTar'],q.sdk.UNCOMPRESSED_TAR)
        self.assertEqual(value['producerJob'],118334374715)
        self.assertEqual(value['producerName'],'Mac build / download-macos-sdk')
        config=self.root/'source.json';config.write_text(json.dumps(value['sourceCandidate']))
        # The current production origin can be either reviewed source. A toy
        # candidate never becomes trusted merely because its producer matches.
        self.assertNotEqual(value['sourceCandidate']['pin'],q.sdk.PIN)
        mismatch = ('sdk-source-producer' if value['producerJob'] != q.sdk.PRODUCER_JOB
                    else 'sdk-source-pin')
        self.rejected(mismatch,lambda:q.sdk.load_sdk_source(self.plan_path,config))
        with mock.patch.multiple(q.sdk,PRODUCER_JOB=value['producerJob'],
                                 PRODUCER_NAME=value['producerName']):
            self.rejected('sdk-source-pin',lambda:q.sdk.load_sdk_source(self.plan_path,config))

    def test_unknown_or_boolean_target_never_selects_arbitrary_pins(self):
        for target in ('foreign',True,None,{}):
            with self.subTest(target=type(target).__name__):
                self.rejected('sdk-qualification-target',lambda:q.qualify(self.old_zip,target=target))

    def test_fixed_plan_mismatch_stops_before_container_read(self):
        self.plan_path.write_text('{}')
        self.rejected('sdk-qualification-plan',lambda:q.qualify(self.old_zip))

    def test_zip_size_or_hash_mismatch(self):
        self.old_zip.write_bytes(b'x'*self.old_zip.stat().st_size)
        self.rejected('sdk-qualification-zip',lambda:q.qualify(self.old_zip))

    def test_provided_tar_must_equal_the_authenticated_zip_entry(self):
        self.tar.write_bytes(self.new_gzip)
        self.rejected('sdk-qualification-tar-binding',lambda:q.qualify(self.old_zip,self.tar))

    def test_old_compressed_tar_hash_is_not_bypassed(self):
        self.pin['tarSHA256']='0'*64
        self.rejected('sdk-qualification-tar',lambda:q.qualify(self.old_zip))

    def test_wire_zip_layout_and_member_size_are_exact(self):
        self.new_zip.write_bytes(self.make_zip(self.new_gzip,entries=[('other',self.new_gzip)]))
        q.NEW_WIRE.update(zipBytes=self.new_zip.stat().st_size,zipSHA256=self.sha(self.new_zip.read_bytes()))
        self.rejected('sdk-qualification-zip-layout',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_crc_failure_stops_before_success(self):
        raw=bytearray(self.new_zip.read_bytes());start=raw.index(b'PK\x01\x02');raw[start+16:start+20]=b'\0'*4
        self.new_zip.write_bytes(raw);q.NEW_WIRE['zipSHA256']=self.sha(raw)
        self.rejected('sdk-qualification-read',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_raw_hash_includes_tar_padding_and_all_headers(self):
        changed=self.raw[:-1]+bytes([self.raw[-1]^1]);self.replace_candidate(gzip.compress(changed,mtime=2))
        self.rejected('sdk-qualification-raw-anchor',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_raw_byte_overflow_is_bounded(self):
        q.sdk.UNCOMPRESSED_TAR={**q.sdk.UNCOMPRESSED_TAR,'bytes':1}
        self.rejected('sdk-qualification-raw-limit',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_raw_short_and_wrong_hash_are_not_zero_or_success(self):
        q.sdk.UNCOMPRESSED_TAR={**q.sdk.UNCOMPRESSED_TAR,'bytes':len(self.raw)+1}
        self.rejected('sdk-qualification-raw-anchor',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_canonical_tree_is_independently_required(self):
        self.pin['draftReaderTreeSHA256']='0'*64;self.plan['sdk']['draftReaderTreeSHA256']='0'*64
        self.plan_path.write_text(json.dumps(self.plan));q.sdk.BASELINE_PLAN_SHA256=self.sha(self.plan_path.read_bytes())
        self.rejected('sdk-qualification-tree',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_actual_settings_version_is_required(self):
        raw=self.make_tar(version='26.6');self.replace_candidate(gzip.compress(raw,mtime=2),raw=raw,tree=True)
        self.rejected('sdk-qualification-version',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_missing_settings_never_implies_baseline_version(self):
        raw=self.make_tar(names=['MacOSX.sdk/unrelated']);self.replace_candidate(gzip.compress(raw,mtime=2),raw=raw,tree=True)
        self.rejected('sdk-qualification-version',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_member_path_and_link_escape_are_rejected(self):
        for raw,code in ((self.make_tar(names=['../escape']),'sdk-qualification-member-path'),
                         (self.make_tar(link='../../escape'),'sdk-qualification-link')):
            self.replace_candidate(gzip.compress(raw,mtime=2))
            self.rejected(code,lambda:q.qualify(self.new_zip,target='candidate'))

    def test_duplicate_member_is_rejected(self):
        raw=self.make_tar(names=['MacOSX.sdk/SDKSettings.plist']*2);self.replace_candidate(gzip.compress(raw,mtime=2))
        self.rejected('sdk-qualification-members',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_malformed_gzip_is_unavailable(self):
        self.replace_candidate(b'not gzip')
        self.rejected('sdk-qualification-read',lambda:q.qualify(self.new_zip,target='candidate'))

    def test_owned_regular_single_link_no_follow_input_required(self):
        link=self.root/'link.zip';link.symlink_to(self.old_zip)
        self.rejected('sdk-qualification-file',lambda:q.qualify(link))
        hard=self.root/'hard.zip';os.link(self.old_zip,hard)
        self.rejected('sdk-qualification-file',lambda:q.qualify(self.old_zip))
        hard.unlink()
        with mock.patch.object(q.os,'geteuid',return_value=os.geteuid()+1):
            self.rejected('sdk-qualification-file',lambda:q.qualify(self.old_zip))

    def test_identity_change_after_content_scan_refuses_success(self):
        original=q.canonical_tar
        def changed(*args):
            result=original(*args);self.old_zip.write_bytes(b'x'*self.old_zip.stat().st_size);return result
        with mock.patch.object(q,'canonical_tar',side_effect=changed):
            self.rejected('sdk-qualification-file-changed',lambda:q.qualify(self.old_zip,self.tar))

    def test_deadline_and_member_caps_are_fail_closed(self):
        with mock.patch.object(q.time,'monotonic',side_effect=[0,121]):
            self.rejected('sdk-qualification-deadline',lambda:q.qualify(self.old_zip))
        with mock.patch.object(q,'MAX_MEMBERS',0):
            self.rejected('sdk-qualification-members',lambda:q.qualify(self.old_zip))

    def test_review_pin_generation_changes_only_three_fixed_assignments(self):
        original='PIN = '+repr(self.pin)+'\nPRODUCER_JOB = '+repr(q.sdk.PRODUCER_JOB)+'\nPRODUCER_NAME = '+repr(q.sdk.PRODUCER_NAME)+'\ndef unchanged(): return 42\n'
        with mock.patch.object(Path,'read_text',return_value=original):
            result,candidate,patch=q.propose(self.new_zip,target='candidate')
        values={node.targets[0].id:ast.literal_eval(node.value) for node in ast.parse(candidate).body if isinstance(node,ast.Assign)}
        self.assertEqual(values['PIN']['tarSHA256'],self.sha(self.new_gzip))
        self.assertEqual(values['PIN']['artifact'],12225585543)
        self.assertEqual(values['PRODUCER_NAME'],'Mac build / download-macos-sdk')
        self.assertEqual(values['PRODUCER_JOB'],118334374715)
        self.assertIn('def unchanged(): return 42',candidate)
        self.assertIn('NOT_ADMITTED',patch)
        self.assertNotIn(str(self.root),json.dumps(result))


if __name__=='__main__':unittest.main()
