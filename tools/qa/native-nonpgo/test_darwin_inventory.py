# SPDX-License-Identifier: MPL-2.0
import ctypes
import errno
import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest import mock

import darwin_inventory as inv
from process_census import BINDING_KEYS
from support import Failure,Runner
from test_process_census import layout,raw


class Query:
    def __init__(self,responses):self.responses=list(responses);self.calls=[]
    def __call__(self,mib,count,output,length,newp,newlen):
        assert count==4 and newp is None and newlen==0
        self.calls.append((tuple(mib),'size' if output is None else 'data'))
        item=self.responses.pop(0)
        size=ctypes.cast(length,ctypes.POINTER(ctypes.c_size_t))
        if output is not None and item.get('data'):
            assert len(item['data'])<=size[0]
            ctypes.memmove(output,item['data'],len(item['data']))
        size[0]=item.get('size',len(item.get('data',b'')))
        ctypes.set_errno(item.get('errno',0))
        return item.get('rc',0)


def reply(pids=None):
    pids=[42] if pids is None else pids
    return {'status':'measured','group':900,'pids':pids,'queries':[],
            'measurement':{'api':'sysctl-kern-proc-pgrp','snapshotBytes':len(pids)*256,
                'snapshotEntries':len(pids),'selfBindingVerified':True,'layoutSHA256':'a'*64}}


