# SPDX-License-Identifier: MPL-2.0
"""Explicit diagnostic compatibility checks for one verified checkpoint origin.

These functions do not fetch, extract, mutate contexts, set environment values,
or authorize a product qualification. The caller must explicitly opt in, verify
the actual current job through GitHub, and bind reviewed code externally. The
normal checkpoint restore path never invokes this module implicitly.
"""
import datetime
import json
import os
from pathlib import Path, PurePosixPath
import re

from qa_core import require


CONTRACT = 'verified-origin-cross-run-checkpoint-replay-v1'
REPOSITORY = 'Floorp-Projects/Floorp-Runtime'
ARTIFACT_NAME = 'checkpoint.tar.gz'
ORIGIN = {
    'run': '39197008470', 'attempt': '1',
    'head': 'fdc6722b6ac5467f7baf6c0fa6a3537ee0a51860',
    'fixture': '4337d51a228834f4b2cbee1f4ef0771eb45737a8',
    'producerJob': 117881829274, 'artifact': 12164479528, 'bytes': 2213942620,
    'sha256': 'f548c7f86f6a05dc23b2b3c9764cec3d4d1d1e7d01014b2c56d5d60aac84ed15',
    'manifestSHA256': 'ec4581e1d23861581b446b4d826c3812df9cc21bc9224fb682f16120690d8644',
    'createdAt': '2026-10-19T05:20:31Z', 'expiresAt': '2026-10-20T05:19:56Z'}
BUILD_DATE = '20261019035607'
CONTEXT_KEYS = frozenset(('planSHA256', 'roots', 'fixture', 'harness', 'runtime',
                          'repository', 'run', 'attempt', 'head', 'image', 'pythonSHA256'))
ROOT_KEYS = frozenset(('source', 'tools', 'sdk', 'python', 'obj', 'mozbuild', 'cargo', 'rustup'))
HISTORIC_KEYS = frozenset(('run', 'attempt', 'head', 'fixture'))
EXECUTION_KEYS = frozenset(('repository', 'run', 'attempt', 'head', 'fixture'))
ENV_KEYS = {'fixture': 'FLOORP_FIXTURE_SHA', 'harness': 'HARNESS_SHA', 'runtime': 'RUNTIME_SHA',
            'repository': 'GITHUB_REPOSITORY', 'run': 'GITHUB_RUN_ID',
            'attempt': 'GITHUB_RUN_ATTEMPT', 'head': 'GITHUB_SHA', 'image': 'ImageVersion'}


def _validate_config(config):
    require(type(config) is dict and set(config) == {'schema', 'contract', 'origin', 'buildDate'} and
            type(config.get('schema')) is int and config['schema'] == 1,
            'checkpoint-replay-config-shape')
    require(type(config['contract']) is str and config['contract'] == CONTRACT,
            'checkpoint-replay-config-contract')
    origin = config['origin']
    require(type(origin) is dict and set(origin) == set(ORIGIN) and
            all(type(origin[key]) is type(value) and origin[key] == value for key, value in ORIGIN.items()),
            'checkpoint-replay-config-origin')
    require(type(config['buildDate']) is str and config['buildDate'] == BUILD_DATE,
            'checkpoint-replay-config-build-date')
    return origin


def load_config(path=None):
    """Read at most 16 KiB and require the exact reviewed schema and origin."""
    def pairs(values):
        result = {}
        for key, value in values:
            require(key not in result, 'checkpoint-replay-config-shape')
            result[key] = value
        return result
    try:
        path = Path(__file__).with_name('checkpoint-replay.json') if path is None else Path(path)
        with path.open('rb') as stream:
            raw = stream.read(16385)
        require(len(raw) <= 16384, 'checkpoint-replay-config-shape')
        config = json.loads(raw.decode('utf-8'), object_pairs_hook=pairs)
    except (OSError, ValueError, RecursionError, TypeError):
        require(False, 'checkpoint-replay-config-read')
    _validate_config(config)
    return config


def _artifact_matches(value, origin):
    if type(value) is not dict or type(value.get('workflow_run')) is not dict:
        return False
    workflow = value['workflow_run']
    return (type(value.get('id')) is int and value['id'] == origin['artifact'] and
            value.get('name') == ARTIFACT_NAME and value.get('expired') is False and
            type(value.get('size_in_bytes')) is int and value['size_in_bytes'] == origin['bytes'] and
            value.get('digest') == 'sha256:' + origin['sha256'] and
            type(workflow.get('id')) is int and workflow['id'] == int(origin['run']) and
            workflow.get('head_sha') == origin['head'] and
            value.get('created_at') == origin['createdAt'] and value.get('expires_at') == origin['expiresAt'])


