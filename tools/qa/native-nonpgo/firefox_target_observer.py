"""Unintegrated fixed-purpose observer after the existing guarded restore."""
from contextlib import ExitStack
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import time

MIB = 1024**2
TOTAL_CAP = 64 * MIB
FIXED = (
 ('summary', 'summary.json', 2*MIB), ('clock', 'started-monotonic', 4096),
 ('context', 'checkpoint-replay-context.json', 2*MIB),
 ('expectedConfig', 'checkpoint-expected-config.json', MIB),
 ('actualConfig', 'effective-config.private.log', 8*MIB),
 ('effectiveConfig', 'effective-config.json', MIB),
 ('rootMk', 'obj/root.mk', 4*MIB), ('rootDepsMk', 'obj/root-deps.mk', 16*MIB),
 ('topMakefile', 'obj/Makefile', 2*MIB), ('configStatus', 'obj/config.status', 4*MIB),
 ('autoconf', 'obj/config/autoconf.mk', 2*MIB),
 ('recursiveBackendInputs', 'obj/backend.RecursiveMakeBackend', 4*MIB),
 ('modulesBackend', 'obj/toolkit/modules/backend.mk', 4*MIB),
 ('XULBackend', 'obj/toolkit/library/build/backend.mk', 2*MIB),
 ('browserBackend', 'obj/browser/app/backend.mk', 2*MIB),
 ('bundleFileListBackend', 'obj/browser/app/macbuild/Contents/backend.mk', MIB),
 ('HTTP3Backend', 'obj/netwerk/test/http3server/backend.mk', 2*MIB),
 ('distBinManifest', 'obj/_build_manifests/install/dist_bin', 4*MIB),
 ('distIncludeManifest', 'obj/_build_manifests/install/dist_include', 4*MIB),
 ('testManifest', 'obj/_build_manifests/install/_tests', 4*MIB),
 ('fasterMakefile', 'obj/faster/Makefile', 4*MIB),
 ('fasterDistBinManifest', 'obj/faster/install_dist_bin', 4*MIB),
 ('appConstants', 'obj/dist/bin/modules/AppConstants.sys.mjs', MIB),
 ('applicationIni', 'obj/dist/bin/application.ini', MIB),
 ('dependentLibs', 'obj/dist/bin/dependentlibs.list', MIB),
 ('bundleFileList', 'obj/browser/app/macbuild/Contents/MacOS-files.txt', MIB),
 ('objConfigHeader', 'obj/mozilla-config.h', MIB),
 ('includeConfigHeader', 'obj/dist/include/mozilla-config.h', MIB))
SUBSTS = ('MOZ_DEBUG', 'ENABLE_TESTS', 'MOZ_PROFILE_GENERATE', 'MOZ_PROFILE_USE', 'MOZ_PGO', 'MOZ_LTO', 'TARGET_CPU', 'HOST_CPU_ARCH', 'OS_ARCH', 'OS_TARGET', 'CC_TYPE', 'CC_VERSION', 'MOZ_APP_VERSION', 'CC', 'CXX', 'HOST_CC', 'HOST_CXX', 'RUSTC', 'CARGO', 'NODEJS', 'MACOS_SDK_DIR', 'WASI_SYSROOT', 'ONNX_RUNTIME', 'MOZ_SANDBOX', 'HOST_CC_TYPE', 'HOST_CC_VERSION', 'RUSTC_VERSION', 'MOZ_MACBUNDLE_NAME', 'MOZ_PACKAGER_FORMAT', 'MOZ_APP_DISPLAYNAME', 'MOZ_CHILD_PROCESS_NAME', 'MOZ_CHILD_PROCESS_BUNDLENAME', 'MOZ_EME_PROCESS_NAME_BRANDED', 'MOZ_EME_PROCESS_BUNDLENAME', 'MOZ_GPU_PROCESS_NAME_BRANDED', 'MOZ_GPU_PROCESS_BUNDLENAME', 'MOZ_PKCS11_PROCESS_NAME_BRANDED', 'MOZ_PKCS11_PROCESS_BUNDLENAME', 'RUST_TARGET', 'MOZ_DEBUG_RUST', 'MOZ_CARGO_DEFAULT_PROFILE_DIR')
CORE = ('summary', 'clock', 'context', 'expectedConfig', 'actualConfig', 'effectiveConfig',
        'rootMk', 'rootDepsMk', 'topMakefile', 'configStatus', 'autoconf')
