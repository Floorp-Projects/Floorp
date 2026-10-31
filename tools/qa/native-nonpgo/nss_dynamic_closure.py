# SPDX-License-Identifier: MPL-2.0
"""Read-only NSS deployment diagnostics; never grants native qualification."""
from contextlib import ExitStack
from pathlib import Path
import re
import struct
import time

from firefox_target_graph import (_assignment, _closure, _token, NODES,
                                  target_graph_proof, Unknown as GraphUnknown)
from firefox_target_observer import _FixedRead, Unknown
from mozilla_target_build import GRAPH_PINS


CONTRACT = 'nss-dynamic-closure-observation-v1'
LIBRARIES = ('libnss3.dylib', 'libsoftokn3.dylib', 'libfreebl3.dylib')
DYNAMIC_TARGETS = {
    'softokn3': 'security/nss/lib/softoken/softoken_softokn3/target',
    'freebl3': 'security/nss/lib/freebl/freebl_freebl3/target',
}


def macho_identity(raw):
    if type(raw) is not bytes or len(raw) < 32:
        return None
    magic, cpu, _, kind, count, size, _, _ = struct.unpack_from('<8I', raw)
    if (magic != 0xfeedfacf or cpu != 0x01000007 or kind != 6 or
            not 0 < count <= 4096 or size > len(raw) - 32):
        return None
    offset, end, uuid = 32, 32 + size, None
    for _ in range(count):
        if offset + 8 > end:
            return None
        command, length = struct.unpack_from('<2I', raw, offset)
        if length < 8 or length % 8 or offset + length > end:
            return None
        if command == 0x1b:
            if length != 24 or uuid is not None:
                return None
            uuid = raw[offset + 8:offset + 24].hex()
        offset += length
    if offset != end or uuid is None:
        return None
    return {'cpu': 'x86_64', 'filetype': kind, 'uuid': uuid}


def graph_facts(root, deps):
    parsed = target_graph_proof(root, deps)
    if parsed['status'] != 'observed':
        return {'status': 'unconfirmed', 'reason': parsed['reason']}
    try:
        declared = _assignment(root.decode().split('\n'), 'compile_targets')
        graph, opaque = {}, set()
        for line in deps.decode().split('\n'):
            if not line or line.startswith(('#', '\t')):
                continue
            left, right = line.split(':', 1)
            if left == '.PHONY':
                continue
            values = right.split()
            for name in left.split():
                graph.setdefault(name, set()).update(value for value in values if _token(value))
                if any(not _token(value) for value in values):
                    opaque.add(name)
        found, _, _ = _closure(NODES['xpcshell'], graph, opaque, time.monotonic() + 10)
        return {'status': 'observed', 'reason': None,
                'scope': 'literal-edges-only',
                'targetsDeclared': {key: node in declared for key, node in DYNAMIC_TARGETS.items()},
                'shellReachesTargets': {key: node in found for key, node in DYNAMIC_TARGETS.items()}}
    except (GraphUnknown, UnicodeError, ValueError):
        return {'status': 'unconfirmed', 'reason': 'graph-unconfirmed'}


def observe(task, bundle):
    result = {'contract': CONTRACT, 'status': 'unconfirmed', 'reason': None,
              'files': {}, 'graph': None, 'graphPinsMatch': False,
              'bindingsStable': False, 'allLibrariesObserved': False,
              'bundleMatchesDist': None, 'realLoadPerformed': False,
              'buildPerformed': False, 'productQualification': False,
              'native42ResultQualification': False}
    if (type(bundle) is not str or re.fullmatch(r'[A-Za-z0-9 _.-]+\.app', bundle) is None or
            '..' in bundle):
        result['reason'] = 'bundle-unconfirmed'
        return result
    try:
        with ExitStack() as stack:
            reader = _FixedRead(Path(task), stack, time.monotonic() + 30)
            data = {}
            for label in ('rootMk', 'rootDepsMk'):
                relative, cap, _ = GRAPH_PINS[label]
                data[label], result['files'][label] = reader.read(relative, cap)
            result['graphPinsMatch'] = all(result['files'][key]['sha256'] == GRAPH_PINS[key][2]
                                           for key in data)
            if all(raw is not None for raw in data.values()):
                result['graph'] = graph_facts(data['rootMk'], data['rootDepsMk'])
            for location, prefix in (('dist', 'obj/dist/bin'),
                                     ('bundle', 'obj/dist/' + bundle + '/Contents/MacOS')):
                for library in LIBRARIES:
                    raw, receipt = reader.read(prefix + '/' + library, 16 * 1024**2)
                    receipt['macho'] = macho_identity(raw) if raw is not None else None
                    result['files'][location + ':' + library] = receipt
            reader.verify()
            result['bindingsStable'] = True
        records = [result['files'][location + ':' + name]
                   for location in ('dist', 'bundle') for name in LIBRARIES]
        result['allLibrariesObserved'] = all(row['status'] == 'observed' and row['macho'] is not None
                                             for row in records)
        if result['allLibrariesObserved']:
            result['bundleMatchesDist'] = all(
                result['files']['dist:' + name]['sha256'] == result['files']['bundle:' + name]['sha256']
                for name in LIBRARIES)
        result['status'] = 'observed'
    except Unknown as error:
        result['reason'] = error.args[0]
    except (OSError, ValueError, TypeError):
        result['reason'] = 'io-unavailable'
    return result
