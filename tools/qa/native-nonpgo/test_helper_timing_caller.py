# SPDX-License-Identifier: MPL-2.0
"""Fail-closed caller qualification, with synthetic positive receipts and real negative controls."""
import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

import helper_timing_caller as caller
import darwin_inventory as inv
import helper_import_preflight as hp
import process_census as pc
import primary_anchor
from qa_core import Failure
from test_process_census import layout
from test_helper_import_preflight import primary_anchor as primary_proof, stage_row


def source_pins():
    return {str(p.relative_to(caller.HERE)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(caller.HERE.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}


def measured_workload():
    abi=layout();layout_sha=hashlib.sha256(pc.canonical(abi)).hexdigest()
    phases=['project-imports','imports-complete','abi-binding',*inv.ABI_PHASES,
        'reader-binding','self-bsd','self-snapshot','self-size','self-data',
        'group-snapshot','group-size','group-data','group-decode']
    progress={'phase':'group-decode','events':len(phases),'timings':[
        {'phase':p,'elapsedNanoseconds':n*1000000,'cpuNanoseconds':n*1000} for n,p in enumerate(phases)]}
    value={'status':'passed','seconds':60,'pairsPerPhase':6,'workers':2,'workerMiB':128,
        'maximumDiskMiB':64,'readyWorkers':2,'barriersReleased':2,'loadSeconds':60,
        'samplingReserveSeconds':8,'samples':[],'loadChecks':[],
        'pressureScope':'bounded-small-memory-and-disk-control','osCacheState':'unknown-cold-or-warm',
        'productionRSSReproduction':False,'osCacheFlushed':False,'exactOldProgramReproduction':False,
        'startupRepairEstablished':False,'originalCauseEstablished':False,'rawProcessMetadataPublished':False,
        'firstFailure':None,'firstControlFailure':None,'workerMeasurements':[],
        'cleanup':[{'index':i,'reaped':True,'exitCode':0,'failure':None,'streamCloseFailed':False} for i in range(2)],
        'temporaryDiskRemoval':{'status':'removed-after-exact-reap','failure':None}}
    for phase in ('idle','contended'):
        for ordinal in range(1,7):
            for variant in hp.VARIANTS:
                presence={'expectedWorkers':2,'runningWorkers':2,'complete':True,'failure':None} if phase=='contended' else None
                value['samples'].append({'phase':phase,'variant':variant,'ordinal':ordinal,
                    'status':'measured','timeoutObserved':False,'failure':None,'elapsedSeconds':.1,
                    'progress':copy.deepcopy(progress),
                    'cleanup':{'authority':'subprocess.run-exact-child','reaped':True,'killedOnTimeout':False,'failure':None},
                    'measurement':{'api':'sysctl-kern-proc-pgrp','snapshotBytes':256,'snapshotEntries':1,
                        'selfBindingVerified':True,'layoutSHA256':layout_sha},
                    'importGraph':{'schema':1,'variant':variant,'moduleCount':100,
                        'modules':{k:k!='support' or variant=='support-preimport' for k in hp.GRAPH_FIELDS}},
                    'loadBefore':copy.deepcopy(presence),'loadAfter':copy.deepcopy(presence)})
                value['loadChecks'].append({'phase':phase,'variant':variant,'ordinal':ordinal,
                    'before':copy.deepcopy(presence),'after':copy.deepcopy(presence)})
    value['counts']={phase:{variant:{'samples':6,'validMeasurements':6,'timeouts':0}
        for variant in hp.VARIANTS} for phase in ('idle','contended')}
    for index in range(2):
        disk=64 if index==0 else 0
        value['workerMeasurements'].append({'schema':1,'index':index,'status':'passed',
            'mode':'duration','iterations':None,'seconds':60,'workerMiB':128,'diskMiB':disk,
            'touchedBytes':128*1024**2,'cpuIterations':10,'cpuHashedBytes':10*4*1024**2,
            'diskInitialWriteBytes':disk*1024**2,'diskReadIterations':10 if disk else 0,
            'diskReadBytes':10*disk*1024**2,'fsyncs':11 if disk else 0,'elapsedSeconds':60,
            'fdClosed':True if disk else None,'failurePresent':False,'sufficient':True,'exitCode':0})
    return value


def projected_stage(name):
    row=stage_row();row['stage']=name
    return caller.project_stage(row)


def measured_experiment():
    return {'schema':1,'status':'passed','firstFailure':None,'monitorFailure':None,
        'monitorDiagnostics':[],'workload':measured_workload(),
        'stages':[hp.stage_projection(stage_row())],'abiReused':True,
        'sdkUsed':False,'floorpBuildPerformed':False,'newArtifactsStored':0,
        'originalCauseEstablished':False,'startupRepairEstablished':False,
        'exactOldProgramReproduction':False,'productionRSSReproduction':False,
        'osCacheFlushed':False,
        'helperTimeoutSeconds':3,'productionRetryPolicyChanged':False,
        'resourceLimitsChanged':False,'allCandidateAccountingChanged':False}


class StrictReceiptTests(unittest.TestCase):
    def test_positive_control_requires_all_24_measurements(self):
        self.assertEqual(len(caller.validate_experiment(measured_experiment(),layout())),24)

    def test_comparison_failure_cannot_pass_even_if_old_workload_status_passed(self):
        value=measured_workload()
        value['firstControlFailure']={'variant':'support-preimport','code':'helper-import-sample-unavailable'}
        with self.assertRaises(Failure):caller.validate_workload(value,layout())

    def test_unknown_timeout_never_becomes_a_zero_resource_measurement(self):
        for variant in ('lean','support-preimport'):
            value=measured_workload();row=next(r for r in value['samples'] if r['variant']==variant)
            row.update(status='timeout',timeoutObserved=True,measurement=None)
            with self.subTest(variant=variant),self.assertRaises(Failure):caller.validate_workload(value,layout())

    def test_missing_reordered_duplicate_or_boolean_counts_are_not_24_samples(self):
        for mode in ('missing','reordered','duplicate','boolean-count'):
            value=measured_workload()
            if mode=='missing':value['samples'].pop()
            elif mode=='reordered':value['samples'].reverse()
            elif mode=='duplicate':value['samples'][1]=copy.deepcopy(value['samples'][0])
            else:value['counts']['idle']['lean']['timeouts']=False
            with self.subTest(mode=mode),self.assertRaises(Failure):caller.validate_workload(value,layout())

    def test_partial_legacy_and_nonfinite_timing_prefixes_are_not_complete(self):
        for mode in ('partial','legacy','nonfinite'):
            value=measured_workload();progress=value['samples'][0]['progress']
            if mode=='partial':progress['timings'].pop()
            elif mode=='legacy':del progress['timings']
            else:progress['timings'][0]['elapsedNanoseconds']=float('nan')
            with self.subTest(mode=mode),self.assertRaises(Failure):caller.validate_workload(value,layout())

    def test_forged_worker_sufficient_flag_cannot_hide_actual_work_or_cleanup_failure(self):
        for field,bad in (('cpuHashedBytes',0),('touchedBytes',0),('elapsedSeconds',59),
                          ('fsyncs',0),('exitCode',1),('fdClosed',False)):
            value=measured_workload();value['workerMeasurements'][0][field]=bad
            with self.subTest(field=field),self.assertRaises(Failure):caller.validate_workload(value,layout())
        for field,bad in (('reaped',False),('failure',{}),('streamCloseFailed',True)):
            value=measured_workload();value['cleanup'][1][field]=bad
            with self.subTest(field=field),self.assertRaises(Failure):caller.validate_workload(value,layout())

    def test_both_before_and_after_load_observations_are_required(self):
        for owner,key in (('sample','loadBefore'),('sample','loadAfter'),('check','before'),('check','after')):
            value=measured_workload();row=(value['samples'] if owner=='sample' else value['loadChecks'])[12]
            row[key]={'expectedWorkers':2,'runningWorkers':1,'complete':True,'failure':None}
            with self.subTest(owner=owner,key=key),self.assertRaises(Failure):caller.validate_workload(value,layout())

    def test_monitor_unknown_receipt_failure_and_incomplete_anchor_are_not_success(self):
        for mode in ('monitor','firstFailure','receipt','anchor','missing-none'):
            value=measured_experiment()
            if mode=='monitor':value['monitorFailure']={'sampleComplete':False}
            elif mode=='firstFailure':value['firstFailure']={}
            elif mode=='receipt':value['receiptWriteFailure']={}
            elif mode=='anchor':value['stages'][0]['primaryAnchor']['actualReaped']=False
            else:del value['monitorFailure']
            with self.subTest(mode=mode),self.assertRaises((Failure,KeyError)):caller.validate_experiment(value,layout())

    def test_stage_projection_cannot_repair_forged_primary_proof(self):
        row=stage_row();row['primaryAnchor']['PRIVATE']='secret'
        with self.assertRaises(Failure):caller.project_stage(row)

    def test_source_coverage_digest_and_no_symlink_are_all_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);p=root/'source.py';p.write_text('pass\n')
            env={'FLOORP_FIXTURE_SHA':'a'*40,'HELPER_SOURCE_PINS_JSON':json.dumps({'source.py':caller.digest(p)})}
            with mock.patch.object(caller,'HERE',root):
                self.assertEqual(len(caller.verify_sources(env)),64)
                p.write_text('changed\n')
                with self.assertRaises(Failure):caller.verify_sources(env)
                p.unlink();p.symlink_to(__file__)
                with self.assertRaises(Failure):caller.verify_sources(env)


class FinalizationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.task=Path(self.temp.name).resolve();self.task.chmod(0o700)
        self.now=1100.;self.rows=[]
        env={k:'local-'+k for k in pc.BINDING_KEYS}
        env.update(GITHUB_SHA='a'*40,FLOORP_FIXTURE_SHA='b'*40,GITHUB_RUN_ID='12345',
            GITHUB_REF='refs/heads/'+caller.BRANCH,DIAGNOSTIC_STEP_OUTCOME='success',
            TASK_ROOT=str(self.task),HELPER_SOURCE_PINS_JSON=json.dumps(source_pins()))
        patch=mock.patch.dict(os.environ,env);patch.start();self.addCleanup(patch.stop)
        patch=mock.patch.object(caller.time,'monotonic',side_effect=lambda:self.now)
        patch.start();self.addCleanup(patch.stop)
        self.origin={'schema':1,'profile':caller.PROFILE,'repository':caller.REPOSITORY,
            'run':12345,'attempt':1,'head':'a'*40,'ref':env['GITHUB_REF'],'event':'push',
            'job':23456,'jobName':caller.JOB_NAME,'startedAt':'2026-10-20T12:00:00Z',
            'platformMinutes':20,'workDeadlineMinutes':15,'clockResetAllowed':False,'monotonicOrigin':1000.}
        caller.store(self.task/'job-origin.json',self.origin)
        started=caller.datetime.datetime.fromisoformat(self.origin['startedAt'].replace('Z','+00:00')).timestamp()
        for name,value in (('started',started),('started-monotonic',1000.)):
            p=self.task/name;p.write_text(str(value));p.chmod(0o600)
        binary=self.task/'census-abi';binary.write_bytes(b'local ABI fixture');binary.chmod(0o500)
        abi={'schema':1,'binding':pc.binding(self.task),'binarySHA256':caller.digest(binary),
            'layout':layout(),'layoutSHA256':hashlib.sha256(pc.canonical(layout())).hexdigest()}
        p=self.task/'census-abi.json';p.write_text(json.dumps(abi));p.chmod(0o400)
        ids=caller.selection()
        self.tests={'status':'passed','selectionSHA256':caller.SELECTION_SHA256,
            'loaded':ids,'started':ids,'successful':ids,'total':103,'failures':0,'errors':0,
            'skipped':0,'expectedFailures':0,'unexpectedSuccesses':0,'lifecycle':{
                'created':1,'reaped':1,'streamsClosed':True,'retainedAnchors':0,'activePopen':0,
                'unraisable':0,'resourceWarnings':0,'gcCollected':True}}
        self.experiment=measured_experiment()
        self.driver={'schema':1,'status':'passed','phase':'complete','firstFailure':None,
            'monitorFailure':None,'originSHA256':caller.digest(self.task/'job-origin.json'),
            'sourcePinsSHA256':caller.verify_sources(),'preflightStages':[projected_stage(n) for n in caller.STAGES[:3]],
            'experiment':self.experiment,'selfProbeVerified':True}
        caller.store(self.task/'helper-timing-tests.json',self.tests)
        caller.store(self.task/'helper-import-preflight.json',self.experiment)
        caller.store(self.task/'helper-timing-driver.json',self.driver)

    def emit(self,raw,flush):
        self.assertIs(flush,True);self.rows.append(json.loads(raw)['helperTiming'])

    def invoke(self):return caller.finalize(self.task,emit=self.emit)

    def inject_after_last_snapshot_digest(self,action):
        original=caller.digest;self.injected=False
        def digest(path):
            result=original(path)
            if self.rows and Path(path).name=='helper-timing-final.json' and not self.injected:
                self.injected=True;action()
            return result
        return mock.patch.object(caller,'digest',side_effect=digest)

    def test_positive_finalizer_returns_zero_with_24_complete_samples(self):
        self.assertEqual(self.invoke(),0)
        self.assertEqual([r['status'] for r in self.rows],['passed'])
        self.assertEqual(len(self.rows[0]['samples']),24)
        self.assertTrue(self.rows[0]['terminalJobSuccessRequired']);self.assertFalse(self.rows[0]['stdoutAloneQualifies'])

    def test_last_snapshot_digest_crossing_work_deadline_revokes_success(self):
        with self.inject_after_last_snapshot_digest(lambda:setattr(self,'now',1900.)):
            self.assertEqual(self.invoke(),1)
        self.assertTrue(self.injected);self.assertEqual([r['status'] for r in self.rows],['passed','failed'])

    def test_last_snapshot_digest_creating_first_failure_revokes_success(self):
        with self.inject_after_last_snapshot_digest(lambda:caller.store(self.task/'first-failure.json',{'code':'failed'})):
            self.assertEqual(self.invoke(),1)
        self.assertTrue(self.injected);self.assertEqual(self.rows[-1]['status'],'failed')

    def test_last_filesystem_check_cannot_hide_expired_work_clock(self):
        original=caller.os.path.lexists;triggered=[]
        def exists(path):
            result=original(path)
            if self.rows and Path(path).name=='helper-timing-postemit-failed.json':
                triggered.append(True);self.now=1900.
            return result
        with mock.patch.object(caller.os.path,'lexists',side_effect=exists):self.assertEqual(self.invoke(),1)
        self.assertTrue(triggered);self.assertEqual(self.rows[-1]['status'],'failed')

    def test_failure_marker_exists_even_as_dangling_symlink_never_qualifies(self):
        for marker in caller.FAILURE_MARKERS:
            with self.subTest(marker=marker):
                p=self.task/marker;p.symlink_to(self.task/'absent')
                with self.assertRaises(Failure):caller.final_proof(self.task,self.driver)
                p.unlink()

    def test_prior_finalization_failure_is_irreversible_on_retry(self):
        os.environ['DIAGNOSTIC_STEP_OUTCOME']='failure';self.assertEqual(self.invoke(),1)
        os.environ['DIAGNOSTIC_STEP_OUTCOME']='success';self.rows=[]
        self.assertEqual(self.invoke(),1);self.assertEqual([r['status'] for r in self.rows],['failed'])

    def test_postemit_driver_change_and_comparison_failure_are_rechecked(self):
        def mutate():
            changed=copy.deepcopy(self.driver);changed['experiment']['workload']['firstControlFailure']={}
            p=self.task/'helper-timing-driver.json';p.write_text(json.dumps(changed))
        with self.inject_after_last_snapshot_digest(mutate):self.assertEqual(self.invoke(),1)
        self.assertTrue(self.injected);self.assertEqual(self.rows[-1]['status'],'failed')

    def test_postemit_active_handle_prevents_zero_exit(self):
        active=[]
        with mock.patch.object(caller.subprocess,'_active',active), \
             self.inject_after_last_snapshot_digest(lambda:active.append(object())):
            self.assertEqual(self.invoke(),1)
        self.assertTrue(self.injected);self.assertEqual(self.rows[-1]['status'],'failed')

    def test_final_receipt_write_failure_returns_nonzero(self):
        original=caller.store
        def store(path,value):
            if Path(path).name=='helper-timing-final.json':raise PermissionError('PRIVATE')
            return original(path,value)
        with mock.patch.object(caller,'store',side_effect=store):self.assertEqual(self.invoke(),1)
        self.assertEqual([r['status'] for r in self.rows],['failed'])
        self.assertNotIn('PRIVATE',json.dumps(self.rows))

    def test_preflight_skip_warning_open_stream_or_unknown_reap_is_failure(self):
        for key,bad in (('skipped',1),('unexpectedSuccesses',1),('total',True)):
            value=copy.deepcopy(self.tests);value[key]=bad
            with self.subTest(key=key),self.assertRaises(Failure):caller.validate_preflight(value)
        for key,bad in (('reaped',0),('streamsClosed',False),('activePopen',1),('resourceWarnings',1)):
            value=copy.deepcopy(self.tests);value['lifecycle'][key]=bad
            with self.subTest(key=key),self.assertRaises(Failure):caller.validate_preflight(value)


class ActualPreflightTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform=='linux','Linux negative control only; actual Darwin qualification is separate')
    def test_real_selected_linux_preflight_skips_are_nonzero_and_all_owned_children_are_reaped(self):
        original=subprocess.Popen;children=[]
        def launch(*args,**kwargs):
            child=original(*args,**kwargs);children.append(child);return child
        with tempfile.TemporaryDirectory() as temporary:
            task=Path(temporary).resolve();task.chmod(0o700)
            with mock.patch.object(subprocess,'Popen',side_effect=launch):
                result=subprocess.run([sys.executable,str(caller.HERE/'helper_timing_caller.py'),
                    '--preflight','--task',str(task)],capture_output=True,text=True,timeout=60,
                    env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'},cwd=caller.HERE)
            self.assertNotEqual(result.returncode,0)
            value=caller.owned_json(task/'helper-timing-tests.json')
        self.assertEqual(value['status'],'failed');self.assertEqual(value['total'],103)
        self.assertEqual((value['failures'],value['errors'],value['skipped']),(0,0,2))
        self.assertEqual(len(value['successful']),101)
        self.assertTrue(caller.lifecycle_pass(value['lifecycle']),value['lifecycle'])
        self.assertIsNotNone(result.returncode)
        self.assertEqual(len(children),1)
        with self.assertRaises(ChildProcessError):os.waitpid(children[0].pid,os.WNOHANG)
        self.assertTrue(all(stream is None or stream.closed for stream in
            (children[0].stdin,children[0].stdout,children[0].stderr)))


if __name__=='__main__':unittest.main()
