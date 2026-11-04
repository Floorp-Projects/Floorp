# SPDX-License-Identifier: MPL-2.0
"""Local diagnostic controls; these do not execute or qualify Darwin APIs."""
import contextlib
import copy
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import darwin_inventory as inv
import helper_import_preflight as hp
import process_census as pc
from qa_core import Failure
from support import Runner
from test_darwin_inventory import Query, reply
from test_process_census import layout, raw


ABI_PHASES=('abi-path','abi-files','abi-json','abi-schema','abi-binding-env',
            'abi-origin-hash','abi-source-hash','abi-inventory-hash','abi-binding-check',
            'abi-binary-hash','abi-layout','abi-complete')


def prefix(phases):
    return ''.join(json.dumps({'inventoryProgress':phase,'sequence':i+1,
        'elapsedNanoseconds':i*1000000,'cpuNanoseconds':i*1000})+'\n'
        for i,phase in enumerate(phases))


@contextlib.contextmanager
def abi_fixture():
    with tempfile.TemporaryDirectory() as temporary:
        task=Path(temporary).resolve()
        (task/'job-origin.json').write_text('{"job":1}\n')
        binary=task/'census-abi';binary.write_bytes(b'local test inspector');binary.chmod(0o500)
        path=task/'census-abi.json'
        env={**{key:'local-'+key for key in pc.BINDING_KEYS},'TASK_ROOT':str(task),
             'FLOORP_CENSUS_ABI':str(path)}
        with mock.patch.dict(os.environ,env):
            value={'schema':1,'binding':pc.binding(task),'binarySHA256':pc.digest(binary),
                   'layout':layout(),'layoutSHA256':pc.hashlib.sha256(pc.canonical(layout())).hexdigest()}
            path.write_text(json.dumps(value));path.chmod(0o400)
            yield task,path,env


