# SPDX-License-Identifier: MPL-2.0
"""Safe unittest failure identities; no arbitrary exception text or raw logs."""
import ast
import builtins
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat

from support import require

HERE = Path(__file__).resolve().parent

HELD_CHILD_CASE = ('test_actual_held_child_rejects_until_exit_and_preserves_unrelated_candidates '
                   '(test_census_convergence.ConvergenceGuards.'
                   'test_actual_held_child_rejects_until_exit_and_preserves_unrelated_candidates)')


def held_resource_public(value):
    from census_convergence import safe_query,safe_reason,read_diagnostic
    require(isinstance(value,dict),'unittest-held-resource-shape')
    result={'status':value.get('status') if value.get('status') in
            ('collecting','measured','unavailable') else 'unavailable',
            'sampleComplete':value.get('sampleComplete') is True,
            'projectionUnavailable':value.get('projectionUnavailable') is True}
    for key in ('rssMiB','cpuSeconds'):
        number=value.get(key)
        require(number is None or type(number) in (int,float) and math.isfinite(number) and
                0<=number<=10**12,'unittest-held-resource-counter')
        result[key]=number
    operations=('self-bsd','candidate-bsd-before','candidate-task','candidate-recheck-delay',
                'candidate-bsd-recheck','candidate-task-recheck','candidate-bsd-confirm-gone','candidate-bsd-after')
    result['operation']=value.get('operation') if value.get('operation') in operations else None
    result['reason']=safe_reason(value.get('reason'))
    code=value.get('failureCode')
    result['failureCode']=code if code in ('census-wait-phase-deadline','census-wait-self-unavailable',
        'census-wait-identity-unavailable','census-wait-resource-unavailable','census-wait-identity-changed',
        'resource-query-exception') else None
    index=value.get('candidateIndex')
    if type(index) is int and 0<=index<=1000000:result['candidateIndex']=index
    for key in ('taskQuery','bsdQuery','retryBSDQuery','goneBSDQuery'):
        if key in value:result[key]=safe_query(value[key])
    for key in ('beforeBSDObservation','retryBSDObservation','goneBSDObservation'):
        observation=value.get(key)
        if not isinstance(observation,dict):continue
        status=observation.get('status')
        result[key]={'status':status if status in ('unbound','unavailable','initializing','runnable',
                          'sleeping','stopped','zombie') else 'unavailable',
                     'inExit':observation.get('inExit') if type(observation.get('inExit')) is bool else None}
    traces=value.get('readDiagnostics',[])
    require(isinstance(traces,list) and len(traces)<=2,'unittest-held-resource-trace')
    result['readDiagnostics']=[]
    for entry in traces:
        require(isinstance(entry,dict),'unittest-held-resource-trace-shape')
        trace=read_diagnostic({**entry,'bsdQuery':entry.get('fallbackBSDQuery')},(0,0,0,0,0))
        trace['sameCandidateBirth']=entry.get('sameCandidateBirth') if type(entry.get('sameCandidateBirth')) is bool else None
        result['readDiagnostics'].append(trace)
    pause=value.get('resourceRetryPause')
    if pause is not None:
        require(isinstance(pause,dict) and type(pause.get('maximumPauses')) is int and pause['maximumPauses']==1 and
                type(pause.get('pauses')) is int and pause['pauses'] in (0,1) and
                type(pause.get('requestedSeconds')) in (int,float) and pause['requestedSeconds'] in (0,.25) and
                type(pause.get('elapsedSeconds')) in (int,float) and math.isfinite(pause['elapsedSeconds']) and
                0<=pause['elapsedSeconds']<=3600 and pause.get('status') in
                ('unused','budget-check','budget-rejected','waiting','interrupted','completed'),
                'unittest-held-resource-pause')
        result['resourceRetryPause']={key:pause[key] for key in
            ('maximumPauses','pauses','requestedSeconds','elapsedSeconds','status')}
    return result