REASONS = ('guard-precondition', 'root-binding', 'directory-binding', 'file-binding',
           'file-kind', 'missing', 'size-limit', 'total-limit', 'processing-limit',
           'clock-unconfirmed', 'json-unconfirmed', 'restore-unconfirmed', 'config-unconfirmed',
           'permission', 'io-unavailable', 'bundle-unconfirmed', 'graph-unconfirmed')


class Unknown(Exception):
    pass


def need(condition, code):
    if not condition:
        raise Unknown(code)


def identity(value, full=True):
    fields = ('st_dev', 'st_ino', 'st_uid', 'st_mode', 'st_nlink')
    if full:
        fields += ('st_size', 'st_mtime_ns', 'st_ctime_ns')
    return tuple(getattr(value, key) for key in fields)


class _FixedRead:
    def __init__(self, task, stack, deadline):
        path = Path(task)
        need(path.is_absolute() and '..' not in path.parts and path.resolve() == path, 'root-binding')
        self.task, self.stack, self.deadline = path, stack, deadline
        self.total, self.entries, self.directories = 0, [], []
        descriptor = self._dir('/', None, False)
        for part in path.parts[1:]:
            descriptor = self._dir(part, descriptor, False)
        need(os.fstat(descriptor).st_uid == os.geteuid(), 'root-binding')
        self.root = descriptor
        self.root_identity = identity(os.fstat(descriptor))
        self.directories.append((descriptor, identity(os.fstat(descriptor)), True, 'root-binding'))

    def budget(self):
        need(time.monotonic() <= self.deadline, 'processing-limit')

    def _dir(self, name, parent, owned):
        before = os.stat(name, dir_fd=parent, follow_symlinks=False)
        self.entries.append((parent, name, identity(before, owned), owned))
        need(stat.S_ISDIR(before.st_mode) and (not owned or before.st_uid == os.geteuid()), 'directory-binding')
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | getattr(os, 'O_CLOEXEC', 0)
        descriptor = os.open(name, flags, dir_fd=parent)
        self.stack.callback(os.close, descriptor)
        need(identity(before) == identity(os.fstat(descriptor)), 'directory-binding')
        self.directories.append((descriptor, identity(before, owned), owned, 'directory-binding'))
        return descriptor

    def read(self, relative, cap):
        self.budget()
        parent = self.root
        parts = Path(relative).parts
        need(parts and not Path(relative).is_absolute() and all(part not in ('.', '..') for part in parts), 'root-binding')
        try:
            for part in parts[:-1]:
                try:
                    parent = self._dir(part, parent, True)
                except FileNotFoundError:
                    self.entries.append((parent, part, None, True))
                    raise
            name = parts[-1]
            try:
                before = os.stat(name, dir_fd=parent, follow_symlinks=False)
            except FileNotFoundError:
                self.entries.append((parent, name, None, True))
                raise
            self.entries.append((parent, name, identity(before), True))
            need(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid() and before.st_nlink == 1, 'file-kind')
            need(0 <= before.st_size <= cap, 'size-limit')
            flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | getattr(os, 'O_CLOEXEC', 0)
            descriptor = os.open(name, flags, dir_fd=parent)
            self.stack.callback(os.close, descriptor)
            need(identity(before) == identity(os.fstat(descriptor)), 'file-binding')
            self.directories.append((descriptor, identity(before), True, 'file-binding'))
            chunks, size = [], 0
            while True:
                self.budget()
                chunk = os.read(descriptor, min(128*1024, cap + 1 - size, TOTAL_CAP + 1 - self.total))
                if not chunk:
                    break
                size += len(chunk)
                self.total += len(chunk)
                need(size <= cap, 'size-limit')
                need(self.total <= TOTAL_CAP, 'total-limit')
                chunks.append(chunk)
            need(size == before.st_size and identity(before) == identity(os.fstat(descriptor)) ==
                 identity(os.stat(name, dir_fd=parent, follow_symlinks=False)), 'file-binding')
            raw = b''.join(chunks)
            return raw, {'status': 'observed', 'reason': None, 'bytes': size,
                         'sha256': hashlib.sha256(raw).hexdigest(), 'lineCount': raw.count(b'\n') + int(bool(raw) and not raw.endswith(b'\n'))}
        except FileNotFoundError:
            code = 'missing'
        except Unknown as error:
            code = error.args[0]
        except PermissionError:
            code = 'permission'
        except OSError:
            code = 'io-unavailable'
        return None, {'status': 'unconfirmed', 'reason': code, 'bytes': None,
                      'sha256': None, 'lineCount': None}

    def verify(self):
        self.budget()
        need(self.task.resolve() == self.task and
             identity(os.stat(self.task, follow_symlinks=False)) == self.root_identity,
             'root-binding')
        for descriptor, expected, full, code in self.directories:
            need(identity(os.fstat(descriptor), full) == expected, code)
        for parent, name, expected, full in self.entries:
            try:
                actual = os.stat(name, dir_fd=parent, follow_symlinks=False)
            except FileNotFoundError:
                need(expected is None, 'file-binding')
            else:
                need(expected is not None and identity(actual, full) == expected, 'file-binding')


