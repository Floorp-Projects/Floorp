# SPDX-License-Identifier: MPL-2.0
"""Prioritize the restored shell/XUL closure, then build its stock smoke assets."""
from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import sys
import time

from firefox_target_graph import _assignment, target_graph_proof
from firefox_target_observer import _FixedRead, _graph_safe, Unknown as GraphUnknown
from support import Failure, digest, require, write_json


CONTRACT = 'verified-origin-root-target-mozilla-smoke-v1'
TARGETS = (
    ('mozilla-shell-build', 'js/xpconnect/shell/target'),
    ('mozilla-http3-build', 'netwerk/test/http3server/target'),
    ('mozilla-browser-build', 'browser/app/target'),
    ('mozilla-updater-build', 'toolkit/mozapps/update/updater/target'),
    ('mozilla-channel-build', 'toolkit/mozapps/macos-frameworks/ChannelPrefs/target'),
    ('mozilla-dmg-build', 'toolkit/mozapps/dmgInstallHelper/target'),
)
STAGES = tuple(name for name, _ in TARGETS) + ('mozilla-resources-build',)
GRAPH_PINS = {
    'rootMk': ('obj/root.mk', 4 * 1024**2,
               '95b1f67e60ee86a14aa42e46525a6cfc6832cef98a53be15540e937ec972c7d6'),
    'rootDepsMk': ('obj/root-deps.mk', 16 * 1024**2,
                   'b5c130fccb393e5caae95e84b1cc2fbbec7e252d94bc7a8078676b9fa7043ba5'),
    'fasterMakefile': ('obj/faster/Makefile', 4 * 1024**2,
                       'c211dbd23114188cf9913820555efdfaab453e2b42681ef9e3fc712c4e9edcb8'),
}


def stage_complete(row):
    leader = row.get('leaderReap') if type(row) is dict else None
    return (type(row) is dict and type(row.get('exitCode')) is int and row['exitCode'] == 0 and
            row.get('groupCleanup') == 'verified-gone' and
            all(key in row and row[key] is None for key in ('reason', 'failurePhase', 'cleanupFailure')) and
            type(leader) is dict and leader.get('status') == 'reaped' and
            type(leader.get('exitCode')) is int and leader['exitCode'] == 0)


def graph_precondition(task):
    with ExitStack() as stack:
        reader = _FixedRead(task, stack, time.monotonic() + 30)
        data, files = {}, {}
        for key, (relative, cap, expected) in GRAPH_PINS.items():
            raw, receipt = reader.read(relative, cap)
            require(raw is not None and receipt['sha256'] == expected, 'mozilla-target-graph-pin')
            data[key], files[key] = raw, receipt
        graph = target_graph_proof(data['rootMk'], data['rootDepsMk'])
        require(graph['status'] == 'observed' and graph['xpcshellLiteralClosure']['complete'] is True and
                all(graph['fixedReachability'][key] is True for key in
                    ('xpcshellToXUL', 'XULToGkrust', 'xpcshellToGkrust', 'xpcshellToCocoaObjects')),
                'mozilla-target-graph-closure')
        declared = _assignment(data['rootMk'].decode().split('\n'), 'compile_targets')
        require(all(target in declared for _, target in TARGETS), 'mozilla-target-node-missing')
        reader.verify()
    return {'files': files, 'graph': graph, 'bindingsStable': True}


def _journal(task, runner, result):
    try:
        write_json(task / 'mozilla-target-build.json', result)
    except OSError:
        runner.fail('mozilla-target-evidence', 'mozilla-target-evidence-write')
        raise Failure('mozilla-target-evidence-write') from None


