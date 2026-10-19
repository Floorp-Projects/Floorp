"""Unintegrated, read-only generated-graph observation; executes no make syntax."""
from collections import deque
import hashlib
import json
import re
import time

MAX_ROOT_BYTES = 4 * 1024**2
MAX_DEPS_BYTES = 16 * 1024**2
MAX_LINE_BYTES = 1024**2
MAX_NODES = 100000
MAX_EDGES = 1000000
MAX_SECONDS = 10
NODES = {'xpcshell': 'js/xpconnect/shell/target', 'XUL': 'toolkit/library/build/target',
         'gkrust': 'toolkit/library/rust/target-objects', 'cocoaObjects': 'widget/cocoa/target-objects',
         'http3server': 'netwerk/test/http3server/target', 'browser': 'browser/app/target'}
TOKEN = re.compile(r'[A-Za-z0-9_.+-]+(?:/[A-Za-z0-9_.+-]+)*\Z')
INSTALL_TARGETS = {'install-dist/' + name for name in ('branding', 'include', 'public', 'private', 'xpi-stage', 'bin')}
INSTALL_TARGETS.add('install-_tests')


class Unknown(Exception):
    pass


def _need(condition, code):
    if not condition:
        raise Unknown(code)


def _token(value):
    return TOKEN.fullmatch(value) is not None and all(part not in ('.', '..') for part in value.split('/'))


def _sha(value):
    return hashlib.sha256(value).hexdigest()


def _canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def _assignment(lines, name):
    prefix = name + ' := '
    matches = [line[len(prefix):] for line in lines if line.startswith(prefix)]
    _need(len(matches) == 1, 'assignment-unconfirmed')
    values = matches[0].split()
    _need(all(_token(value) for value in values) and len(set(values)) == len(values), 'assignment-unconfirmed')
    return set(values)


def _root_statements(lines, deadline):
    """Recognize the fixed generator's statements; never expand make variables."""
    allowed = {'export_dirs', 'libs_dirs', 'misc_dirs', 'tools_dirs', 'check_dirs',
               'pre_compile_targets', 'compile_targets', 'syms_targets', 'rust_targets',
               'non_default_tiers'}
    assignments, includes = {}, 0
    for line in lines:
        _need(time.monotonic() <= deadline, 'processing-limit')
        if not line or line.startswith('#'):
            continue
        if line == 'include root-deps.mk':
            includes += 1
            continue
        match = re.fullmatch(r'([A-Za-z0-9_+-]+) := (.*)', line)
        _need(match is not None, 'unsupported-make')
        name, value = match.groups()
        _need(name not in assignments, 'assignment-unconfirmed')
        values = value.split()
        _need(all(_token(item) for item in values) and len(values) == len(set(values)),
              'assignment-unconfirmed')
        assignments[name] = values
    _need(includes == 1, 'include-unconfirmed')
    categories = assignments.get('non_default_tiers', ())
    _need(all(re.fullmatch(r'[A-Za-z0-9_+-]+', name) is not None for name in categories),
          'assignment-unconfirmed')
    allowed.update(name + '_dirs' for name in categories)
    _need(set(assignments) <= allowed, 'unsupported-make')


def _closure(root, graph, opaque, deadline):
    seen, edges, pending = set(), set(), deque([root])
    complete = True
    while pending:
        _need(time.monotonic() <= deadline, 'processing-limit')
        node = pending.popleft()
        if node in seen:
            continue
        seen.add(node)
        _need(len(seen) <= MAX_NODES, 'node-limit')
        _need(node not in opaque, 'opaque-closure')
        for dependency in graph.get(node, ()):
            edges.add((node, dependency))
            _need(len(edges) <= MAX_EDGES, 'edge-limit')
            pending.append(dependency)
    return seen, edges, complete


def _empty(code):
    return {'schema': 1, 'status': 'unconfirmed', 'reason': code,
            'scope': 'root-deps-mk-literal-edges-only', 'files': None,
            'declaredCompileNodeCount': None, 'declaredCompileNodesSHA256': None,
            'fixedNodesDeclared': dict.fromkeys(NODES), 'fixedReachability': None,
            'xpcshellLiteralClosure': None, 'stockSuffixTierDeclarations': None,
            'suffixGraphFacts': None, 'minimalSufficientSuffixEstablished': False,
            'fullHostQualification': False, 'realBuildPerformed': False,
            'durationToCompletion': 'UNKNOWN', 'makeOrRecipeExecuted': False}


