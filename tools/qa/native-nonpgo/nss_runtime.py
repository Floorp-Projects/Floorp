# SPDX-License-Identifier: MPL-2.0
"""Opt-in same-OBJDIR NSS builds and bounded runtime diagnostics."""
from contextlib import ExitStack
import hashlib
import math
import os
from pathlib import Path
import sys
import time

from firefox_target_observer import _FixedRead, decode, identity
import mozilla_harness_smoke as smoke
from mozilla_target_build import GRAPH_PINS, stage_complete
from nss_dynamic_closure import DYNAMIC_TARGETS, graph_facts, _valid_bundle
from support import require


HERE = Path(__file__).resolve().parent
CONTRACT = 'same-objdir-nss-runtime-v1'
TARGETS = (('nss-softoken-build', DYNAMIC_TARGETS['softokn3']),
           ('nss-freebl-build', DYNAMIC_TARGETS['freebl3']))
STACK_STAGE = 'nss-stack-fixer-probe'
STACK_MARKER = b'FLOORP_QA_REAL_STACK_FIXER_V1'
WRAPPER_SHA256 = 'cfdca169762f2b6c82b7d73566069251ad4da3a260e434b53cefcf0eb5b07071'
STACK_SCRIPT = '''import runpy,sys
m = runpy.run_path(sys.argv[1])
out = m['fixSymbols']('#01: ???[' + sys.argv[2] + ' +0x0]\\n')
p = m['fixSymbols'].__globals__['fix_stacks']
assert p is not None and p.args == [sys.argv[3]]
assert type(out) is str and 0 < len(out) <= 16384 and out.endswith('\\n')
p.stdin.close()
assert p.wait(timeout=5) == 0
print('FLOORP_QA_REAL_STACK_FIXER_V1')
'''
PROBE_INPUTS = {'nss-probe/test_nss_sha256.js': '7146ef80d28c5b626eefebf20aac1c4b370e75b61e64a6a358e4b974db758494', 'nss-probe/xpcshell.toml': 'df31ff7e712b518759725164200ff83218307888fb4e5a4dd95b7866d65990f5'}


def remaining_deadline(runner, deadline, cap_seconds):
    elapsed = runner.elapsed()
    require(math.isfinite(elapsed) and math.isfinite(deadline) and
            0 <= elapsed < deadline * 60 < runner.limits['jobMinutes'] * 60,
            'nss-runtime-deadline')
    return min(deadline, (elapsed + cap_seconds) / 60)


def clean_precondition(runner):
    require(runner.failure is None and runner.monitor_failure is None and
            all(row.get('groupCleanup') == 'verified-gone' for row in runner.records),
            'nss-runtime-cleanup')


def graph_precondition(task, runner, deadline):
    clean_precondition(runner)
    end = remaining_deadline(runner, deadline, 30)
    with ExitStack() as stack:
        reader = _FixedRead(task, stack, time.monotonic() + end * 60 - runner.elapsed())
        data, files = {}, {}
        for key in ('rootMk', 'rootDepsMk'):
            relative, cap, expected = GRAPH_PINS[key]
            data[key], files[key] = reader.read(relative, cap)
            require(data[key] is not None and files[key]['sha256'] == expected,
                    'nss-runtime-graph-pin')
        graph = graph_facts(data['rootMk'], data['rootDepsMk'])
        require(graph['status'] == 'observed' and
                all(graph['targetsDeclared'].get(key) is True for key in DYNAMIC_TARGETS),
                'nss-runtime-node-missing')
        reader.verify()
    require(runner.elapsed() < end * 60, 'nss-runtime-deadline')
    return {'files': files, 'graph': graph, 'bindingsStable': True}


def run_build(source, task, runner, env, deadline, result):
    result.update(contract=CONTRACT, status='failed', currentStage='graph-precondition',
                  completedStages=[], graph=None, sameObjectTree=True, clockResetAllowed=False,
                  productQualification=False, native42ResultQualification=False)
    result['graph'] = graph_precondition(task, runner, deadline)
    require(not any(key in env for key in ('RECURSE_BASE_DIR', 'FASTER_RECURSIVE_MAKE')),
            'nss-runtime-environment')
    for stage, node in TARGETS:
        clean_precondition(runner)
        remaining_deadline(runner, deadline, 30)
        result['currentStage'] = stage
        before = len(runner.records)
        runner.run(stage, [sys.executable, 'mach', 'build', '-j3', '-C', '.', node],
                   source, env, runner.limits['jobMinutes'], phase_deadline_minutes=deadline)
        require(len(runner.records) == before + 1 and
                runner.records[-1].get('stage') == stage and stage_complete(runner.records[-1]),
                'nss-runtime-build-proof')
        clean_precondition(runner)
        result['completedStages'].append(stage)
        require(graph_precondition(task, runner, deadline) == result['graph'],
                'nss-runtime-post-graph')
    result.update(status='built', currentStage=None)