class DarwinInventoryGuards(unittest.TestCase):
    def read(self,responses):
        queries=[];query=Query(responses)
        data=inv.read_records(layout(),2,900,queries,allow_empty=True,system=SimpleNamespace(sysctl=query))
        return data,query,queries

    def test_read_only_group_selector_and_complete_raw_records(self):
        data,query,queries=self.read([{'size':256},{'data':raw({})}])
        self.assertEqual(inv.pids_from_records(data,layout()),[41])
        self.assertEqual(query.calls,[((1,14,2,900),'size'),((1,14,2,900),'data')])
        self.assertEqual(queries[-1]['errno'],0)

    def test_partial_enomem_is_discarded_before_complete_fresh_retry(self):
        data,query,queries=self.read([{'size':256},{'data':raw({'pid':999}),'rc':-1,'errno':errno.ENOMEM},
                                     {'size':256},{'data':raw({'pid':42})}])
        self.assertEqual(inv.pids_from_records(data,layout()),[42])
        self.assertEqual(len(query.calls),4)
        self.assertEqual(queries[1]['errno'],errno.ENOMEM)

    def test_permission_never_switches_to_another_api(self):
        for responses in ([{'rc':-1,'errno':errno.EPERM}],
                          [{'size':256},{'rc':-1,'errno':errno.EACCES}]):
            with self.assertRaisesRegex(Failure,'inventory-query-permission'):
                self.read(responses)

    def test_size_partial_duplicate_and_invalid_identity_are_not_empty(self):
        for responses in ([{'size':inv.CAP+1}],[{'size':257}],
                          [{'size':256},{'data':raw({})[:-1]}]):
            with self.assertRaises(Failure):self.read(responses)
        for data in (raw({},{'pid':41}),raw({'pid':0}),raw({'startSec':0}),raw({'state':6}),
                     raw({'uid':2**32-1})):
            with self.assertRaises(Failure):inv.pids_from_records(data,layout())

    def test_zombies_and_other_owners_are_retained_for_existing_membership_checks(self):
        self.assertEqual(inv.pids_from_records(raw({}, {'pid':42,'state':5},
                          {'pid':43,'uid':0,'ruid':0}),layout()),[41,42,43])

    def test_empty_success_is_explicit_and_not_a_resource_measurement(self):
        data,_,_=self.read([{'size':0},{'size':0}])
        self.assertEqual(data,b'')
        self.assertEqual(inv.decode_reply(json.dumps(reply([])),900)['pids'],[])
        from darwin_activity import DarwinActivity
        sensor=object.__new__(DarwinActivity)
        sensor.numer=sensor.denom=1
        result=sensor.group('',900)
        self.assertFalse(result['sampleComplete'])
        self.assertIsNone(result['rssMiB'])

    def test_group_arguments_reject_zero_negative_boolean_and_overflow(self):
        for pgid in (0,-1,True,2**31):
            with self.assertRaisesRegex(Failure,'inventory-group-id'):
                inv.collect(pgid,[])
        for selector,subject in ((0,900),(3,900),(True,900),(2,0),(2,True)):
            query=Query([])
            with self.assertRaisesRegex(Failure,'inventory-selector'):
                inv.read_records(layout(),selector,subject,[],system=SimpleNamespace(sysctl=query))
            self.assertEqual(query.calls,[])

    def test_self_snapshot_binds_birth_credentials_and_exact_count(self):
        reader=mock.Mock();reader.bsd.return_value=('measured',
            SimpleNamespace(pid=41,uid=501,ruid=501,start_sec=123,start_usec=4),None)
        responses=[{'size':256},{'data':raw({})},{'size':256},{'data':raw({'pid':42,'uid':0})}]
        query=Query(responses)
        with mock.patch.object(inv.os,'getpid',return_value=41), \
             mock.patch.object(inv.os,'geteuid',return_value=501),mock.patch.object(inv.os,'getuid',return_value=501):
            value=inv.collect(900,[],layout=layout(),reader=reader,system=SimpleNamespace(sysctl=query))
        self.assertEqual(value['pids'],[42])
        self.assertEqual([call[0][2] for call in query.calls],[1,1,2,2])

    def test_protocol_rejects_malformed_private_payload_without_echo(self):
        values=['PRIVATE_SENTINEL',json.dumps({**reply(),'pids':[42,42]}),
                json.dumps({**reply(),'group':901}),json.dumps({**reply(),'private':'hidden'})]
        for value in values:
            with self.assertRaises((Failure,ValueError)):inv.decode_reply(value,900)

    def native_env(self):
        return {**{key:'bound-'+key for key in BINDING_KEYS},'TASK_ROOT':'/private/task',
                'FLOORP_CENSUS_ABI':'/private/task/census-abi.json','GITHUB_TOKEN':'private-token'}

    def test_primary_calls_bounded_helper_with_no_token_or_ps(self):
        complete={'sampleComplete':True,'rssMiB':1,'cpuSeconds':1,'rssLowerBoundMiB':1,
                  'cpuLowerBoundSeconds':1,'processes':1,'states':['R'],'_pids':((42,123,4),),
                  'unknownKinds':{},'formatExamples':[]}
        with mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,self.native_env()), \
             mock.patch('support.subprocess.run',return_value=mock.Mock(stdout=json.dumps(reply()))) as command, \
             mock.patch('darwin_activity.probe') as sensor:
            sensor.return_value.group.return_value=complete
            value=Runner.group_activity(900)
        self.assertTrue(value['sampleComplete'])
        argv=command.call_args.args[0]
        self.assertEqual(argv[0],sys.executable)
        self.assertTrue(argv[1].endswith('darwin_inventory.py'))
        self.assertNotIn('/bin/ps',argv)
        self.assertEqual(command.call_args.kwargs['timeout'],3)
        self.assertNotIn('GITHUB_TOKEN',command.call_args.kwargs['env'])
        sensor.return_value.group.assert_called_once_with('900 42\n',900)

    def test_missing_abi_is_unknown_without_bootstrap_fallback(self):
        with mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,{},clear=True), \
             mock.patch('support.subprocess.run') as command,mock.patch('support.time.sleep'):
            value=Runner.group_activity(900)
        self.assertFalse(value['sampleComplete']);self.assertIsNone(value['rssMiB'])
        self.assertEqual(value['queryFailure']['failureCode'],'inventory-abi-missing')
        self.assertEqual(value['sampleAttempts'],3)
        command.assert_not_called()

    def test_helper_failure_codes_errno_and_phase_survive_without_pids(self):
        data={'status':'unavailable','pids':None,'group':900,'phase':'group-snapshot',
              'queries':[{'query':'group-data','attempt':1,'returnCode':-1,'errno':1,'bytes':0}],
              'failureCode':'inventory-query-permission','errorType':'failure'}
        with mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,self.native_env()), \
             mock.patch('support.subprocess.run',return_value=mock.Mock(stdout=json.dumps(data))), \
             mock.patch('support.time.sleep'),mock.patch('darwin_activity.probe') as sensor:
            value=Runner.group_activity(900)
        self.assertFalse(value['sampleComplete']);self.assertIsNone(value['rssMiB'])
        self.assertEqual(value['queryFailure']['phase'],'group-snapshot')
        self.assertEqual(value['queryFailure']['queries'][0]['errno'],1)
        sensor.assert_not_called()

    def test_three_helper_timeouts_keep_null_measurements_and_exact_budget(self):
        with mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,self.native_env()), \
             mock.patch('support.subprocess.run',side_effect=subprocess.TimeoutExpired('private',3)) as command, \
             mock.patch('support.time.sleep'):
            value=Runner.group_activity(900)
        self.assertEqual(command.call_count,3);self.assertIsNone(value['rssMiB'])
        self.assertEqual(value['queryFailure']['operation'],'kern-proc-pgrp-inventory')
        self.assertEqual(len(value['partialSamples']),3)
        self.assertNotIn('private',json.dumps(value))

    def test_nonzero_or_malformed_helper_never_calls_ps_or_counter_probe(self):
        for side in (subprocess.CalledProcessError(7,'private'),mock.Mock(stdout='PRIVATE_SENTINEL')):
            with mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,self.native_env()), \
                 mock.patch('support.subprocess.run',side_effect=side if isinstance(side,Exception) else None,
                            return_value=side) as command, mock.patch('support.time.sleep'), \
                 mock.patch('darwin_activity.probe') as sensor:
                value=Runner.group_activity(900)
            self.assertEqual(command.call_count,3);sensor.assert_not_called()
            self.assertFalse(value['sampleComplete']);self.assertIsNone(value['rssMiB'])
            self.assertNotIn('PRIVATE',json.dumps(value))

    @unittest.skipUnless(sys.platform=='darwin','actual header-bound Darwin group inventory')
    def test_actual_group_contains_exact_owned_child_and_live_resource_measurement(self):
        child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],start_new_session=True,
                               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            data=inv.collect(child.pid,[])
            self.assertIn(child.pid,data['pids'])
            value=Runner.group_activity(child.pid)
            self.assertTrue(value['sampleComplete'])
            self.assertGreater(value['rssMiB'],0)
            self.assertEqual(value['inventoryBackend'],'kern-proc-pgrp-helper')
        finally:
            from census_experiment import cleanup_child
            cleanup=cleanup_child(child)
            self.assertTrue(cleanup['leaderReaped'])
            self.assertEqual(cleanup['groupCleanup'],'verified-gone')
            self.assertIsNone(cleanup['cleanupFailure'])

    @unittest.skipUnless(sys.platform=='darwin','actual unreaped Darwin zombie inventory')
    def test_actual_unreaped_child_remains_in_inventory_until_exact_reap(self):
        from monitor_ps_lifetime import child_status
        import time
        child=subprocess.Popen([sys.executable,'-c','import sys;sys.stdin.buffer.read(1)'],
                               start_new_session=True,stdin=subprocess.PIPE,
                               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            self.assertFalse(child_status(child.pid)['exitPending'])
            self.assertIn(child.pid,inv.collect(child.pid,[])['pids'])
            child.stdin.close()
            deadline=time.monotonic()+5
            while not child_status(child.pid)['exitPending']:
                self.assertLess(time.monotonic(),deadline);time.sleep(0.01)
            self.assertTrue(child_status(child.pid)['matchingChild'])
            os.kill(child.pid,0)  # Individual signal zero still succeeds for the zombie.
            with self.assertRaises(PermissionError) as denied:Runner.group_alive(child.pid)
            self.assertEqual(denied.exception.errno,errno.EPERM)
            data=inv.collect(child.pid,[])
            self.assertIn(child.pid,data['pids'])
            value=Runner.group_activity(child.pid)
            self.assertTrue(value['sampleComplete'])
            self.assertGreaterEqual(value['confirmedGone'],1)
            self.assertEqual(value['rssMiB'],0)
        finally:
            if not child.stdin.closed:child.stdin.close()
            child.wait(timeout=5)
            self.assertFalse(Runner.group_alive(child.pid))

    def test_actual_query_timeout_reaps_exact_helper_before_retry(self):
        original_run,original_popen=subprocess.run,subprocess.Popen
        children=[]
        def launch(*args,**kwargs):
            child=original_popen(*args,**kwargs);children.append(child);return child
        def query(args,**kwargs):
            self.assertTrue(args[1].endswith('darwin_inventory.py'))
            self.assertEqual(kwargs['timeout'],3)
            if not children:
                return original_run([sys.executable,'-c','import time;time.sleep(30)'],
                                    **{**kwargs,'timeout':0.05})
            self.assertIsNotNone(children[0].returncode)
            with self.assertRaises(ChildProcessError):os.waitpid(children[0].pid,os.WNOHANG)
            return mock.Mock(stdout=json.dumps(reply()))
        complete={'sampleComplete':True,'rssMiB':1,'cpuSeconds':1,'rssLowerBoundMiB':1,
                  'cpuLowerBoundSeconds':1,'processes':1,'states':['R'],'_pids':(),
                  'unknownKinds':{},'formatExamples':[]}
        with mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,self.native_env()), \
             mock.patch('support.subprocess.Popen',side_effect=launch), \
             mock.patch('support.subprocess.run',side_effect=query),mock.patch('support.time.sleep'), \
             mock.patch('darwin_activity.probe') as sensor:
            sensor.return_value.group.return_value=complete
            value=Runner.group_activity(900)
        self.assertTrue(value['sampleComplete']);self.assertEqual(value['sampleAttempts'],2)
        self.assertEqual(len(children),1);self.assertIsNotNone(children[0].returncode)

    def test_bootstrap_flag_cannot_launch_an_unrelated_stage(self):
        import tempfile,time
        with tempfile.TemporaryDirectory() as directory:
            task=Path(directory)
            limits=json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
            runner=Runner(task,time.time(),limits);runner.inventory_bootstrap=True
            with mock.patch('support.subprocess.Popen') as launch:
                with self.assertRaisesRegex(Failure,'census-abi-bootstrap-stage'):
                    runner.run('unrelated',['ignored'],task,{},360)
            launch.assert_not_called()
