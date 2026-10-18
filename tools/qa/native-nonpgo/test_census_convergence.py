# SPDX-License-Identifier: MPL-2.0
import copy
from contextlib import contextmanager
import ctypes
import errno
import json
import os
import select
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import census_convergence as cc
from checkpoint import owned_processes,measure_owned_boundary
from census_experiment import cleanup_child
from support import Failure,Runner,clean_environment


ROW=(42,123,4,501,501)
OWN=(41,123,4,501,501)


def bsd(row):
    return SimpleNamespace(pid=row[0],start_sec=row[1],start_usec=row[2],uid=row[3],ruid=row[4])


def activity(rows):
    return {'status':'measured','sampleComplete':True,'rssMiB':20.,'cpuSeconds':3.,
            'processes':len(rows)+1,'states':['S'],'_pids':tuple(row[:3] for row in rows),
            '_progress':{'rssMiB':10.,'cpuSeconds':2.,'processes':len(rows),
                         'states':['S'],'_pids':tuple(row[:3] for row in rows)}}


@contextmanager
def controlled_child(child):
    try:
        yield child
    finally:
        primary=sys.exception()
        cleanup=cleanup_child(child)
        stream_failures=[]
        for name in ('stdin','stdout'):
            stream=getattr(child,name,None)
            try:
                if stream is not None and not stream.closed:stream.close()
            except BaseException:
                stream_failures.append(name)
        failed=(not cleanup['leaderReaped'] or cleanup['groupCleanup']!='verified-gone' or
                cleanup['cleanupFailure'] is not None or bool(stream_failures))
        if failed:
            if primary is not None:primary.add_note('controlled-child-cleanup-failed')
            else:raise AssertionError('controlled-child-cleanup-failed')