def run_stack_probe(task, runner, env, deadline, result):
    result.update(status='failed', realToolExecuted=False, symbolicationQuality='UNVERIFIED',
                  productQualification=False, native42ResultQualification=False)
    clean_precondition(runner)
    end = remaining_deadline(runner, deadline, 30)
    tools = task / 'tools'
    executable = tools / 'fix-stacks/fix-stacks'
    require(env.get('MOZ_FETCHES_DIR') == str(tools) and
            not any(key.startswith(('DYLD_', 'LD_')) for key in env), 'nss-runtime-environment')
    with ExitStack() as stack:
        until = time.monotonic() + end * 60 - runner.elapsed()
        wrapper_reader = _FixedRead(HERE / 'fixtures', stack, until)
        raw, result['wrapper'] = wrapper_reader.read('upstream-fix_stacks.py', 64 * 1024)
        require(raw is not None and hashlib.sha256(raw).hexdigest() == WRAPPER_SHA256,
                'nss-runtime-wrapper-pin')
        tool_reader = _FixedRead(tools, stack, until)
        raw, result['tool'] = tool_reader.read('fix-stacks/fix-stacks', 32 * 1024**2)
        require(raw is not None and os.access(executable, os.X_OK), 'nss-runtime-tool')
        wrapper_reader.verify()
        tool_reader.verify()
        before = len(runner.records)
        runner.run(STACK_STAGE, [sys.executable, '-c', STACK_SCRIPT,
                   str(HERE / 'fixtures/upstream-fix_stacks.py'), str(executable), str(executable)],
                   task, smoke.child_environment(env), runner.limits['jobMinutes'],
                   log_limit=1024**2, phase_deadline_minutes=end)
        require(len(runner.records) == before + 1 and
                runner.records[-1].get('stage') == STACK_STAGE and stage_complete(runner.records[-1]),
                'nss-runtime-stack-proof')
        clean_precondition(runner)
        wrapper_reader.verify()
        tool_reader.verify()
    with ExitStack() as stack:
        reader = _FixedRead(task, stack, time.monotonic() + 5)
        raw, result['log'] = reader.read(STACK_STAGE + '.private.log', 1024**2)
        require(raw is not None and raw.splitlines().count(STACK_MARKER) == 1,
                'nss-runtime-stack-marker')
        reader.verify()
    require(runner.elapsed() < end * 60, 'nss-runtime-deadline')
    result.update(status='passed', realToolExecuted=True, bindingsStable=True)