def validate_origin(config, source_run, source_jobs, artifact, source_artifacts, *, now=None):
    """Validate fixed read-only GitHub API replies before any artifact download.

    `now` is an aware datetime for deterministic controls; production omits it.
    The source run failed in its consumer; its exact producer must have succeeded.
    """
    origin = _validate_config(config)
    require(type(source_run) is dict and type(source_run.get('repository')) is dict and
            source_run['repository'].get('full_name') == REPOSITORY and
            type(source_run.get('id')) is int and source_run['id'] == int(origin['run']) and
            source_run.get('head_sha') == origin['head'] and
            type(source_run.get('run_attempt')) is int and source_run['run_attempt'] == 1 and
            source_run.get('event') == 'push' and source_run.get('status') == 'completed' and
            source_run.get('conclusion') == 'failure', 'checkpoint-replay-source-run')
    require(type(source_jobs) is dict and type(source_jobs.get('total_count')) is int and
            source_jobs['total_count'] == 2 and type(source_jobs.get('jobs')) is list and
            len(source_jobs['jobs']) == 2 and all(type(row) is dict for row in source_jobs['jobs']),
            'checkpoint-replay-source-jobs')
    jobs = source_jobs['jobs']
    require(all(type(row.get('id')) is int and row['id'] > 0 and
                type(row.get('run_id')) is int and row['run_id'] == int(origin['run']) for row in jobs) and
            len({row['id'] for row in jobs}) == 2, 'checkpoint-replay-source-jobs')
    producers = [row for row in jobs if row.get('name') == 'checkpoint-producer']
    require(len(producers) == 1 and producers[0]['id'] == origin['producerJob'] and
            producers[0].get('status') == 'completed' and producers[0].get('conclusion') == 'success' and
            producers[0].get('labels') == ['macos-15-intel'], 'checkpoint-replay-producer-job')
    require(_artifact_matches(artifact, origin), 'checkpoint-replay-artifact')
    require(type(source_artifacts) is dict and type(source_artifacts.get('total_count')) is int and
            source_artifacts['total_count'] == 1 and type(source_artifacts.get('artifacts')) is list and
            len(source_artifacts['artifacts']) == 1 and
            _artifact_matches(source_artifacts['artifacts'][0], origin), 'checkpoint-replay-source-artifacts')
    created = datetime.datetime.fromisoformat(origin['createdAt'].replace('Z', '+00:00'))
    expires = datetime.datetime.fromisoformat(origin['expiresAt'].replace('Z', '+00:00'))
    now = datetime.datetime.now(datetime.timezone.utc) if now is None else now
    require(type(now) is datetime.datetime and now.tzinfo is not None and now.utcoffset() is not None,
            'checkpoint-replay-clock')
    require(0 < (expires-created).total_seconds() <= 86400+120 and created <= now < expires,
            'checkpoint-replay-artifact-expired')
    return {'contract': CONTRACT, 'originVerified': True, 'producerJobVerified': True,
            'soleArtifactVerified': True, 'unexpiredAtVerification': True,
            'strictSameRunQualified': False, 'productQualification': False}


def _decimal(value):
    return type(value) is str and re.fullmatch('[1-9][0-9]{0,18}', value) is not None


def _sha(value, length):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', value) is not None


def _context(value):
    require(type(value) is dict and set(value) == CONTEXT_KEYS, 'checkpoint-replay-context-keyset')
    roots = value['roots']
    require(type(roots) is dict and set(roots) == ROOT_KEYS and all(
        type(path) is str and path and not path.startswith('//') and '\\' not in path and
        all(ord(char) >= 32 and ord(char) != 127 for char in path) and
        PurePosixPath(path).is_absolute() and '..' not in PurePosixPath(path).parts and
        str(PurePosixPath(path)) == path for path in roots.values()), 'checkpoint-replay-context-roots')
    require(_sha(value['planSHA256'], 64) and _sha(value['pythonSHA256'], 64) and
            all(_sha(value[key], 40) for key in ('fixture', 'harness', 'runtime', 'head')) and
            value['repository'] == REPOSITORY and _decimal(value['run']) and _decimal(value['attempt']) and
            type(value['image']) is str and re.fullmatch('[A-Za-z0-9_.-]{1,80}', value['image']) is not None,
            'checkpoint-replay-context-value')


def _current_context(current_context, origin):
    _context(current_context)
    require(all(type(os.environ.get(env)) is str and current_context[key] == os.environ[env]
                for key, env in ENV_KEYS.items()), 'checkpoint-replay-current-environment')
    require(current_context['run'] != origin['run'], 'checkpoint-replay-same-run')
    require(current_context['fixture'] != origin['fixture'], 'checkpoint-replay-current-fixture')


def validate_receipt(config, replay, current_context):
    """Validate only replay metadata; caller still verifies archive/manifest bytes.

    The receipt has exactly contract/origin/currentExecution. currentExecution
    has exactly repository/run/attempt/head/fixture from the real current job.
    current_context must be recomputed by checkpoint.context, not replayed env.
    """
    origin = _validate_config(config)
    require(type(replay) is dict and set(replay) == {'contract', 'origin', 'currentExecution'} and
            type(replay.get('contract')) is str and replay['contract'] == CONTRACT,
            'checkpoint-replay-receipt-shape')
    candidate = replay['origin']
    require(type(candidate) is dict and set(candidate) == set(origin) and all(
            type(candidate[key]) is type(value) and candidate[key] == value for key, value in origin.items()),
            'checkpoint-replay-receipt-origin')
    _current_context(current_context, origin)
    current = replay['currentExecution']
    require(type(current) is dict and set(current) == EXECUTION_KEYS and all(
            type(current[key]) is str and current[key] == current_context[key] for key in EXECUTION_KEYS),
            'checkpoint-replay-current-execution')
    return {'contract': CONTRACT, 'originPinned': True, 'currentExecutionBound': True,
            'crossRun': True, 'strictSameRunQualified': False, 'productQualification': False}


def compare_context(manifest_context, current_context, config):
    """Require complete context equality except four explicitly pinned origins.

    Return only fixed context-key booleans; never raw roots or environment values.
    This does not mutate either context or os.environ, including on failure.
    """
    origin = _validate_config(config)
    _context(manifest_context)
    _current_context(current_context, origin)
    require(all(manifest_context[key] == origin[key] for key in HISTORIC_KEYS),
            'checkpoint-replay-context-origin')
    checks = {key: manifest_context[key] == (origin[key] if key in HISTORIC_KEYS else current_context[key])
              for key in sorted(CONTEXT_KEYS)}
    require(all(checks.values()), 'checkpoint-replay-context-incompatible')
    return {'contract': CONTRACT, 'checks': checks, 'historicOriginPinned': True,
            'remainingContextExact': True, 'strictSameRunQualified': False, 'productQualification': False}
