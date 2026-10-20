# SPDX-License-Identifier: MPL-2.0
"""Portable flow models: no compiler, process, network or selected Mozilla test runs."""
import builtins
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import copy
import hashlib
import io
import json
import math
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest import mock

import consumer_time
import fresh45 as fresh
import mozilla_target_build as target
import mozilla_upstream as upstream
import pipeline
import test_mozilla_target_build as target_models
import test_mozilla_upstream as upstream_models
from qa_core import Failure


def asset(kind=2):
    return {'bytes': 100, 'sha256': 'a'*64, 'filetype': kind, 'uuid': 'b'*32,
            'sections': 1, 'sectionsSHA256': 'c'*64}


def target_fixture():
    files = {key: {'status': 'observed', 'sha256': pin[2], 'bytes': 1}
             for key, pin in target.GRAPH_PINS.items()}
    nodes = [node for _, node in target.TARGETS] + ['toolkit/library/build/target',
        'toolkit/library/rust/target-objects', 'widget/cocoa/target-objects']
    root = ('compile_targets := '+' '.join(nodes)+'\nmisc_dirs := toolkit/modules\n'
            'libs_dirs := browser/app\ntools_dirs := browser/app\ninclude root-deps.mk\n').encode()
    deps = (b'js/xpconnect/shell/target: toolkit/library/build/target widget/cocoa/target-objects\n'
            b'toolkit/library/build/target: toolkit/library/rust/target-objects\n'
            b'recurse_tools: browser/app/tools\n')
    graph = target.target_graph_proof(root, deps)
    graph['files'] = {key: {field: files[key][field] for field in ('bytes','sha256')}
                      for key in ('rootMk','rootDepsMk')}
    bindings = {'bindingsStable': True, 'files': files, 'graph': graph}
    assets = {'binaries': {key: asset(6 if key in ('XUL','channelPrefs') else 2)
                          for key in ('xpcshell','XUL','http3server','browser','updater','channelPrefs','dmgHelper')},
              'resources': {key: {'bytes':1,'sha256':'d'*64} for key in
                            ('appConstants','applicationIni','dependentLibs','channelInfo')}}
    cases = []
    for label, code, status in (('compiled-debug',0,'passed'),('failure-control',1,'failed'),
                                ('upstream-version-comparator',0,'passed')):
        cases.append({'case':label,'mode':'negative-control' if code else 'positive',
            'expectedExitCode':code,'expectationSatisfied':True,
            'process':{'available':True,'exitCode':code,'exactReaped':True,'groupGone':True,
                'cleanupClean':True,'monitorFailureAbsent':True,
                'reason':'nonzero-exit' if code else None,'failurePhase':'wait' if code else None},
            'runnerFailure':{'stage':'mozilla-xpcshell','code':'nonzero-exit'} if code else None,
            'caughtCode':'nonzero-exit' if code else None,'caseError':None,'evidenceWriteFailure':None,
            'evidenceCollectionFailure':None,'harness':{'status':status,'completeLog':True,'expectationSatisfied':True}})
    smoke = {'contract':'current-source-mozilla-xpcshell-smoke-v1','status':'passed','monitorFailure':None,
        'actualCompiledDebug':True,'actualCurrentSourceUpstreamTest':True,'intentionalFailureDetected':True,
        'allCasesExactReaped':True,'allCasesGroupsGone':True,
        'builtAssets':{key:assets['binaries'][key] for key in ('xpcshell','XUL')},'cases':cases}
    return bindings, assets, smoke


class ModelRunner:
    def __init__(self, model, task, started, limits):
        self.model, self.task, self.started, self.limits = model, task, started, limits
        self.monotonic_started = float((task/'started-monotonic').read_text())
        self.phase_deadline_minutes = None
        self.records, self.monitor_diagnostics = [], []
        self.failure = self.monitor_failure = self.native_cleanup = None

    def elapsed(self): return self.model.clock-self.monotonic_started

    def fail(self, stage, code):
        if self.failure is None: self.failure = {'stage':stage,'code':code}

    def run(self, stage, command, cwd, env, deadline, *args, **kwargs):
        self.model.before(stage)
        phase = kwargs.get('phase_deadline_minutes', self.phase_deadline_minutes)
        if phase is None: phase = self.phase_deadline_minutes
        if phase is not None and self.elapsed() >= phase*60:
            self.fail(stage,'phase-deadline-before-launch');raise Failure('phase-deadline-before-launch')
        self.model.launches.append((stage,list(command),phase,dict(env)))
        if stage == self.model.fail_stage:
            self.fail(stage,'controlled-functional-failure');raise Failure('controlled-functional-failure')
        self.records.append(target_models.row(stage))
        path = self.task/(stage+'.private.log')
        versions = {'version-clang':'clang version 22.1.8','version-rust':'rustc 1.95.0 (59807616e 2026-04-14)',
                    'version-node':'v22.16.0','version-nasm':'NASM version 3.02'}
        path.write_text(versions.get(stage,''))
        if stage == 'sdk-materialize':
            (self.task/'sdk-materialized.json').write_text(json.dumps({'path':str(self.task/'sdk')}))
        if stage == 'configure':
            (self.task/'obj').mkdir()
        if stage == target.STAGES[-1]:
            host=self.task/'obj/widget/cocoa/MacWebAppService.o'
            host.parent.mkdir(parents=True);host.write_bytes(b'model-object')
        self.model.after(stage)
        return path


