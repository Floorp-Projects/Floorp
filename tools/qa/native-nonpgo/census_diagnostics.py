# SPDX-License-Identifier: MPL-2.0
"""Optional process hints; no exclusion, signal permission or writer verdict."""
import os


ROLES = {
    b'python3.12': 'python', b'Python': 'python', b'python3': 'python',
    b'rustc': 'rust-compiler', b'cargo': 'cargo', b'clang': 'clang',
    b'clang++': 'clang', b'ld': 'linker', b'ar': 'archiver',
    b'make': 'make', b'gmake': 'make', b'sh': 'shell', b'bash': 'shell',
    b'cfprefsd': 'preferences-service', b'distnoted': 'notifications-service',
    b'lsd': 'launch-services', b'trustd': 'trust-service',
    b'mdworker': 'metadata-worker', b'mdworker_shared': 'metadata-worker',
    b'nsurlsessiond': 'url-session-service', b'fontd': 'font-service',
    b'iconservicesagent': 'icon-service', b'corespotlightd': 'spotlight-service',
}


def identity(value):
    return (value.pid, value.start_sec, value.start_usec, value.uid, value.ruid)


def describe(row, completion, baseline, observed_births, reader=None):
    """Bind hints twice to the primary census identity; never authorize action."""
    result = {'status': 'unavailable', 'role': None, 'parentRelation': None,
              'observedInStageGroup': 'observed' if row[:3] in observed_births else 'not-observed',
              'ancestryComplete': False, 'writerRoleEstablished': False}
    try:
        if reader is None:
            from darwin_activity import probe
            reader = probe()
        query = {}
        state, value, reason = reader.bsd(row[0], query)
        result['query'] = {key: query[key] for key in ('bytes', 'expectedBytes', 'errno') if key in query}
        if value is not None and identity(value) != row:
            result['status'] = 'identity-changed-after-snapshot'
            return result
        if state == 'gone':
            result['status'] = 'gone-after-snapshot'
            return result
        if state != 'measured' or value is None:
            return result
        role = ROLES.get(bytes(value.name), 'other')
        parent_relation = 'unknown'
        parent_observation = None
        if value.ppid == os.getpid():
            parent_relation = 'caller'
        elif value.ppid == 1:
            parent_relation = 'pid-one'
        else:
            parent = next((item for item in completion if item[0] == value.ppid), None)
            if parent is not None:
                parent_state, parent_value, _ = reader.bsd(parent[0])
                if parent_state == 'measured' and parent_value is not None and identity(parent_value) == parent:
                    parent_relation = 'baseline-candidate' if parent in baseline else 'new-candidate'
                    parent_observation = (
                        'observed' if parent[:3] in observed_births else 'not-observed')
        later_state, later, _ = reader.bsd(row[0])
        if (later_state != 'measured' or later is None or identity(later) != row or
                (later.ppid, later.pgid, bytes(later.name)) != (value.ppid, value.pgid, bytes(value.name))):
            result['status'] = 'changed-during-diagnostic'
            return result
        result.update(status='identity-verified', role=role, parentRelation=parent_relation,
                      credentialRelation='worker-both' if row[3:] == (os.geteuid(), os.getuid()) else 'mixed')
        if parent_observation is not None:
            result['parentObservedInStageGroup'] = parent_observation
    except Exception:
        result['status'] = 'diagnostic-unavailable'
    return result