class ProgressProtocolTests(unittest.TestCase):
    def test_timed_prefix_and_partial_line_keep_only_complete_safe_boundaries(self):
        output=prefix(('project-imports','imports-complete','abi-json'))
        expected=inv.progress_reply(output)
        self.assertEqual(expected['phase'],'abi-json');self.assertEqual(expected['events'],3)
        self.assertEqual(expected['timings'][-1],{'phase':'abi-json','elapsedNanoseconds':2000000,
                                                'cpuNanoseconds':2000})
        for data in (output,output.encode(),output+'{"PRIVATE":'):
            self.assertEqual(inv.progress_reply(data),expected)
        self.assertNotIn('PRIVATE',json.dumps(expected))

    def test_bad_times_sequence_keys_and_mixed_schemas_cannot_qualify_final_reply(self):
        first=json.loads(prefix(('project-imports',)))
        invalid=[]
        for key in ('elapsedNanoseconds','cpuNanoseconds'):
            for bad in (True,-1,2**63,1.5,None,'PRIVATE'):
                invalid.append({**first,key:bad})
            row=dict(first);del row[key];invalid.append(row)
        invalid.extend(({**first,'sequence':True},{**first,'sequence':2},
                        {**first,'PRIVATE':'secret'},{**first,'inventoryProgress':'PRIVATE'}))
        for row in invalid:
            text=json.dumps(row)+'\n'
            with self.subTest(row=row):
                self.assertEqual(inv.progress_reply(text),{'phase':'unobserved','events':0})
                with self.assertRaises(Failure):inv.decode_reply(text+json.dumps(reply()),900)
        good=prefix(('project-imports','imports-complete'))
        rows=[json.loads(line) for line in good.splitlines()]
        for key in ('elapsedNanoseconds','cpuNanoseconds'):
            changed=copy.deepcopy(rows);changed[0][key]=100;changed[1][key]=50
            text=''.join(json.dumps(row)+'\n' for row in changed)
            self.assertEqual(inv.progress_reply(text)['events'],1)
            with self.assertRaises(Failure):inv.decode_reply(text+json.dumps(reply()),900)
        old='{"inventoryProgress":"project-imports","sequence":1}\n'
        mixed=old+json.dumps(rows[1])+'\n'
        self.assertEqual(inv.progress_reply(mixed),{'phase':'project-imports','events':1})
        with self.assertRaises(Failure):inv.decode_reply(mixed+json.dumps(reply()),900)

    def test_old_two_key_schema_is_unchanged_and_progress_alone_is_never_a_reply(self):
        old='{"inventoryProgress":"project-imports","sequence":1}\n'
        self.assertEqual(inv.progress_reply(old),{'phase':'project-imports','events':1})
        for text in (old,prefix(('project-imports','imports-complete','abi-complete'))):
            with self.assertRaises(Failure):inv.decode_reply(text,900)

    def test_limits_remain_32_events_and_64kib(self):
        self.assertEqual(inv.MAX_PROGRESS_EVENTS,32)
        self.assertEqual(inv.progress_reply(prefix(('abi-path',)*32))['events'],32)
        for text in (prefix(('abi-path',)*33),'x'*65537):
            self.assertEqual(inv.progress_reply(text),{'phase':'unobserved','events':0})

    def test_emitter_has_monotonic_and_process_cpu_clocks_without_identity_fields(self):
        stream=io.StringIO()
        with mock.patch.object(inv,'_PROGRESS_SEQUENCE',0), \
             mock.patch.object(inv,'_PROGRESS_START',100),mock.patch.object(inv,'_PROGRESS_CPU_START',200), \
             mock.patch.object(inv,'time',SimpleNamespace(monotonic_ns=lambda:150,process_time_ns=lambda:220)), \
             contextlib.redirect_stdout(stream):
            inv._emit_progress('abi-path')
        value=inv.progress_reply(stream.getvalue())
        self.assertEqual(value['timings'],[{'phase':'abi-path','elapsedNanoseconds':50,'cpuNanoseconds':20}])

    def test_public_monitor_and_cleanup_projections_keep_only_valid_timing_prefix(self):
        progress=inv.progress_reply(prefix(('project-imports','imports-complete','abi-origin-hash')))
        dirty={**progress,'PRIVATE':'secret','path':'/private/path'}
        outputs=[hp.cleanup_diagnostic_projection({'helperProgress':dirty})['helperProgress'],
                 hp.monitor_projection({'helperProgress':dirty})['helperProgress'],
                 hp.monitor_projection({'queryFailure':{'helperProgress':dirty}})['queryFailure']['helperProgress']]
        self.assertTrue(all(value==progress for value in outputs))
        for bad in (True,-1,2**63,None):
            malformed=copy.deepcopy(progress);malformed['timings'][-1]['cpuNanoseconds']=bad
            value=hp.monitor_projection({'queryFailure':{'helperProgress':malformed}})
            self.assertNotIn('timings',value['queryFailure']['helperProgress'])
            self.assertNotIn('PRIVATE',json.dumps(value))
        malformed=copy.deepcopy(progress);malformed['timings'][-1]['phase']='PRIVATE'
        self.assertIsNone(inv.timing_projection(malformed))
        malformed=copy.deepcopy(progress);malformed['timings'][0]['path']='PRIVATE'
        self.assertIsNone(inv.timing_projection(malformed))

    def test_completed_unavailable_nonzero_and_measured_queries_preserve_safe_timings(self):
        output=prefix(('project-imports','imports-complete','abi-json'))
        unavailable={'status':'unavailable','pids':None,'group':900,'queries':[],
                     'phase':'abi-json','failureCode':'inventory-abi-unavailable','errorType':'failure'}
        complete={'sampleComplete':True,'rssMiB':1,'cpuSeconds':1,'rssLowerBoundMiB':1,
                  'cpuLowerBoundSeconds':1,'processes':1,'states':['R'],'_pids':(),
                  'unknownKinds':{},'formatExamples':[]}
        for mode in ('unavailable','nonzero','measured'):
            side=subprocess.CalledProcessError(7,'PRIVATE',output=output) if mode=='nonzero' else None
            data=unavailable if mode=='unavailable' else reply()
            with abi_fixture(),mock.patch('support.sys.platform','darwin'), \
                 mock.patch('support.subprocess.run',side_effect=side,
                            return_value=mock.Mock(stdout=output+json.dumps(data))), \
                 mock.patch('support.time.sleep'),mock.patch('darwin_activity.probe') as probe:
                probe.return_value.group.return_value=complete
                value=Runner.group_activity(900)
            if mode=='measured':
                self.assertTrue(value['sampleComplete']);self.assertEqual(value['sampleAttempts'],1)
                observed=value['helperProgress']
            else:
                self.assertFalse(value['sampleComplete']);self.assertIsNone(value['rssMiB'])
                self.assertEqual(value['sampleAttempts'],3);probe.assert_not_called()
                observed=value['queryFailure']['helperProgress']
            self.assertEqual(observed,inv.progress_reply(output))
            projected=hp.monitor_projection(value)
            safe=projected['helperProgress'] if mode=='measured' else projected['queryFailure']['helperProgress']
            self.assertEqual(safe,observed);self.assertNotIn('PRIVATE',json.dumps(projected))