class Fresh45Flow(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory(prefix='fresh45-model-');self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.task=self.root/'task';self.task.mkdir()
        self.origin=b'1000.0';self.clock=1001.0;self.wall=1700000000.0
        (self.task/'started-monotonic').write_bytes(self.origin)
        (self.task/'started').write_text(str(self.wall))
        for name,value in [('monitor-preflight.json',{'status':'passed'}),
                           ('toolchain-receipts.json',[{}]*11),('source.json',{'verified':True}),
                           ('effective-config.json',{'debug':True,'pgo':False})]:
            (self.task/name).write_text(json.dumps(value))
        model=upstream_models.UpstreamTests();model.setUp();self.addCleanup(model.doCleanups)
        self.upstream=model.proof()
        self.bindings,self.assets,self.smoke=target_fixture()
        self.launches=[];self.fail_stage=None;self.runner=None
        self.before=lambda _:None;self.after=lambda _:None
        self.after_scope=lambda:None;self.after_asset=lambda _:None
        self.writer_before=lambda _:None;self.writer_after=lambda _:None
        self.reader_after=lambda *_:None;self.print_after=lambda:None
        self.stack=ExitStack();self.addCleanup(self.stack.close)
        def forbidden(*a,**kw):raise AssertionError('process-or-network-forbidden')
        for name in ('Popen','run','check_output','check_call','call'):
            self.stack.enter_context(mock.patch.object(pipeline.subprocess,name,side_effect=forbidden))
        self.stack.enter_context(mock.patch('urllib.request.OpenerDirector.open',side_effect=forbidden))
        self.stack.enter_context(mock.patch('socket.socket.connect',side_effect=forbidden))

    def prepare(self, source, task, runner, env, deadline, result):
        result.update(copy.deepcopy(self.upstream));result.update(status='failed',assets=None,case=None)
        for stage in upstream.EXTRA_STAGES[:2]:runner.run(stage,[],source,env,360,phase_deadline_minutes=deadline)

    def scope(self, source, task, runner, env, deadline, result):
        self.assertEqual(deadline,345)
        result.update(copy.deepcopy(self.upstream));self.after_scope()

    def verify_asset(self, task, runner, macho, *, diagnostic):
        diagnostic.update(schema=1,status='passed',checkedAssets=6,failure=None,inspection=None)
        self.after_asset(diagnostic)
        return copy.deepcopy(self.upstream['assets'])

    def invoke(self, extra=()):
        argv=['pipeline.py','--source',str(self.root/'source'),'--fixture',str(self.root/'fixture'),
              '--harness',str(self.root/'harness'),'--task',str(self.task),'--fresh-firefox45',*extra]
        output=io.StringIO();original_write=fresh.write_summary;original_read=fresh.read_summary
        def writer(path,value):
            self.writer_before(value);original_write(path,value);self.writer_after(value)
        def reader(path):
            value=original_read(path);self.reader_after(path,value);return value
        def print_report(*a,**kw):builtins.print(*a,**kw);self.print_after()
        def runner(task,started,limits):self.runner=ModelRunner(self,task,started,limits);return self.runner
        def smoke(*args):args[-1].update(copy.deepcopy(self.smoke))
        with ExitStack() as stack:
            for name,fn in [('Runner',runner),('preflight',lambda *a:{'status':'passed'}),
                            ('prepare_harness',lambda *a:{'verified':True}),('prepare_source',lambda *a:None),
                            ('verify_config',lambda *a:None),('clean_environment',lambda *a:{'PATH':'/usr/bin'}),
                            ('macho',lambda *a:asset(1)),('load_sdk_source',lambda *a:{})]:
                stack.enter_context(mock.patch.object(pipeline,name,side_effect=fn))
            for obj,name,fn in [(target,'graph_precondition',lambda _:copy.deepcopy(self.bindings)),
                (target,'verify_assets',lambda *a,**k:copy.deepcopy(self.assets)),
                (upstream,'prepare',self.prepare),(upstream,'run_scope',self.scope),
                (upstream,'verify_scope_assets',self.verify_asset),
                (fresh,'write_summary',writer),(fresh,'read_summary',reader)]:
                stack.enter_context(mock.patch.object(obj,name,side_effect=fn))
            stack.enter_context(mock.patch('mozilla_harness_smoke.run_smoke',side_effect=smoke))
            stack.enter_context(mock.patch.object(fresh,'print',side_effect=print_report,create=True))
            stack.enter_context(mock.patch.object(pipeline.time,'monotonic',side_effect=lambda:self.clock))
            stack.enter_context(mock.patch.object(sys,'argv',argv))
            stack.enter_context(mock.patch.dict(os.environ,{'CHECKPOINT_BUILD_DATE':'19990101000000'}))
            stack.enter_context(redirect_stdout(output));stack.enter_context(redirect_stderr(io.StringIO()))
            code=pipeline.main()
        self.stdout=output.getvalue()
        path=self.task/'summary.json'
        return code,json.loads(path.read_text()) if path.is_file() else json.loads(self.stdout.splitlines()[-1])

    def test_complete_route_is_fresh_targeted45_without_checkpoint_package_or_native(self):
        code,result=self.invoke();self.assertEqual(code,0);self.assertEqual(result['status'],fresh.STATUS)
        self.assertEqual(result['qualification'],fresh.QUALIFICATION)
        self.assertTrue(result['freshObjdir']);self.assertTrue(result['sameJob']);self.assertFalse(result['checkpointUsed'])
        stages=[row[0] for row in self.launches]
        self.assertEqual(stages[-15:],['configure','effective-config',fresh.EXPORT_STAGE,
            *upstream.EXTRA_STAGES[:2],*target.STAGES,*upstream.EXTRA_STAGES[2:]])
        self.assertTrue(all(row[2]==345 for row in self.launches))
        self.assertFalse(any(stage in fresh.FORBIDDEN_STAGES for stage in stages))
        self.assertEqual(result['mozillaUpstream']['case']['harness']['counts']['executed'],42)
        self.assertEqual(result['mozillaUpstream']['case']['harness']['counts']['skipped'],3)
        self.assertFalse(result['fullHostQualification']);self.assertEqual(result['nativeAuthentication'],'NOT_TESTED')
        self.assertEqual((self.task/'started-monotonic').read_bytes(),self.origin)
        self.assertIn(upstream.CHILD_LIBS_STAGE,result['targetStageLogs'])
        self.assertNotIn('checkpointStageLogs',result)
        exports=next(row for row in self.launches if row[0]==fresh.EXPORT_STAGE)
        self.assertEqual(exports[1][1:],['mach','build','-j3','pre-export','export'])
        self.assertTrue(all(row[3].get('MOZ_BUILD_DATE')!='19990101000000' for row in self.launches))

    def test_every_legacy_mode_is_rejected_before_runner_or_work(self):
        for args in [('--checkpoint-role','consumer'),('--checkpoint-role','producer'),('--restore-only',),
                     ('--replay-origin',),('--consumer-shared-deadline',),('--mozilla-only',),
                     ('--mozilla-first',),('--mozilla-targeted',),('--mozilla-upstream-xpcom',)]:
            with self.subTest(args=args),self.assertRaisesRegex(Failure,'fresh-firefox45-mode'):self.invoke(args)
        self.assertIsNone(self.runner);self.assertEqual(self.launches,[])

    def test_existing_object_directory_or_broken_symlink_is_rejected(self):
        (self.task/'obj').symlink_to(self.task/'missing')
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['firstFailure']['code'],'objdir-not-fresh')
        self.assertEqual(self.launches,[])

    def test_exports_failure_is_primary_and_prevents_targets(self):
        self.fail_stage=fresh.EXPORT_STAGE
        code,result=self.invoke();self.assertEqual(code,1)
        self.assertEqual(result['firstFailure'],{'stage':fresh.EXPORT_STAGE,'code':'controlled-functional-failure'})
        self.assertNotIn(target.STAGES[0],[row[0] for row in self.launches])

    def test_missing_exports_anchor_blocks_build_immediately(self):
        def after(stage):
            if stage==fresh.EXPORT_STAGE:del self.runner.records[-1]['primaryAnchor']
        self.after=after
        code,result=self.invoke();self.assertEqual(code,1)
        self.assertEqual(result['firstFailure']['code'],'fresh-exports-proof-incomplete')
        self.assertNotIn(target.STAGES[0],[row[0] for row in self.launches])

    def test_child_libs_failure_is_primary_and_no_resources_or_scope_success(self):
        self.fail_stage=upstream.CHILD_LIBS_STAGE
        code,result=self.invoke();self.assertEqual(code,1)
        self.assertEqual(result['firstFailure']['stage'],upstream.CHILD_LIBS_STAGE)
        self.assertNotIn(upstream.RESOURCE_STAGE,[row[0] for row in self.launches])
        self.assertIsNone(result['mozillaUpstream']['case'])

    def test_asset_failure_retains_first_false_predicate_and_no_executed45_counts(self):
        def failed(diagnostic):
            diagnostic.update(status='failed',checkedAssets=0,failure={'asset':'plugin-container','predicate':'file'})
            raise Failure('mozilla-upstream-current-native-asset')
        self.after_asset=failed
        code,result=self.invoke();self.assertEqual(code,1)
        self.assertEqual(result['firstFailure']['code'],'mozilla-upstream-current-native-asset')
        self.assertEqual(result['mozillaUpstream']['nativeAssetPrecondition']['failure'],
                         {'asset':'plugin-container','predicate':'file'})
        self.assertIsNone(result['mozillaUpstream']['case'])

    def test_current_clock_after_legacy_cutoff_is_allowed_without_reset(self):
        self.clock=1000+321*60;code,result=self.invoke();self.assertEqual(code,0)
        self.assertEqual(result['executionTimeBudget']['sharedWorkDeadlineMinutes'],345)

    def test_original_deadline_before_work_has_no_launch(self):
        self.clock=1000+345*60;code,result=self.invoke();self.assertEqual(code,1)
        self.assertEqual(result['firstFailure']['code'],'fresh-work-deadline');self.assertEqual(self.launches,[])

    def test_scope_crossing_original_deadline_cannot_leave_success(self):
        self.after_scope=lambda:setattr(self,'clock',1000+345*60)
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['status'],'failed')

    def test_late_failed_proof_after_summary_write_is_latched(self):
        def after(value):value['mozillaUpstream']['case']['process']['exactReaped']=False
        self.writer_after=after
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['status'],'failed')
        self.assertEqual(result['firstFailure']['code'],'fresh-firefox45-proof-incomplete')

    def test_late_print_first_failure_is_retained_and_disk_repaired(self):
        primary={'stage':'controlled-late-stage','code':'controlled-late-failure'}
        self.print_after=lambda:setattr(self.runner,'failure',copy.deepcopy(primary))
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['status'],'failed')
        self.assertEqual(result['firstFailure'],primary)

    def test_late_print_monitor_failure_is_inconclusive_and_disk_repaired(self):
        self.print_after=lambda:setattr(self.runner,'monitor_failure',{'stage':'late','code':'unavailable'})
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['status'],'failed')
        self.assertEqual(result['validationOutcome']['execution'],'monitor-inconclusive')

    def test_late_print_clock_reset_rejects_and_does_not_repair_origin(self):
        self.print_after=lambda:(self.task/'started-monotonic').write_bytes(b'1001.0')
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['status'],'failed')
        self.assertEqual((self.task/'started-monotonic').read_bytes(),b'1001.0')

    def test_summary_readback_substitution_is_latched(self):
        self.reader_after=lambda _,value:value.update(freshObjdir=1)
        code,result=self.invoke();self.assertEqual(code,1);self.assertEqual(result['status'],'failed')

    def test_late_changed_contracts_cannot_keep_a_ready_summary(self):
        # Each case starts from the complete flow, then changes one independent
        # final proof after writing the first receipt. No compiler/child runs.
        changes = {
            'post-diagnostic':lambda r:r['mozillaUpstream'].pop('postNativeAssetPrecondition'),
            'first-diagnostic-unknown':lambda r:r['mozillaUpstream']['nativeAssetPrecondition'].update(status='unavailable'),
            'disabled-test-passed':lambda r:r['mozillaUpstream']['case']['harness']['counts'].update(skipped=0),
            'scope-incomplete':lambda r:r['mozillaUpstream']['case']['harness']['counts'].update(executed=41),
            'target-graph-unbound':lambda r:r['mozillaTargets']['graph'].update(bindingsStable=False),
            'smoke-negative-pass':lambda r:r['mozilla']['cases'][1]['process'].update(exitCode=0),
            'smoke-missing-debug':lambda r:r['mozilla'].update(actualCompiledDebug=False),
            'checkpoint-replay':lambda r:r.update(checkpointReplay={}),
            'qualification':lambda r:r.update(qualification='verified-origin-root-target-mozilla-smoke-v1'),
            'bool-freshness':lambda r:r.update(freshObjdir=1),
            'full-host-claim':lambda r:r.update(fullHostQualification=True),
            'native-claim':lambda r:r.update(nativeAuthentication='passed'),
            'budget-reset':lambda r:r['executionTimeBudget'].update(clockResetAllowed=True),
            'anchor-not-retired':lambda r:r['stages'][-1]['primaryAnchor'].update(retirementVerified=False),
            'extra-libs':lambda r:r['stages'].append(copy.deepcopy(r['stages'][-2])),
            'forbidden-full-build':lambda r:r['stages'].append(target_models.row('full-build')),
        }
        # Run separate bounded fixtures so a previous latched failure cannot
        # make a later negative case pass trivially.
        for label,change in changes.items():
            with self.subTest(label=label):
                model=Fresh45Flow();model.setUp()
                try:
                    model.writer_after=change
                    code,result=model.invoke()
                    self.assertEqual(code,1);self.assertEqual(result['status'],'failed')
                    self.assertIsNotNone(result['firstFailure'])
                finally:model.doCleanups()

    def test_first_failed_summary_cannot_be_promoted_by_writer_callback(self):
        self.fail_stage=target.STAGES[0]
        self.writer_after=lambda value:value.update(status=fresh.STATUS,firstFailure=None)
        code,result=self.invoke();self.assertEqual(code,1)
        self.assertEqual(result['status'],'failed');self.assertEqual(result['firstFailure']['code'],'controlled-functional-failure')