def decode(raw):
    need(type(raw) is bytes, 'json-unconfirmed')
    def pairs(rows):
        result = {}
        for key, value in rows:
            need(key not in result, 'json-unconfirmed')
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=pairs,
                      parse_constant=lambda _: (_ for _ in ()).throw(Unknown('json-unconfirmed')))


def _zero(value):
    return type(value) is int and value == 0


def _restore_consistent(summary, context):
    need(type(summary) is dict and type(context) is dict, 'restore-unconfirmed')
    replay = summary.get('checkpointRestore')
    source = summary.get('source')
    need(type(replay) is dict and type(source) is dict, 'restore-unconfirmed')
    need(summary.get('status') == 'restore-ready' and summary.get('restoreOnly') is True and
         summary.get('resumedEffectiveConfigVerified') is True and
         'firstFailure' in summary and summary['firstFailure'] is None and
         'monitorFailure' in summary and summary['monitorFailure'] is None and
         summary.get('native') is None and summary.get('fullHostXUL') == 'NOT_TESTED' and
         summary.get('nativeAuthentication') == 'NOT_TESTED' and
         summary.get('planSHA256') == '33fb00c07a7ad9f66950489f06c0b5039ee173bc78297bc13b39265b24f4d460' and
         source.get('runtime') == '7dd619742b25567e479eeda97ae071d10542ad0b' and
         replay.get('qualification') == 'verified-origin-cross-run-checkpoint-replay-v1' and
         replay.get('archiveSHA256') == 'f548c7f86f6a05dc23b2b3c9764cec3d4d1d1e7d01014b2c56d5d60aac84ed15' and
         replay.get('manifestSHA256') == 'ec4581e1d23861581b446b4d826c3812df9cc21bc9224fb682f16120690d8644' and
         replay.get('buildDate') == '20261019035607' and
         context.get('contract') == 'verified-origin-cross-run-checkpoint-replay-v1' and
         context.get('status') == 'passed' and context.get('keysMatch') is True and
         type(context.get('keyMatches')) is dict and bool(context['keyMatches']) and
         all(item is True for item in context['keyMatches'].values()), 'restore-unconfirmed')
    rows = summary.get('stages')
    need(type(rows) is list and bool(rows) and all(type(row) is dict for row in rows), 'restore-unconfirmed')
    names = [row.get('stage') for row in rows]
    need(all(type(name) is str for name in names) and len(names) == len(set(names)) and
         names.count('checkpoint-restore') == 1 and names.count('effective-config') == 1 and
         not {'full-build', 'stage-package', 'native-suite'}.intersection(names), 'restore-unconfirmed')
    for row in rows:
        leader = row.get('leaderReap')
        need(_zero(row.get('exitCode')) and row.get('groupCleanup') == 'verified-gone' and
             all(key in row and row[key] is None for key in ('reason', 'failurePhase', 'cleanupFailure')) and
             type(leader) is dict and leader.get('status') == 'reaped' and _zero(leader.get('exitCode')),
             'restore-unconfirmed')


