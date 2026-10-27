# SPDX-License-Identifier: MPL-2.0
"""Inventory the real registered suite and run the complete upstream XPCOM manifest."""
import argparse
import ast
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
HELPER_INSTALL_STAGE = 'mozilla-upstream-helpers-install'
INVENTORY_STAGE = 'mozilla-upstream-inventory'
CHILD_STAGE = 'mozilla-upstream-child-build'
CHILD_LIBS_STAGE = 'mozilla-upstream-child-libs'
RESOURCE_STAGE = 'mozilla-upstream-resources-build'
PREPARE_STAGES = (PREPARE_STAGE, HELPER_INSTALL_STAGE, INVENTORY_STAGE)
EXTRA_STAGES = (*PREPARE_STAGES, CHILD_STAGE, CHILD_LIBS_STAGE, RESOURCE_STAGE)


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


BOOTSTRAP_RECEIPT = {'bytes': 23749,
    'sha256': 'ca18450abb98a15bbf84d8a815616a41e0dda36db6f7091866422ce382d1ffaf'}
JOURNAL_CONTRACT = 'upstream-inventory-diagnostic-v1'
JOURNAL_FILE = 'mozilla-upstream-inventory-diagnostic.json'
INVENTORY_PHASES = ('entry', 'imports', 'category-source', 'category-register', 'stock-module',
    'source-plan', 'build-environment', 'install-tests', 'registered-manifest',
    'global-options', 'global-invoke', 'global-logger-cleanup', 'global-read',
    'scope-options', 'scope-invoke', 'scope-logger-cleanup', 'scope-read',
    'restore-stock-class', 'inventory-write', 'complete')
DIAGNOSTIC_ERRORS = ('Failure', 'MachError', 'ModuleNotFoundError', 'ImportError', 'SyntaxError',
    'OSError', 'FileNotFoundError', 'PermissionError', 'ValueError', 'TypeError', 'KeyError',
    'AttributeError', 'RuntimeError', 'AssertionError', 'SystemExit', 'KeyboardInterrupt',
    'JSONDecodeError', 'Unknown', 'unavailable')
DIAGNOSTIC_CODES = ('mozilla-upstream-category-source', 'mozilla-upstream-category-binding',
    'mozilla-upstream-source-plan', 'mozilla-upstream-source-input', 'mozilla-upstream-build-binding',
    'mozilla-upstream-registered-manifest', 'mozilla-upstream-inventory-existing-output',
    'mozilla-upstream-inventory-started-test', 'mozilla-upstream-inventory-exit',
    'mozilla-upstream-inventory-lifecycle', 'mozilla-upstream-inventory-output',
    'mozilla-upstream-inventory-shape', 'mozilla-upstream-scope-selection')


def diagnostic_error(error):
    kind = type(error).__name__
    value = {'type': kind if kind in DIAGNOSTIC_ERRORS else 'unavailable', 'code': None,
             'exitKind': None, 'exitCode': None}
    if type(error) is Failure and len(error.args) == 1 and error.args[0] in DIAGNOSTIC_CODES:
        value['code'] = error.args[0]
    if isinstance(error, SystemExit):
        value['exitKind'] = ('none' if error.code is None else
                             'integer' if type(error.code) is int else 'other')
        if type(error.code) is int and -255 <= error.code <= 255:
            value['exitCode'] = error.code
    return value


class InventoryJournal:
    """A fixed vocabulary of observations, never exception text or private paths."""
    def __init__(self, task):
        self.path = task / JOURNAL_FILE
        self.value = {'contract': JOURNAL_CONTRACT, 'status': 'running', 'phases': [],
            'lastReached': None, 'errorPhase': None, 'error': None, 'cleanupError': None,
            'writeFailure': None, 'categorySource': None, 'categoryInitiallyPresent': None}

    def persist(self):
        try:
            write_json(self.path, self.value)
        except BaseException as error:
            # Diagnostic persistence cannot replace an execution/cleanup failure.
            self.value['writeFailure'] = diagnostic_error(error)['type']

    def phase(self, name):
        require(name in INVENTORY_PHASES, 'mozilla-upstream-inventory-shape')
        self.value['lastReached'] = name
        self.value['phases'].append(name)
        self.persist()

    def fail(self, error, cleanup=False):
        if cleanup:
            self.value['cleanupError'] = diagnostic_error(error)
        elif self.value['error'] is None:
            self.value.update(status='failed', errorPhase=self.value['lastReached'],
                              error=diagnostic_error(error))
        self.persist()


