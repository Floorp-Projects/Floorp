# SPDX-License-Identifier: MPL-2.0
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import census_convergence as cc
from checkpoint import owned_processes,measure_owned_boundary
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