def _bundle_fields(raw, task, bundle):
    value = decode(raw)
    need(type(value) is dict, 'bundle-unconfirmed')
    def count(key):
        item = value.get(key)
        return len(item) if type(item) is list else None
    return {'configuredBundleBinding': value['bundle'] == str(task/'obj/dist'/bundle) if type(value.get('bundle')) is str else None,
            'ownedStageBinding': value['stage'] == str(task/'obj/dist/bin') if type(value.get('stage')) is str else None,
            **{key+'Count': count(key) for key in ('binaries', 'extra_files', 'moves', 'copies')}}


def _same_json(left, right, depth=0):
    """JSON type identity matters: bool is not int, nor int float."""
    if depth > 64 or type(left) is not type(right):
        return False
    if type(left) is dict:
        return set(left) == set(right) and all(_same_json(left[key], right[key], depth+1) for key in left)
    if type(left) is list:
        return len(left) == len(right) and all(_same_json(a, b, depth+1) for a, b in zip(left, right))
    if type(left) is float:
        return math.isfinite(left) and math.isfinite(right) and left == right
    return (left is None or type(left) in (bool, int, str)) and left == right


def _config_consistency(expected, actual, effective):
    """Narrow retained-data checks, never the existing full configuration guard."""
    need(type(expected) is dict and type(expected.get('substs')) is dict and
         set(expected['substs']) == set(SUBSTS) and type(actual) is dict and
         type(actual.get('substs')) is dict and type(effective) is dict, 'config-unconfirmed')
    before, after = expected['substs'], actual['substs']
    complete = True
    for key in SUBSTS:
        if key not in after:
            # The legacy selection records absent optional substitutions as None.
            # That is unknown consistency here, never an equality proof.
            need(before[key] is None, 'config-unconfirmed')
            complete = False
        else:
            need(_same_json(before[key], after[key]), 'config-unconfirmed')
    def enabled(value):
        return value is True or (type(value) is int and value == 1) or (type(value) is str and value == '1')
    def disabled(value):
        return value is None or value is False or (type(value) is int and value == 0) or (type(value) is str and value == '')
    for key in ('MOZ_DEBUG', 'ENABLE_TESTS', 'MOZ_SANDBOX'):
        need(key in after and enabled(after[key]), 'config-unconfirmed')
    # Absence is the upstream non-enabled representation for these optional
    # substitutions; effective pgo=False is still required below. No absent
    # substitution is counted as a selected-value consistency match.
    for key in ('MOZ_PROFILE_GENERATE', 'MOZ_PROFILE_USE', 'MOZ_PGO', 'MOZ_LTO'):
        need(key not in after or disabled(after[key]), 'config-unconfirmed')
    profile = {'TARGET_CPU': 'x86_64', 'HOST_CPU_ARCH': 'x86_64', 'OS_ARCH': 'Darwin',
               'OS_TARGET': 'Darwin', 'CC_TYPE': 'clang', 'CC_VERSION': '22.1.8',
               'MOZ_APP_VERSION': '158.0.1'}
    for key, value in profile.items():
        need(type(after.get(key)) is str and after[key] == value, 'config-unconfirmed')
    need(type(after.get('MOZ_WIDGET_TOOLKIT')) is str and after['MOZ_WIDGET_TOOLKIT'] == 'cocoa',
         'config-unconfirmed')
    need(effective.get('debug') is True and effective.get('pgo') is False and
         effective.get('artifact') is False and effective.get('compiledSandbox') is True,
         'config-unconfirmed')
    for key in (*profile, 'MOZ_DEBUG', 'ENABLE_TESTS', 'MOZ_MACBUNDLE_NAME'):
        need(key in after and key in effective and _same_json(after[key], effective[key]),
             'config-unconfirmed')
    return complete