def bootstrap_testing_category(source, journal):
    # Read only the pinned literal. Importing mach_initialize would install meta-path hooks.
    from mach.registrar import Registrar
    journal.phase('category-source')
    with ExitStack() as stack:
        reader = _FixedRead(source, stack, time.monotonic()+30)
        raw, receipt = reader.read('build/mach_initialize.py', 1024**2)
        require(raw is not None and {key:receipt[key] for key in ('bytes','sha256')} ==
                BOOTSTRAP_RECEIPT, 'mozilla-upstream-category-source')
        tree = ast.parse(raw)
        definitions = [node.value for node in tree.body if isinstance(node, ast.Assign) and
            len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) and
            node.targets[0].id == 'CATEGORIES']
        require(len(definitions) == 1, 'mozilla-upstream-category-source')
        metadata = ast.literal_eval(definitions[0])['testing']
        require(metadata == {'short':'Testing','long':'Run tests.','priority':60},
                'mozilla-upstream-category-source')
        reader.verify()
    journal.value['categorySource'] = dict(BOOTSTRAP_RECEIPT)
    journal.phase('category-register')
    expected = (metadata['short'], metadata['long'], metadata['priority'])
    present = 'testing' in Registrar.categories
    journal.value['categoryInitiallyPresent'] = present
    if present:
        value = Registrar.categories['testing']
        require(type(value) is tuple and len(value) == 3 and
                type(value[0]) is str and type(value[1]) is str and type(value[2]) is int and
                value == expected, 'mozilla-upstream-category-binding')
    else:
        require(not Registrar.commands_by_category.get('testing'),
                'mozilla-upstream-category-binding')
        Registrar.register_category('testing', *expected)
    journal.persist()


def inventory_diagnostic(value):
    """Validate rather than publish arbitrary child-controlled JSON fields."""
    require(type(value) is dict and set(value) == {'contract','status','phases','lastReached',
        'errorPhase','error','cleanupError','writeFailure','categorySource','categoryInitiallyPresent'} and
        value['contract'] == JOURNAL_CONTRACT and value['status'] in ('running','failed','complete') and
        type(value['phases']) is list and 0 < len(value['phases']) <= len(INVENTORY_PHASES) and
        value['phases'][0] == 'entry' and all(type(x) is str and x in INVENTORY_PHASES
            for x in value['phases']) and len(set(value['phases'])) == len(value['phases']) and
        [INVENTORY_PHASES.index(x) for x in value['phases']] == sorted(INVENTORY_PHASES.index(x) for x in value['phases']) and
        value['lastReached'] == value['phases'][-1] and
        (value['errorPhase'] is None or value['errorPhase'] in value['phases']) and
        (value['writeFailure'] is None or value['writeFailure'] in DIAGNOSTIC_ERRORS) and
        (value['categorySource'] is None or value['categorySource'] == BOOTSTRAP_RECEIPT) and
        (value['categoryInitiallyPresent'] is None or type(value['categoryInitiallyPresent']) is bool),
        'mozilla-upstream-inventory-shape')
    for name in ('error','cleanupError'):
        error = value[name]
        if error is None: continue
        require(type(error) is dict and set(error) == {'type','code','exitKind','exitCode'} and
            error['type'] in DIAGNOSTIC_ERRORS and (error['code'] is None or error['code'] in DIAGNOSTIC_CODES) and
            error['exitKind'] in (None,'none','integer','other') and
            (error['exitCode'] is None or type(error['exitCode']) is int and -255 <= error['exitCode'] <= 255),
            'mozilla-upstream-inventory-shape')
    return value


