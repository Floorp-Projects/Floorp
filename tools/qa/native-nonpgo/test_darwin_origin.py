# SPDX-License-Identifier: MPL-2.0
import ctypes
import errno
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import darwin_origin as origin
import origin_control as control
import process_census as pc
from support import Failure
from test_process_census import layout,raw


ROW=(42,123,4,501,501)
ANCHOR=(1,100,2,0,0)


def info(**changes):
    value=origin.UniqueInfo()
    value.uuid[0]=1
    value.uniqueid=10
    value.parent_uniqueid=20
    value.idversion=-7
    value.original_parent_version=-9
    for name,item in changes.items():
        setattr(value,name,item)
    return value


def bsd(**changes):
    return SimpleNamespace(pid=42,start_sec=123,start_usec=4,uid=501,ruid=501,
                           ppid=changes.get('ppid',1),pgid=42)


class OriginGuards(unittest.TestCase):
    def test_published_abi_and_live_query_flavor(self):
        calls=[]
        def query(pid,flavor,value):
            calls.append((pid,flavor,ctypes.sizeof(value)))
            ctypes.memmove(ctypes.byref(value),ctypes.byref(info()),56)
            return 56,0
        details={}
        value=origin.unique(SimpleNamespace(query=query),42,details)
        self.assertEqual(calls,[(42,17,56)])
        self.assertEqual(value.idversion,-7)
        self.assertEqual(details,{'bytes':56,'expectedBytes':56,'errno':0})

    def test_short_permission_and_errno_are_unavailable(self):
        for size,code in ((0,errno.EPERM),(55,0),(56,errno.EIO)):
            with self.subTest(size=size,code=code),self.assertRaisesRegex(Failure,'origin-unique-unavailable'):
                origin.unique(SimpleNamespace(query=lambda *args:(size,code)),42,{})

    def test_zero_identity_uuid_and_reserved_are_invalid(self):
        for changes in ({'uniqueid':0},{'reserved2':1},{'reserved3':1},{'zero_uuid':True}):
            def query(pid,flavor,value):
                source=info(**{k:v for k,v in changes.items() if k!='zero_uuid'})
                if changes.get('zero_uuid'):source.uuid[0]=0
                ctypes.memmove(ctypes.byref(value),ctypes.byref(source),56)
                return 56,0
            with self.subTest(changes=changes),self.assertRaisesRegex(Failure,'origin-unique-invalid'):
                origin.unique(SimpleNamespace(query=query),42,{})

    def test_capture_rejects_exec_reparent_and_birth_races(self):
        for after,second in ((bsd(),info(idversion=-6)),(bsd(ppid=5),info()),
                             (SimpleNamespace(**{**vars(bsd()),'start_sec':124}),info())):
            reader=mock.Mock()
            reader.bsd.side_effect=[('measured',bsd(),None),('measured',after,None)]
            with mock.patch.object(origin,'unique',side_effect=[info(),second]),self.assertRaisesRegex(Failure,'origin-candidate-changed'):
                origin.capture(ROW,reader)

    def test_capture_never_uses_gone_or_unavailable_identity(self):
        for state in ('gone','unavailable'):
            reader=mock.Mock();reader.bsd.return_value=(state,bsd(),None)
            with self.assertRaisesRegex(Failure,'origin-candidate-unavailable'):
                origin.capture(ROW,reader)

    def describe(self,candidate=None,anchors=None,anchor_info=None,observed=None):
        anchor_info=anchor_info or info(uniqueid=20,idversion=-9)
        with mock.patch.object(origin,'unique',side_effect=[anchor_info,anchor_info]), \
             mock.patch.object(origin,'capture',return_value=(candidate or info(),1)):
            return origin.describe_origin(ROW,observed or set(),mock.Mock(),
                                          mock.Mock(side_effect=anchors or [ANCHOR,ANCHOR]))

    def test_positive_consistency_never_authorizes_exclusion_or_signals(self):
        result=self.describe(observed={ROW[:3]})
        self.assertEqual(result['classification'],'pid1-original-parent-consistent')
        self.assertEqual(result['observedInStageGroup'],'observed')
        for key in ('ancestryComplete','writerRoleEstablished','exclusionAuthorized','signalOwnershipGranted'):
            self.assertIs(result[key],False)

    def test_orphan_exec_parent_unique_match_does_not_establish_original_parent(self):
        result=self.describe(candidate=info(original_parent_version=-8))
        self.assertTrue(result['parentUniqueMatchesPID1'])
        self.assertFalse(result['originalParentVersionMatchesPID1'])
        self.assertEqual(result['classification'],'original-parent-not-established')

    def test_anchor_birth_and_credentials_must_match_twice(self):
        for anchors in ([ANCHOR,(1,101,2,0,0)],[(1,100,2,501,501)]):
            result=self.describe(anchors=anchors)
            self.assertEqual(result['status'],'diagnostic-unavailable')
            self.assertIsNone(result['classification'])

    def test_anchor_exec_change_is_unknown(self):
        with mock.patch.object(origin,'unique',side_effect=[info(),info(idversion=9)]), \
             mock.patch.object(origin,'capture',return_value=(info(),1)):
            result=origin.describe_origin(ROW,set(),mock.Mock(),lambda:ANCHOR)
        self.assertEqual(result['status'],'diagnostic-unavailable')

    def test_unavailable_retains_null_and_hides_raw_exception(self):
        result=origin.describe_origin(ROW,set(),mock.Mock(),mock.Mock(side_effect=OSError('private PID 8675309')))
        self.assertIsNone(result['parentUniqueMatchesPID1'])
        self.assertIsNone(result['originalParentVersionMatchesPID1'])
        self.assertNotIn('8675309',json.dumps(result))

    def test_optional_pid_one_snapshot_does_not_add_root_to_candidates(self):
        data=raw({}, {'pid':1,'uid':0,'ruid':0,'startSec':100})
        value=pc.decode(data,layout(),501,501,41,(123,4),include_pid_one=True)
        self.assertEqual(value['pidOne'],(1,100,4,0,0))
        self.assertEqual(value['identities'],{(41,123,4,501,501)})
        self.assertNotIn('pidOne',pc.decode(data,layout(),501,501,41,(123,4)))

    def test_optional_pid_one_missing_or_invalid_is_not_empty(self):
        for data in (raw({}),raw({}, {'pid':1,'uid':0,'ruid':0,'state':5}),
                     raw({}, {'pid':1,'uid':0,'ruid':0,'startSec':0})):
            with self.assertRaises(Failure):
                pc.decode(data,layout(),501,501,41,(123,4),include_pid_one=True)

    def test_cleanup_closes_pipe_then_reaps_and_checks_group(self):
        parent=mock.Mock(pid=42,returncode=0);receipt={}
        with mock.patch.object(control.os,'close') as close, \
             mock.patch.object(control.Runner,'group_alive',return_value=False):
            control.cleanup(parent,7,receipt)
        close.assert_called_once_with(7)
        parent.wait.assert_called_once_with(timeout=5)
        self.assertEqual(receipt['groupCleanup'],'verified-gone')
        self.assertTrue(receipt['leaderReaped'])

    def test_cleanup_failure_keeps_stage_and_no_arbitrary_signal(self):
        parent=mock.Mock(pid=42,returncode=None);receipt={}
        with mock.patch.object(control.os,'close',side_effect=PermissionError('private detail')):
            control.cleanup(parent,7,receipt)
        self.assertEqual(receipt['cleanupFailure'],{'operation':'close-control-pipe','kind':'permission'})
        self.assertFalse(receipt['leaderReaped'])
        parent.terminate.assert_not_called()
        self.assertNotIn('private detail',json.dumps(receipt))

    def run_failed_cleanup(self,primary):
        reader=mock.Mock()
        reader.bsd.side_effect=[('measured',bsd(),None),('measured',bsd(),None),
                               ('measured',bsd(ppid=42),None)]
        parent=mock.Mock(pid=42,returncode=0)
        hint={'status':'identity-verified','currentParentIsPID1':True,
              'parentUniqueMatchesPID1':True,'originalParentVersionMatchesPID1':False,
              'classification':'original-parent-not-established','exclusionAuthorized':False}
        def fail_cleanup(parent,fd,receipt):
            receipt.update(leaderReaped=True,groupCleanup='unverified',
                           cleanupFailure={'operation':'wait-group','kind':'timeout'})
        with tempfile.TemporaryDirectory() as temporary:
            task=Path(temporary);(task/'origin-grandchild').write_text('42')
            result={}
            with mock.patch.object(control,'probe',return_value=reader), \
                 mock.patch.object(control,'capture',side_effect=[
                     (info(),1),(info(original_parent_version=-7,parent_uniqueid=10),1),
                     (info(original_parent_version=-7,parent_uniqueid=10),42),
                     (info(original_parent_version=-7,parent_uniqueid=10),1),
                     (info(idversion=-6,original_parent_version=-7,parent_uniqueid=20),1)]), \
                 mock.patch.object(control,'describe_origin',return_value=hint) as describe, \
                 mock.patch.object(control.os,'pipe',return_value=(7,8)), \
                 mock.patch.object(control.os,'geteuid',return_value=501), \
                 mock.patch.object(control.os,'getuid',return_value=501), \
                 mock.patch.object(control.os,'close'),mock.patch.object(control.os,'write'), \
                 mock.patch.object(control.subprocess,'Popen',return_value=parent) as popen, \
                 mock.patch.dict(control.os.environ,{'FLOORP_TEST_PRIVATE_ENV':'excluded'}), \
                 mock.patch.object(control,'wait_ready',side_effect=Failure('primary-code') if primary else None), \
                 mock.patch.object(control,'cleanup',side_effect=fail_cleanup):
                with self.assertRaisesRegex(Failure,'primary-code' if primary else 'origin-control-cleanup'):
                    control.run_control(task,result)
            self.assertEqual(result['status'],'failed')
            self.assertEqual(result['cleanup']['cleanupFailure']['operation'],'wait-group')
            self.assertNotIn('FLOORP_TEST_PRIVATE_ENV',popen.call_args.kwargs['env'])
            if not primary:
                self.assertEqual(len(describe.call_args_list),3)
                self.assertTrue(all(call.args[1]==set() for call in describe.call_args_list))

    def test_primary_failure_survives_additional_cleanup_failure(self):
        self.run_failed_cleanup(True)

    def test_cleanup_failure_is_primary_when_control_otherwise_passes(self):
        self.run_failed_cleanup(False)