def run_build(source, task, runner, env, deadline, result):
    result.update(contract=CONTRACT, status='failed', currentStage='graph-precondition',
                  completedStages=[], graph=None, assets=None, fullHostQualification=False,
                  minimalSufficientSuffixEstablished=False, nativeAuthentication='NOT_TESTED',
                  crossJobProgressPersisted=False, sameObjectTreeAcrossStages=True,
                  clockResetAllowed=False, durationToCompletion='UNKNOWN')
    # This is the graph and FasterMake backend actually observed in Recovery56,
    # after the original archive/source/toolchain/configuration restore guards.
    result['graph'] = graph_precondition(task)
    require(not any(key in env for key in ('RECURSE_BASE_DIR', 'FASTER_RECURSIVE_MAKE')),
            'mozilla-target-environment')
    commands = [(name, [sys.executable, 'mach', 'build', '-j3', '-C', '.', target])
                for name, target in TARGETS]
    # No -C .: the documented faster directory default must run main-app
    # repackage. The root faster alias suppresses it in hybrid recursion.
    commands.append(('mozilla-resources-build', [sys.executable, 'mach', 'build', '-j3', 'faster']))
    for stage, command in commands:
        result['currentStage'] = stage
        _journal(task, runner, result)
        print(json.dumps({'mozillaTargetMilestone': stage, 'state': 'started'}), file=sys.stderr, flush=True)
        before = len(runner.records)
        runner.run(stage, command, source, env, runner.limits['jobMinutes'],
                   phase_deadline_minutes=deadline)
        require(len(runner.records) == before + 1 and runner.records[-1].get('stage') == stage and
                stage_complete(runner.records[-1]) and runner.failure is None and runner.monitor_failure is None,
                'mozilla-target-stage-proof')
        result['completedStages'].append(stage)
        _journal(task, runner, result)
        print(json.dumps({'mozillaTargetMilestone': stage, 'state': 'completed'}), file=sys.stderr, flush=True)
    result.update(status='built', currentStage=None)
    _journal(task, runner, result)


def verify_assets(task, runner, macho):
    config = json.loads((task / 'effective-config.private.log').read_text())['substs']
    bundle, app_name = config.get('MOZ_MACBUNDLE_NAME'), config.get('MOZ_APP_NAME')
    require(type(bundle) is str and re.fullmatch(r'[A-Za-z0-9 _.-]+\.app', bundle) is not None and
            '..' not in bundle and type(app_name) is str and re.fullmatch(r'[A-Za-z0-9_-]+', app_name),
            'mozilla-target-bundle')
    root = (task / 'obj').resolve()
    app = root / 'dist' / bundle / 'Contents'
    binaries = {
        'xpcshell': (app / 'MacOS/xpcshell', 2), 'XUL': (app / 'MacOS/XUL', 6),
        'http3server': (root / 'dist/bin/http3server', 2),
        'browser': (app / 'MacOS' / app_name, 2),
        'updater': (app / 'Library/LaunchServices/org.mozilla.updater', 2),
        'channelPrefs': (app / 'Frameworks/ChannelPrefs.framework/ChannelPrefs', 6),
        'dmgHelper': (app / 'Library/LaunchServices/org.mozilla.dmgInstallHelper', 2),
    }
    result = {'binaries': {}, 'resources': {}}
    for label, (path, kind) in binaries.items():
        require(path.is_file() and path.resolve().is_relative_to(root) and
                path.stat().st_uid == os.geteuid() and os.access(path, os.X_OK) and
                path.stat().st_mtime >= runner.started, 'mozilla-target-current-asset')
        record = macho(path, kind)
        require(record['uuid'] is not None, 'mozilla-target-asset-identity')
        result['binaries'][label] = record
    for label, path in (
        ('appConstants', app / 'Resources/modules/AppConstants.sys.mjs'),
        ('applicationIni', app / 'Resources/application.ini'),
        ('dependentLibs', app / 'Resources/dependentlibs.list'),
        ('channelInfo', app / 'Frameworks/ChannelPrefs.framework/Resources/Info.plist'),
    ):
        require(path.is_file() and path.resolve().is_relative_to(root) and
                path.stat().st_uid == os.geteuid() and 0 < path.stat().st_size <= 1024**2,
                'mozilla-target-resource')
        result['resources'][label] = {'bytes': path.stat().st_size, 'sha256': digest(path)}
    return result


