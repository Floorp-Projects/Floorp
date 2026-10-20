# SPDX-License-Identifier: MPL-2.0
"""Inventory the real registered suite and run the complete upstream XPCOM manifest."""
import argparse
from contextlib import ExitStack
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import time

from firefox_target_observer import _FixedRead, identity
from mozilla_harness_receipt import (END_STATUSES, SUB_STATUSES, _inputs, _parse, _read,
                                     _records, _Invalid)
from mozilla_harness_smoke import (child_environment, error_code, monitor_evidence,
                                   process_evidence, stage_evidence)
from support import Failure, Runner, digest, require, write_json

HERE = Path(__file__).resolve().parent
CONTRACT = 'current-source-upstream-xpcom-manifest-v1'
SCOPE = 'xpcom/tests/unit'
HELPERS = ('TestArguments', 'TestBlockingProcess', 'TestPRIntN',
           'TestQuickReturn', 'TestUnicodeArguments')
PREPARE_STAGE = 'mozilla-upstream-helpers-build'
INVENTORY_STAGE = 'mozilla-upstream-inventory'
CHILD_STAGE = 'mozilla-upstream-child-build'
RESOURCE_STAGE = 'mozilla-upstream-resources-build'
EXTRA_STAGES = (PREPARE_STAGE, INVENTORY_STAGE, CHILD_STAGE, RESOURCE_STAGE)


def plan_definition():
    raw = (HERE / 'mozilla-upstream.json').read_bytes()
    value = json.loads(raw)
    require(type(value) is dict and set(value) == {'contract','scope','source','tests','files'} and
            len(raw) <= 65536 and value['contract'] == CONTRACT and value['scope'] == SCOPE and
            value['source'] == '7dd619742b25567e479eeda97ae071d10542ad0b' and
            type(value['tests']) is list and len(value['tests']) == 45 and
            all(type(name) is str and re.fullmatch(r'test_[A-Za-z0-9_.-]+\.js',name)
                for name in value['tests']) and len(set(value['tests'])) == 45 and
            type(value['files']) is list and len(value['files']) == 58 and
            all(type(row) is dict and set(row) == {'path','bytes','sha256'} and
                type(row['path']) is str and not Path(row['path']).is_absolute() and
                '..' not in Path(row['path']).parts and '\\' not in row['path'] and
                type(row['bytes']) is int and 0 < row['bytes'] <= 1024**2 and
                sha256(row['sha256']) for row in value['files']) and
            len({row['path'] for row in value['files']}) == 58 and
            {SCOPE+'/'+name for name in value['tests']}.issubset(
                {row['path'] for row in value['files']}),
            'mozilla-upstream-source-plan')
    return value, hashlib.sha256(raw).hexdigest()


def sha256(value):
    return type(value) is str and re.fullmatch(r'[0-9a-f]{64}',value) is not None


def scope_plan(source):
    value,checksum=plan_definition()
    with ExitStack() as stack:
        reader=_FixedRead(source,stack,time.monotonic()+30)
        for row in value['files']:
            raw,record=reader.read(row['path'],1024**2)
            require(raw is not None and record['bytes']==row['bytes'] and
                    record['sha256']==row['sha256'],'mozilla-upstream-source-input')
        reader.verify()
    return value,checksum


def inventory_summary(rows, registered):
    require(type(rows) is list and 0 < len(rows) <= 10000 and
            type(registered) is int and registered >= len(rows), 'mozilla-upstream-inventory-shape')
    require(all(type(row) is dict and type(row.get('id')) is str and
                0 < len(row['id']) <= 4096 and type(row.get('path')) is str and
                row.get('expected') in ('pass', 'fail') for row in rows),
            'mozilla-upstream-inventory-shape')
    enabled = [row for row in rows if 'disabled' not in row]
    return {'registeredRecords': registered, 'activeRecords': len(rows),
            'manifestEnabledRecords': len(enabled), 'manifestDisabledRecords': len(rows)-len(enabled),
            'excludedByDefaultFilters': registered-len(rows),
            'uniqueIds': len({row['id'] for row in rows}),
            'uniqueEnabledIds': len({row['id'] for row in enabled}),
            'uniqueFiles': len({row['path'] for row in rows}),
            'expectedFailureRecords': sum(row['expected'] == 'fail' for row in enabled)}