class ABIBoundaryTests(unittest.TestCase):
    def test_existing_validation_results_and_fixed_boundary_order_are_preserved(self):
        with abi_fixture() as (task,path,env):
            phases=[]
            self.assertEqual(pc.load_abi(path,progress=phases.append),layout())
            self.assertEqual(tuple(phases),ABI_PHASES)
            self.assertEqual(pc.load_abi(path),layout())
            with mock.patch.dict(os.environ,{'GITHUB_RUN_ID':'changed'}):
                with self.assertRaisesRegex(Failure,'census-abi-binding'):
                    pc.load_abi(path)

    def test_bad_schema_binding_binary_modes_layout_and_digest_remain_failures(self):
        with abi_fixture() as (task,path,env):
            original=json.loads(path.read_text())
            variants=[]
            for key,value in (('schema',2),('binding',{}),('binarySHA256','0'*64),
                              ('layoutSHA256','0'*64)):
                variants.append({**original,key:value})
            bad=copy.deepcopy(original);bad['layout']['fields']['pid']['offset']=99999;variants.append(bad)
            for value in variants:
                path.chmod(0o600);path.write_text(json.dumps(value));path.chmod(0o400)
                phases=[]
                with self.assertRaises(Failure):pc.load_abi(path,progress=phases.append)
                self.assertNotIn('abi-complete',phases)
            path.chmod(0o600);path.write_text(json.dumps(original));path.chmod(0o400)
            for subject,bad_mode,restore in ((path,0o600,0o400),(task/'census-abi',0o700,0o500)):
                subject.chmod(bad_mode)
                with self.assertRaisesRegex(Failure,'census-abi-file'):pc.load_abi(path)
                subject.chmod(restore)
            other=task/'other.json';other.symlink_to(path)
            with self.assertRaisesRegex(Failure,'census-abi-path'):pc.load_abi(other)

    def test_diagnostic_callback_error_is_not_swallowed_as_success(self):
        with abi_fixture() as (task,path,env):
            def fail(phase):
                if phase=='abi-origin-hash':raise OSError('PRIVATE')
            with self.assertRaises(OSError):pc.load_abi(path,progress=fail)

    def test_worst_case_native_enomem_retries_fit_original_32_event_budget(self):
        owned_uid=os.geteuid()
        responses=[]
        for subject in (raw({}),raw({'pid':42})):
            responses.extend([{'size':256},{'rc':-1,'errno':12},
                              {'size':256},{'rc':-1,'errno':12},
                              {'size':256},{'data':subject}])
        query=Query(responses);reader=mock.Mock()
        reader.bsd.return_value=('measured',SimpleNamespace(pid=41,uid=501,ruid=501,
                                                           start_sec=123,start_usec=4),None)
        phases=['project-imports','imports-complete']
        with abi_fixture(),mock.patch.object(inv.os,'getpid',return_value=41), \
             mock.patch.object(inv.os,'geteuid',return_value=501),mock.patch.object(inv.os,'getuid',return_value=501), \
             mock.patch('darwin_activity.probe',return_value=reader):
            # load_abi uses actual ownership; limit credential stubs to native
            # reads, rather than making a valid local ABI fixture appear foreign.
            original=inv.load_abi
            def owned_load(*args,**kwargs):
                with mock.patch.object(inv.os,'geteuid',return_value=owned_uid):
                    return original(*args,**kwargs)
            with mock.patch.object(inv,'load_abi',side_effect=owned_load):
                value=inv.collect(900,[],system=SimpleNamespace(sysctl=query),notify=phases.append)
        self.assertEqual(value['pids'],[42]);self.assertEqual(len(query.calls),12)
        self.assertEqual(len(phases),32)
        output=prefix(phases)
        self.assertEqual(inv.progress_reply(output)['events'],32)
        self.assertEqual(inv.decode_reply(output+json.dumps(value),900)['pids'],[42])


# Hooks live only in this local test child. Each selected operation sleeps after
# its real candidate marker, and before its real read/hash/validation. No
# production delay option or environment escape hatch is introduced.
SLOW_CHILD=r'''
import builtins,os,runpy,sys,time
root,task,mode,ready=sys.argv[1:]
sys.path.insert(0,root)
original_import=builtins.__import__
def delay():
 with open(ready,'w') as stream:stream.write('ready')
 time.sleep(30)
def importer(name,*args,**kwargs):
 if mode=='imports' and name=='ctypes':
  assert name not in sys.modules
  delay()
 result=original_import(name,*args,**kwargs)
 if name=='process_census':
  pc=sys.modules[name]
  original_digest=pc.digest
  def digest(path):
   phases={'job-origin.json':'abi-origin-hash','census_abi.c':'abi-source-hash',
           'darwin_inventory.py':'abi-inventory-hash','census-abi':'abi-binary-hash'}
   if phases.get(path.name)==mode:delay()
   return original_digest(path)
  pc.digest=digest
  if mode=='abi-json':
   read=pc.Path.read_text
   def read_text(path,*args,**kwargs):
    if path.name=='census-abi.json':delay()
    return read(path,*args,**kwargs)
   pc.Path.read_text=read_text
  if mode=='abi-layout':
   validate=pc.validate_layout
   def validate_layout(value):
    delay()
    return validate(value)
   pc.validate_layout=validate_layout
 return result
builtins.__import__=importer
sys.platform='darwin'  # stops before native APIs; not a Darwin success control
sys.argv=[root+'/darwin_inventory.py','--pgid','900']
if mode=='startup':delay()
runpy.run_path(sys.argv[0],run_name='__main__')
'''


