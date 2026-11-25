"""Bounded, whitelisted observations from the marked synthetic native profile."""
import hashlib
import json
import re

MAX_DIAGNOSTIC = 2 * 1024 * 1024


def failure_diagnostic(result, context, marker_bytes):
    if not isinstance(result, dict) or result.get('ok') is not False or result.get('syntheticProfileVerified') is not True:
        raise ValueError('No verified synthetic failure observation')
    if type(context.get('attempt')) is not int or context['attempt'] != 1 or type(context.get('run')) is not int or context['run'] <= 0:
        raise ValueError('Original diagnostic run/attempt required')
    if re.fullmatch('[0-9a-f]{40}', context.get('floorpHead', '')) is None or re.fullmatch('[0-9a-f]{32}', context.get('nonce', '')) is None:
        raise ValueError('Diagnostic source/nonce required')
    if context.get('case') not in ['normal', 'workspace-exit'] or context.get('boot') not in [1, 2] or type(context['boot']) is not int:
        raise ValueError('Original diagnostic case/boot required')
    if len(marker_bytes) > 4096 or json.loads(marker_bytes) != {'nonce': context['nonce'], 'case': context['case'], 'head': context['floorpHead']}:
        raise ValueError('Creation-only synthetic marker differs')
    profile = context['profileIdentity']
    if hashlib.sha256(marker_bytes).hexdigest() != profile.get('markerSha256') or profile.get('realpath') != context['expectedSyntheticProfile']:
        raise ValueError('Marked synthetic profile identity differs')
    phase = 'prepared' if context['boot'] == 1 else 'verified'
    if result.get('case') != context['case'] or result.get('nonce') != context['nonce'] or result.get('phase') != phase:
        raise ValueError('Diagnostic case/nonce/phase differs')
    state = result.get('observedState')
    if not isinstance(state, dict):
        raise ValueError('Native failure snapshot unavailable')
    identity = state.get('identity', {})
    if type(identity.get('pid')) is not int or identity['pid'] != context['nativePid'] or identity.get('profile') != profile['realpath']:
        raise ValueError('Diagnostic native profile/PID differs')
    if identity.get('version') != context['runtimeVersion'] or identity.get('buildID') != context['runtimeBuildID']:
        raise ValueError('Diagnostic native Runtime differs')
    tabs = state.get('tabs')
    if not isinstance(tabs, list) or len(tabs) > 256:
        raise ValueError('Bounded tab observations required')
    def nullable_text(value):
        if value is not None and not isinstance(value, str):
            raise ValueError('Native diagnostic text field differs')
        return value
    observed = []
    for tab in tabs:
        if not isinstance(tab, dict) or type(tab.get('hidden')) is not bool or type(tab.get('selected')) is not bool:
            raise ValueError('Native tab visibility/selection missing')
        observed.append({key: nullable_text(tab.get(key)) for key in ['url', 'currentURI', 'workspace']} | {'hidden': tab['hidden'], 'selected': tab['selected']})
    bootstrap = state.get('bootstrapIsolation')
    if bootstrap is not None:
        if not isinstance(bootstrap, dict):
            raise ValueError('Native diagnostic bootstrap differs')
        bootstrap = {key: nullable_text(bootstrap.get(key)) for key in ['runId', 'status', 'startupTest']} | {
            'aborted': bootstrap.get('aborted') if type(bootstrap.get('aborted')) is bool else None,
            **{key: bootstrap.get(key) if type(bootstrap.get(key)) is int and bootstrap[key] >= 0 else None for key in ['resultsCount', 'discoveredCount']},
        }
    diagnostic = {
        'schema': 'qa3-workspace-restart-synthetic-failure-diagnostic-v1',
        'scope': 'ORDINARY_DEVELOPMENT_E2E', 'syntheticProfileOnly': True,
        **{key: context[key] for key in ['floorpHead', 'run', 'attempt', 'case', 'nonce', 'boot']},
        'profileIdentity': {key: profile[key] for key in ['realpath', 'device', 'inode', 'markerSha256']},
        'native': {key: identity[key] for key in ['pid', 'profile', 'version', 'buildID']},
        'expectedBootstrapRunId': nullable_text(context.get('expectedBootstrapRunId')),
        'bootstrapIsolation': bootstrap,
        'bootstrapRunMatches': bootstrap is not None and bootstrap['runId'] == context.get('expectedBootstrapRunId'),
        'observationStage': nullable_text(result.get('observationStage')),
        'originalError': nullable_text(result.get('error')),
        'snapshotError': nullable_text(result.get('diagnosticSnapshotError')),
        'configuredNewTabURL': nullable_text(state.get('configuredNewTabURL')),
        'expectedConfiguredNewTabURL': nullable_text(context.get('expectedConfiguredNewTabURL')),
        'homepage': nullable_text(state.get('homepage')),
        'activeWorkspace': nullable_text(state.get('activeWorkspace')),
        'pendingExit': state.get('pendingExit') if type(state.get('pendingExit')) is bool else None,
        'tabs': observed,
    }
    encoded = (json.dumps(diagnostic, indent=2, ensure_ascii=False) + '\n').encode()
    if len(encoded) > MAX_DIAGNOSTIC:
        raise ValueError('Synthetic diagnostic byte bound')
    return diagnostic, encoded