def _graph_safe(value):
    fields = {'schema','status','reason','scope','files','declaredCompileNodeCount','declaredCompileNodesSHA256',
              'fixedNodesDeclared','fixedReachability','xpcshellLiteralClosure','stockSuffixTierDeclarations',
              'suffixGraphFacts','minimalSufficientSuffixEstablished','fullHostQualification','realBuildPerformed',
              'durationToCompletion','makeOrRecipeExecuted'}
    need(type(value) is dict and set(value) == fields and type(value.get('schema')) is int and value['schema'] == 1 and
         value.get('status') in ('observed','unconfirmed') and value.get('scope') == 'root-deps-mk-literal-edges-only' and
         all(value.get(key) is False for key in ('minimalSufficientSuffixEstablished','fullHostQualification',
                                               'realBuildPerformed','makeOrRecipeExecuted')), 'graph-unconfirmed')
    keys = fields | {'rootMk','rootDepsMk','bytes','sha256','xpcshell','XUL','gkrust','cocoaObjects','http3server','browser',
        'xpcshellToXUL','XULToGkrust','xpcshellToGkrust','xpcshellToCocoaObjects','xpcshellToHttp3Server',
        'complete','observedNodeCount','observedEdgeCount','misc','libs','tools','directoryCount',
        'toolsTraversalReachesBrowserApp','browserAppToolsReachesBrowserBinary'}
    strings = {'observed','unconfirmed','root-deps-mk-literal-edges-only','UNKNOWN','input-unconfirmed',
        'size-limit','line-limit','include-unconfirmed','assignment-unconfirmed','required-node-unconfirmed',
        'processing-limit','node-limit','edge-limit','graph-limit','literal-graph-observed','parser-unconfirmed',
        'unsupported-make','line-continuation','opaque-closure'}
    def check(item, depth=0):
        need(depth <= 8, 'graph-unconfirmed')
        if type(item) is dict:
            need(set(item) <= keys, 'graph-unconfirmed')
            for val in item.values():check(val, depth+1)
        elif type(item) is str:
            need(item in strings or re.fullmatch('[0-9a-f]{64}', item) is not None, 'graph-unconfirmed')
        else:
            need(item is None or type(item) is bool or (type(item) is int and 0 <= item <= 2**31), 'graph-unconfirmed')
    check(value)