def target_graph_proof(root_mk, root_deps):
    """Caller must first finish the existing guarded restore, reap and group cleanup."""
    try:
        _need(type(root_mk) is bytes and type(root_deps) is bytes, 'input-unconfirmed')
        _need(0 < len(root_mk) <= MAX_ROOT_BYTES and 0 < len(root_deps) <= MAX_DEPS_BYTES, 'size-limit')
        deadline = time.monotonic() + MAX_SECONDS
        # Fixed Makefile.dump emits LF-separated single lines. Continuations are
        # unsupported even in comments/recipes, where they change the next line.
        _need(b'\r' not in root_mk + root_deps and b'\x00' not in root_mk + root_deps,
              'unsupported-make')
        root_lines = root_mk.decode('utf-8').split('\n')
        dep_lines = root_deps.decode('utf-8').split('\n')
        _need(all(len(line.encode()) <= MAX_LINE_BYTES for line in (*root_lines, *dep_lines)), 'line-limit')
        _need(all(not line.endswith('\\') for line in (*root_lines, *dep_lines)), 'line-continuation')
        _root_statements(root_lines, deadline)
        declared = _assignment(root_lines, 'compile_targets')
        _need(all(NODES[name] in declared for name in ('xpcshell', 'XUL', 'gkrust')), 'required-node-unconfirmed')
        tier_dirs = {tier: _assignment(root_lines, tier + '_dirs') for tier in ('misc', 'libs', 'tools')}
        graph, opaque = {}, set()
        recipe_allowed = False
        for line in dep_lines:
            _need(time.monotonic() <= deadline, 'processing-limit')
            if not line or line.startswith('#'):
                continue
            if line.startswith('\t'):
                _need(recipe_allowed, 'unsupported-make')
                continue
            _need(re.match(r'(?:-?include|sinclude|ifeq|ifneq|ifdef|ifndef|else|endif|define|endef|'
                           r'override|export|unexport|vpath|undefine|private)(?:\s|$)', line) is None,
                  'unsupported-make')
            _need(':' in line and not line[0].isspace(), 'unsupported-make')
            left, right = line.split(':', 1)
            targets = left.split()
            _need(targets and all(_token(target) for target in targets) and
                  not right.startswith('=') and ':' not in right and ';' not in right,
                  'unsupported-make')
            dependencies = right.split()
            if targets == ['.PHONY']:
                _need(all(_token(value) for value in dependencies), 'unsupported-make')
                recipe_allowed = False
                continue
            _need(all(not target.startswith('.') or '/' in target for target in targets),
                  'unsupported-make')
            recipe_allowed = True
            valid = [value for value in dependencies if _token(value)]
            if len(valid) != len(dependencies):
                # The fixed generator emits this one opaque prerequisite form.
                # Other expressions can have read-time global effects even in
                # disconnected rules; do not try to classify/evaluate functions.
                _need(len(targets) == 1 and targets[0] in INSTALL_TARGETS and
                      dependencies == ['$(install_manifest_depends)'], 'unsupported-make')
            for target in targets:
                graph.setdefault(target, set()).update(valid)
                if len(valid) != len(dependencies):
                    opaque.add(target)
        _need(len(graph) <= MAX_NODES and sum(map(len, graph.values())) <= MAX_EDGES, 'graph-limit')
        # These are every source closure queried below. Unrelated generated
        # install-* rules may have opaque RHS; a queried closure may not.
        closures = {name: _closure(NODES[name], graph, opaque, deadline) for name in ('xpcshell', 'XUL')}
        x_nodes, x_edges, complete = closures['xpcshell']
        def reachable(source, target):
            found, _, literal_complete = closures[source]
            if NODES[source] not in declared or NODES[target] not in declared:
                return None
            return True if NODES[target] in found else False if literal_complete else None
        suffix_root, _, suffix_complete = _closure('recurse_tools', graph, opaque, deadline)
        app_root, _, app_complete = _closure('browser/app/tools', graph, opaque, deadline)
        def fact(node, found, parsed):
            return True if node in found else False if parsed else None
        result = _empty('literal-graph-observed')
        result.update(status='observed', files={
            'rootMk': {'bytes': len(root_mk), 'sha256': _sha(root_mk)},
            'rootDepsMk': {'bytes': len(root_deps), 'sha256': _sha(root_deps)}},
            declaredCompileNodeCount=len(declared), declaredCompileNodesSHA256=_sha(_canonical(sorted(declared))),
            fixedNodesDeclared={name: node in declared for name, node in NODES.items()},
            fixedReachability={'xpcshellToXUL': reachable('xpcshell', 'XUL'),
                               'XULToGkrust': reachable('XUL', 'gkrust'),
                               'xpcshellToGkrust': reachable('xpcshell', 'gkrust'),
                               'xpcshellToCocoaObjects': reachable('xpcshell', 'cocoaObjects'),
                               'xpcshellToHttp3Server': reachable('xpcshell', 'http3server')},
            xpcshellLiteralClosure={'complete': complete, 'observedNodeCount': len(x_nodes),
                                    'observedEdgeCount': len(x_edges),
                                    'sha256': _sha(_canonical({'nodes': sorted(x_nodes), 'edges': sorted(x_edges)}))},
            stockSuffixTierDeclarations={tier: {'directoryCount': len(values),
                'sha256': _sha(_canonical(sorted(values)))} for tier, values in tier_dirs.items()},
            suffixGraphFacts={'toolsTraversalReachesBrowserApp': fact('browser/app/tools', suffix_root, suffix_complete),
                              'browserAppToolsReachesBrowserBinary': fact(NODES['browser'], app_root, app_complete)})
        return result
    except Unknown as error:
        return _empty(error.args[0])
    except Exception:
        return _empty('parser-unconfirmed')