def held_child_control_passed(value):
    """A test-control result is never checkpoint eligibility."""
    samples=value['sampleCounts']
    common=(len(samples)>=2 and samples[0][5] is True and
            type(value['baselineCandidateIdentities']) is int and value['baselineCandidateIdentities']>0 and
            all(type(row[1]) is int and type(row[2]) is int for row in samples) and
            all(samples[index][0]<=samples[index+1][0] for index in range(len(samples)-1)) and
            all(row[5] is False for row in samples[1:]) and
            all(row[3] is True and row[4] is True for row in samples) and
            all(value[key] is True for key in ('childBindingVerified','childReleased',
                'childExitVerifiedBeforeBoundary','childReaped','childGroupGone',
                'childCleanupComplete','monitorFailureAbsent')) and value['childExitCode']==0 and
            all(value[key] is False for key in ('baselineReset','candidatesExcluded',
                                               'signalsSent','checkpointEligibilityEstablished')) and
            value['lastResource'].get('sampleComplete') is True and
            value['lastResource'].get('status')=='measured' and
            value['lastResource'].get('projectionUnavailable') is False and
            all(type(value['lastResource'].get(key)) in (int,float) and
                math.isfinite(value['lastResource'][key]) and value['lastResource'][key]>=0
                for key in ('rssMiB','cpuSeconds')))
    if not common:return False
    if value['outcome']=='natural-zero':
        return (value['receiptStatus']=='passed' and value['finalBoundaryZero'] is True and
                value['phaseFailure'] is None and samples[-1][1]==0 and all(row[1]>0 for row in samples[:-1]))
    if value['outcome']=='safely-rejected-deadline':
        return (value['receiptStatus']=='failed' and value['finalBoundaryZero'] is False and
                value['phaseFailure']=='census-wait-phase-deadline' and all(row[1]>0 for row in samples))
    return False


def held_child_public(value):
    """Fixed diagnostic fields only; private identities and free text are discarded."""
    require(isinstance(value,dict) and type(value.get('schema')) is int and value['schema']==1,
            'unittest-held-child-schema')
    result={'schema':1}
    for key,allowed in (
        ('status',('passed','failed')),('outcome',('natural-zero','safely-rejected-deadline','failed')),
        ('receiptStatus',('passed','failed','waiting','unavailable')),
        ('phaseFailure',(None,'census-wait-phase-deadline'))):
        require(value.get(key) in allowed,'unittest-held-child-enum')
        result[key]=value[key]
    for key in ('childBindingVerified','childReleased','childExitVerifiedBeforeBoundary',
                'childReaped','childGroupGone','childCleanupComplete','monitorFailureAbsent',
                'baselineReset','candidatesExcluded','signalsSent','finalBoundaryZero',
                'checkpointEligibilityEstablished'):
        entry=value.get(key)
        require(entry is None or type(entry) is bool,'unittest-held-child-boolean')
        result[key]=entry
    code=value.get('childExitCode')
    require(code is None or type(code) is int and -255<=code<=255,'unittest-held-child-exit')
    result['childExitCode']=code
    baseline=value.get('baselineCandidateIdentities')
    require(baseline is None or type(baseline) is int and 0<=baseline<=1000000,
            'unittest-held-child-baseline')
    result['baselineCandidateIdentities']=baseline
    samples=value.get('sampleCounts')
    require(isinstance(samples,list) and len(samples)<=128,'unittest-held-child-samples')
    result['sampleCounts']=[]
    for row in samples:
        require(isinstance(row,list) and len(row)==6,'unittest-held-child-sample')
        require(type(row[0]) in (int,float) and math.isfinite(row[0]) and 0<=row[0]<=3600 and
                all(n is None or type(n) is int and 0<=n<=1000000 for n in row[1:3]) and
                all(type(n) is bool for n in row[3:5]) and (row[5] is None or type(row[5]) is bool),
                'unittest-held-child-sample-fields')
        result['sampleCounts'].append(list(row))
    result['sampleCountColumns']=['elapsedSeconds','newCandidateIdentities','removedCandidateIdentities',
                                  'resourceComplete','resourceTotalsAvailable','heldChildPresent']
    result['lastResource']=held_resource_public(value.get('lastResource'))
    require(result['checkpointEligibilityEstablished'] is False,'unittest-held-child-no-qualification')
    require(result['status']!='passed' or held_child_control_passed(result),'unittest-held-child-control')
    return result