def inventory_driver(source, task, binary):
    """Use stock prefs/mozinfo/normalization, suppressing both server startups only for listing."""
    import runxpcshelltests as stock
    from xpcshellcommandline import parser_desktop
    from mozbuild.base import MozbuildObject
    from mozbuild.controller.building import BuildDriver
    from mach.logging import LoggingManager
    from mozlog.structuredlog import StructuredLogger
    spec = importlib.util.spec_from_file_location('floorp_stock_xpcshell_mach',
        source/'testing/xpcshell/mach_commands.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    XPCShellRunner = module.XPCShellRunner

    plan, plan_hash = scope_plan(source)
    build = MozbuildObject.from_environment(cwd=str(source))
    build.log_manager = LoggingManager()
    require(Path(build.topsrcdir).resolve() == source and
            Path(build.topobjdir).resolve() == task / 'obj', 'mozilla-upstream-build-binding')
    build._spawn(BuildDriver).install_tests(force=True)
    original = stock.XPCShellTests
    captures = []

    class NeverRun:
        def __init__(self, *args, **kwargs):
            raise Failure('mozilla-upstream-inventory-started-test')

    class InventoryOnly(original):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.suppressed = []
            self.registered = None
            captures.append(self)

        def trySetupNode(self):
            self.suppressed.append('http2')

        def startHttp3Server(self):
            self.suppressed.append('http3')

        def buildTestList(self, *args, **kwargs):
            self.registered = len(self.getTestManifest(self.manifest).tests)
            return super().buildTestList(*args, **kwargs)

        def runTests(self, options, *args, **kwargs):
            return super().runTests(options, testClass=NeverRun)

    result = {'contract': CONTRACT, 'planSHA256': plan_hash, 'serverProcessesStarted': 0,
              'testsExecuted': 0, 'global': None, 'scope': None, 'scopeTests': [],
              'status': 'failed', 'temporaryPrefsOwnership': 'stock-obj-temp-not-removed'}
    manifest = task / 'obj/_tests/xpcshell/xpcshell.toml'
    require(manifest.is_file(), 'mozilla-upstream-registered-manifest')
    try:
        stock.XPCShellTests = InventoryOnly
        for label, paths in (('global', []), ('scope', [SCOPE])):
            output = task / ('upstream-' + label + '.raw.json')
            require(not output.exists(), 'mozilla-upstream-inventory-existing-output')
            options = vars(parser_desktop().parse_args([
                '--xpcshell', str(binary),
                '--dump-tests', str(output), *paths]))
            options.update(log=StructuredLogger('floorp-upstream-inventory-'+label),
                           self_test=False, profileName='firefox', threadCount=1)
            runner = build._spawn(XPCShellRunner)
            code = 'not-exited'
            try:
                runner._run_xpcshell_harness(**options)
            except SystemExit as error:
                code = error.code
            finally:
                # The stock method exits before its normal logger restoration.
                runner.log_manager.disable_unstructured()
            require(code is None or (type(code) is int and code == 0),
                    'mozilla-upstream-inventory-exit')
            capture = captures[-1]
            require(capture.suppressed == ['http2', 'http3'] and
                    type(capture.testCount) is int and capture.testCount == 0 and
                    capture.mozHttp2Server is None and capture.http3Server is None,
                    'mozilla-upstream-inventory-lifecycle')
            with ExitStack() as stack:
                reader = _FixedRead(task, stack, time.monotonic()+30)
                raw, receipt = reader.read(output.name, 16*1024**2)
                require(raw is not None, 'mozilla-upstream-inventory-output')
                data = json.loads(raw)
                require(type(data) is dict and set(data) == {'active_tests'},
                        'mozilla-upstream-inventory-shape')
                rows = data['active_tests']
                summary = inventory_summary(rows, capture.registered)
                reader.verify()
            result[label] = {**summary, 'dump': {key:receipt[key] for key in ('bytes','sha256')}}
            if label == 'scope':
                expected = {SCOPE+'/'+name for name in plan['tests']}
                require(len(rows) == 45 and {row['id'] for row in rows} == expected,
                        'mozilla-upstream-scope-selection')
                result['scopeTests'] = sorted(({'id': row['id'], 'expected':
                    'SKIP' if 'disabled' in row else row['expected'].upper()} for row in rows),
                    key=lambda row:row['id'])
        result['status'] = 'observed'
    finally:
        stock.XPCShellTests = original
    write_json(task / 'mozilla-upstream-inventory.json', result)
    print(json.dumps({'upstreamInventory':'observed',
                      'registeredRecords':result['global']['registeredRecords'],
                      'manifestEnabledRecords':result['global']['manifestEnabledRecords'],
                      'scopeRecords':result['scope']['activeRecords']}), flush=True)


def prepare(source, task, runner, env, deadline, result):
    plan, checksum = scope_plan(source)
    result.update(contract=CONTRACT, scope=SCOPE, status='failed', planSHA256=checksum,
                  inventory=None, assets=None, case=None, fullUpstreamSuiteExecuted=False,
                  fullHostQualification=False, nativeAuthentication='NOT_TESTED')
    runner.run(PREPARE_STAGE, [sys.executable,'mach','build','-j3','-C','.',
               'xpcom/tests/target'], source, env, runner.limits['jobMinutes'],
               phase_deadline_minutes=deadline)
    config = json.loads((task/'effective-config.private.log').read_text())['substs']
    bundle = config.get('MOZ_MACBUNDLE_NAME')
    require(type(bundle) is str and re.fullmatch(r'[A-Za-z0-9 _.-]+\.app',bundle) is not None and
            '..' not in bundle, 'mozilla-upstream-bundle')
    binary = task/'obj/dist'/bundle/'Contents/MacOS/xpcshell'
    runner.run(INVENTORY_STAGE, [sys.executable,'mach','python',str(HERE/'mozilla_upstream.py'),
               '--inventory','--source',str(source),'--task',str(task),'--binary',str(binary)],
               source, child_environment(env), runner.limits['jobMinutes'],
               phase_deadline_minutes=deadline)
    with ExitStack() as stack:
        reader = _FixedRead(task, stack, time.monotonic()+30)
        raw, record = reader.read('mozilla-upstream-inventory.json', 1024**2)
        require(raw is not None, 'mozilla-upstream-inventory-output')
        inventory = json.loads(raw)
        require(inventory.get('status') == 'observed' and inventory.get('planSHA256') == checksum,
                'mozilla-upstream-inventory-proof')
        reader.verify()
    result['inventory'] = inventory
    result['inventoryReceipt'] = {key:record[key] for key in ('bytes','sha256')}


def child_and_resources(source, task, runner, env, deadline):
    runner.run(CHILD_STAGE, [sys.executable,'mach','build','-j3','-C','.',
               'ipc/app/target'], source, env, runner.limits['jobMinutes'],
               phase_deadline_minutes=deadline)
    runner.run(RESOURCE_STAGE, [sys.executable,'mach','build','-j3','faster'],
               source, env, runner.limits['jobMinutes'], phase_deadline_minutes=deadline)


def verify_scope_assets(task, runner, macho):
    root = (task/'obj').resolve()
    unit = root/'_tests/xpcshell'/SCOPE
    config = json.loads((task/'effective-config.private.log').read_text())['substs']
    app = root/'dist'/config['MOZ_MACBUNDLE_NAME']/'Contents/MacOS'
    name = config.get('MOZ_CHILD_PROCESS_NAME')
    require(name == 'plugin-container', 'mozilla-upstream-child-name')
    paths = {name: app/(name+'.app')/'Contents/MacOS'/name,
             **{helper:unit/helper for helper in HELPERS}}
    result = {'binaries':{},'xpcomTypelib':None}
    for label,path in paths.items():
        require(path.is_file() and path.resolve().is_relative_to(root) and
                path.stat().st_uid == os.geteuid() and os.access(path,os.X_OK) and
                path.stat().st_mtime >= runner.started, 'mozilla-upstream-current-native-asset')
        value = macho(path,2)
        require(value.get('uuid') is not None, 'mozilla-upstream-native-identity')
        result['binaries'][label] = value
    # XPIDL now feeds this JSON module into xptdata.cpp and XUL. The legacy
    # manifest generated-files entry does not install a unit/xpcomtest.xpt.
    typelib=root/'config/makefiles/xpidl/xpcomtest.xpt'
    binding=identity(typelib.lstat());resolved=typelib.resolve()
    require(resolved.is_relative_to(root),'mozilla-upstream-typelib')
    with ExitStack() as stack:
        reader = _FixedRead(task,stack,time.monotonic()+30)
        raw,record = reader.read(str(resolved.relative_to(task)),1024**2)
        require(raw is not None and len(raw)>0,'mozilla-upstream-typelib')
        interfaces=json.loads(raw)
        require(type(interfaces) is list and len(interfaces)==2 and
                all(type(row) is dict for row in interfaces) and
                {row.get('name'):row.get('uuid') for row in interfaces}=={
                    'nsIScriptableOK':'93142a4f-e4cf-424a-b833-e638f87d2607',
                    'nsIScriptableWithNotXPCOM':'237d01a3-771e-4c6e-adf9-c97f9aab2950'},
                'mozilla-upstream-typelib')
        reader.verify()
        require(identity(typelib.lstat())==binding and typelib.resolve()==resolved,
                'mozilla-upstream-typelib')
    result['xpcomTypelib'] = {key:record[key] for key in ('bytes','sha256')}
    result['xpcomTypelib']['interfacesVerified']=True
    return result


def scope_receipt(path, expectations):
    result = {'status':'unavailable','completeLog':False,'reason':None,
              'bytes':None,'sha256':None,'counts':None,'tests':[],
              'expectationsSatisfied':False}
    try:
        selected = _inputs(list(expectations),'positive',None,None)
        require(all(value in ('PASS','FAIL','SKIP') for value in expectations.values()),
                'mozilla-upstream-expectations')
        raw = _read(path)
        result.update(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest())
        counts,_,_ = _parse(raw,selected,None,None)
        ends = {row['test']:row for row in _records(raw) if row['action']=='test_end'}
        tests=[]
        for name in sorted(selected):
            row=ends[name]; expected=expectations[name];status=row['status']
            require((row.get('expected')==expected) if 'expected' in row else status==expected,
                    'mozilla-upstream-expectation-binding')
            tests.append({'id':name,'expected':expected,'actual':status,
                          'satisfied':status==expected})
        passed=sum(row['actual']=='PASS' for row in tests)
        skipped=sum(row['actual']=='SKIP' for row in tests)
        expected_fail=sum(row['expected']==row['actual']=='FAIL' for row in tests)
        failed=sum(row['actual']!=row['expected'] for row in tests)
        clean=not any(counts[key] for key in ('crashes','timeouts','otherFailures','errorLogs',
                       'nonzeroProcessExits','unexpectedSubtests','unexpectedTestEnds'))
        satisfied=clean and failed==0 and passed+skipped+expected_fail==len(tests)
        result.update(status='passed' if satisfied else 'failed',completeLog=True,
                      expectationsSatisfied=satisfied,tests=tests,
                      counts={'selected':len(tests),'executed':len(tests)-skipped,
                              'passed':passed,'expectedFailures':expected_fail,
                              'unexpectedResults':failed,'skipped':skipped,
                              'crashes':counts['crashes'],'timeouts':counts['timeouts'],
                              'errorLogs':counts['errorLogs'],'unsupported':0,
                              'unattempted':0,'rawLifecycle':counts})
    except _Invalid as error:
        result['reason']=error.reason
    except (OSError,Failure,ValueError,KeyError,TypeError):
        result['reason']='mozilla-upstream-receipt-unavailable'
    return result


def run_scope(source, task, parent_runner, env, deadline, result):
    case_task=task/'mozilla-upstream-xpcom'
    private=case_task/'native-private';raw=private/'harness.raw.jsonl'
    value={'process':process_evidence(None),'runnerFailure':None,'monitorFailure':None,
           'monitorFailureObserved':None,'monitorDiagnostics':[],'stages':[],
           'harness':{'status':'unavailable','completeLog':False,'counts':None,'tests':[],
                      'reason':'mozilla-upstream-not-executed','expectationsSatisfied':False},
           'caughtCode':None,'caseError':None,'caseFailurePhase':None,
           'evidenceWriteFailure':None,'evidenceCollectionFailure':None,
           'expectationsSatisfied':False,'inputBindingStable':False}
    result['case']=value;result['monitorFailure']=None
    case_runner=None;created=False;phase='setup';private_errors={}
    def retain(label,error):
        private_errors[label]={'type':type(error).__name__,'message':str(error)[:4096]}
    try:
        case_task.mkdir(mode=0o700);created=True
        (case_task/'tmp').mkdir(mode=0o700);private.mkdir(mode=0o700)
        (case_task/'started-monotonic').write_text(str(parent_runner.monotonic_started))
        case_runner=Runner(case_task,parent_runner.started,parent_runner.limits)
        phase='source-binding'
        _,checksum=scope_plan(source)
        require(checksum==result['planSHA256'],'mozilla-upstream-source-plan')
        config=json.loads((task/'effective-config.private.log').read_text())['substs']
        binary=task/'obj/dist'/config['MOZ_MACBUNDLE_NAME']/'Contents/MacOS/xpcshell'
        expected={row['id']:row['expected'] for row in result['inventory']['scopeTests']}
        phase='environment'
        case_env=child_environment(env);case_env['TMPDIR']=str(case_task/'tmp')
        command=[sys.executable,'mach','xpcshell-test','--verbose','--sequential','--threads','1',
                 '--keep-going','--no-logfiles','--xpcshell',str(binary),
                 '--log-raw',str(raw),SCOPE]
        phase='launch'
        try:
            case_runner.run('mozilla-xpcshell',command,source,case_env,parent_runner.limits['jobMinutes'],
                            phase_deadline_minutes=deadline)
        except BaseException as error:
            retain('launch',error);value['caughtCode']=error_code(error)
        phase='harness-receipt'
        value['harness']=scope_receipt(raw,expected)
        phase='post-source-binding'
        value['inputBindingStable']=scope_plan(source)[1]==checksum
    except BaseException as error:
        retain(phase,error);value.update(caseError=error_code(error),caseFailurePhase=phase)
    finally:
        private_evidence=None
        try:
            if case_runner is not None:
                private_evidence={'stages':case_runner.records,'runnerFailure':case_runner.failure,
                    'monitorFailure':case_runner.monitor_failure,
                    'monitorDiagnostics':case_runner.monitor_diagnostics[-16:]}
                value['monitorFailureObserved']=case_runner.monitor_failure is not None
                if value['monitorFailureObserved']:
                    result['monitorFailure']={'stage':'mozilla-xpcshell','details':'unavailable'}
                value['runnerFailure']=case_runner.failure
                value['process']=process_evidence(case_runner)
                value['stages']=[stage_evidence(row) for row in case_runner.records]
                value['monitorFailure']=monitor_evidence(case_runner.monitor_failure)
                value['monitorDiagnostics']=[monitor_evidence(row) for row in case_runner.monitor_diagnostics[-16:]]
            p=value['process']
            value['expectationsSatisfied']=(value['harness']['expectationsSatisfied'] is True and
                value['inputBindingStable'] is True and value['monitorFailureObserved'] is False and
                all(value[key] is None for key in ('caseError','caughtCode','runnerFailure','monitorFailure',
                    'evidenceWriteFailure','evidenceCollectionFailure')) and p['available'] is True and
                type(p['exitCode']) is int and p['exitCode']==0 and
                all(p[key] is True for key in ('exactReaped','groupGone','cleanupClean','monitorFailureAbsent')) and
                p.get('reason') is None)
        except BaseException as error:
            retain('evidence-collection',error)
            value.update(evidenceCollectionFailure=error_code(error),expectationsSatisfied=False)
        if created:
            try:
                write_json(case_task/'receipt.json',{**value,'privateEvidence':private_evidence,
                                                    'privateErrors':private_errors})
            except BaseException as error:
                value.update(evidenceWriteFailure=error_code(error),expectationsSatisfied=False)
        else:
            value.update(evidenceWriteFailure='case-directory-unavailable',expectationsSatisfied=False)
    result['status']='passed' if value['expectationsSatisfied'] else 'failed'


def file_receipt(value, maximum):
    return (type(value) is dict and type(value.get('bytes')) is int and
            0 < value['bytes'] <= maximum and sha256(value.get('sha256')))


def inventory_counts(value):
    keys=('registeredRecords','activeRecords','manifestEnabledRecords','manifestDisabledRecords',
          'excludedByDefaultFilters','uniqueIds','uniqueEnabledIds','uniqueFiles','expectedFailureRecords')
    return (type(value) is dict and all(type(value.get(key)) is int and value[key]>=0 for key in keys) and
            value['registeredRecords']>=value['activeRecords']>0 and
            value['manifestEnabledRecords']+value['manifestDisabledRecords']==value['activeRecords'] and
            value['excludedByDefaultFilters']==value['registeredRecords']-value['activeRecords'] and
            0<value['uniqueIds']<=value['activeRecords'] and
            value['uniqueEnabledIds']<=value['manifestEnabledRecords'] and
            0<value['uniqueFiles']<=value['activeRecords'] and
            value['expectedFailureRecords']<=value['manifestEnabledRecords'] and
            file_receipt(value.get('dump'),16*1024**2))


def proof_complete(value, rows):
    from mozilla_target_build import stage_complete
    try:
        plan,checksum=plan_definition()
        expected_ids={SCOPE+'/'+name for name in plan['tests']}
        inventory=value['inventory'];scope=inventory['scope'];whole=inventory['global']
        case=value['case'];p=case['process'];harness=case['harness'];counts=harness['counts']
        selected=[row for row in rows if row.get('stage') in EXTRA_STAGES]
        inventory_raw=(json.dumps(inventory,sort_keys=True,indent=2)+'\n').encode()
        require(file_receipt(value.get('inventoryReceipt'),1024**2) and
                value['inventoryReceipt']=={'bytes':len(inventory_raw),
                    'sha256':hashlib.sha256(inventory_raw).hexdigest()},'mozilla-upstream-inventory-proof')
        tests=inventory['scopeTests']
        require(type(tests) is list and len(tests)==45 and all(type(row) is dict and
                set(row)=={'id','expected'} and row['id'] in expected_ids and
                row['expected'] in ('PASS','FAIL','SKIP') for row in tests) and
                {row['id'] for row in tests}==expected_ids,'mozilla-upstream-scope-selection')
        expectations={row['id']:row['expected'] for row in tests}
        enabled=sum(status!='SKIP' for status in expectations.values())
        skipped=45-enabled;known_fail=sum(status=='FAIL' for status in expectations.values())
        require(type(harness['tests']) is list and len(harness['tests'])==45 and
                all(type(row) is dict and set(row)=={'id','expected','actual','satisfied'} and
                    row['id'] in expectations and row['expected']==expectations[row['id']] and
                    row['actual']==row['expected'] and row['satisfied'] is True for row in harness['tests']) and
                {row['id'] for row in harness['tests']}==expected_ids,'mozilla-upstream-test-binding')
        expected_counts={'selected':45,'executed':enabled,'passed':enabled-known_fail,
                         'expectedFailures':known_fail,'unexpectedResults':0,'skipped':skipped,
                         'crashes':0,'timeouts':0,'errorLogs':0,'unsupported':0,'unattempted':0}
        require(all(type(counts.get(key)) is int and counts[key]==number
                    for key,number in expected_counts.items()),'mozilla-upstream-counts')
        lifecycle=counts['rawLifecycle']
        require(type(lifecycle) is dict and all(type(number) is int and number>=0
                for key,number in lifecycle.items() if key not in ('testEndStatuses','subtestStatuses')) and
                all(type(lifecycle.get(key)) is dict and set(lifecycle[key])==set(statuses) and
                    all(type(number) is int and number>=0 for number in lifecycle[key].values())
                    for key,statuses in (('testEndStatuses',END_STATUSES),('subtestStatuses',SUB_STATUSES))),
                'mozilla-upstream-counts')
        require(all(type(lifecycle.get(key)) is int and lifecycle[key]==number for key,number in
                    {'suiteStarts':1,'suiteEnds':1,'listedTests':45,'expectedTests':45,
                     'testStarts':45,'testEnds':45,'unexpectedSubtests':0,'unexpectedTestEnds':0,
                     'unexpectedFailSubtests':0,'unexpectedFailTestEnds':0,
                     'crashes':0,'timeouts':0,'otherFailures':0,'errorLogs':0,'nonzeroProcessExits':0}.items()) and
                lifecycle['testEndStatuses']=={status:{'PASS':enabled-known_fail,'FAIL':known_fail,
                    'SKIP':skipped}.get(status,0) for status in END_STATUSES} and
                sum(lifecycle['subtestStatuses'].values())==lifecycle['subtests'] and
                lifecycle['skips']==skipped+lifecycle['subtestStatuses']['SKIP'] and
                all(number==0 for status,number in lifecycle['subtestStatuses'].items()
                    if status not in ('PASS','FAIL','SKIP')),
                'mozilla-upstream-counts')
        binaries=value['assets']['binaries'];typelib=value['assets']['xpcomTypelib']
        return (value.get('contract')==CONTRACT and value.get('scope')==SCOPE and
            value.get('status')=='passed' and value.get('planSHA256')==checksum and
            value.get('fullUpstreamSuiteExecuted') is False and
            value.get('fullHostQualification') is False and value.get('nativeAuthentication')=='NOT_TESTED' and
            [row.get('stage') for row in selected]==list(EXTRA_STAGES) and all(stage_complete(row) for row in selected) and
            inventory.get('status')=='observed' and inventory.get('contract')==CONTRACT and
            inventory.get('planSHA256')==checksum and
            inventory.get('temporaryPrefsOwnership')=='stock-obj-temp-not-removed' and
            type(inventory.get('serverProcessesStarted')) is int and inventory['serverProcessesStarted']==0 and
            type(inventory.get('testsExecuted')) is int and inventory['testsExecuted']==0 and
            inventory_counts(whole) and inventory_counts(scope) and
            whole['registeredRecords']==scope['registeredRecords'] and
            whole['activeRecords']>=scope['activeRecords']==45 and
            whole['manifestEnabledRecords']>=scope['manifestEnabledRecords']==enabled>0 and
            scope['manifestDisabledRecords']==skipped and scope['expectedFailureRecords']==known_fail and
            scope['uniqueIds']==scope['uniqueFiles']==45 and scope['uniqueEnabledIds']==enabled and
            case.get('expectationsSatisfied') is True and case.get('monitorFailureObserved') is False and
            all(case.get(key) is None for key in ('caseError','caughtCode','evidenceWriteFailure','evidenceCollectionFailure')) and
            value.get('monitorFailure') is None and
            case.get('inputBindingStable') is True and
            case.get('runnerFailure') is None and case.get('monitorFailure') is None and
            harness.get('status')=='passed' and harness.get('completeLog') is True and
            harness.get('reason') is None and harness.get('expectationsSatisfied') is True and
            file_receipt(harness,16*1024**2) and
            p.get('available') is True and type(p.get('exitCode')) is int and p['exitCode']==0 and
            all(p.get(key) is True for key in ('exactReaped','groupGone','cleanupClean','monitorFailureAbsent')) and
            p.get('reason') is None and set(binaries)==set(HELPERS)|{'plugin-container'} and
            all(file_receipt(row,1024**3) and type(row.get('filetype')) is int and row['filetype']==2 and
                type(row.get('uuid')) is str and re.fullmatch(r'[0-9a-f]{32}',row['uuid']) is not None and
                sha256(row.get('sectionsSHA256')) and type(row.get('sections')) is int and row['sections']>0
                for row in binaries.values()) and file_receipt(typelib,1024**2) and
            typelib.get('interfacesVerified') is True)
    except (Failure,OSError,ValueError,KeyError,TypeError,AttributeError):
        return False


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--inventory',action='store_true',required=True)
    parser.add_argument('--source',type=Path,required=True);parser.add_argument('--task',type=Path,required=True)
    parser.add_argument('--binary',type=Path,required=True);args=parser.parse_args()
    inventory_driver(args.source,args.task,args.binary)
