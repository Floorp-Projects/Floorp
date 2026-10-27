# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace, ModuleType
import unittest
from unittest import mock

import mozilla_upstream as upstream
from support import Failure


def stage(name):
    return {'stage':name,'exitCode':0,'reason':None,'failurePhase':None,'cleanupFailure':None,
            'groupCleanup':'verified-gone','leaderReap':{'status':'reaped','exitCode':0},
            'primaryAnchor':{'policy':'retained-direct-child-v1','retirementVerified':True,
                'actualReaped':True,'actualReturncode':0,'signalAuthorityRevoked':True,
                'postReapAbsence':{'status':'observed','groupEmpty':True,
                                   'readOnly':True,'signalAuthority':False},
                'scope':'active-process-group-epoch','sessionOrDescendantClosure':'unconfirmed'}}


def binding(value):
    raw=(json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
    return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}


class UpstreamTests(unittest.TestCase):
    def setUp(self):
        directory=tempfile.TemporaryDirectory(prefix='mozilla-upstream-')
        self.addCleanup(directory.cleanup)
        self.task=Path(directory.name).resolve()
        self.path=self.task/'private.raw.jsonl'
        self.plan,self.plan_hash=upstream.plan_definition()
        self.ids=[upstream.SCOPE+'/'+name for name in self.plan['tests']]
        self.expected={name:'SKIP' if i>=42 else 'PASS' for i,name in enumerate(self.ids)}

    def raw_rows(self, expected=None):
        expected=expected or self.expected
        def row(action,**values):
            return {'action':action,'time':1802923200000,'thread':'MainThread','pid':1,
                    'source':'xpcshell',**values}
        rows=[row('suite_start',tests={'xpcom/tests/unit/xpcshell.toml':list(expected)},name='xpcshell')]
        for name,status in expected.items():
            rows.extend([row('test_start',test=name),row('test_end',test=name,status=status)])
        rows.append(row('suite_end'))
        return rows

    def receipt(self, rows=None, expected=None):
        self.path.write_text(''.join(json.dumps(row)+'\n' for row in
                                    (self.raw_rows(expected) if rows is None else rows)))
        return upstream.scope_receipt(self.path,expected or self.expected)

    def inventory_rows(self):
        return [{'id':name,'path':'/private/source/'+name,'expected':'pass',
                 **({'disabled':'windows only'} if expected=='SKIP' else {})}
                for name,expected in self.expected.items()]

    def proof(self):
        rows=self.inventory_rows();scope=upstream.inventory_summary(rows,100)
        whole=upstream.inventory_summary(rows+[{'id':'other/test.js','path':'/private/other.js','expected':'pass'}],100)
        for summary in (scope,whole): summary['dump']={'bytes':100,'sha256':'a'*64}
        inventory={'contract':upstream.CONTRACT,'status':'observed','planSHA256':self.plan_hash,
                   'serverProcessesStarted':0,'testsExecuted':0,'scope':scope,'global':whole,
                   'scopeTests':[{'id':name,'expected':expected} for name,expected in self.expected.items()],
                   'temporaryPrefsOwnership':'stock-obj-temp-not-removed'}
        process={'available':True,'exitCode':0,'exactReaped':True,'groupGone':True,
                 'cleanupClean':True,'monitorFailureAbsent':True,'reason':None}
        asset={'bytes':100,'sha256':'a'*64,'filetype':2,'uuid':'b'*32,'sectionsSHA256':'c'*64,'sections':2}
        return {'contract':upstream.CONTRACT,'scope':upstream.SCOPE,'status':'passed','planSHA256':self.plan_hash,
                'fullUpstreamSuiteExecuted':False,'fullHostQualification':False,'nativeAuthentication':'NOT_TESTED',
                'inventory':inventory,'inventoryReceipt':binding(inventory),
                'case':{'process':process,'harness':self.receipt(),'expectationsSatisfied':True,
                        'inputBindingStable':True,'caughtCode':None,'runnerFailure':None,'monitorFailure':None,
                        'monitorFailureObserved':False,'caseError':None,'evidenceWriteFailure':None,
                        'evidenceCollectionFailure':None},
                'assets':{'binaries':{name:dict(asset) for name in (*upstream.HELPERS,'plugin-container')},
                          'xpcomTypelib':{'bytes':100,'sha256':'d'*64,'interfacesVerified':True}}}

    def test_skip_is_complete_and_not_pass_or_executed(self):
        receipt=self.receipt()
        self.assertEqual(receipt['status'],'passed')
        self.assertEqual({key:receipt['counts'][key] for key in ('selected','executed','passed','skipped')},
                         {'selected':45,'executed':42,'passed':42,'skipped':3})
        self.assertTrue(all(row['actual']=='SKIP' for row in receipt['tests'][-3:]))

    def test_pre_harness_ledger_preserves_all_cases_without_inventing_skips(self):
        value=self.proof();value.update(status='failed',case=None)
        original=copy.deepcopy(value)
        ledger=upstream.execution_ledger(value)
        self.assertEqual(value,original)
        self.assertEqual(ledger['status'],'not-executed')
        self.assertTrue(ledger['inventoryVerified']);self.assertFalse(ledger['terminalLogObserved'])
        self.assertEqual(ledger['counts'],{'planned':45,'eligible':42,'manifestDisabled':3,
            'recorded':0,'executed':0,'passed':0,'expectedFailures':0,'unexpectedResults':0,
            'skipped':0,'notExecuted':42,'unverified':0})
        self.assertEqual({row['id'] for row in ledger['tests']},set(self.ids))
        self.assertEqual(sum(row['execution']=='NOT_EXECUTED' for row in ledger['tests']),42)
        self.assertEqual(sum(row['execution']=='MANIFEST_DISABLED' for row in ledger['tests']),3)
        self.assertTrue(all(row['actual'] is None for row in ledger['tests']))
        self.assertFalse(ledger['productQualification'])

    def test_unknown_or_tampered_inventory_keeps_eligibility_and_results_unknown(self):
        value=self.proof();value.update(status='failed',case=None)
        variants=[None,{}, {**value,'inventoryReceipt':{'bytes':1,'sha256':'0'*64}}]
        for field,replacement in (('scopeTests',value['inventory']['scopeTests'][:-1]),
                                  ('testsExecuted',True),('planSHA256','0'*64)):
            changed=copy.deepcopy(value);changed['inventory'][field]=replacement
            changed['inventoryReceipt']=binding(changed['inventory']);variants.append(changed)
        changed=copy.deepcopy(value)
        changed['inventory']['scopeTests'][-1]=dict(changed['inventory']['scopeTests'][0])
        changed['inventoryReceipt']=binding(changed['inventory']);variants.append(changed)
        changed=copy.deepcopy(value);changed['inventory']['scope']['manifestEnabledRecords']=True
        changed['inventoryReceipt']=binding(changed['inventory']);variants.append(changed)
        for variant in variants:
            with self.subTest(variant=variant is None):
                ledger=upstream.execution_ledger(variant)
                self.assertEqual(ledger['status'],'unverified')
                self.assertEqual(ledger['counts']['planned'],45)
                self.assertEqual(ledger['counts']['unverified'],45)
                for key in ('eligible','manifestDisabled','executed','passed','skipped','notExecuted'):
                    self.assertIsNone(ledger['counts'][key])
                self.assertTrue(all(row['execution']=='UNVERIFIED' and row['actual'] is None
                                    for row in ledger['tests']))

    def test_missing_case_or_incomplete_log_cannot_be_reported_as_not_executed(self):
        value=self.proof()
        absent=copy.deepcopy(value);del absent['case']
        missing=copy.deepcopy(value);missing['case']={}
        incomplete=copy.deepcopy(value)
        incomplete['case']['harness']=self.receipt(self.raw_rows()[:-1])
        self.assertEqual(incomplete['case']['harness']['status'],'unavailable')
        for variant in (absent,missing,incomplete):
            ledger=upstream.execution_ledger(variant)
            self.assertEqual(ledger['status'],'unverified')
            self.assertTrue(ledger['inventoryVerified'])
            self.assertEqual(ledger['counts']['unverified'],42)
            self.assertIsNone(ledger['counts']['executed'])
            self.assertIsNone(ledger['counts']['notExecuted'])
            self.assertTrue(all(row['actual'] is None for row in ledger['tests']))

    def test_terminal_results_are_separate_from_process_and_product_qualification(self):
        value=self.proof();value['private']='PRIVATE'
        ledger=upstream.execution_ledger(value)
        self.assertEqual(ledger['status'],'recorded');self.assertTrue(ledger['processEvidenceComplete'])
        self.assertEqual(ledger['counts']['recorded'],45)
        self.assertEqual(ledger['counts']['executed'],42)
        self.assertEqual(ledger['counts']['passed'],42);self.assertEqual(ledger['counts']['skipped'],3)
        self.assertFalse(ledger['productQualification']);self.assertNotIn('PRIVATE',json.dumps(ledger))
        value['case']['process']['exactReaped']=False
        value['case']['monitorFailureObserved']=True
        value['case']['monitorFailure']={'private':'PRIVATE'}
        changed=upstream.execution_ledger(value)
        self.assertEqual(changed['counts'],ledger['counts'])
        self.assertFalse(changed['processEvidenceComplete']);self.assertFalse(changed['productQualification'])
        self.assertNotIn('PRIVATE',json.dumps(changed))

    def test_unexpected_failure_and_expected_failure_keep_different_observed_counts(self):
        value=self.proof();rows=self.raw_rows();rows[2].update(status='FAIL',expected='PASS')
        value['case']['harness']=self.receipt(rows)
        ledger=upstream.execution_ledger(value)
        self.assertEqual(ledger['status'],'recorded')
        self.assertEqual(ledger['counts']['passed'],41)
        self.assertEqual(ledger['counts']['unexpectedResults'],1)
        self.assertEqual(ledger['counts']['expectedFailures'],0)
        self.assertEqual(next(row for row in ledger['tests'] if row['id']==self.ids[0])['actual'],'FAIL')
        value=self.proof();self.expected[self.ids[0]]='FAIL'
        value['inventory']['scopeTests'][0]['expected']='FAIL'
        for key in ('scope','global'):value['inventory'][key]['expectedFailureRecords']=1
        value['inventoryReceipt']=binding(value['inventory'])
        value['case']['harness']=self.receipt()
        ledger=upstream.execution_ledger(value)
        self.assertEqual(ledger['status'],'recorded')
        self.assertEqual(ledger['counts']['passed'],41)
        self.assertEqual(ledger['counts']['unexpectedResults'],0)
        self.assertEqual(ledger['counts']['expectedFailures'],1)

    def test_terminal_ledger_rejects_duplicate_ids_boolean_counts_and_wrong_lifecycle(self):
        original=self.proof()
        variants=[]
        for path,replacement in [(('counts','executed'),True),(('counts','passed'),43),
                                  (('counts','rawLifecycle','suiteEnds'),False),
                                  (('counts','rawLifecycle','testEndStatuses','SKIP'),0),
                                  (('sha256',),'invalid'),(('completeLog',),False)]:
            value=copy.deepcopy(original);node=value['case']['harness']
            for key in path[:-1]:node=node[key]
            node[path[-1]]=replacement;variants.append(value)
        value=copy.deepcopy(original)
        value['case']['harness']['tests'][-1]=dict(value['case']['harness']['tests'][0]);variants.append(value)
        for value in variants:
            ledger=upstream.execution_ledger(value)
            self.assertEqual(ledger['status'],'unverified')
            self.assertTrue(ledger['inventoryVerified']);self.assertFalse(ledger['terminalLogObserved'])
            self.assertIsNone(ledger['counts']['passed']);self.assertIsNone(ledger['counts']['executed'])
            self.assertTrue(all(row['actual'] is None for row in ledger['tests']))

    def test_unreadable_plan_still_returns_diagnostic_failure_without_case_claims(self):
        with mock.patch.object(upstream,'plan_definition',side_effect=OSError('PRIVATE')):
            ledger=upstream.execution_ledger(self.proof())
        self.assertEqual(ledger['status'],'unverified');self.assertEqual(ledger['tests'],[])
        self.assertTrue(all(value is None for value in ledger['counts'].values()))
        self.assertNotIn('PRIVATE',json.dumps(ledger));self.assertFalse(ledger['productQualification'])

    def test_known_failure_keeps_actual_fail_and_separate_count(self):
        expected=dict(self.expected);expected[self.ids[0]]='FAIL'
        receipt=self.receipt(expected=expected)
        self.assertEqual(receipt['status'],'passed')
        self.assertEqual(receipt['counts']['passed'],41)
        self.assertEqual(receipt['counts']['expectedFailures'],1)
        self.assertEqual(receipt['tests'][0]['actual'],'FAIL')

    def test_unexpected_failure_and_error_never_pass(self):
        rows=self.raw_rows();rows[2].update(status='FAIL',expected='PASS')
        receipt=self.receipt(rows)
        self.assertFalse(receipt['expectationsSatisfied'])
        self.assertTrue(receipt['completeLog'])
        self.assertEqual(receipt['counts']['unexpectedResults'],1)
        rows=self.raw_rows();rows.insert(-1,{**rows[0],'action':'log','level':'ERROR','message':'private text'})
        receipt=self.receipt(rows)
        self.assertFalse(receipt['expectationsSatisfied'])
        self.assertNotIn('private text',json.dumps(receipt))

    def test_disabled_test_cannot_pass_or_enabled_test_skip(self):
        for index,status in ((2,'SKIP'),(-2,'PASS')):
            rows=self.raw_rows();rows[index]['status']=status
            receipt=self.receipt(rows)
            self.assertEqual(receipt['status'],'unavailable')
            self.assertIsNone(receipt['counts'])

    def test_partial_duplicate_wrong_ids_or_missing_input_stay_unknown(self):
        rows=self.raw_rows()
        for malformed in (rows[:-1],rows+[rows[-1]],rows[:3]+[rows[2]]+rows[3:]):
            receipt=self.receipt(malformed)
            self.assertEqual(receipt['status'],'unavailable')
            self.assertIsNone(receipt['counts'])
        self.path.unlink()
        self.assertIsNone(upstream.scope_receipt(self.path,self.expected)['counts'])

    def test_inventory_counts_duplicate_memberships_without_inventing_files(self):
        rows=self.inventory_rows();rows.append(dict(rows[0]))
        value=upstream.inventory_summary(rows,100)
        self.assertEqual(value['activeRecords'],46)
        self.assertEqual(value['uniqueIds'],45)
        self.assertEqual(value['uniqueFiles'],45)
        self.assertEqual(value['manifestDisabledRecords'],3)
        self.assertEqual(value['excludedByDefaultFilters'],54)
        for invalid in (True,45,0):
            with self.assertRaises(Failure): upstream.inventory_summary(rows,invalid)

    def test_complete_proof_binds_plan_ids_counts_assets_and_inventory(self):
        value=self.proof()
        self.assertTrue(upstream.proof_complete(value,[stage(name) for name in upstream.EXTRA_STAGES]))
        self.assertFalse(value['fullUpstreamSuiteExecuted'])
        self.assertFalse(value['fullHostQualification'])

    def test_forged_or_partial_proof_is_rejected(self):
        paths=[('planSHA256',),('inventoryReceipt','sha256'),('case','harness','sha256'),
               ('case','harness','counts','selected'),('case','harness','counts','executed'),
               ('case','harness','counts','skipped'),('case','harness','counts','passed'),
               ('case','harness','counts','unsupported'),('case','harness','counts','unattempted'),
               ('case','harness','counts','rawLifecycle','testEnds'),('case','inputBindingStable'),
               ('case','harness','counts','rawLifecycle','testEndStatuses','FAIL'),
               ('case','harness','counts','rawLifecycle','testEndStatuses','SKIP'),
               ('case','harness','counts','rawLifecycle','testEndStatuses','ERROR'),
               ('case','harness','counts','rawLifecycle','testEndStatuses','TIMEOUT'),
               ('case','process','exitCode'),('case','process','exactReaped'),('case','monitorFailure'),
               ('fullUpstreamSuiteExecuted',),('fullHostQualification',),
               ('assets','binaries','TestArguments','uuid'),('assets','xpcomTypelib','bytes')]
        base=self.proof();stages=[stage(name) for name in upstream.EXTRA_STAGES]
        for path in paths:
            value=copy.deepcopy(base);node=value
            for key in path[:-1]: node=node[key]
            old=node[path[-1]];node[path[-1]]=True if type(old) is int else (False if old is True else 'forged')
            with self.subTest(path=path): self.assertFalse(upstream.proof_complete(value,stages))
        for field,replacement in (('planSHA256','a'*64),('serverProcessesStarted',True),('testsExecuted',1)):
            value=copy.deepcopy(base);value['inventory'][field]=replacement
            value['inventoryReceipt']=binding(value['inventory'])
            self.assertFalse(upstream.proof_complete(value,stages))
        for replace in ('id','expected'):
            value=copy.deepcopy(base);value['case']['harness']['tests'][0][replace]='forged'
            self.assertFalse(upstream.proof_complete(value,stages))
        value=copy.deepcopy(base);value['inventory']['scope']['manifestDisabledRecords']=True
        value['inventoryReceipt']=binding(value['inventory'])
        self.assertFalse(upstream.proof_complete(value,stages))
        self.assertFalse(upstream.proof_complete(base,stages[:-1]))
        self.assertFalse(upstream.proof_complete(base,stages+[stages[0]]))
        stages[0]['exitCode']=True
        self.assertFalse(upstream.proof_complete(base,stages))

    def test_build_commands_share_original_deadline(self):
        calls=[];runner=SimpleNamespace(limits={'jobMinutes':360},run=lambda *a,**k:calls.append((a,k)))
        upstream.child_and_resources(self.task,self.task,runner,{},345)
        self.assertEqual([call[0][1][1:] for call in calls],
                         [['mach','build','-j3','-C','.','ipc/app/target'],
                          ['mach','build','-j3','-C','ipc/app','libs'],['mach','build','-j3','faster']])
        self.assertTrue(all(call[0][4]==360 and call[1]['phase_deadline_minutes']==345 for call in calls))

    def test_child_libs_failure_does_not_start_resource_repackage(self):
        calls=[]
        def run(stage,*args,**kwargs):
            calls.append(stage)
            if stage==upstream.CHILD_LIBS_STAGE: raise Failure('controlled-libs-failure')
        runner=SimpleNamespace(limits={'jobMinutes':360},run=run)
        with self.assertRaisesRegex(Failure,'controlled-libs-failure'):
            upstream.child_and_resources(self.task,self.task,runner,{},345)
        self.assertEqual(calls,[upstream.CHILD_STAGE,upstream.CHILD_LIBS_STAGE])

    def scope_assets(self, *, wrong_uuid=False, diagnostic=None):
        import os
        root=self.task/'obj';unit=root/'_tests/xpcshell'/upstream.SCOPE
        unit.mkdir(parents=True)
        app=root/'dist/Floorp.app/Contents/MacOS/plugin-container.app/Contents/MacOS'
        app.mkdir(parents=True)
        paths=[unit/name for name in upstream.HELPERS]+[app/'plugin-container']
        for path in paths: path.write_bytes(b'current executable');path.chmod(0o700)
        typelib=root/'config/makefiles/xpidl/xpcomtest.xpt';typelib.parent.mkdir(parents=True)
        typelib.write_text(json.dumps([
            {'name':'nsIScriptableOK','uuid':'93142a4f-e4cf-424a-b833-e638f87d2607'},
            {'name':'nsIScriptableWithNotXPCOM','uuid':'wrong' if wrong_uuid else '237d01a3-771e-4c6e-adf9-c97f9aab2950'}]))
        (self.task/'effective-config.private.log').write_text(json.dumps({'substs':{
            'MOZ_MACBUNDLE_NAME':'Floorp.app','MOZ_CHILD_PROCESS_NAME':'plugin-container'}}))
        runner=SimpleNamespace(started=1)
        return upstream.verify_scope_assets(self.task,runner,lambda path,kind:{'filetype':kind,'uuid':'a'*32},
                                            diagnostic=diagnostic)

    def test_native_asset_precondition_reports_all_six_without_private_values(self):
        diagnostic={'private':'must be cleared'}
        self.scope_assets(diagnostic=diagnostic)
        self.assertEqual(diagnostic,{'schema':1,'status':'passed','checkedAssets':6,'failure':None,'inspection':None})
        self.assertEqual(upstream.asset_precondition_evidence(diagnostic),diagnostic)

    def test_native_asset_precondition_preserves_ordered_rejection_and_macho_boundary(self):
        import os
        from types import SimpleNamespace
        self.scope_assets()
        for predicate in upstream.ASSET_PREDICATES:
            with self.subTest(predicate=predicate):
                diagnostic={};macho=mock.Mock()
                with mock.patch.object(Path,'is_file',return_value=predicate!='file'), \
                     mock.patch.object(Path,'is_relative_to',return_value=predicate!='object-root'), \
                     mock.patch.object(Path,'stat',return_value=SimpleNamespace(
                         st_uid=os.geteuid()+1 if predicate=='owner' else os.geteuid(),
                         st_mtime=0 if predicate=='current-mtime' else 2)), \
                     mock.patch.object(upstream.os,'access',return_value=predicate!='executable'):
                    with self.assertRaisesRegex(Failure,'^mozilla-upstream-current-native-asset$'):
                        upstream.verify_scope_assets(self.task,SimpleNamespace(started=1),macho,
                                                     diagnostic=diagnostic)
                self.assertEqual(diagnostic,{'schema':1,'status':'failed','checkedAssets':0,
                    'failure':{'asset':'plugin-container','predicate':predicate},'inspection':None})
                self.assertEqual(upstream.asset_precondition_evidence(diagnostic),diagnostic)
                macho.assert_not_called()

    def test_native_asset_precondition_stale_helper_is_rejected_without_touching_mtime(self):
        import os
        from types import SimpleNamespace
        self.scope_assets()
        path=self.task/'obj/_tests/xpcshell'/upstream.SCOPE/'TestBlockingProcess'
        os.utime(path,(0,0));diagnostic={};macho=mock.Mock(return_value={'uuid':'a'*32})
        with self.assertRaisesRegex(Failure,'mozilla-upstream-current-native-asset'):
            upstream.verify_scope_assets(self.task,SimpleNamespace(started=1),macho,diagnostic=diagnostic)
        self.assertEqual(diagnostic,{'schema':1,'status':'failed','checkedAssets':2,
            'failure':{'asset':'TestBlockingProcess','predicate':'current-mtime'},'inspection':None})
        self.assertEqual(path.stat().st_mtime,0)
        self.assertEqual(macho.call_count,2)

    def test_native_asset_inspection_error_remains_unavailable_and_preserves_exception(self):
        from types import SimpleNamespace
        self.scope_assets();diagnostic={};error=PermissionError('PRIVATE_PATH')
        with mock.patch.object(Path,'is_file',side_effect=error):
            with self.assertRaises(PermissionError) as caught:
                upstream.verify_scope_assets(self.task,SimpleNamespace(started=1),mock.Mock(),
                                             diagnostic=diagnostic)
        self.assertIs(caught.exception,error)
        self.assertEqual(diagnostic['status'],'unavailable')
        self.assertEqual(diagnostic['checkedAssets'],0)
        self.assertIsNone(diagnostic['failure'])
        self.assertEqual(diagnostic['inspection'],{'asset':'plugin-container','predicate':'file'})
        self.assertNotIn('PRIVATE_PATH',json.dumps(upstream.asset_precondition_evidence(diagnostic)))

    def test_native_asset_projection_rejects_private_or_inconsistent_records(self):
        base={'schema':1,'status':'failed','checkedAssets':0,
              'failure':{'asset':'plugin-container','predicate':'current-mtime'},'inspection':None}
        unknown={'schema':1,'status':'unavailable','checkedAssets':None,'failure':None,'inspection':None}
        for change in (lambda d:d.update(schema=True),lambda d:d.update(checkedAssets=True),
                       lambda d:d.update(checkedAssets=6),lambda d:d.update(status='passed'),
                       lambda d:d.update(path='PRIVATE'),lambda d:d['failure'].update(asset='/PRIVATE'),
                       lambda d:d['failure'].update(predicate='PRIVATE'),
                       lambda d:d['failure'].update(message='PRIVATE')):
            value=copy.deepcopy(base);change(value)
            self.assertEqual(upstream.asset_precondition_evidence(value),unknown)
        self.assertEqual(upstream.asset_precondition_evidence(None),unknown)

    def test_typelib_gate_uses_actual_compiled_input_without_legacy_unit_copy(self):
        value=self.scope_assets()
        self.assertTrue(value['xpcomTypelib']['interfacesVerified'])
        self.assertFalse((self.task/'obj/_tests/xpcshell'/upstream.SCOPE/'xpcomtest.xpt').exists())
        self.assertEqual(set(value['binaries']),set(upstream.HELPERS)|{'plugin-container'})

    def test_typelib_gate_rejects_wrong_interface_identity(self):
        with self.assertRaisesRegex(Failure,'mozilla-upstream-typelib'): self.scope_assets(wrong_uuid=True)

    def run_case_control(self, *, fail=False, collection=False, writing=False, monitoring=False, setup=False):
        (self.task/'effective-config.private.log').write_text(json.dumps({'substs':{'MOZ_MACBUNDLE_NAME':'Floorp.app'}}))
        if setup: (self.task/'mozilla-upstream-xpcom').mkdir()
        primary={'stage':'mozilla-xpcshell','code':'nonzero-exit','elapsedSeconds':1}
        runner=SimpleNamespace(records=[],failure=primary if fail else None,
            monitor_failure={'stage':'mozilla-xpcshell','reason':'monitor-unavailable'} if monitoring else None,
            monitor_diagnostics=[])
        runner.run=mock.Mock(side_effect=Failure('nonzero-exit') if fail else None)
        parent=SimpleNamespace(started=1,monotonic_started=20,limits={'jobMinutes':360})
        process={'available':True,'exitCode':1 if fail else 0,'exactReaped':True,'groupGone':True,
                 'cleanupClean':True,'monitorFailureAbsent':not monitoring,'reason':'nonzero-exit' if fail else None}
        def evidence(value):
            if value is not None and collection: raise ValueError('private evidence failure')
            return process if value is not None else {'available':False}
        result={'planSHA256':self.plan_hash,'inventory':{'scopeTests':[
            {'id':name,'expected':expected} for name,expected in self.expected.items()]}}
        receipt=self.receipt()
        with mock.patch.object(upstream,'Runner',return_value=runner), \
                mock.patch.object(upstream,'scope_plan',return_value=(self.plan,self.plan_hash)), \
                mock.patch.object(upstream,'process_evidence',side_effect=evidence), \
                mock.patch.object(upstream,'scope_receipt',return_value=receipt), \
                mock.patch.object(upstream,'child_environment',return_value={}), \
                mock.patch.object(upstream,'monitor_evidence',side_effect=lambda value:value), \
                mock.patch.object(upstream,'write_json',side_effect=PermissionError('private write') if writing else None):
            upstream.run_scope(self.task,self.task,parent,{},345,result)
        if not setup:
            args,kwargs=runner.run.call_args
            self.assertEqual(args[4],360)
            self.assertEqual(kwargs['phase_deadline_minutes'],345)
            self.assertNotIn('--manifest',args[1])
            self.assertIn('--sequential',args[1])
            self.assertEqual(args[1][args[1].index('--threads')+1],'1')
        return result,primary

    def test_run_scope_uses_owned_sequential_process_and_original_clock(self):
        result,_=self.run_case_control()
        self.assertEqual(result['status'],'passed')
        self.assertTrue(result['case']['inputBindingStable'])

    def test_run_scope_setup_failure_keeps_unattempted_counts_unknown(self):
        result,_=self.run_case_control(setup=True)
        self.assertEqual(result['status'],'failed')
        self.assertEqual(result['case']['caseFailurePhase'],'setup')
        self.assertIsNone(result['case']['harness']['counts'])

    def test_write_failure_cannot_replace_child_primary_or_keep_pass(self):
        result,primary=self.run_case_control(fail=True,writing=True)
        self.assertEqual(result['status'],'failed')
        self.assertEqual(result['case']['runnerFailure'],primary)
        self.assertEqual(result['case']['caughtCode'],'nonzero-exit')
        self.assertIsNotNone(result['case']['evidenceWriteFailure'])

    def test_collection_failure_cannot_replace_child_primary_or_keep_pass(self):
        result,primary=self.run_case_control(fail=True,collection=True)
        self.assertEqual(result['status'],'failed')
        self.assertEqual(result['case']['runnerFailure'],primary)
        self.assertEqual(result['case']['caughtCode'],'nonzero-exit')
        self.assertIsNotNone(result['case']['evidenceCollectionFailure'])

    def test_nested_monitor_failure_is_inconclusive_not_product_failure(self):
        import pipeline
        result,_=self.run_case_control(monitoring=True,collection=True)
        self.assertTrue(result['case']['monitorFailureObserved'])
        outcome=pipeline.validation_outcome({'status':'failed','mozillaUpstream':result})
        self.assertEqual(outcome['execution'],'monitor-inconclusive')
        self.assertFalse(outcome['monitorFailureEstablishesProductFailure'])

    def driver(self, *, execute=False, mismatch=False, exit_code=None, cleanup_error=False):
        source=self.task/'source';source.mkdir()
        task=self.task/'task';(task/'obj/_tests/xpcshell').mkdir(parents=True)
        (task/'obj/_tests/xpcshell/xpcshell.toml').write_text('')
        calls=[]
        def disable():
            calls.append('disable')
            if cleanup_error: raise PermissionError('PRIVATE_SENTINEL')
        manager=SimpleNamespace(enable_unstructured=lambda:calls.append('enable'),disable_unstructured=disable)
        stock=ModuleType('runxpcshelltests');rows=self.inventory_rows()
        class Original:
            def __init__(self,**kwargs):
                self.mozHttp2Server=self.http3Server=None;self.testCount=0
            def getTestManifest(self,manifest): return SimpleNamespace(tests=rows)
            def buildTestList(self,*a,**k): return rows
            def runTests(self,options,*,testClass):
                self.manifest=options['manifest'];self.trySetupNode();self.startHttp3Server();self.buildTestList()
                if execute: testClass()
                Path(options['dump_tests']).write_text(json.dumps({'active_tests':rows[:-1] if mismatch else rows}))
                raise SystemExit(exit_code)
        stock.XPCShellTests=Original
        class Build:
            topsrcdir=str(source);topobjdir=str(task/'obj');log_manager=None
            def _spawn(self,cls):
                if cls is FakeDriver: return SimpleNamespace(install_tests=lambda **kw:calls.append(('install',kw)))
                outer=self
                class Harness:
                    log_manager=outer.log_manager
                    def _run_xpcshell_harness(self,**options):
                        self.log_manager.enable_unstructured()
                        if options['manifest']: options['manifest'].test_paths()
                        options['manifest']=str(task/'obj/_tests/xpcshell/xpcshell.toml')
                        return stock.XPCShellTests().runTests(options)
                return Harness()
        FakeDriver=type('FakeDriver',(),{})
        def parser():
            def parse(args):
                self.assertNotIn('--manifest',args)
                return SimpleNamespace(manifest=None,testPaths=[] if args[-1]!=upstream.SCOPE else [upstream.SCOPE],
                                       dump_tests=args[args.index('--dump-tests')+1])
            return SimpleNamespace(parse_args=parse)
        module=ModuleType('stock_mach_commands');module.XPCShellRunner=type('Runner',(),{})
        spec=SimpleNamespace(loader=SimpleNamespace(exec_module=lambda m:None))
        modules={name:ModuleType(name) for name in ('xpcshellcommandline','mozbuild','mozbuild.base',
                   'mozbuild.controller','mozbuild.controller.building','mach','mach.logging','mozlog','mozlog.structuredlog')}
        modules['runxpcshelltests']=stock;modules['xpcshellcommandline'].parser_desktop=parser
        modules['mozbuild.base'].MozbuildObject=SimpleNamespace(from_environment=lambda **kw:Build())
        modules['mozbuild.controller.building'].BuildDriver=FakeDriver
        modules['mach.logging'].LoggingManager=lambda:manager
        modules['mozlog.structuredlog'].StructuredLogger=lambda name:None
        with mock.patch.dict('sys.modules',modules),mock.patch.object(upstream,'scope_plan',return_value=(self.plan,self.plan_hash)), \
                mock.patch.object(upstream,'bootstrap_testing_category'), \
                mock.patch.object(upstream.importlib.util,'spec_from_file_location',return_value=spec), \
                mock.patch.object(upstream.importlib.util,'module_from_spec',return_value=module):
            try: upstream.inventory_driver(source,task,task/'future/xpcshell')
            finally:
                self.journal=json.loads((task/upstream.JOURNAL_FILE).read_text())
                self.assertIs(stock.XPCShellTests,Original)
                self.assertEqual(calls.count('enable'),calls.count('disable'))
        self.assertEqual(calls,[('install',{'force':True}),'enable','disable','enable','disable'])
        return json.loads((task/'mozilla-upstream-inventory.json').read_text())

    def test_inventory_uses_default_manifest_live_log_manager_and_no_tests(self):
        value=self.driver()
        self.assertEqual(value['status'],'observed')
        self.assertEqual(value['testsExecuted'],0)
        self.assertEqual(value['serverProcessesStarted'],0)
        self.assertEqual(value['scope']['manifestDisabledRecords'],3)

    def test_inventory_refuses_test_instantiation_and_restores_globals(self):
        with self.assertRaisesRegex(Failure,'inventory-started-test'): self.driver(execute=True)

    def test_inventory_refuses_partial_scope_and_restores_globals(self):
        with self.assertRaisesRegex(Failure,'scope-selection'): self.driver(mismatch=True)

    def test_inventory_journal_records_both_dumps_and_success(self):
        self.driver()
        journal=upstream.inventory_diagnostic(self.journal)
        self.assertEqual(journal['status'],'complete')
        self.assertIn('global-read',journal['phases'])
        self.assertIn('scope-read',journal['phases'])
        self.assertEqual(journal['lastReached'],'complete')
        self.assertIsNone(journal['error'])

    def test_inventory_journal_keeps_failure_phase_before_cleanup(self):
        with self.assertRaises(Failure): self.driver(mismatch=True)
        self.assertEqual(self.journal['errorPhase'],'scope-read')
        self.assertEqual(self.journal['lastReached'],'restore-stock-class')
        self.assertEqual(self.journal['error']['code'],'mozilla-upstream-scope-selection')
        self.assertEqual(self.journal['status'],'failed')

    def bootstrap(self, existing=None, *, tamper=False, orphan=False):
        source=self.task/'bootstrap';(source/'build').mkdir(parents=True)
        raw=b"CATEGORIES={'testing':{'short':'Testing','long':'Run tests.','priority':60}}\n"
        (source/'build/mach_initialize.py').write_bytes(raw+(b'# changed' if tamper else b''))
        commands={'testing':{'existing-command'}} if existing is not None or orphan else {}
        class Registrar:
            categories={} if existing is None else {'testing':existing}
            commands_by_category=commands
            @classmethod
            def register_category(cls,name,*metadata):
                cls.categories[name]=metadata;cls.commands_by_category[name]=set()
        module=ModuleType('mach.registrar');module.Registrar=Registrar
        journal=upstream.InventoryJournal(self.task);journal.phase('entry')
        receipt={'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
        with mock.patch.dict('sys.modules',{'mach.registrar':module}), \
                mock.patch.object(upstream,'BOOTSTRAP_RECEIPT',receipt):
            upstream.bootstrap_testing_category(source,journal)
        return Registrar,journal

    def test_category_bootstrap_registers_real_metadata_and_preserves_existing_commands(self):
        registrar,journal=self.bootstrap(('Testing','Run tests.',60))
        self.assertEqual(registrar.commands_by_category['testing'],{'existing-command'})
        self.assertTrue(journal.value['categoryInitiallyPresent'])

    def test_category_bootstrap_initializes_empty_registry(self):
        registrar,journal=self.bootstrap()
        self.assertEqual(registrar.categories['testing'],('Testing','Run tests.',60))
        self.assertFalse(journal.value['categoryInitiallyPresent'])

    def test_category_bootstrap_rejects_foreign_metadata_and_changed_source(self):
        with self.assertRaisesRegex(Failure,'category-binding'): self.bootstrap(('foreign','private',True))
        (self.task/'bootstrap/build/mach_initialize.py').unlink()
        (self.task/'bootstrap/build').rmdir();(self.task/'bootstrap').rmdir()
        with self.assertRaisesRegex(Failure,'category-source'): self.bootstrap(tamper=True)

    def test_journal_preserves_primary_when_persistence_and_cleanup_fail(self):
        journal=upstream.InventoryJournal(self.task)
        with mock.patch.object(upstream,'write_json',side_effect=PermissionError('PRIVATE_SENTINEL')):
            journal.phase('entry');journal.phase('imports')
            journal.fail(ModuleNotFoundError('PRIVATE_SENTINEL',name='PRIVATE_SENTINEL'))
            journal.fail(ValueError('PRIVATE_SENTINEL'),cleanup=True)
        journal.persist()
        output=upstream.inventory_diagnostic(json.loads(journal.path.read_text()))
        self.assertEqual(output['error']['type'],'ModuleNotFoundError')
        self.assertEqual(output['errorPhase'],'imports')
        self.assertEqual(output['writeFailure'],'PermissionError')
        self.assertEqual(output['cleanupError']['type'],'ValueError')
        self.assertNotIn('PRIVATE_SENTINEL',json.dumps(output))

    def test_early_stock_import_failure_records_typed_error_without_text(self):
        with mock.patch.object(upstream,'_inventory_driver',side_effect=ImportError('PRIVATE_SENTINEL')):
            with self.assertRaises(ImportError): upstream.inventory_driver(self.task,self.task,self.path)
        result={};upstream.collect_inventory_diagnostic(self.task,result)
        self.assertEqual(result['inventoryDiagnostic']['status'],'observed')
        self.assertEqual(result['inventoryDiagnostic']['journal']['error']['type'],'ImportError')
        self.assertNotIn('PRIVATE_SENTINEL',json.dumps(result))

    def test_missing_invalid_or_private_journal_is_unknown(self):
        result={};upstream.collect_inventory_diagnostic(self.task,result)
        self.assertEqual(result['inventoryDiagnostic']['status'],'unavailable')
        journal=upstream.InventoryJournal(self.task);journal.phase('entry')
        value=dict(journal.value);value['PRIVATE_SENTINEL']='private'
        journal.path.write_text(json.dumps(value))
        upstream.collect_inventory_diagnostic(self.task,result)
        self.assertEqual(result['inventoryDiagnostic']['status'],'unavailable')
        self.assertIsNone(result['inventoryDiagnostic']['journal'])
        self.assertNotIn('PRIVATE_SENTINEL',json.dumps(result))

    def test_failed_inventory_process_still_collects_journal_and_preserves_primary(self):
        journal=upstream.InventoryJournal(self.task);journal.phase('entry');journal.phase('imports')
        journal.fail(ImportError('PRIVATE_SENTINEL'))
        primary=Failure('nonzero-exit')
        calls=[]
        def run(name,*args,**kwargs):
            calls.append(name)
            if name==upstream.INVENTORY_STAGE: raise primary
        runner=SimpleNamespace(limits={'jobMinutes':360},run=run)
        (self.task/'effective-config.private.log').write_text(json.dumps({'substs':{'MOZ_MACBUNDLE_NAME':'Floorp.app'}}))
        result={}
        with mock.patch.object(upstream,'scope_plan',return_value=(self.plan,self.plan_hash)):
            with self.assertRaises(Failure) as raised:
                upstream.prepare(self.task,self.task,runner,{},345,result)
        self.assertIs(raised.exception,primary)
        self.assertEqual(result['inventoryDiagnostic']['journal']['errorPhase'],'imports')
        self.assertEqual(result['status'],'failed')
        self.assertIsNone(result['inventory'])
        self.assertEqual(calls,list(upstream.PREPARE_STAGES))

    def test_helper_install_failure_stops_before_inventory_under_original_deadline(self):
        calls=[]
        primary=Failure('controlled-install-failure')
        def run(stage,command,cwd,env,limit,**kwargs):
            calls.append(stage)
            self.assertEqual(limit,360)
            self.assertEqual(kwargs['phase_deadline_minutes'],345)
            if stage==upstream.HELPER_INSTALL_STAGE:
                self.assertEqual(command[1:],['mach','build','-j3','-C','xpcom/tests','misc'])
                raise primary
        runner=SimpleNamespace(limits={'jobMinutes':360},run=run)
        with mock.patch.object(upstream,'scope_plan',return_value=(self.plan,self.plan_hash)):
            with self.assertRaises(Failure) as raised:
                upstream.prepare(self.task,self.task,runner,{},345,{})
        self.assertIs(raised.exception,primary)
        self.assertEqual(calls,[upstream.PREPARE_STAGE,upstream.HELPER_INSTALL_STAGE])

    def test_build_without_helper_install_cannot_qualify(self):
        rows=[stage(name) for name in upstream.EXTRA_STAGES if name!=upstream.HELPER_INSTALL_STAGE]
        self.assertFalse(upstream.proof_complete(self.proof(),rows))

    def test_diagnostic_exception_codes_are_fixed_and_system_exit_is_typed(self):
        self.assertIsNone(upstream.diagnostic_error(Failure('PRIVATE_SENTINEL'))['code'])
        self.assertEqual(upstream.diagnostic_error(SystemExit(3))['exitCode'],3)
        self.assertIsNone(upstream.diagnostic_error(SystemExit(True))['exitCode'])
        self.assertEqual(upstream.diagnostic_error(SystemExit('PRIVATE_SENTINEL'))['exitKind'],'other')

    def test_category_bootstrap_rejects_orphan_commands_without_registration(self):
        with self.assertRaisesRegex(Failure,'category-binding'): self.bootstrap(orphan=True)

    def test_nonzero_system_exit_keeps_original_code_and_phase_across_cleanup_failure(self):
        for cleanup_error in (False,True):
            with self.subTest(cleanup_error=cleanup_error):
                with self.assertRaisesRegex(Failure,'inventory-exit'):
                    self.driver(exit_code=7,cleanup_error=cleanup_error)
                self.assertEqual(self.journal['errorPhase'],'global-invoke')
                self.assertEqual(self.journal['error']['type'],'SystemExit')
                self.assertEqual(self.journal['error']['exitCode'],7)
                if cleanup_error:self.assertEqual(self.journal['cleanupError']['type'],'PermissionError')
                else:self.assertIsNone(self.journal['cleanupError'])
                import shutil
                shutil.rmtree(self.task/'source');shutil.rmtree(self.task/'task')


if __name__=='__main__': unittest.main()