def run_load_probe(source, task, runner, env, deadline, macho, assets, result):
    result.update(status='failed', realNSSLoadObserved=False, exactDSOMapping='UNVERIFIED',
                  productQualification=False, native42ResultQualification=False, case={})
    clean_precondition(runner)
    end = remaining_deadline(runner, deadline, 120)
    require(not any(key.startswith(('DYLD_', 'LD_')) for key in env), 'nss-runtime-environment')
    with ExitStack() as stack:
        reader = _FixedRead(task, stack, time.monotonic() + 10)
        raw, result['config'] = reader.read('effective-config.private.log', 2 * 1024**2)
        require(raw is not None, 'nss-runtime-config')
        value = decode(raw)
        bundle = value.get('substs', {}).get('MOZ_MACBUNDLE_NAME') if type(value) is dict else None
        require(_valid_bundle(bundle), 'nss-runtime-config')
        reader.verify()
    binary = task / 'obj/dist' / bundle / 'Contents/MacOS/xpcshell'
    paths = {'xpcshell': (binary, 2), 'XUL': (binary.with_name('XUL'), 6)}
    before = {}
    for key, (path, kind) in paths.items():
        require(path.is_file() and path.resolve().is_relative_to(task / 'obj') and
                path.stat().st_uid == os.geteuid() and os.access(path, os.X_OK) and
                path.stat().st_mtime >= runner.started and macho(path, kind) == assets['binaries'][key],
                'nss-runtime-current-asset')
        before[key] = identity(path.stat())
    with ExitStack() as stack:
        reader = _FixedRead(HERE, stack, time.monotonic() + 10)
        for path, checksum in PROBE_INPUTS.items():
            raw, _ = reader.read(path, 64 * 1024)
            require(raw is not None and hashlib.sha256(raw).hexdigest() == checksum,
                    'nss-runtime-fixture-pin')
        reader.verify()
    smoke.run_case('nss-load', source, binary, HERE / 'nss-probe/xpcshell.toml',
                   HERE / 'nss-probe/test_nss_sha256.js', task, runner, env, end,
                   value=result['case'])
    require(result['case']['expectationSatisfied'] is True and
            result['case']['harness']['counts']['subtestStatuses'].get('PASS', 0) >= 2,
            'nss-runtime-load-proof')
    for key, (path, kind) in paths.items():
        require(identity(path.stat()) == before[key] and macho(path, kind) == assets['binaries'][key],
                'nss-runtime-post-asset')
    clean_precondition(runner)
    require(runner.elapsed() < end * 60, 'nss-runtime-deadline')
    result.update(status='passed', realNSSLoadObserved=True, bindingsStable=True,
                  oracle='NSS-PK11-32bytes-and-Rust-SHA256-abc', assets=assets['binaries'],
                  fixtureSHA256=dict(PROBE_INPUTS))


def proof_complete(value, records, assets):
    try:
        stages = [name for name, _ in TARGETS]
        selected = [row for row in records if row.get('stage') in [STACK_STAGE, *stages]]
        build, tool, load = (value[key] for key in ('build', 'stackProbe', 'loadProbe'))
        graph = build['graph']
        def receipt(row, cap):
            return (row.get('status') == 'observed' and type(row.get('bytes')) is int and
                    0 < row['bytes'] <= cap and type(row.get('sha256')) is str and
                    len(row['sha256']) == 64 and all(c in '0123456789abcdef' for c in row['sha256']))
        return (value.get('contract') == CONTRACT and value.get('productQualification') is False and
                value.get('native42ResultQualification') is False and
                [row['stage'] for row in selected] == [STACK_STAGE, *stages] and
                all(stage_complete(row) for row in selected) and
                build.get('contract') == CONTRACT and build.get('status') == 'built' and
                build.get('sameObjectTree') is True and build.get('clockResetAllowed') is False and
                build.get('currentStage') is None and build.get('completedStages') == stages and
                graph.get('bindingsStable') is True and graph['graph'].get('status') == 'observed' and
                all(graph['graph']['targetsDeclared'].get(key) is True for key in DYNAMIC_TARGETS) and
                set(graph['files']) == {'rootMk', 'rootDepsMk'} and
                all(receipt(graph['files'][key], GRAPH_PINS[key][1]) and
                    graph['files'][key]['sha256'] == GRAPH_PINS[key][2] for key in graph['files']) and
                tool.get('status') == 'passed' and tool.get('realToolExecuted') is True and
                tool.get('bindingsStable') is True and receipt(tool['tool'], 32 * 1024**2) and
                receipt(tool['wrapper'], 64 * 1024) and tool['wrapper']['sha256'] == WRAPPER_SHA256 and
                receipt(tool['log'], 1024**2) and
                load.get('status') == 'passed' and load.get('realNSSLoadObserved') is True and
                load.get('bindingsStable') is True and load.get('assets') == assets['binaries'] and
                load.get('fixtureSHA256') == PROBE_INPUTS and
                load.get('oracle') == 'NSS-PK11-32bytes-and-Rust-SHA256-abc' and
                load['case'].get('case') == 'nss-load' and load['case'].get('mode') == 'positive' and
                type(load['case'].get('expectedExitCode')) is int and load['case']['expectedExitCode'] == 0 and
                load['case']['harness'].get('status') == 'passed' and
                load['case']['harness'].get('completeLog') is True and
                type(load['case']['harness']['counts']['subtestStatuses'].get('PASS')) is int and
                load['case']['harness']['counts']['subtestStatuses']['PASS'] >= 2 and
                smoke.case_satisfied(load['case'], 0))
    except (KeyError, TypeError, AttributeError):
        return False