def observe_restored_task(task, graph_proof, *, restore_guards_succeeded=False):
    """Call only after the existing restore stage naturally exits, is reaped and its group is gone."""
    result = {'schema': 1, 'status': 'unconfirmed', 'reason': None, 'failurePhase': None, 'files': {}, 'graph': None,
              'restoreReceiptConsistent': None, 'restoredConfigStillMatches': None, 'configConsistency': 'unconfirmed',
              'bindingsStable': None, 'bindingScope': 'opened-fds-and-observed-fixed-entries-only', 'bundleSpec': None, 'existingGuardsReexecuted': False,
              'cleanupFailure': None, 'processingLimitSeconds': 30, 'absoluteRestoreDeadlineMinutes': 60,
              'clockResetAllowed': False, 'makeOrRecipeExecuted': False,
              'minimalSufficientSuffixEstablished': False, 'fullHostQualification': False,
              'realBuildPerformed': False, 'durationToCompletion': 'UNKNOWN'}
    phase = 'guard'
    stack = ExitStack()
    try:
        need(restore_guards_succeeded is True, 'guard-precondition')
        session = _FixedRead(task, stack, time.monotonic()+30)
        data = {}
        phase = 'read'
        for label, relative, cap in FIXED:
            raw, receipt = session.read(relative, cap)
            result['files'][label] = receipt
            data[label] = raw
            if label == 'clock':
                need(raw is not None and len(raw) <= 4096, 'clock-unconfirmed')
                origin = float(raw)
                now = time.monotonic()
                need(math.isfinite(origin) and now-3600 <= origin <= now, 'clock-unconfirmed')
                session.deadline = min(session.deadline, origin + 3600)
        need(all(data[label] is not None for label in CORE), 'restore-unconfirmed')
        phase = 'restore-consistency'
        _restore_consistent(decode(data['summary']), decode(data['context']))
        result['restoreReceiptConsistent'] = True
        phase = 'config-consistency'
        expected, actual = decode(data['expectedConfig']), decode(data['actualConfig'])
        effective = decode(data['effectiveConfig'])
        if _config_consistency(expected, actual, effective):
            result.update(restoredConfigStillMatches=True, configConsistency='matched')
        phase = 'bundle'
        bundle = effective.get('MOZ_MACBUNDLE_NAME')
        need(type(bundle) is str and re.fullmatch(r'[A-Za-z0-9 _.-]+\.app', bundle) is not None and
             '..' not in bundle, 'bundle-unconfirmed')
        relative = 'obj/browser/app/dist_' + bundle.replace(' ', '') + '.bundle.json'
        raw, receipt = session.read(relative, 1024**2)
        result['files']['bundleSpec'] = receipt
        if raw is not None:
            result['bundleSpec'] = _bundle_fields(raw, session.task, bundle)
        phase = 'graph'
        graph = graph_proof(data['rootMk'], data['rootDepsMk'])
        _graph_safe(graph)
        result['graph'] = graph if graph['status'] == 'observed' else None
        phase = 'binding'
        session.verify()
        result['bindingsStable'] = True
        if result['graph'] is not None:
            result['status'] = 'observed'
        else:
            result.update(reason='graph-unconfirmed', failurePhase='graph')
    except Unknown as error:
        result['status'] = 'unconfirmed'
        result['reason'] = error.args[0] if error.args[0] in REASONS else 'io-unavailable'
        result['failurePhase'] = phase
        if phase == 'binding':
            result.update(graph=None, restoreReceiptConsistent=None, restoredConfigStillMatches=None,
                          configConsistency='unconfirmed')
            for row in result['files'].values():
                row.update(status='unconfirmed', reason=result['reason'], bytes=None, sha256=None, lineCount=None)
    except PermissionError:
        result.update(status='unconfirmed', reason='permission', failurePhase=phase)
    except Exception:
        result.update(status='unconfirmed', reason='io-unavailable', failurePhase=phase)
    finally:
        try:
            stack.close()
        except BaseException as error:
            result['cleanupFailure'] = {'operation': 'close-owned-fds',
                'kind': 'os-error' if isinstance(error, OSError) else 'exception',
                'errno': error.errno if isinstance(error, OSError) and type(error.errno) is int else None}
            result['status'] = 'unconfirmed'
            if result['reason'] is None:
                result.update(reason='io-unavailable', failurePhase='cleanup')
    if result['failurePhase'] == 'binding' or result['cleanupFailure'] is not None:
        result.update(graph=None, bindingsStable=None, restoreReceiptConsistent=None, restoredConfigStillMatches=None,
                      configConsistency='unconfirmed')
        for row in result['files'].values():
            row.update(status='unconfirmed', reason=result['reason'], bytes=None, sha256=None, lineCount=None)
    return result