class ActualDelayedChildTests(unittest.TestCase):
    def test_fresh_startup_import_and_real_abi_operations_have_distinct_timeout_prefixes(self):
        modes=('startup','imports','abi-json','abi-origin-hash','abi-source-hash',
               'abi-inventory-hash','abi-binary-hash','abi-layout')
        with abi_fixture() as (task,path,env):
            for mode in modes:
                with self.subTest(mode=mode):
                    ready=task/'ready';ready.unlink(missing_ok=True)
                    child=subprocess.Popen([sys.executable,'-c',SLOW_CHILD,str(Path(inv.__file__).parent),
                        str(task),mode,str(ready)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                        text=True,env={**os.environ,**env,'PYTHONDONTWRITEBYTECODE':'1'})
                    try:
                        deadline=time.monotonic()+5
                        while not ready.exists() and child.poll() is None and time.monotonic()<deadline:
                            time.sleep(0.01)
                        self.assertTrue(ready.exists(),'child did not reach deterministic delay hook')
                        with self.assertRaises(subprocess.TimeoutExpired):child.communicate(timeout=0.05)
                    finally:
                        if child.poll() is None:child.kill()
                        output,error=child.communicate(timeout=5)
                    self.assertIsNotNone(child.returncode)
                    with self.assertRaises(ChildProcessError):os.waitpid(child.pid,os.WNOHANG)
                    value=inv.progress_reply(output)
                    expected='unobserved' if mode=='startup' else 'project-imports' if mode=='imports' else mode
                    self.assertEqual(value['phase'],expected,error)
                    self.assertNotIn('PRIVATE',json.dumps(value))
                    if mode not in ('startup','imports'):
                        self.assertIn('imports-complete',[row['phase'] for row in value['timings']])
                        self.assertNotIn('abi-complete',[row['phase'] for row in value['timings']])
                    with self.assertRaises(Failure):inv.decode_reply(output,900)

    def test_three_real_3second_queries_exhaust_and_reap_before_next_attempt(self):
        original_run,original_popen=subprocess.run,subprocess.Popen
        children=[]
        def launch(*args,**kwargs):
            child=original_popen(*args,**kwargs);children.append(child);return child
        def query(argv,**kwargs):
            self.assertEqual(kwargs['timeout'],3)
            for child in children:
                self.assertIsNotNone(child.returncode)
                with self.assertRaises(ChildProcessError):os.waitpid(child.pid,os.WNOHANG)
            return original_run([sys.executable,'-c',
                'import sys,time;sys.stdout.write('+repr(prefix(('project-imports','imports-complete','abi-json')))+
                ');sys.stdout.flush();time.sleep(30)'],**kwargs)
        with abi_fixture() as (task,path,env),mock.patch('support.sys.platform','darwin'), \
             mock.patch('support.subprocess.Popen',side_effect=launch), \
             mock.patch('support.subprocess.run',side_effect=query),mock.patch('support.time.sleep'), \
             mock.patch('darwin_activity.probe') as probe:
            value=Runner.group_activity(900)
        self.assertEqual(len(children),3);self.assertFalse(value['sampleComplete'])
        self.assertEqual(value['sampleAttempts'],3);self.assertEqual(len(value['partialSamples']),3)
        probe.assert_not_called()
        for key in ('rssMiB','cpuSeconds','rssLowerBoundMiB','cpuLowerBoundSeconds','processes'):
            self.assertIsNone(value[key])
        for sample in value['partialSamples']:
            self.assertEqual(sample['queryFailure']['helperProgress']['phase'],'abi-json')
            self.assertEqual(sample['queryFailure']['timeoutSeconds'],3)
        for child in children:
            self.assertIsNotNone(child.returncode)
            with self.assertRaises(ChildProcessError):os.waitpid(child.pid,os.WNOHANG)


if __name__=='__main__':unittest.main()