class ConvergenceGuards(unittest.TestCase):
    def runner(self,task):
        return Runner(task,0,{'jobMinutes':360,'stallQuietMinutes':20,
                'maxOwnedRSSMiB':10240,'minFreeDuringBuildGiB':8,
                'maxStageLogBytes':268435456,'maxNativePrivateBytes':134217728})

    def wait(self,series,resource=None,elapsed=None):
        receipt={};baseline={OWN};seen=[]
        with tempfile.TemporaryDirectory() as temporary:
            runner=self.runner(Path(temporary))
            if elapsed is not None:runner.elapsed=mock.Mock(side_effect=elapsed)
            def measure(actual,value,observed):
                self.assertIs(actual,baseline)
                seen.append(actual)
                rows=next(series)
                value.update(status='rejected' if rows else 'passed',newCandidateIdentities=len(rows))
                return rows
            with mock.patch.object(cc,'resources',side_effect=resource or (lambda rows,reader,details:activity(rows))), \
                 mock.patch.object(cc.shutil,'disk_usage',return_value=SimpleNamespace(free=90*1024**3)):
                result=cc.wait_for_boundary(baseline,receipt,runner,reader=mock.Mock(),measure=measure,
                                            sleep=lambda _:None)
            return result,receipt,runner,seen

    def test_all_nonempty_observations_retained_until_same_baseline_passes(self):
        result,receipt,runner,seen=self.wait(iter([{ROW},{ROW},set()]))
        self.assertEqual([v['boundary']['newCandidateIdentities'] for v in receipt['samples']],[1,1,0])
        self.assertEqual(result['status'],'passed')
        for key in ('baselineReset','candidatesExcluded','signalsSent'):
            self.assertIs(receipt[key],False)
        self.assertEqual(len(seen),3)

    def test_primary_unavailable_is_immediate_and_retained(self):
        with tempfile.TemporaryDirectory() as temporary:
            runner=self.runner(Path(temporary));receipt={}
            def unavailable(baseline,value,observed):
                value.update(status='unavailable',newCandidateIdentities=None)
                raise Failure('census-sysctl-permission')
            with self.assertRaisesRegex(Failure,'census-sysctl-permission'):
                cc.wait_for_boundary({OWN},receipt,runner,reader=mock.Mock(),measure=unavailable)
            self.assertIsNone(receipt['samples'][0]['boundary']['newCandidateIdentities'])
            self.assertEqual(receipt['status'],'failed')

    def test_resource_unknown_has_null_rss_and_stop_diagnostic(self):
        with tempfile.TemporaryDirectory() as temporary:
            runner=self.runner(Path(temporary));receipt={}
            def measure(baseline,value,observed):
                value.update(status='rejected',newCandidateIdentities=1);return {ROW}
            with mock.patch.object(cc,'resources',side_effect=PermissionError('private detail')), \
                 self.assertRaisesRegex(Failure,'monitor-unavailable'):
                cc.wait_for_boundary({OWN},receipt,runner,reader=mock.Mock(),measure=measure)
            self.assertIsNone(receipt['samples'][0]['resource']['rssMiB'])
            self.assertEqual(runner.monitor_failure['status'],'unavailable')
            self.assertNotIn('private detail',str(receipt))

    def test_rss_limit_applies_to_all_new_candidates(self):
        def large(rows,reader,details):
            value=activity(rows);value['rssMiB']=10241;return value
        with self.assertRaisesRegex(Failure,'owned-rss-limit'):
            self.wait(iter([{ROW}]),resource=large)

    def test_phase_deadline_cannot_be_extended_by_retries(self):
        with self.assertRaisesRegex(Failure,'census-wait-phase-deadline'):
            self.wait(iter([{ROW}]),elapsed=[260*60])

    def test_deadline_crossing_before_sleep_keeps_fixed_failure(self):
        with self.assertRaisesRegex(Failure,'census-wait-phase-deadline'):
            self.wait(iter([{ROW}]),elapsed=[0,0,0,260*60+1])

    def test_multisignal_stall_does_not_count_monitor_self_cpu(self):
        resource=activity({ROW})
        self.assertEqual(resource['_progress']['cpuSeconds'],2.)
        with self.assertRaisesRegex(Failure,'multisignal-stall'):
            self.wait(iter([{ROW},{ROW}]),resource=lambda rows,reader,details:resource,
                      elapsed=[0,0,0,0,1201,1201])

    def test_candidate_and_self_rss_are_summed_with_full_identity_rechecks(self):
        reader=mock.Mock()
        def get_bsd(pid,details=None):return 'measured',bsd(OWN if pid==41 else ROW),None
        reader.bsd.side_effect=get_bsd
        reader.read.side_effect=lambda pid,**kw:{'status':'measured','_birth':(OWN if pid==41 else ROW)[:3],
                                               'rssMiB':10.,'cpuSeconds':5.,'state':'S'}
        with mock.patch.object(cc.os,'getpid',return_value=41):
            result=cc.resources({ROW},reader)
        self.assertEqual(result['rssMiB'],20.)
        self.assertEqual(result['_progress']['rssMiB'],10.)
        self.assertEqual(result['_progress']['cpuSeconds'],5.)
        self.assertEqual(reader.read.call_count,2)

    def test_credential_or_birth_change_is_unknown(self):
        for changed in ((42,124,4,501,501),(42,123,4,0,501)):
            reader=mock.Mock()
            reader.bsd.side_effect=[('measured',bsd(OWN),None),('measured',bsd(OWN),None),
                                    ('measured',bsd(OWN),None),('measured',bsd(changed),None)]
            reader.read.return_value={'status':'measured','_birth':OWN[:3],'rssMiB':10.,'cpuSeconds':1.,'state':'S'}
            with mock.patch.object(cc.os,'getpid',return_value=41),self.assertRaisesRegex(Failure,'census-wait-identity-unavailable'):
                cc.resources({ROW},reader)

    def test_valid_zombie_is_confirmed_gone_not_unavailable_zero(self):
        reader=mock.Mock()
        reader.bsd.side_effect=[('measured',bsd(OWN),None),('measured',bsd(OWN),None),
                                ('measured',bsd(OWN),None),('gone',bsd(ROW),'bsd-zombie')]
        reader.read.return_value={'status':'measured','_birth':OWN[:3],'rssMiB':10.,'cpuSeconds':1.,'state':'S'}
        with mock.patch.object(cc.os,'getpid',return_value=41):result=cc.resources({ROW},reader)
        self.assertEqual(result['confirmedGoneCandidates'],1)
        self.assertEqual(result['rssMiB'],10.)

    def test_post_measure_credential_change_is_unknown(self):
        reader=mock.Mock()
        reader.bsd.side_effect=[('measured',bsd(OWN),None),('measured',bsd(OWN),None),
                                ('measured',bsd(OWN),None),('measured',bsd(ROW),None),
                                ('measured',bsd((42,123,4,0,501)),None)]
        reader.read.side_effect=lambda pid,**kw:{'status':'measured','_birth':(OWN if pid==41 else ROW)[:3],
                                               'rssMiB':10.,'cpuSeconds':5.,'state':'S'}
        with mock.patch.object(cc.os,'getpid',return_value=41),self.assertRaisesRegex(Failure,'census-wait-identity-changed'):
            cc.resources({ROW},reader)

    def test_exit_during_task_query_accepts_only_authoritative_gone(self):
        for reason,birth,passes in (('bsd-zombie',ROW[:3],True),('signal-zero-esrch',None,True),
                                    ('generation-replaced',(42,124,4),False),('bsd-zombie',(42,124,4),False)):
            reader=mock.Mock()
            reader.bsd.side_effect=[('measured',bsd(OWN),None),('measured',bsd(OWN),None),
                                    ('measured',bsd(OWN),None),('measured',bsd(ROW),None)]
            if passes:reader.bsd.side_effect=list(reader.bsd.side_effect)+[('gone',bsd(ROW),'bsd-zombie')]
            reader.read.side_effect=[{'status':'measured','_birth':OWN[:3],'rssMiB':10.,'cpuSeconds':1.,'state':'S'},
                                     {'status':'gone','reason':reason,'_birth':birth}]
            with mock.patch.object(cc.os,'getpid',return_value=41):
                if passes:
                    result=cc.resources({ROW},reader)
                    self.assertEqual(result['confirmedGoneCandidates'],1)
                else:
                    with self.assertRaisesRegex(Failure,'census-wait-resource-unavailable'):
                        cc.resources({ROW},reader)

    def test_failed_resource_operation_errno_and_fixed_reason_survive(self):
        for reason in ('permission','private detail 8675309'):
            reader=mock.Mock();details={}
            reader.bsd.side_effect=[('measured',bsd(OWN),None),('measured',bsd(OWN),None),
                                    ('measured',bsd(OWN),None),('measured',bsd(ROW),None)]
            reader.read.side_effect=[{'status':'measured','_birth':OWN[:3],'rssMiB':10.,'cpuSeconds':1.,'state':'S'},
                                     {'status':'unavailable','reason':reason,
                                      'taskQuery':{'bytes':0,'expectedBytes':232,'errno':1,'private':'hidden'}}]
            with mock.patch.object(cc.os,'getpid',return_value=41),self.assertRaisesRegex(Failure,'census-wait-resource-unavailable'):
                cc.resources({ROW},reader,details)
            self.assertEqual(details['operation'],'candidate-task')
            self.assertEqual(details['reason'],'permission' if reason=='permission' else 'unknown-query-reason')
            self.assertEqual(details['taskQuery'],{'bytes':0,'expectedBytes':232,'errno':1})
            self.assertNotIn('8675309',str(details))
        reader=mock.Mock();details={}
        reader.bsd.side_effect=[('measured',bsd(OWN),None),('measured',bsd(OWN),None),
                                ('measured',bsd(OWN),None),('measured',bsd(ROW),None)]
        reader.read.side_effect=[{'status':'measured','_birth':OWN[:3],'rssMiB':10.,'cpuSeconds':1.,'state':'S',
                                 'taskQuery':{'bytes':232,'expectedBytes':232,'errno':0}},
                                PermissionError(1,'private')]
        with mock.patch.object(cc.os,'getpid',return_value=41),self.assertRaises(PermissionError):
            cc.resources({ROW},reader,details)
        self.assertEqual(details['operation'],'candidate-task')
        self.assertNotIn('taskQuery',details)
        from unittest_receipt import known_failure_codes
        def denied(rows,reader,details):
            details.update(operation='candidate-task',reason='permission',
                           taskQuery={'bytes':0,'expectedBytes':232,'errno':1})
            raise Failure('census-wait-resource-unavailable')
        with mock.patch.object(cc,'resources',side_effect=denied), \
             self.assertRaisesRegex(Failure,'census-wait-metadata-task-permission'):
            cc.metadata_resource_gate({ROW},mock.Mock(),{})
        self.assertIn('census-wait-metadata-task-permission',known_failure_codes())
        with self.assertRaisesRegex(Failure,'census-wait-metadata-no-candidate'):
            cc.metadata_resource_gate(set(),mock.Mock(),{})
        with mock.patch.object(cc,'resources',return_value=activity(set())), \
             self.assertRaisesRegex(Failure,'census-wait-metadata-no-measurement'):
            cc.metadata_resource_gate({ROW},mock.Mock(),{})

    def test_more_than_sixteen_samples_keep_all_counts_and_stop_details(self):
        result,receipt,_,_=self.wait(iter([{ROW}]*20+[set()]))
        self.assertEqual(len(receipt['samples']),21)
        self.assertTrue(all(v['boundary']['newCandidateIdentities']==1 for v in receipt['samples'][:-1]))
        self.assertIs(receipt['samples'][0]['boundary']['deltaDetailsRetained'],False)
        self.assertEqual(receipt['firstBoundary']['newCandidateIdentities'],1)
        self.assertEqual(receipt['finalBoundary']['newCandidateIdentities'],0)

    def test_stage_cleanup_failure_prevents_any_new_measurement(self):
        with tempfile.TemporaryDirectory() as temporary:
            runner=self.runner(Path(temporary));runner.groups.add(42);measure=mock.Mock();receipt={}
            with self.assertRaisesRegex(Failure,'census-wait-stage-cleanup'):
                cc.wait_for_boundary({OWN},receipt,runner,reader=mock.Mock(),measure=measure)
            measure.assert_not_called()
            self.assertEqual(receipt['status'],'failed')

    def test_low_disk_and_existing_log_overflow_stop_before_pass(self):
        for free,size,code in ((1024**3,0,'free-disk-reserve'),
                               (90*1024**3,268435457,'private-log-limit')):
            with tempfile.TemporaryDirectory() as temporary:
                runner=self.runner(Path(temporary));receipt={}
                if size:
                    runner.records=[{'stage':'fixed'}]
                    with (runner.task/'fixed.private.log').open('wb') as stream:stream.truncate(size)
                def measure(baseline,value,observed):
                    value.update(status='passed',newCandidateIdentities=0);return set()
                with mock.patch.object(cc,'resources',return_value=activity(set())), \
                     mock.patch.object(cc.shutil,'disk_usage',return_value=SimpleNamespace(free=free)), \
                     self.assertRaisesRegex(Failure,code):
                    cc.wait_for_boundary({OWN},receipt,runner,reader=mock.Mock(),measure=measure)
                self.assertIsNone(receipt['finalBoundary'])

    @unittest.skipUnless(sys.platform=='darwin','actual Darwin convergence gate')
    def test_actual_metadata_candidates_support_bound_task_measurement(self):
        from census_diagnostics import describe
        from darwin_activity import probe
        baseline=owned_processes()
        roles=[(row,describe(row,baseline,baseline,set())) for row in sorted(baseline)]
        metadata=[row for row,hint in roles if hint.get('role')=='metadata-worker']
        details={}
        value={'status':'failed','roleVerifiedMetadataCandidates':len(metadata),
               'roleUnavailableCandidates':sum(hint['status']!='identity-verified' for _,hint in roles),
               'sampledCandidates':min(len(metadata),16),'truncated':len(metadata)>16,
               'qualificationExclusion':False,'resource':None,'failureDiagnostic':details}
        try:
            resource=cc.metadata_resource_gate(set(metadata[:16]),probe(),details)
            value['resource']={key:item for key,item in resource.items() if not key.startswith('_')}
            value['measuredMetadataCandidates']=resource['_progress']['processes']
            self.assertGreater(resource['_progress']['processes'],0,
                               'representative metadata candidates all exited before task measurement')
            self.assertLessEqual(resource['rssMiB'],10240)
            value.update(status='passed',failureDiagnostic=None)
        finally:
            print(json.dumps({'censusMetadataResourceGate':value}),flush=True)

    @unittest.skipUnless(sys.platform=='darwin','actual Darwin convergence gate')
    def test_actual_held_child_blocks_until_exit_reap_and_strict_zero(self):
        with tempfile.TemporaryDirectory() as temporary:
            task=Path(temporary);runner=self.runner(task);receipt={}
            baseline=owned_processes()
            read_fd,write_fd=os.pipe();child=None
            try:
                child=subprocess.Popen([sys.executable,'-c',
                    'import os,select,sys; fd=int(sys.argv[1]); select.select([fd],[],[],30); os.read(fd,1)',str(read_fd)],
                    pass_fds=(read_fd,),start_new_session=True,env=clean_environment(task),
                    stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                os.close(read_fd);read_fd=None
                released=False
                def release(seconds):
                    nonlocal released,write_fd
                    if not released:
                        os.close(write_fd);write_fd=None;child.wait(timeout=5);released=True
                    else:
                        # Keep the real strict census; unrelated new candidates cannot be waived.
                        import time;time.sleep(seconds)
                def measure(actual,value,observed):
                    rows=measure_owned_boundary(actual,value,observed)
                    if not released:self.assertIn(child.pid,{row[0] for row in rows})
                    return rows
                cc.wait_for_boundary(baseline,receipt,runner,deadline_minutes=1,measure=measure,sleep=release)
                self.assertGreater(receipt['samples'][0]['boundary']['newCandidateIdentities'],0)
                self.assertEqual(receipt['finalBoundary']['newCandidateIdentities'],0)
                self.assertIsNotNone(child.returncode)
                self.assertFalse(Runner.group_alive(child.pid))
            finally:
                if read_fd is not None:os.close(read_fd)
                if write_fd is not None:os.close(write_fd)
                if child is not None:
                    try:child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        child.kill();child.wait(timeout=5)


class CandidateRecheckGuards(unittest.TestCase):
    def missing(self,row=ROW):
        return {'status':'unavailable','reason':'short-task-info','fallbackReason':'measured',
                '_birth':row[:3],'taskQuery':{'bytes':0,'expectedBytes':232,'errno':errno.ESRCH},
                'bsdQuery':{'bytes':136,'expectedBytes':136,'errno':0},
                'esrchRecheck':{'firstTaskQuery':{'bytes':0,'expectedBytes':232,'errno':errno.ESRCH},
                    'firstBSDQuery':{'bytes':136,'expectedBytes':136,'errno':0},
                    'sameGeneration':True,'outcome':'unavailable','reason':'short-task-info'}}

    def measured(self,row=ROW):
        return {'status':'measured','_birth':row[:3],'rssMiB':10.,'cpuSeconds':5.,'state':'S',
                'taskQuery':{'bytes':232,'expectedBytes':232,'errno':0}}

    def stable_bsd(self,pid,details=None):
        if details is not None:details.update(bytes=136,expectedBytes=136,errno=0)
        return 'measured',bsd(OWN if pid==41 else ROW),None

    def test_bound_recheck_measures_once_and_preserves_internal_retry_trace(self):
        reader=mock.Mock();reader.bsd.side_effect=self.stable_bsd
        reader.read.side_effect=[self.measured(OWN),self.missing(),self.measured()]
        details={}
        with mock.patch.object(cc.os,'getpid',return_value=41):value=cc.resources({ROW},reader,details)
        self.assertEqual(value['rssMiB'],20.);self.assertEqual(value['_progress']['rssMiB'],10.)
        self.assertEqual(reader.read.call_count,3);self.assertEqual(len(details['readDiagnostics']),2)
        self.assertEqual(value['resourceRecheckCandidates'],1)
        self.assertEqual(value['resourceRecheckDiagnostics'][0]['readDiagnostics'],details['readDiagnostics'])
        self.assertTrue(details['retryIdentityMatches'])
        self.assertEqual(details['readDiagnostics'][0]['esrchRecheck']['firstTaskQuery']['errno'],errno.ESRCH)
        self.assertEqual(details['readDiagnostics'][1]['status'],'measured')

    def test_anchor_replacement_credential_change_and_invalid_bsd_fail(self):
        bad=[('measured',bsd((42,124,4,501,501)),None),
             ('measured',bsd((42,123,4,0,501)),None),
             ('measured',bsd((42,123,4,501,0)),None),
             ('gone',bsd((42,124,4,501,501)),'bsd-zombie'),
             ('gone',bsd((42,123,4,0,501)),'bsd-zombie'),
             ('unavailable',None,'permission'),('unavailable',None,'short-bsd-info'),
             ('unavailable',None,'invalid-identity'),('unavailable',None,'invalid-bsd-state')]
        for anchor in bad:
            reader=mock.Mock();reader.read.return_value=self.missing();reader.bsd.return_value=anchor
            with mock.patch.object(cc.os,'getpid',return_value=41), \
                 self.assertRaisesRegex(Failure,'census-wait-identity-unavailable'):
                cc.candidate_read(ROW,reader,{})
            self.assertEqual(reader.read.call_count,1)

    def test_permission_other_errno_and_unbound_missing_do_not_retry(self):
        changes=[{'reason':'permission','taskQuery':{'bytes':0,'expectedBytes':232,'errno':code}} for code in (1,13)]
        changes += [{'taskQuery':{'bytes':0,'expectedBytes':232,'errno':5}},
                    {'taskQuery':{'bytes':1,'expectedBytes':232,'errno':3}},
                    {'taskQuery':{'bytes':0,'expectedBytes':999,'errno':3}},
                    {'_birth':(42,124,4)},{'fallbackReason':'permission'},
                    {'status':'gone','reason':'generation-replaced'}]
        for change in changes:
            reader=mock.Mock();reader.read.return_value={**self.missing(),**change}
            with mock.patch.object(cc.os,'getpid',return_value=41):value=cc.candidate_read(ROW,reader,{})
            self.assertNotIn('_goneConfirmed',value);reader.bsd.assert_not_called()
            self.assertEqual(reader.read.call_count,1)

    def test_persistent_missing_is_bounded_and_cannot_produce_resource_totals(self):
        reader=mock.Mock();reader.bsd.side_effect=self.stable_bsd
        reader.read.side_effect=[self.measured(OWN),self.missing(),self.missing()]
        details={'rssMiB':None,'cpuSeconds':None}
        with mock.patch.object(cc.os,'getpid',return_value=41), \
             self.assertRaisesRegex(Failure,'census-wait-resource-unavailable'):
            cc.resources({ROW},reader,details)
        self.assertEqual(reader.read.call_count,3);self.assertEqual(len(details['readDiagnostics']),2)
        self.assertIsNone(details['rssMiB']);self.assertIsNone(details['cpuSeconds'])

    def test_fresh_anchor_accepts_only_matching_zombie_or_independent_esrch(self):
        for anchor in [('gone',bsd(ROW),'bsd-zombie'),('gone',None,'signal-zero-esrch')]:
            reader=mock.Mock();reader.read.return_value=self.missing();reader.bsd.return_value=anchor
            with mock.patch.object(cc.os,'getpid',return_value=41):value=cc.candidate_read(ROW,reader,{})
            self.assertTrue(value['_goneConfirmed']);self.assertEqual(reader.read.call_count,1)

    def test_second_gone_read_requires_new_five_field_confirmation(self):
        for final,passes in [(ROW,True),((42,123,4,0,501),False),((42,124,4,501,501),False)]:
            reader=mock.Mock();reader.read.side_effect=[self.missing(),{'status':'gone','reason':'bsd-zombie','_birth':ROW[:3]}]
            reader.bsd.side_effect=[('measured',bsd(ROW),None),('gone',bsd(final),'bsd-zombie')]
            with mock.patch.object(cc.os,'getpid',return_value=41):
                if passes:self.assertTrue(cc.candidate_read(ROW,reader,{})['_goneConfirmed'])
                else:
                    with self.assertRaisesRegex(Failure,'census-wait-identity-changed'):cc.candidate_read(ROW,reader,{})

    def test_post_measurement_credentials_still_must_match_after_recheck(self):
        reader=mock.Mock();count=0
        def query(pid,details=None):
            nonlocal count
            if pid==42:
                count+=1
                if count==3:return 'measured',bsd((42,123,4,501,0)),None
            return self.stable_bsd(pid,details)
        reader.bsd.side_effect=query
        reader.read.side_effect=[self.measured(OWN),self.missing(),self.measured()]
        with mock.patch.object(cc.os,'getpid',return_value=41), \
             self.assertRaisesRegex(Failure,'census-wait-identity-changed'):
            cc.resources({ROW},reader,{})

    def test_self_cannot_retry_or_receive_gone_credit(self):
        for value in [self.missing(OWN),{'status':'gone','reason':'bsd-zombie','_birth':OWN[:3]}]:
            reader=mock.Mock();reader.read.return_value=value;reader.bsd.side_effect=self.stable_bsd
            with mock.patch.object(cc.os,'getpid',return_value=41), \
                 self.assertRaisesRegex(Failure,'census-wait-resource-unavailable'):
                cc.resources(set(),reader,{})
            self.assertEqual(reader.read.call_count,1)

    def test_private_trace_extensions_and_reasons_are_not_projected(self):
        value=self.missing();value['pidTag']='PRIVATE';value['bsdQuery']['private']='PRIVATE'
        value['esrchRecheck'].update(reason='PRIVATE',identityTag='PRIVATE',private=1234567)
        value['esrchRecheck']['firstTaskQuery']['private']='PRIVATE'
        result=cc.read_diagnostic(value,ROW)
        self.assertEqual(result['esrchRecheck']['reason'],'unknown-query-reason')
        self.assertNotIn('PRIVATE',json.dumps(result));self.assertNotIn('1234567',json.dumps(result))

    def test_recheck_task_permission_remains_a_fixed_task_permission_failure(self):
        reader=mock.Mock();reader.bsd.side_effect=self.stable_bsd
        reader.read.side_effect=[self.measured(OWN),self.missing(),
            {'status':'unavailable','reason':'permission','taskQuery':{'bytes':0,'expectedBytes':232,'errno':1}}]
        with mock.patch.object(cc.os,'getpid',return_value=41), \
             self.assertRaisesRegex(Failure,'census-wait-metadata-task-permission'):
            cc.metadata_resource_gate({ROW},reader,{})
        self.assertEqual(reader.read.call_count,3)

    def test_success_keeps_last_sixteen_rechecks_and_all_candidate_counts(self):
        rows={(pid,123,4,501,501) for pid in range(42,62)};reader=mock.Mock();seen={}
        def query(pid,details=None):
            return 'measured',bsd(OWN if pid==41 else (pid,123,4,501,501)),None
        def read(pid,**kwargs):
            row=OWN if pid==41 else (pid,123,4,501,501)
            seen[pid]=seen.get(pid,0)+1
            return self.missing(row) if pid!=41 and seen[pid]==1 else self.measured(row)
        reader.bsd.side_effect=query;reader.read.side_effect=read
        with mock.patch.object(cc.os,'getpid',return_value=41):value=cc.resources(rows,reader,{})
        self.assertEqual(value['resourceRecheckCandidates'],20)
        self.assertEqual(len(value['resourceRecheckDiagnostics']),16)
        self.assertEqual([v['candidateIndex'] for v in value['resourceRecheckDiagnostics']],list(range(5,21)))
        self.assertEqual(value['rssMiB'],210.)

    def cleanup_mock(self):
        child=mock.Mock(returncode=None)
        child.poll.return_value=None
        child.stdin.closed=False;child.stdout.closed=False
        return child

    def test_controlled_child_kills_after_timeout_and_preserves_primary(self):
        child=self.cleanup_mock();primary=AssertionError('original-controlled-failure')
        def reaped(**kwargs):child.returncode=-9;return -9
        calls=0
        def wait(**kwargs):
            nonlocal calls
            calls+=1
            if calls==1:raise subprocess.TimeoutExpired(['private'],5)
            return reaped(**kwargs)
        child.wait.side_effect=wait
        with mock.patch.object(Runner,'group_alive',return_value=False):
            with self.assertRaises(AssertionError) as caught:
                with controlled_child(child):raise primary
        self.assertIs(caught.exception,primary)
        child.terminate.assert_called_once();child.kill.assert_called_once()
        self.assertEqual(child.wait.call_args_list,[mock.call(timeout=5),mock.call(timeout=5)])
        child.stdin.close.assert_called_once();child.stdout.close.assert_called_once()

    def test_controlled_child_second_timeout_and_stream_failure_preserve_primary(self):
        child=self.cleanup_mock();primary=RuntimeError('original-api-failure')
        child.wait.side_effect=subprocess.TimeoutExpired(['private'],5)
        child.stdin.close.side_effect=OSError('private')
        with self.assertRaises(RuntimeError) as caught:
            with controlled_child(child):raise primary
        self.assertIs(caught.exception,primary)
        self.assertEqual(primary.__notes__,['controlled-child-cleanup-failed'])
        child.kill.assert_called_once();self.assertEqual(child.wait.call_count,2)
        child.stdout.close.assert_called_once()

    def test_controlled_child_cleanup_failure_is_not_silent_without_primary(self):
        child=self.cleanup_mock();child.wait.side_effect=subprocess.TimeoutExpired(['private'],5)
        with self.assertRaisesRegex(AssertionError,'^controlled-child-cleanup-failed$'):
            with controlled_child(child):pass
        child.kill.assert_called_once();self.assertEqual(child.wait.call_count,2)
        child.stdin.close.assert_called_once();child.stdout.close.assert_called_once()

    def actual_case(self,exit_after_unknown):
        from darwin_activity import DarwinActivity,probe
        from monitor_ps_lifetime import child_status
        real=probe();forced=0;task_calls=0
        with controlled_child(subprocess.Popen([sys.executable,'-c',"import sys;print('ready',flush=True);sys.stdin.buffer.read(1)"],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,start_new_session=True)) as child:
            self.assertTrue(select.select([child.stdout],[],[],5)[0])
            self.assertEqual(child.stdout.readline(),b'ready\n')
            state,before,_=real.bsd(child.pid);self.assertEqual(state,'measured');row=cc.identity(before)
            def query(pid,flavor,arg,buffer,size):
                nonlocal forced,task_calls
                if pid==child.pid and flavor==2:task_calls+=1
                if pid==child.pid and flavor==2 and forced<2:
                    forced+=1;ctypes.set_errno(errno.ESRCH);return 0
                return real.lib.proc_pidinfo(pid,flavor,arg,buffer,size)
            sensor=DarwinActivity(lib=SimpleNamespace(proc_pidinfo=query))
            native_read=sensor.read
            def read(pid,*args,**kwargs):
                value=native_read(pid,*args,**kwargs)
                if exit_after_unknown and pid==child.pid and value['status']=='unavailable':
                    child.stdin.write(b'1');child.stdin.close();deadline=time.monotonic()+5
                    while not child_status(child.pid)['exitPending']:
                        self.assertLess(time.monotonic(),deadline);time.sleep(.01)
                    self.assertIsNone(child.returncode)
                return value
            sensor.read=read;details={};observed={};original=cc.candidate_read
            def capture(actual,reader,evidence):
                value=original(actual,reader,evidence)
                if actual==row:observed.update(copy.deepcopy(evidence))
                return value
            with mock.patch.object(cc,'candidate_read',side_effect=capture):
                result=cc.resources({row},sensor,details)
            self.assertEqual(forced,2)
            self.assertEqual(task_calls,2 if exit_after_unknown else 3)
            self.assertEqual(result['resourceRecheckCandidates'],1)
            self.assertTrue(observed['readDiagnostics'][0]['esrchRecheck']['sameGeneration'])
            self.assertEqual(result['confirmedGoneCandidates'],int(exit_after_unknown))
            self.assertEqual(result['_progress']['processes'],int(not exit_after_unknown))
            self.assertEqual(len(observed['readDiagnostics']),1 if exit_after_unknown else 2)
            if not exit_after_unknown:child.stdin.write(b'1');child.stdin.close()
            self.assertEqual(child.wait(timeout=5),0);self.assertFalse(Runner.group_alive(child.pid))

    @unittest.skipUnless(sys.platform=='darwin','actual Darwin task recheck')
    def test_actual_live_child_recovers_after_two_controlled_missing_task_reads(self):
        self.actual_case(False)

    @unittest.skipUnless(sys.platform=='darwin','actual Darwin task recheck')
    def test_actual_unreaped_child_is_gone_only_after_fresh_bound_bsd(self):
        self.actual_case(True)