def collect_inventory_diagnostic(task, result):
    output = {'status':'unavailable', 'journal':None, 'receipt':None, 'collectionError':None}
    result['inventoryDiagnostic'] = output
    try:
        with ExitStack() as stack:
            reader = _FixedRead(task, stack, time.monotonic()+30)
            raw, receipt = reader.read(JOURNAL_FILE, 16384)
            if raw is not None:
                output['journal'] = inventory_diagnostic(json.loads(raw))
                output['receipt'] = {key:receipt[key] for key in ('bytes','sha256')}
                output['status'] = 'observed'
            reader.verify()
    except BaseException as error:
        output.update(status='unavailable', journal=None, receipt=None,
                      collectionError=diagnostic_error(error)['type'])


def inventory_driver(source, task, binary):
    journal = InventoryJournal(task)
    journal.phase('entry')
    try:
        _inventory_driver(source, task, binary, journal)
    except BaseException as error:
        journal.fail(error)
        raise
    else:
        journal.value['status'] = 'complete'
        journal.phase('complete')


def _inventory_driver(source, task, binary, journal):
    """Use stock prefs/mozinfo/normalization, suppressing both server startups only for listing."""
    journal.phase('imports')
    import runxpcshelltests as stock
    from xpcshellcommandline import parser_desktop
    from mozbuild.base import MozbuildObject
    from mozbuild.controller.building import BuildDriver
    from mach.logging import LoggingManager
    from mozlog.structuredlog import StructuredLogger
    bootstrap_testing_category(source, journal)
    journal.phase('stock-module')
    spec = importlib.util.spec_from_file_location('floorp_stock_xpcshell_mach',
        source/'testing/xpcshell/mach_commands.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    XPCShellRunner = module.XPCShellRunner

    journal.phase('source-plan')
    plan, plan_hash = scope_plan(source)
    journal.phase('build-environment')
    build = MozbuildObject.from_environment(cwd=str(source))
    build.log_manager = LoggingManager()
    require(Path(build.topsrcdir).resolve() == source and
            Path(build.topobjdir).resolve() == task / 'obj', 'mozilla-upstream-build-binding')
    journal.phase('install-tests')
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
    journal.phase('registered-manifest')
    manifest = task / 'obj/_tests/xpcshell/xpcshell.toml'
    require(manifest.is_file(), 'mozilla-upstream-registered-manifest')
    try:
        stock.XPCShellTests = InventoryOnly
        for label, paths in (('global', []), ('scope', [SCOPE])):
            journal.phase(label+'-options')
            output = task / ('upstream-' + label + '.raw.json')
            require(not output.exists(), 'mozilla-upstream-inventory-existing-output')
            options = vars(parser_desktop().parse_args([
                '--xpcshell', str(binary),
                '--dump-tests', str(output), *paths]))
            options.update(log=StructuredLogger('floorp-upstream-inventory-'+label),
                           self_test=False, profileName='firefox', threadCount=1)
            runner = build._spawn(XPCShellRunner)
            code = 'not-exited'
            journal.phase(label+'-invoke')
            try:
                runner._run_xpcshell_harness(**options)
                raise Failure('mozilla-upstream-inventory-exit')
            except SystemExit as error:
                code = error.code
                if not (code is None or type(code) is int and code == 0):
                    journal.fail(error)
                    raise Failure('mozilla-upstream-inventory-exit') from error
            except BaseException as error:
                journal.fail(error)
                raise
            finally:
                active_error = sys.exc_info()[0] is not None
                journal.phase(label+'-logger-cleanup')
                try:
                    runner.log_manager.disable_unstructured()
                except BaseException as error:
                    journal.fail(error, cleanup=True)
                    if not active_error: raise
            capture = captures[-1]
            require(capture.suppressed == ['http2', 'http3'] and
                    type(capture.testCount) is int and capture.testCount == 0 and
                    capture.mozHttp2Server is None and capture.http3Server is None,
                    'mozilla-upstream-inventory-lifecycle')
            journal.phase(label+'-read')
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
    except BaseException as error:
        journal.fail(error)
        raise
    finally:
        journal.phase('restore-stock-class')
        stock.XPCShellTests = original
    journal.phase('inventory-write')
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
    runner.run(HELPER_INSTALL_STAGE, [sys.executable,'mach','build','-j3','-C','xpcom/tests',
               'misc'], source, env, runner.limits['jobMinutes'],
               phase_deadline_minutes=deadline)
    config = json.loads((task/'effective-config.private.log').read_text())['substs']
    bundle = config.get('MOZ_MACBUNDLE_NAME')
    require(type(bundle) is str and re.fullmatch(r'[A-Za-z0-9 _.-]+\.app',bundle) is not None and
            '..' not in bundle, 'mozilla-upstream-bundle')
    binary = task/'obj/dist'/bundle/'Contents/MacOS/xpcshell'
    result['inventoryDiagnostic'] = {'status':'unavailable','journal':None,'receipt':None,'collectionError':None}
    try:
        runner.run(INVENTORY_STAGE, [sys.executable,'mach','python',str(HERE/'mozilla_upstream.py'),
                   '--inventory','--source',str(source),'--task',str(task),'--binary',str(binary)],
                   source, child_environment(env), runner.limits['jobMinutes'],
                   phase_deadline_minutes=deadline)
    finally:
        collect_inventory_diagnostic(task, result)
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
    # Non-stage MacOSBundle assembly is a directory libs recipe. A bare
    # root ipc/app/libs dependency does not select CURRENT_TIER's recipe.
    runner.run(CHILD_LIBS_STAGE, [sys.executable,'mach','build','-j3','-C','ipc/app','libs'],
               source, env, runner.limits['jobMinutes'], phase_deadline_minutes=deadline)
    runner.run(RESOURCE_STAGE, [sys.executable,'mach','build','-j3','faster'],
               source, env, runner.limits['jobMinutes'], phase_deadline_minutes=deadline)


ASSET_PREDICATES = ('file', 'object-root', 'owner', 'executable', 'current-mtime')


def asset_precondition_evidence(value):
    """Project bounded guard observations, never paths, owners or wall times."""
    unknown = {'schema':1, 'status':'unavailable', 'checkedAssets':None, 'failure':None, 'inspection':None}
    labels = ('plugin-container', *HELPERS)
    if not (type(value) is dict and set(value) == set(unknown) and
            type(value['schema']) is int and value['schema'] == 1 and
            type(value['status']) is str and value['status'] in ('checking','passed','failed','unavailable') and
            type(value['checkedAssets']) is int and 0 <= value['checkedAssets'] <= len(labels)):
        return unknown
    observations = {}
    for key,status in (('failure','failed'), ('inspection','unavailable')):
        row = value[key]
        if row is not None:
            if not (type(row) is dict and set(row) == {'asset','predicate'} and
                    value['checkedAssets'] < len(labels) and type(row['asset']) is str and
                    row['asset'] == labels[value['checkedAssets']] and
                    type(row['predicate']) is str and row['predicate'] in ASSET_PREDICATES and
                    value['status'] == status):
                return unknown
            row = dict(row)
        observations[key] = row
    if value['status'] == 'failed' and observations['failure'] is None:
        return unknown
    if value['status'] == 'passed' and value['checkedAssets'] != len(labels):
        return unknown
    return {'schema':1, 'status':'unavailable' if value['status'] == 'checking' else value['status'],
            'checkedAssets':value['checkedAssets'], **observations}


def verify_scope_assets(task, runner, macho, *, diagnostic=None):
    root = (task/'obj').resolve()
    unit = root/'_tests/xpcshell'/SCOPE
    config = json.loads((task/'effective-config.private.log').read_text())['substs']
    app = root/'dist'/config['MOZ_MACBUNDLE_NAME']/'Contents/MacOS'
    name = config.get('MOZ_CHILD_PROCESS_NAME')
    require(name == 'plugin-container', 'mozilla-upstream-child-name')
    paths = {name: app/(name+'.app')/'Contents/MacOS'/name,
             **{helper:unit/helper for helper in HELPERS}}
    result = {'binaries':{},'xpcomTypelib':None}
    if type(diagnostic) is dict:
        diagnostic.clear()
        diagnostic.update(schema=1, status='checking', checkedAssets=0, failure=None, inspection=None)
    for label,path in paths.items():
        predicates = (lambda: path.is_file(), lambda: path.resolve().is_relative_to(root),
                      lambda: path.stat().st_uid == os.geteuid(),
                      lambda: os.access(path,os.X_OK), lambda: path.stat().st_mtime >= runner.started)
        for name,predicate in zip(ASSET_PREDICATES,predicates):
            try:
                accepted = predicate()
            except BaseException:
                if type(diagnostic) is dict:
                    diagnostic.update(status='unavailable', inspection={'asset':label,'predicate':name})
                raise
            if not accepted:
                if type(diagnostic) is dict:
                    diagnostic.update(status='failed', failure={'asset':label,'predicate':name})
                raise Failure('mozilla-upstream-current-native-asset')
        if type(diagnostic) is dict:
            diagnostic['checkedAssets'] += 1
        value = macho(path,2)
        require(value.get('uuid') is not None, 'mozilla-upstream-native-identity')
        result['binaries'][label] = value
    if type(diagnostic) is dict:
        diagnostic['status'] = 'passed'
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


def execution_ledger(value):
    """Explain every planned case without granting a qualification or inventing skips."""
    ledger = {'schema': 1, 'contract': 'upstream-xpcom-execution-ledger-v1',
        'status': 'unverified', 'reason': 'inventory-unverified',
        'scope': SCOPE, 'plannedSource': None, 'planSHA256': None,
        'inventoryVerified': False, 'terminalLogObserved': False,
        'processEvidenceComplete': False, 'productQualification': False,
        'counts': {'planned': None, 'eligible': None, 'manifestDisabled': None,
            'recorded': None, 'executed': None, 'passed': None, 'expectedFailures': None,
            'unexpectedResults': None, 'skipped': None, 'notExecuted': None,
            'unverified': None}, 'tests': []}
    try:
        plan, checksum = plan_definition()
        ids = {SCOPE + '/' + name for name in plan['tests']}
        ledger.update(plannedSource=plan['source'], planSHA256=checksum)
        ledger['counts'].update(planned=len(ids), unverified=len(ids))
        ledger['tests'] = [{'id': name, 'expected': None, 'actual': None,
            'execution': 'UNVERIFIED', 'reason': 'inventory-unverified'} for name in sorted(ids)]
        require(type(value) is dict and value.get('contract') == CONTRACT and
                value.get('scope') == SCOPE and value.get('planSHA256') == checksum,
                'mozilla-upstream-inventory-proof')
        inventory = value['inventory']
        raw = (json.dumps(inventory, sort_keys=True, indent=2) + '\n').encode()
        require(file_receipt(value.get('inventoryReceipt'), 1024**2) and
                value['inventoryReceipt'] == {'bytes': len(raw),
                    'sha256': hashlib.sha256(raw).hexdigest()} and
                inventory.get('status') == 'observed' and inventory.get('contract') == CONTRACT and
                inventory.get('planSHA256') == checksum and
                inventory.get('temporaryPrefsOwnership') == 'stock-obj-temp-not-removed' and
                all(type(inventory.get(key)) is int and inventory[key] == 0
                    for key in ('serverProcessesStarted', 'testsExecuted')),
                'mozilla-upstream-inventory-proof')
        tests = inventory['scopeTests']
        require(type(tests) is list and len(tests) == len(ids) and
                all(type(row) is dict and set(row) == {'id', 'expected'} and
                    type(row['id']) is str and row['id'] in ids and
                    type(row['expected']) is str and row['expected'] in ('PASS', 'FAIL', 'SKIP')
                    for row in tests) and {row['id'] for row in tests} == ids,
                'mozilla-upstream-scope-selection')
        expected = {row['id']: row['expected'] for row in tests}
        enabled = sum(status != 'SKIP' for status in expected.values())
        disabled = len(ids) - enabled
        scope, whole = inventory['scope'], inventory['global']
        require(inventory_counts(scope) and inventory_counts(whole) and
                whole['registeredRecords'] == scope['registeredRecords'] and
                whole['activeRecords'] >= scope['activeRecords'] == len(ids) and
                whole['manifestEnabledRecords'] >= scope['manifestEnabledRecords'] == enabled > 0 and
                scope['manifestDisabledRecords'] == disabled and
                scope['expectedFailureRecords'] == sum(status == 'FAIL' for status in expected.values()) and
                scope['uniqueIds'] == scope['uniqueFiles'] == len(ids) and
                scope['uniqueEnabledIds'] == enabled, 'mozilla-upstream-inventory-proof')
        ledger['inventoryVerified'] = True
        ledger['counts'].update(eligible=enabled, manifestDisabled=disabled, unverified=enabled)
        ledger['tests'] = [{'id': name, 'expected': expected[name], 'actual': None,
            'execution': 'MANIFEST_DISABLED' if expected[name] == 'SKIP' else 'UNVERIFIED',
            'reason': 'upstream-manifest' if expected[name] == 'SKIP' else 'harness-unverified'}
            for name in sorted(ids)]
        if 'case' in value and value['case'] is None:
            ledger.update(status='not-executed', reason='before-harness')
            ledger['counts'].update(recorded=0, executed=0, passed=0, expectedFailures=0,
                unexpectedResults=0, skipped=0, notExecuted=enabled, unverified=0)
            for row in ledger['tests']:
                if row['expected'] != 'SKIP':
                    row.update(execution='NOT_EXECUTED', reason='before-harness')
            return ledger
        ledger['reason'] = 'harness-unverified'
        case = value['case']
        harness = case['harness']
        observed, counts = harness['tests'], harness['counts']
        require(harness.get('completeLog') is True and harness.get('status') in ('passed', 'failed') and
                file_receipt(harness, 16*1024**2) and type(observed) is list and len(observed) == len(ids) and
                all(type(row) is dict and set(row) == {'id', 'expected', 'actual', 'satisfied'} and
                    type(row['id']) is str and row['id'] in expected and
                    row['expected'] == expected[row['id']] and type(row['actual']) is str and
                    row['actual'] in END_STATUSES and type(row['satisfied']) is bool and
                    row['satisfied'] is (row['actual'] == row['expected']) for row in observed) and
                {row['id'] for row in observed} == ids, 'mozilla-upstream-test-binding')
        totals = {'selected': len(ids), 'executed': sum(row['actual'] != 'SKIP' for row in observed),
            'passed': sum(row['actual'] == 'PASS' for row in observed),
            'expectedFailures': sum(row['expected'] == row['actual'] == 'FAIL' for row in observed),
            'unexpectedResults': sum(not row['satisfied'] for row in observed),
            'skipped': sum(row['actual'] == 'SKIP' for row in observed), 'unsupported': 0, 'unattempted': 0}
        require(type(counts) is dict and all(type(counts.get(key)) is int and counts[key] == count
                for key, count in totals.items()) and all(type(counts.get(key)) is int and counts[key] >= 0
                for key in ('crashes', 'timeouts', 'errorLogs')), 'mozilla-upstream-counts')
        lifecycle = counts['rawLifecycle']
        require(type(lifecycle) is dict and all(type(lifecycle.get(key)) is int and lifecycle[key] == count
                for key, count in {'suiteStarts': 1, 'suiteEnds': 1, 'listedTests': len(ids),
                    'expectedTests': len(ids), 'testStarts': len(ids), 'testEnds': len(ids)}.items()) and
                type(lifecycle.get('testEndStatuses')) is dict and
                set(lifecycle['testEndStatuses']) == set(END_STATUSES) and
                all(type(lifecycle['testEndStatuses'][status]) is int and
                    lifecycle['testEndStatuses'][status] == sum(row['actual'] == status for row in observed)
                    for status in END_STATUSES), 'mozilla-upstream-counts')
        ledger.update(status='recorded', reason='terminal-log', terminalLogObserved=True)
        process = case.get('process', {})
        ledger['processEvidenceComplete'] = (type(process) is dict and process.get('available') is True and
            type(process.get('exitCode')) is int and all(process.get(key) is True
                for key in ('exactReaped', 'groupGone', 'cleanupClean', 'monitorFailureAbsent')) and
            case.get('monitorFailureObserved') is False and case.get('monitorFailure') is None)
        ledger['counts'].update({key: count for key, count in totals.items()
                                 if key in ledger['counts']})
        ledger['counts'].update(recorded=len(observed), notExecuted=0, unverified=0)
        ledger['tests'] = [{'id': row['id'], 'expected': row['expected'], 'actual': row['actual'],
            'execution': 'RECORDED', 'reason': 'terminal-log'}
            for row in sorted(observed, key=lambda row: row['id'])]
    except (Failure, OSError, ValueError, KeyError, TypeError, AttributeError, RecursionError):
        pass
    return ledger


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
