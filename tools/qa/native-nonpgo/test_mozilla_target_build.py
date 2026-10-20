# SPDX-License-Identifier: MPL-2.0
import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import mozilla_target_build as target
from support import Failure


def primary_anchor(code=0):
    return {'policy': 'retained-direct-child-v1', 'retirementVerified': True,
            'actualReaped': True, 'actualReturncode': code, 'signalAuthorityRevoked': True,
            'postReapAbsence': {'status': 'observed', 'groupEmpty': True,
                                'readOnly': True, 'signalAuthority': False},
            'scope': 'active-process-group-epoch', 'sessionOrDescendantClosure': 'unconfirmed'}


def row(stage):
    return {'stage': stage, 'exitCode': 0, 'reason': None, 'failurePhase': None,
            'cleanupFailure': None, 'groupCleanup': 'verified-gone',
            'leaderReap': {'status': 'reaped', 'exitCode': 0}, 'primaryAnchor': primary_anchor()}


class TargetBuildTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='mozilla-target-')
        self.addCleanup(temporary.cleanup)
        self.task = Path(temporary.name).resolve()
        self.calls = []
        self.runner = SimpleNamespace(records=[], failure=None, monitor_failure=None,
                                      limits={'jobMinutes': 360}, started=1, fail=mock.Mock())
        def run(stage, command, cwd, env, platform, *, phase_deadline_minutes):
            self.calls.append((stage, command, cwd, env, platform, phase_deadline_minutes))
            self.runner.records.append(row(stage))
        self.runner.run = run

    def build(self, env=None):
        result = {}
        with mock.patch.object(target, 'graph_precondition', return_value={'bindingsStable': True}), \
                contextlib.redirect_stderr(io.StringIO()):
            target.run_build(self.task, self.task, self.runner, env or {}, 345, result)
        return result

    def test_root_compile_then_documented_faster_default_with_one_existing_deadline(self):
        result = self.build()
        self.assertEqual(result['completedStages'], list(target.STAGES))
        self.assertEqual(result['status'], 'built')
        for call, (_, node) in zip(self.calls, target.TARGETS):
            self.assertEqual(call[1][1:], ['mach', 'build', '-j3', '-C', '.', node])
        self.assertEqual(self.calls[-1][1][1:], ['mach', 'build', '-j3', 'faster'])
        self.assertTrue(all(call[4:] == (360, 345) for call in self.calls))
        self.assertFalse(result['clockResetAllowed'])
        self.assertFalse(result['crossJobProgressPersisted'])
        self.assertEqual(json.loads((self.task/'mozilla-target-build.json').read_text()), result)

    def test_filter_or_hybrid_environment_rejected_before_any_command(self):
        for key in ('RECURSE_BASE_DIR', 'FASTER_RECURSIVE_MAKE'):
            with self.subTest(key=key), self.assertRaises(Failure):
                self.build({key: '1'})
        self.assertEqual(self.calls, [])

    def test_failed_later_stage_preserves_completed_shell_and_never_starts_resources(self):
        original = self.runner.run
        def fail(stage, *args, **kwargs):
            if stage == 'mozilla-http3-build':
                raise Failure('controlled-target-failure')
            return original(stage, *args, **kwargs)
        self.runner.run = fail
        with self.assertRaisesRegex(Failure, 'controlled-target-failure'):
            self.build()
        saved = json.loads((self.task/'mozilla-target-build.json').read_text())
        self.assertEqual(saved['completedStages'], ['mozilla-shell-build'])
        self.assertEqual(saved['currentStage'], 'mozilla-http3-build')
        self.assertEqual(saved['status'], 'failed')
        self.assertEqual(len(self.calls), 1)

    def test_journal_failure_is_primary_and_prevents_launch(self):
        with mock.patch.object(target, 'write_json', side_effect=OSError), self.assertRaises(Failure):
            self.build()
        self.runner.fail.assert_called_once_with('mozilla-target-evidence', 'mozilla-target-evidence-write')
        self.assertEqual(self.calls, [])

    def test_success_requires_integer_zero_exact_reap_and_explicit_cleanup_fields(self):
        self.assertTrue(target.stage_complete(row('stage')))
        variants = [dict(exitCode=False), dict(reason='nonzero-exit'), dict(cleanupFailure='unknown'),
                    dict(groupCleanup='unverified'), dict(leaderReap={'status':'reaped','exitCode':True}),
                    dict(leaderReap={'status':'pending','exitCode':0})]
        for changes in variants:
            value = row('stage'); value.update(changes)
            self.assertFalse(target.stage_complete(value))
        value = row('stage'); del value['cleanupFailure']
        self.assertFalse(target.stage_complete(value))

    def test_primary_anchor_requires_exact_shape_types_retirement_and_group_scope(self):
        valid = row('stage')
        self.assertTrue(target.stage_complete(valid))
        variants = []
        old = copy.deepcopy(valid); del old['primaryAnchor']; variants.append(old)
        for key in target.PRIMARY_ANCHOR_KEYS:
            value = copy.deepcopy(valid); del value['primaryAnchor'][key]; variants.append(value)
        for key in target.PRIMARY_ABSENCE_KEYS:
            value = copy.deepcopy(valid); del value['primaryAnchor']['postReapAbsence'][key]; variants.append(value)
        for key, bad in (
            ('policy', 'legacy'), ('retirementVerified', False), ('retirementVerified', 1),
            ('actualReaped', None), ('actualReaped', 1), ('actualReturncode', None),
            ('actualReturncode', False), ('actualReturncode', 7), ('signalAuthorityRevoked', False),
            ('signalAuthorityRevoked', 1), ('scope', 'all-descendants'),
            ('sessionOrDescendantClosure', 'verified'), ('privatePath', '/PRIVATE')):
            value = copy.deepcopy(valid); value['primaryAnchor'][key] = bad; variants.append(value)
        for key, bad in (('status', 'unconfirmed'), ('status', None), ('groupEmpty', None),
                         ('groupEmpty', 1), ('groupEmpty', False), ('readOnly', 1),
                         ('readOnly', False), ('signalAuthority', 0), ('signalAuthority', True),
                         ('argv', ['PRIVATE'])):
            value = copy.deepcopy(valid); value['primaryAnchor']['postReapAbsence'][key] = bad
            variants.append(value)
        for value in variants:
            with self.subTest(proof=value.get('primaryAnchor')):
                self.assertFalse(target.stage_complete(value))

    def test_invalid_anchor_stops_build_before_any_next_target(self):
        original = self.runner.run
        def old_record(stage, *args, **kwargs):
            original(stage, *args, **kwargs)
            del self.runner.records[-1]['primaryAnchor']
        self.runner.run = old_record
        with self.assertRaisesRegex(Failure, 'mozilla-target-stage-proof'):
            self.build()
        self.assertEqual(len(self.calls), 1)
        saved = json.loads((self.task/'mozilla-target-build.json').read_text())
        self.assertEqual(saved['completedStages'], [])

    def graph_files(self):
        nodes = [node for _, node in target.TARGETS] + [
            'toolkit/library/build/target', 'toolkit/library/rust/target-objects', 'widget/cocoa/target-objects']
        raw = ('compile_targets := '+' '.join(nodes)+'\nmisc_dirs := toolkit/modules\nlibs_dirs := browser/app\n'
               'tools_dirs := browser/app\ninclude root-deps.mk\n').encode()
        deps = (b'js/xpconnect/shell/target: toolkit/library/build/target widget/cocoa/target-objects\n'
                b'toolkit/library/build/target: toolkit/library/rust/target-objects\n'
                b'recurse_tools: browser/app/tools\n')
        values = {'rootMk':raw,'rootDepsMk':deps,'fasterMakefile':b'# fixed generated backend\n'}
        pins = {}
        for name, data in values.items():
            relative, cap, _ = target.GRAPH_PINS[name]
            path = self.task/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
            pins[name] = relative, cap, hashlib.sha256(data).hexdigest()
        return pins

    def test_actual_bounded_reader_and_graph_parser_bind_all_compile_targets(self):
        with mock.patch.object(target, 'GRAPH_PINS', self.graph_files()):
            proof = target.graph_precondition(self.task)
        self.assertTrue(proof['bindingsStable'])
        self.assertTrue(proof['graph']['fixedReachability']['xpcshellToXUL'])

    def test_changed_or_missing_backend_and_symlink_are_not_accepted(self):
        pins = self.graph_files()
        with mock.patch.object(target, 'GRAPH_PINS', pins):
            (self.task/'obj/faster/Makefile').write_bytes(b'changed')
            with self.assertRaises(Failure): target.graph_precondition(self.task)
        pins = self.graph_files()
        (self.task/'obj/faster/Makefile').unlink()
        (self.task/'obj/faster/Makefile').symlink_to(self.task/'obj/root.mk')
        with mock.patch.object(target, 'GRAPH_PINS', pins), self.assertRaises(Exception):
            target.graph_precondition(self.task)

    def test_missing_move_asset_cannot_become_valid_bundle(self):
        (self.task/'effective-config.private.log').write_text(json.dumps({'substs':{
            'MOZ_MACBUNDLE_NAME':'Test.app','MOZ_APP_NAME':'test'}}))
        with self.assertRaises(Failure): target.verify_assets(self.task, self.runner, mock.Mock())

    def test_channel_static_source_link_is_fixed_hash_bound_and_other_external_links_fail(self):
        root = self.task/'obj'; app = root/'dist/Test.app/Contents'
        (self.task/'effective-config.private.log').write_text(json.dumps({'substs':{
            'MOZ_MACBUNDLE_NAME':'Test.app','MOZ_APP_NAME':'test'}}))
        for relative in ('MacOS/xpcshell','MacOS/XUL','MacOS/test',
                         'Library/LaunchServices/org.mozilla.updater',
                         'Frameworks/ChannelPrefs.framework/ChannelPrefs',
                         'Library/LaunchServices/org.mozilla.dmgInstallHelper'):
            path=app/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'binary');path.chmod(0o755)
        http3=root/'dist/bin/http3server';http3.parent.mkdir(parents=True);http3.write_bytes(b'binary');http3.chmod(0o755)
        for relative in ('Resources/modules/AppConstants.sys.mjs','Resources/application.ini','Resources/dependentlibs.list'):
            path=app/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'resource')
        source=self.task/'source'; reference=source/target.CHANNEL_INFO_SOURCE
        reference.parent.mkdir(parents=True);reference.write_bytes(b'fixed source plist')
        link=app/'Frameworks/ChannelPrefs.framework/Resources/Info.plist';link.parent.mkdir(parents=True)
        link.symlink_to(reference)
        checksum=hashlib.sha256(reference.read_bytes()).hexdigest()
        with mock.patch.object(target,'CHANNEL_INFO_SHA256',checksum):
            assets=target.verify_assets(self.task,self.runner,lambda *_:{'uuid':'a'*32},source=source)
            self.assertEqual(assets['resources']['channelInfo']['sha256'],checksum)
            with self.assertRaises(Failure):target.verify_assets(self.task,self.runner,lambda *_:{'uuid':'a'*32})
            outside=self.task/'unrelated.plist';outside.write_bytes(reference.read_bytes())
            original_verify=target._FixedRead.verify
            def retarget_after_verify(reader):
                original_verify(reader)
                link.unlink();link.symlink_to(outside)
            for content in (reference.read_bytes(), b'foreign source plist'):
                outside.write_bytes(content)
                with mock.patch.object(target._FixedRead,'verify',retarget_after_verify):
                    with self.assertRaises(Failure):
                        target.verify_assets(self.task,self.runner,lambda *_:{'uuid':'a'*32},source=source)
                link.unlink();link.symlink_to(reference)
            link.unlink();link.symlink_to(outside)
            with self.assertRaises(Failure):target.verify_assets(self.task,self.runner,lambda *_:{'uuid':'a'*32},source=source)
            link.unlink();link.symlink_to(reference);reference.write_bytes(b'changed source plist')
            with self.assertRaises(Failure):target.verify_assets(self.task,self.runner,lambda *_:{'uuid':'a'*32},source=source)

    def test_typed_case_and_stage_gate_rejects_missing_unknown_or_wrong_negative(self):
        def asset(kind=2):
            return {'bytes':100,'sha256':'a'*64,'filetype':kind,'uuid':'b'*32,
                    'sections':1,'sectionsSHA256':'c'*64}
        assets = {'xpcshell':asset(),'XUL':asset(6)}
        built = {**assets, **{key:asset(6 if key=='channelPrefs' else 2)
                             for key in ('http3server','browser','updater','channelPrefs','dmgHelper')}}
        value = {'contract':target.CONTRACT,'status':'passed','completedStages':list(target.STAGES),
                 'fullHostQualification':False,'clockResetAllowed':False,'crossJobProgressPersisted':False,
                 'minimalSufficientSuffixEstablished':False,'nativeAuthentication':'NOT_TESTED',
                 'currentStage':None,'durationToCompletion':'UNKNOWN',
                 'sameObjectTreeAcrossStages':True,
                 'assets':{'binaries':built,'resources':dict.fromkeys(
                     ('appConstants','applicationIni','dependentLibs','channelInfo'), {'bytes':1,'sha256':'d'*64})}}
        files = {key:{'status':'observed','sha256':pin[2],'bytes':1} for key,pin in target.GRAPH_PINS.items()}
        value['graph'] = {'bindingsStable':True,'files':files,'graph':{
            'status':'observed','scope':'root-deps-mk-literal-edges-only','xpcshellLiteralClosure':{'complete':True},
            'fixedReachability':dict.fromkeys(('xpcshellToXUL','XULToGkrust','xpcshellToGkrust','xpcshellToCocoaObjects'),True),
            'files':{key:{'bytes':1,'sha256':files[key]['sha256']} for key in ('rootMk','rootDepsMk')},
            'realBuildPerformed':False,'makeOrRecipeExecuted':False,'minimalSufficientSuffixEstablished':False,
            'fullHostQualification':False,'durationToCompletion':'UNKNOWN'}}
        self.graph_files()
        parsed = target.target_graph_proof((self.task/'obj/root.mk').read_bytes(),
                                          (self.task/'obj/root-deps.mk').read_bytes())
        parsed['files'] = value['graph']['graph']['files']
        value['graph']['graph'] = parsed
        cases = []
        for label, code, status in (('compiled-debug',0,'passed'),('failure-control',1,'failed'),
                                    ('upstream-version-comparator',0,'passed')):
            cases.append({'case':label,'mode':'negative-control' if code else 'positive',
                'expectedExitCode':code,'expectationSatisfied':True,
                'process':{'available':True,'exitCode':code,'exactReaped':True,
                'groupGone':True,'cleanupClean':True,'monitorFailureAbsent':True,
                'reason':'nonzero-exit' if code else None,'failurePhase':'wait' if code else None},
                'runnerFailure':{'stage':'mozilla-xpcshell','code':'nonzero-exit'} if code else None,
                'caughtCode':'nonzero-exit' if code else None,'caseError':None,'evidenceWriteFailure':None,
                'evidenceCollectionFailure':None,'harness':{'status':status,'completeLog':True,'expectationSatisfied':True}})
        smoke = {'contract':'current-source-mozilla-xpcshell-smoke-v1','builtAssets':assets,'cases':cases}
        rows = [row(stage) for stage in target.STAGES]
        self.assertTrue(target.proof_complete(value, smoke, rows))
        for change in ('negative-exit','unknown-cleanup','case-order','missing-log','stage-bool','asset-binding',
                       'missing-graph','unstable-graph','graph-pin','minimal','native','inprogress','forecast',
                       'missing-current-stage','extra-graph-field','missing-anchor','forged-anchor'):
            a,b,c=copy.deepcopy((value,smoke,rows))
            if change=='negative-exit':b['cases'][1]['process']['exitCode']=0
            elif change=='unknown-cleanup':b['cases'][0]['process']['cleanupClean']=None
            elif change=='case-order':b['cases'].reverse()
            elif change=='missing-log':del b['cases'][0]['harness']['completeLog']
            elif change=='stage-bool':c[0]['exitCode']=False
            elif change=='asset-binding':b['builtAssets']['XUL']={'uuid':'different'}
            elif change=='missing-graph':del a['graph']
            elif change=='unstable-graph':a['graph']['bindingsStable']=False
            elif change=='graph-pin':a['graph']['files']['rootMk']['sha256']='0'*64
            elif change=='minimal':a['minimalSufficientSuffixEstablished']=True
            elif change=='native':a['nativeAuthentication']='passed'
            elif change=='inprogress':a['currentStage']='mozilla-shell-build'
            elif change=='missing-current-stage':del a['currentStage']
            elif change=='extra-graph-field':a['graph']['graph']['privatePath']='/private/sentinel'
            elif change=='missing-anchor':del c[0]['primaryAnchor']
            elif change=='forged-anchor':c[0]['primaryAnchor']['postReapAbsence']['signalAuthority']=True
            else:a['durationToCompletion']='45 minutes'
            self.assertFalse(target.proof_complete(a,b,c), change)


if __name__ == '__main__':
    unittest.main()
