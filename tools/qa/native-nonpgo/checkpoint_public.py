# SPDX-License-Identifier: MPL-2.0
"""Project an already validated restore receipt into fixed public evidence.

This is not an archive, manifest, ownership, or restore validation substitute.
Raw boundary/stage records stay private; no input or environment is modified.
"""
import os
import re

from checkpoint_replay import CONTRACT, EXECUTION_KEYS, ENV_KEYS, REPOSITORY, load_config
from qa_core import Failure, require


NORMAL_QUALIFICATION = 'verified-resumed-build-across-two-standard-intel-jobs'
BOUNDARY_CONTRACT = 'trusted-single-purpose-runner-v1'
NODE = 'toolkit/library/rust/target-objects'
REQUIRED_STAGES = ('configure', 'effective-config', 'checkpoint-exports',
                   'checkpoint-rust', 'checkpoint-rust-archive')
STAGE_KEYS = frozenset(('stage', 'exitCode', 'reason', 'failurePhase',
                        'groupCleanup', 'leaderReap', 'cleanupFailure'))


def _sha(value, length=64):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', value) is not None


def _zero(value):
    return type(value) is int and value == 0


def _boundary(value):
    require(type(value) is dict and type(value.get('contract')) is str and
            value['contract'] == BOUNDARY_CONTRACT and type(value.get('status')) is str and
            value['status'] == 'passed' and type(value.get('node')) is str and value['node'] == NODE and
            _sha(value.get('archiveSHA256')) and _sha(value.get('depInfoSHA256')) and
            _zero(value.get('knownBuildSurvivors')) and value.get('interrupted') is False,
            'checkpoint-public-boundary')
    check = value.get('knownBuildIdentityCheck')
    require(type(check) is dict and type(check.get('status')) is str and check['status'] == 'passed' and
            _zero(check.get('remainingLive')) and _zero(check.get('remainingZombies')) and
            type(check.get('observedBirths')) is int and check['observedBirths'] > 0,
            'checkpoint-public-identity-check')
    stages = value.get('stages')
    require(type(stages) is list and bool(stages) and all(
            type(row) is dict and STAGE_KEYS <= set(row) and
            type(row['stage']) is str and bool(row['stage']) for row in stages),
            'checkpoint-public-stage-shape')
    require(all(sum(row['stage'] == stage for row in stages) == 1 for stage in REQUIRED_STAGES),
            'checkpoint-public-required-stages')
    natural = all(_zero(row['exitCode']) and row['reason'] is None and
                  row['failurePhase'] is None for row in stages)
    reaped = all(type(row['leaderReap']) is dict and type(row['leaderReap'].get('status')) is str and
                 row['leaderReap']['status'] == 'reaped' and
                 _zero(row['leaderReap'].get('exitCode')) for row in stages)
    gone = all(type(row['groupCleanup']) is str and row['groupCleanup'] == 'verified-gone' for row in stages)
    clean = all(row['cleanupFailure'] is None for row in stages)
    require(natural and reaped and gone and clean, 'checkpoint-public-stage-completion')
    return {'contract': BOUNDARY_CONTRACT, 'status': 'passed', 'node': NODE,
            'archiveSHA256': value['archiveSHA256'], 'depInfoSHA256': value['depInfoSHA256'],
            'knownBuildSurvivors': 0, 'interrupted': False,
            'knownBuildIdentityCheck': {'status': 'passed', 'remainingLive': 0, 'remainingZombies': 0,
                                       'observedBirths': check['observedBirths']},
            'stageCount': len(stages), 'allStagesNaturalZero': natural,
            'allExactReapedZero': reaped, 'allGroupsGone': gone, 'noCleanupFailures': clean}


def _replay(value, restored):
    require(type(value) is dict and set(value) == {'contract', 'origin', 'currentExecution'} and
            type(value.get('contract')) is str and value['contract'] == CONTRACT,
            'checkpoint-public-replay-shape')
    try:
        config = load_config()
    except Failure:
        raise Failure('checkpoint-public-replay-config') from None
    origin = config['origin']
    candidate = value['origin']
    require(type(candidate) is dict and set(candidate) == set(origin) and all(
            type(candidate[key]) is type(expected) and candidate[key] == expected
            for key, expected in origin.items()), 'checkpoint-public-replay-origin')
    require(restored['buildDate'] == config['buildDate'] and
            restored['archiveSHA256'] == origin['sha256'] and
            restored['manifestSHA256'] == origin['manifestSHA256'],
            'checkpoint-public-replay-payload')
    current = value['currentExecution']
    require(type(current) is dict and set(current) == EXECUTION_KEYS and
            all(type(current[key]) is str for key in EXECUTION_KEYS) and
            current['repository'] == REPOSITORY and
            all(_sha(current[key], 40) for key in ('head', 'fixture')) and
            all(re.fullmatch('[1-9][0-9]{0,18}', current[key]) is not None for key in ('run', 'attempt')),
            'checkpoint-public-current-execution')
    require(all(type(os.environ.get(ENV_KEYS[key])) is str and
                current[key] == os.environ[ENV_KEYS[key]] for key in EXECUTION_KEYS) and
            current['run'] != origin['run'] and current['fixture'] != origin['fixture'],
            'checkpoint-public-current-environment')
    return {'contract': CONTRACT, 'origin': dict(origin),
            'currentExecution': {key: current[key] for key in sorted(EXECUTION_KEYS)}}


def public_restore(value):
    """Return only safe, typed restore evidence or a fixed checkpoint-public code.

    Both the normal qualification and the explicit fixed-origin replay contract
    are supported. Missing or failed required completion evidence fails closed.
    Optional measurements and all unknown fields are omitted, never zero-filled.
    """
    require(type(value) is dict, 'checkpoint-public-restore-shape')
    require(type(value.get('buildDate')) is str and
            re.fullmatch('[0-9]{14}', value['buildDate']) is not None and
            _sha(value.get('archiveSHA256')) and _sha(value.get('manifestSHA256')) and
            value.get('freshObjdir') is False and value.get('sameJob') is False,
            'checkpoint-public-restore-fields')
    qualification = value.get('qualification')
    require(type(qualification) is str and qualification in (NORMAL_QUALIFICATION, CONTRACT),
            'checkpoint-public-qualification')
    boundary = _boundary(value.get('producerBoundary'))
    if qualification == CONTRACT:
        replay = _replay(value.get('replay'), value)
    else:
        require(value.get('replay') is None, 'checkpoint-public-unexpected-replay')
        replay = None
    return {'buildDate': value['buildDate'], 'archiveSHA256': value['archiveSHA256'],
            'manifestSHA256': value['manifestSHA256'], 'qualification': qualification,
            'replay': replay, 'freshObjdir': False, 'sameJob': False, 'producerBoundary': boundary}