def proof_complete(targets, mozilla, rows):
    from mozilla_harness_smoke import case_satisfied
    try:
        selected = [row for row in rows if row.get('stage') in STAGES]
        if not (targets.get('contract') == CONTRACT and targets.get('status') == 'passed' and
                targets.get('fullHostQualification') is False and targets.get('clockResetAllowed') is False and
                targets.get('minimalSufficientSuffixEstablished') is False and
                targets.get('nativeAuthentication') == 'NOT_TESTED' and
                'currentStage' in targets and targets['currentStage'] is None and
                targets.get('durationToCompletion') == 'UNKNOWN' and
                targets.get('crossJobProgressPersisted') is False and targets.get('sameObjectTreeAcrossStages') is True and
                targets.get('completedStages') == list(STAGES) and
                [row.get('stage') for row in selected] == list(STAGES) and
                all(stage_complete(row) for row in selected)):
            return False
        bound = targets['graph']
        graph = bound['graph']
        _graph_safe(graph)
        if not (bound.get('bindingsStable') is True and set(bound['files']) == set(GRAPH_PINS) and
                all(bound['files'][key].get('status') == 'observed' and
                    bound['files'][key].get('sha256') == pin[2] and
                    type(bound['files'][key].get('bytes')) is int and 0 < bound['files'][key]['bytes'] <= pin[1]
                    for key, pin in GRAPH_PINS.items()) and
                graph.get('status') == 'observed' and graph.get('scope') == 'root-deps-mk-literal-edges-only' and
                graph['xpcshellLiteralClosure'].get('complete') is True and
                all(graph['fixedReachability'].get(key) is True for key in
                    ('xpcshellToXUL','XULToGkrust','xpcshellToGkrust','xpcshellToCocoaObjects')) and
                all(graph['files'][key] == {field: bound['files'][key][field] for field in ('bytes','sha256')}
                    for key in ('rootMk','rootDepsMk')) and
                all(graph.get(key) is False for key in ('realBuildPerformed','makeOrRecipeExecuted',
                    'minimalSufficientSuffixEstablished','fullHostQualification')) and
                graph.get('durationToCompletion') == 'UNKNOWN'):
            return False
        assets = targets['assets']
        def hashed(record):
            return (type(record) is dict and type(record.get('bytes')) is int and record['bytes'] > 0 and
                    type(record.get('sha256')) is str and re.fullmatch('[0-9a-f]{64}', record['sha256']) is not None)
        def binary(record, kind):
            return (hashed(record) and type(record.get('filetype')) is int and record['filetype'] == kind and
                    type(record.get('uuid')) is str and re.fullmatch('[0-9a-f]{32}', record['uuid']) is not None and
                    type(record.get('sections')) is int and record['sections'] > 0 and
                    type(record.get('sectionsSHA256')) is str and
                    re.fullmatch('[0-9a-f]{64}', record['sectionsSHA256']) is not None)
        if not (set(assets['binaries']) == {'xpcshell', 'XUL', 'http3server', 'browser',
                                          'updater', 'channelPrefs', 'dmgHelper'} and
                set(assets['resources']) == {'appConstants', 'applicationIni', 'dependentLibs', 'channelInfo'} and
                all(binary(value, 6 if key in ('XUL', 'channelPrefs') else 2)
                    for key, value in assets['binaries'].items()) and
                all(hashed(value) for value in assets['resources'].values()) and
                all(assets['binaries'][key] == mozilla['builtAssets'][key] for key in ('xpcshell', 'XUL'))):
            return False
        cases = mozilla['cases']
        expected = (('compiled-debug', 0, 'passed'), ('failure-control', 1, 'failed'),
                    ('upstream-version-comparator', 0, 'passed'))
        return (mozilla.get('contract') == 'current-source-mozilla-xpcshell-smoke-v1' and
                len(cases) == 3 and all(case.get('case') == label and
                case.get('expectationSatisfied') is True and type(case.get('expectedExitCode')) is int and
                case['expectedExitCode'] == code and case.get('mode') == ('negative-control' if code else 'positive') and
                case['harness']['status'] == status and
                case['harness']['completeLog'] is True and case_satisfied(case, code)
                for case, (label, code, status) in zip(cases, expected)))
    except (KeyError, TypeError, AttributeError, GraphUnknown):
        return False
