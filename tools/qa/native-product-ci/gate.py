#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Fail-closed evidence contract. Not an artifact authentication/runner adapter.

An authenticated producer and trusted resolver must supply the immutable plan;
never accept a plan generated from the result being judged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

GROUPS = (
    'p0-tabs', 'p1-sessionstore-browser', 'p1-sessionstore-oldformat',
    'p1-sessionstore-startup-telemetry', 'p1-sessionstore-unit',
    'p1-sessionstore-marionette', 'p2-contextualidentity-unit',
    'p2-contextualidentity-browser',
)
PILOT_GROUPS = tuple(g for g in GROUPS if g != 'p1-sessionstore-marionette')

MANIFESTS = (
    'browser/components/tabbrowser/test/browser/tabs/browser.toml',
    'browser/components/sessionstore/test/browser.toml',
    'browser/components/sessionstore/test/browser_oldformat.toml',
    'browser/components/sessionstore/test/browser_startup_telemetry.toml',
    'browser/components/sessionstore/test/unit/xpcshell.toml',
    'browser/components/sessionstore/test/marionette/manifest.toml',
    'toolkit/components/contextualidentity/tests/unit/xpcshell.toml',
    'toolkit/components/contextualidentity/tests/browser/browser.toml',
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    return type(value) is int and value > 0


def unique(items, field):
    require(isinstance(items, list) and items, f'empty {field}')
    require(all(isinstance(x, str) and x for x in items), f'invalid {field}')
    require(len(items) == len(set(items)), f'duplicate {field}')
    return set(items)


def sha(value, length=64):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{%d}' % length, value)


def material(root, name, expected):
    require(isinstance(name, str) and '\\' not in name, 'unsafe material path')
    p = Path(name)
    require(not p.is_absolute() and '..' not in p.parts and p.parts, 'unsafe material path')
    target = root / p
    require(not any((root / Path(*p.parts[:i])).is_symlink() for i in range(1, len(p.parts)+1)), 'symlink material')
    require(target.is_file() and target.resolve().is_relative_to(root.resolve()), 'missing material')
    require(sha(expected), 'invalid material digest')
    require(hashlib.sha256(target.read_bytes()).hexdigest() == expected, 'material digest mismatch')
    return target


def validate_producer(record, floorp_sha, runtime_sha):
    require(record.get('schema') == 'floorp-native-product-ci/1', 'unsupported producer schema')
    require(sha(floorp_sha, 40) and sha(runtime_sha, 40), 'full source SHA required')
    require(record.get('floorp_sha') == floorp_sha and record.get('runtime_sha') == runtime_sha, 'source mismatch')
    require(record.get('platform') == 'linux-x86_64', 'unsupported platform')
    require(record.get('tests_enabled') is True and record.get('product_finalized') is True, 'not a finalized test-enabled product')
    cohort = record.get('cohort', {})
    require(cohort.get('repository') == 'Floorp-Projects/Floorp-Runtime', 'unexpected producer repository')
    require(number(cohort.get('run_id')) and number(cohort.get('run_attempt')), 'missing producer run/attempt')
    require(isinstance(cohort.get('build_id'), str) and re.fullmatch('[0-9]{14}', cohort['build_id']), 'invalid build ID')
    require(set(record.get('manifests', {})) == set(MANIFESTS), 'incomplete manifest inventory')
    require(all(sha(v) for v in record['manifests'].values()), 'missing manifest digest')
    artifacts = record.get('artifacts', {})
    require(set(artifacts) == {'runtime', 'test_support', 'product'}, 'missing artifact cohort')
    for role, artifact in artifacts.items():
        require(sha(artifact.get('sha256')), 'missing artifact digest')
        require(artifact.get('build_id') == cohort['build_id'], 'mixed build IDs')
        require(artifact.get('runtime_sha') == runtime_sha, 'mixed Runtime sources')
        if role != 'product':
            require(artifact.get('producer_run_id') == cohort['run_id'] and artifact.get('producer_run_attempt') == cohort['run_attempt'], 'mixed producer attempts')
        else:
            require(artifact.get('floorp_sha') == floorp_sha, 'foreign product frontend')
    return record


def raw_summary(path, expected_ids, id_mode="exact", disabled_ids=None):
    disabled_ids = set() if disabled_ids is None else set(disabled_ids)
    require(disabled_ids <= expected_ids, "disabled identity outside native plan")
    def canonical(value):
        require(isinstance(value, str), "invalid native test identity")
        return value.rsplit("/", 1)[-1] if id_mode == "unique-file-basename" else value
    started, ended, active = False, False, set()
    observed, executed, skipped = set(), set(), set()
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        e = json.loads(line)  # malformed/truncated logs must not be ignored
        action = e.get('action')
        if action == 'suite_start':
            require(not started and not ended, 'multiple native invocations in one report')
            started = True
        elif action == 'suite_end':
            require(started and not ended and not active, 'invalid/incomplete suite end')
            ended = True
        elif action in ('test_start', 'test_status', 'test_end'):
            require(started and not ended, 'test outside native suite')
            identity = canonical(e.get('test'))
            require(identity in expected_ids, 'unplanned native test identity')
            if action != 'test_start':
                allowed = {'PASS','FAIL','ERROR','TIMEOUT','ASSERT','PRECONDITION_FAILED','SKIP'}
                allowed |= {'NOTRUN'} if action == 'test_status' else {'OK','CRASH'}
                require(e.get('status') in allowed, 'missing/unknown native status')
                require('expected' not in e or e['expected'] in allowed, 'unknown native expected status')
                intermittent = e.get('known_intermittent', [])
                require(isinstance(intermittent,list) and all(v in allowed for v in intermittent), 'invalid intermittent status list')
                # Serialized mozlog omits expected on matching outcomes (including
                # expected FAIL). status itself is mandatory, never defaulted.
                require('expected' not in e or e['status'] == e['expected'] or e['status'] in intermittent, 'unexpected native status')
            if action == 'test_start':
                require(identity not in active and identity not in observed, 'duplicate test/retry')
                active.add(identity)
            elif action == 'test_status':
                require(e['status'] not in {'ERROR','TIMEOUT','ASSERT','PRECONDITION_FAILED'}, 'subtest infrastructure failure')
                require(identity in active, 'subtest outside active test')
                require(isinstance(e.get('subtest'),str) and bool(e['subtest']), 'missing subtest identity')
            elif action == 'test_end':
                require(identity not in observed, 'duplicate terminal status')
                # Crash/timeout never certify a successful product gate, even expected.
                require(e['status'] not in {'ERROR', 'TIMEOUT', 'CRASH', 'ASSERT', 'PRECONDITION_FAILED'}, 'native error/timeout/crash')
                require(identity in active, 'test did not start')
                active.discard(identity)
                observed.add(identity)
                if e['status'] == 'SKIP':
                    skipped.add(identity)
                else:
                    executed.add(identity)
        elif action == 'crash' or (action == 'log' and e.get('level') in {'ERROR', 'CRITICAL'}):
            raise ValueError('native infrastructure error')
    require(started and ended and not active, 'native suite incomplete')
    require(observed == expected_ids, 'missing native test coverage')
    require(skipped == disabled_ids, 'native skips differ from pre-resolved disabled tests')
    require(executed, 'zero executed tests / skip-only')
    return len(executed)


def aggregate(plan, report, evidence_root, needs, scope="full"):
    require(scope in {"full", "browser-xpcshell"}, "unknown gate scope")
    required = GROUPS if scope == "full" else PILOT_GROUPS
    if scope != "full":
        require(plan.get("scope") == "browser-xpcshell", "pilot plan required")
    else:
        require(plan.get("scope", "full") == "full", "partial plan cannot satisfy full gate")
    require(isinstance(needs, dict) and set(needs) == {'execute'} and
            isinstance(needs['execute'], dict) and needs['execute'].get('result') == 'success',
            'execute job did not succeed')
    require(plan.get('schema') == 1 and report.get('schema') == 1, 'unsupported evidence schema')
    require(report.get('plan_sha256') == hashlib.sha256(json.dumps(plan, sort_keys=True, separators=(',', ':')).encode()).hexdigest(), 'plan binding mismatch')
    require(report.get('finished') is True and not report.get('interrupted') and not report.get('manual_cleanup_required'), 'unfinished/unsafe execution')
    groups = plan.get('groups', [])
    require(len(groups) == len(required) and {g['id'] for g in groups} == set(required), 'missing/duplicate requested group')
    rows = report.get('groups', [])
    require(len(rows) == len(required) and {g['id'] for g in rows} == set(required), 'missing/duplicate result group')
    actual = {g['id']: g for g in rows}
    count = 0
    for group in groups:
        row = actual[group['id']]
        require(type(row.get('exit_code')) is int and row['exit_code'] == 0, 'native exit failure')
        expected = unique(group.get('resolved_test_ids'), 'resolved tests')
        logfile = material(evidence_root, row['raw_log'], row['raw_sha256'])
        mode = group.get('id_mode', 'exact')
        require(mode == 'exact' or (scope == 'browser-xpcshell' and mode == 'unique-file-basename'), 'unsupported identity mode')
        disabled = group.get('disabled_test_ids', [])
        require(isinstance(disabled,list) and len(disabled)==len(set(disabled)), 'invalid disabled inventory')
        if scope == 'browser-xpcshell': require('disabled_test_ids' in group, 'missing native disabled inventory')
        count += raw_summary(logfile, expected, mode, disabled)
    return {'status': 'PASS_EVIDENCE_CONTRACT' if scope == 'full' else 'PASS_BROWSER_XPCSHELL_PILOT_ONLY', 'groups': len(groups), 'executed': count}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--needs', required=True)
    parser.add_argument('--scope', choices=['full','browser-xpcshell'], default='full')
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    root = args.evidence
    # No fallback and no empty default: missing download is a failing gate.
    plan = json.loads((root/'plan.json').read_text())
    report = json.loads((root/'report.json').read_text())
    print(json.dumps(aggregate(plan, report, root, json.loads(args.needs), args.scope)))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        print('Native product gate failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