def source_identities(recording=False):
    cases, phases, lines = set(), set(), {}
    paths = list(HERE.glob('*.py'))
    if recording:
        root = HERE.parents[1] / 'app-shim'
        paths.extend(root / name for name in ('test_shim_failure_diagnostics.py',
            'shim_failure_diagnostics.py', 'test-runtime.py', 'test-context-menu.py', 'shim_lifecycle.py'))
    for path in paths:
        source = path.read_text()
        lines[path.resolve()] = len(source.splitlines())
        if not path.name.startswith('test_'):
            continue
        tree = ast.parse(source)
        classes = {node.name: node for node in tree.body if isinstance(node, ast.ClassDef)}
        def methods(name, seen=None):
            seen = set() if seen is None else seen
            if name in seen or name not in classes:
                return set()
            seen.add(name)
            node = classes[name]
            result = {item.name for item in node.body if isinstance(item, ast.FunctionDef)
                      and item.name.startswith('test_')}
            for base in node.bases:
                if isinstance(base, ast.Name):
                    result.update(methods(base.id, seen))
            return result
        for name in classes:
            identity = path.stem+'.'+name
            for method in methods(name):
                cases.add((method, identity+'.'+method))
            phases.update((phase, identity) for phase in ('setUpClass','tearDownClass'))
        phases.update((phase, path.stem) for phase in ('setUpModule','tearDownModule'))
        cases.add((path.stem, 'unittest.loader._FailedTest.'+path.stem))
    return cases | phases, lines


def known_failure_codes():
    codes = set()
    for path in HERE.glob('*.py'):
        if path.name.startswith('test_'):
            continue
        for node in ast.walk(ast.parse(path.read_text())):
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Name):
                continue
            index = 1 if node.func.id == 'require' else 0 if node.func.id == 'Failure' else None
            if index is not None and len(node.args) > index and isinstance(node.args[index], ast.Constant):
                value = node.args[index].value
                if isinstance(value, str) and re.fullmatch('[a-z][a-z0-9-]{0,95}', value):
                    codes.add(value)
    return codes