class Fresh45PureGuards(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.path=self.root/'summary.json'

    def test_fresh_selector_is_explicit_fixed_345_and_does_not_mutate_plan(self):
        raw=Path(__file__).with_name('plan.json').read_bytes();plan=json.loads(raw);before=copy.deepcopy(plan)
        self.assertEqual(consumer_time.select_fresh45_time(plan,'foreign'),{'deadlineMinutes':None,'evidence':None})
        result=consumer_time.select_fresh45_time(plan,hashlib.sha256(raw).hexdigest(),enabled=True)
        self.assertEqual(plan,before);self.assertEqual(result['deadlineMinutes'],345)
        self.assertEqual(set(result['evidence']['deadlines'].values()),{345})
        self.assertFalse(result['evidence']['clockResetAllowed'])
        for enabled in (None,0,1,'true'):
            with self.assertRaises(Failure):consumer_time.select_fresh45_time(plan,'foreign',enabled=enabled)
        for change in ({'limits':{'jobMinutes':True}}, {'checkpoint':{'finalDeadlineMinutes':344}}):
            malformed=copy.deepcopy(plan);malformed.update(change)
            with self.assertRaises(Failure):consumer_time.select_fresh45_time(malformed,consumer_time.BASELINE_PLAN_SHA256,enabled=True)
        with self.assertRaises(Failure):consumer_time.select_fresh45_time(plan,'foreign',enabled=True)

    def test_reader_refuses_duplicate_nonfinite_symlink_and_hardlink(self):
        for raw in [b'{"a":1,"a":2}',b'{"a":NaN}',b'{"a":Infinity}',b'{"a":1e309}',b'[]']:
            self.path.write_bytes(raw)
            with self.assertRaises((Failure,ValueError)):fresh.read_summary(self.path)
        self.path.unlink();target_path=self.root/'private';target_path.write_text('{"a":1}')
        self.path.symlink_to(target_path)
        with self.assertRaises(Failure):fresh.read_summary(self.path)
        self.path.unlink();os.link(target_path,self.path)
        with self.assertRaises(Failure):fresh.read_summary(self.path)

    def test_atomic_writer_keeps_owned_single_link_0600_and_never_follows_symlink(self):
        fresh.write_summary(self.path,{'value':True});self.assertEqual(fresh.read_summary(self.path),{'value':True})
        self.assertEqual(stat.S_IMODE(self.path.stat().st_mode),0o600)
        self.assertEqual(list(self.root.glob('.fresh-summary-*')),[])
        target_path=self.root/'private';target_path.write_bytes(b'PRIVATE')
        self.path.unlink();self.path.symlink_to(target_path)
        with self.assertRaises(Failure):fresh.write_summary(self.path,{'value':False})
        self.assertEqual(target_path.read_bytes(),b'PRIVATE')

    def test_type_strict_equality_never_accepts_bool_as_count(self):
        self.assertFalse(fresh.same_value({'count':True},{'count':1}))
        self.assertTrue(fresh.same_value({'value':[None,True,1]},{'value':[None,True,1]}))


if __name__=='__main__':unittest.main()
