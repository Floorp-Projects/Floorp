# SPDX-License-Identifier: MPL-2.0
"""Explicit consumer deadlines measured from the existing job origin.

This pure selector neither changes the plan nor creates or resets a clock.
The caller keeps its original Runner, platform and resource/cleanup limits.
"""
from qa_core import Failure, require


PROFILE = 'consumer-shared-final-deadline-v1'
BASELINE_PLAN_SHA256 = '33fb00c07a7ad9f66950489f06c0b5039ee173bc78297bc13b39265b24f4d460'
KEYS = ('consumerRestoreDeadlineMinutes', 'consumerBuildDeadlineMinutes',
        'consumerPackageDeadlineMinutes', 'consumerNativeDeadlineMinutes', 'finalDeadlineMinutes')
LEGACY = (60, 300, 315, 335, 345)
PLATFORM_MINUTES = 360


def select_consumer_time(plan, plan_sha256, checkpoint_role, *, shared=False, restore_only=False):
    """Keep legacy values by default; explicitly share the fixed final deadline."""
    require(type(shared) is bool and type(restore_only) is bool, 'consumer-time-flag')
    try:
        legacy = {key: plan['checkpoint'][key] for key in KEYS}
    except (KeyError, TypeError):
        raise Failure('consumer-time-plan-shape') from None
    if not shared:
        return {'deadlines': legacy, 'evidence': None}
    require(type(checkpoint_role) is str and checkpoint_role == 'consumer', 'consumer-time-role')
    require(not restore_only, 'consumer-time-restore-only')
    require(type(plan_sha256) is str and plan_sha256 == BASELINE_PLAN_SHA256,
            'consumer-time-plan-binding')
    try:
        platform = plan['limits']['jobMinutes']
    except (KeyError, TypeError):
        raise Failure('consumer-time-plan-shape') from None
    values = tuple(legacy[key] for key in KEYS)
    require(all(type(value) is int for value in (*values, platform)), 'consumer-time-caps')
    restore, build, package, native, final = values
    require(0 < restore <= build <= package <= native <= final < platform and platform - final == 15,
            'consumer-time-order')
    require(values == LEGACY and platform == PLATFORM_MINUTES, 'consumer-time-caps')
    deadlines = dict(legacy)
    for key in KEYS[1:4]:
        deadlines[key] = final
    evidence = {'schema': 1, 'profile': PROFILE, 'baselinePlanSHA256': BASELINE_PLAN_SHA256,
                'platformMinutes': platform, 'sharedWorkDeadlineMinutes': final,
                'cleanupReserveMinutes': platform - final, 'clock': 'fresh-job-monotonic-origin',
                'deadlineReference': 'absolute-from-existing-job-origin',
                'deadlines': dict(deadlines), 'legacyDeadlines': dict(legacy),
                'onlyExecutionTimingChanged': True, 'resourceAndCompilationInputsChanged': False,
                'clockResetAllowed': False, 'stageBudgetsAreAdditive': False}
    return {'deadlines': deadlines, 'evidence': evidence}