def receipt(path, *, recording=False):
    result = {'complete': False, 'rawContentPublished': False, 'exceptionTextPublished': False,
              'total': None, 'failures': None, 'errors': None, 'skipped': None, 'status': 'unknown'}
    try:
        identities, source_lines = source_identities(recording)
        exception_types = {name for name, value in vars(builtins).items()
                           if isinstance(value, type) and issubclass(value, BaseException)}
        exception_types.update(('support.Failure', 'qa_core.Failure', 'subprocess.CalledProcessError',
                                'subprocess.TimeoutExpired', 'json.decoder.JSONDecodeError',
                                'ctypes.ArgumentError', 'unittest.case.SkipTest'))
        codes = known_failure_codes()
        checksum = hashlib.sha256()
        failures, counts, outcomes, current, diagnostics = [], [], [], None, []
        held_diagnostics=[];held_seen=False
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(descriptor, 'rb') as stream:
            info = os.fstat(stream.fileno())
            require(stat.S_ISREG(info.st_mode) and info.st_size <= 256*1024**2, 'unittest-log-bound')
            total = 0
            while raw := stream.readline(2*1024**2 + 1):
                require(len(raw) <= 2*1024**2 and raw.endswith(b'\n'), 'unittest-log-line')
                total += len(raw)
                checksum.update(raw)
                line = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', raw.decode('utf-8')).rstrip('\n\r')
                if line.startswith(HELD_CHILD_CASE) and ' ... skipped ' not in line:
                    held_seen=True
                held_marker=line.find('{"heldChildBoundaryControl":')
                if held_marker>=0:
                    require(not held_diagnostics,'unittest-held-child-duplicate')
                    held=json.loads(line[held_marker:])
                    require(set(held)=={'heldChildBoundaryControl'},'unittest-held-child-marker')
                    held_diagnostics.append(held_child_public(held['heldChildBoundaryControl']))
                marker=line.find('{"nativeSelectionDiagnostic":')
                if marker>=0:
                    from native_selection_diagnostic import public
                    require(not diagnostics,'unittest-native-selection-duplicate')
                    value=json.loads(line[marker:])
                    require(set(value)=={'nativeSelectionDiagnostic'},'unittest-native-selection-marker')
                    diagnostics.append(public(value['nativeSelectionDiagnostic']))
                header = re.fullmatch(r'(ERROR|FAIL): ([A-Za-z_][A-Za-z_0-9]*) \(([A-Za-z_][A-Za-z_0-9.]*)\)(?: \(.*\))?', line)
                if header:
                    identity = header[3]
                    require((header[2], identity) in identities,
                            'unittest-log-test-identity')
                    require(len(failures) < 1000, 'unittest-log-case-cap')
                    current = {'outcome': header[1], 'test': identity, 'phase': header[2], 'frames': []}
                    failures.append(current)
                count = re.fullmatch(r'Ran ([0-9]+) tests? in ([0-9.]+)s', line)
                if count:
                    counts.append((int(count[1]), float(count[2])))
                ok = re.fullmatch(r'OK(?: \(skipped=([0-9]+)\))?', line)
                failed = re.fullmatch(r'FAILED \(([a-z]+=[0-9]+(?:, [a-z]+=[0-9]+)*)\)', line)
                if ok:
                    outcomes.append({'status': 'passed', 'failures': 0, 'errors': 0, 'skipped': int(ok[1] or 0)})
                if failed:
                    values = dict(item.split('=') for item in failed[1].split(', '))
                    require(set(values) <= {'failures','errors','skipped','expected failures','unexpected successes'},
                            'unittest-log-result-shape')
                    outcomes.append({'status': 'failed', **{key:int(values.get(key, 0)) for key in ('failures','errors','skipped')}})
                if current is None:
                    continue
                frame = re.fullmatch(r'  File "([^"]+)", line ([0-9]+), in ([A-Za-z_][A-Za-z_0-9]*|<module>)', line)
                if frame:
                    file = Path(frame[1])
                    fixed_file = file.resolve()
                    if (fixed_file in source_lines and
                            0 < int(frame[2]) <= source_lines[fixed_file] and len(current['frames']) < 32):
                        current['frames'].append({'file':fixed_file.name, 'line':int(frame[2])})
                exception = re.fullmatch(r'([A-Za-z_][A-Za-z_0-9.]*)(?:: (.*))?', line)
                if exception and exception[1] in exception_types:
                    current['exceptionType'] = exception[1]
                    if exception[1] in ('support.Failure', 'qa_core.Failure') and exception[2] in codes:
                        current['failureCode'] = exception[2]
                    number = re.match(r'\[Errno ([0-9]+)\]', exception[2] or '')
                    if number:
                        current['errno'] = int(number[1])
            final = os.fstat(stream.fileno())
            require(total == info.st_size == final.st_size and info.st_mtime_ns == final.st_mtime_ns,
                    'unittest-log-changed')
        require(len(counts) == len(outcomes) == 1, 'unittest-log-result-missing')
        require(held_seen==bool(held_diagnostics),'unittest-held-child-missing')
        outcome = outcomes[0]
        require(outcome['status']!='passed' or all(row['status']=='passed' for row in held_diagnostics),
                'unittest-held-child-status')
        require(sum(row['outcome']=='ERROR' for row in failures)==outcome['errors'] and
                sum(row['outcome']=='FAIL' for row in failures)==outcome['failures'], 'unittest-log-case-count')
        return {**result, **outcome, 'complete': True, 'total':counts[0][0], 'seconds':counts[0][1],
                'bytes':total, 'sha256':checksum.hexdigest(), 'cases':failures,
                'nativeSelectionDiagnostics':diagnostics,'heldChildBoundaryDiagnostics':held_diagnostics}
    except Exception as error:
        return {**result, 'errorType':type(error).__name__}
