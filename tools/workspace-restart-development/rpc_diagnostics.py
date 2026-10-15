"""Host-only failure metadata; never query native state while diagnosing RPC."""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import stat

from protocol import COMMANDS, MAX_FRAME, NATIVE_ERROR_CODES, QA_SYMBOLS

MAX_RPC_DIAGNOSTIC = 16 * 1024
STAGES = {'listener-startup', 'initialization', 'phase', 'quit'}
IO_STAGES = {'connect', 'handshake', 'write', 'reply', 'native-error', 'phase-result'}
EXCEPTION_TYPES = {'ConnectionRefusedError', 'ConnectionResetError', 'BrokenPipeError',
                   'TimeoutError', 'EOFError', 'ValueError', 'TypeError', 'OSError',
                   'RuntimeError', 'UnicodeDecodeError', 'JSONDecodeError',
                   'InterruptedError', 'KeyboardInterrupt', 'SystemExit'}
PROCESS_STATES = {'VERIFIED_LIVE_NATIVE', 'OWNED_NATIVE_EXITED', 'NO_OWNED_NATIVE',
                  'OWNERSHIP_REJECTED', 'PROBE_UNAVAILABLE'}


def exception_type(error):
    try:
        name = type(error).__name__
        return name if name in EXCEPTION_TYPES else 'OtherException'
    except BaseException: return 'OtherException'


def safe_errno(error):
    try:
        value = getattr(error, 'errno', None)
        return value if type(value) is int and 0 <= value <= 4095 else None
    except BaseException: return None


def failure_text(error, observation=None):
    """The same live exception is retained; its arbitrary text is not persisted."""
    try:
        if observation is not None:
            value = safe_rpc_failure(error, False, observation)
            text = 'Original RPC failure: ' + exception_type(error) + ' [' + value['stage'] + ']'
            if value['command'] is not None: text += ' ' + value['command']
            errno = safe_errno(error)
            if errno is not None: text += ' errno=' + str(errno)
            return text
        # Non-RPC controller failures retain their original text when available.
        return str(error)
    except BaseException: return 'Original failure: UNAVAILABLE (' + exception_type(error) + ')'


def verify_profile(profile, expected, context):
    """Use one no-follow directory descriptor and read only its tiny QA marker."""
    if type(context.get('run')) is not int or context['run'] <= 0 or context.get('attempt') != 1 or type(context['attempt']) is not int:
        raise ValueError('Original RPC diagnostic run/attempt required')
    if context.get('case') not in ['normal', 'workspace-exit'] or type(context.get('boot')) is not int or context['boot'] not in [1, 2]:
        raise ValueError('Original RPC diagnostic case/boot required')
    if re.fullmatch('[0-9a-f]{40}', context.get('floorpHead', '')) is None or re.fullmatch('[0-9a-f]{32}', context.get('nonce', '')) is None:
        raise ValueError('Original RPC diagnostic source/nonce required')
    if str(profile.resolve()) != expected.get('realpath') or len(expected['realpath'].encode()) > 4096:
        raise ValueError('Owned RPC profile path differs')
    fd = os.open(profile, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        row = os.fstat(fd)
        if row.st_dev != expected.get('device') or row.st_ino != expected.get('inode'):
            raise ValueError('Owned RPC profile directory changed')
        marker = os.open('qa3-restart-profile.json', os.O_RDONLY | os.O_NOFOLLOW, dir_fd=fd)
        try:
            before = os.fstat(marker)
            if not stat.S_ISREG(before.st_mode) or before.st_size > 4096:
                raise ValueError('Owned bounded RPC marker required')
            data = os.read(marker, 4097)
            after = os.fstat(marker)
            if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
                raise ValueError('RPC marker changed during read')
            if len(data) > 4096 or hashlib.sha256(data).hexdigest() != expected.get('markerSha256'):
                raise ValueError('Owned RPC marker hash differs')
            if json.loads(data) != {'nonce': context['nonce'], 'case': context['case'], 'head': context['floorpHead']}:
                raise ValueError('Owned RPC marker source/case/nonce differs')
        finally:
            os.close(marker)
        current = profile.stat(follow_symlinks=False)
        if not stat.S_ISDIR(current.st_mode) or (current.st_dev, current.st_ino) != (row.st_dev, row.st_ino):
            raise ValueError('Owned RPC profile was replaced')
    finally:
        os.close(fd)


def safe_rpc_failure(error, native_owner_confirmed, observation=None):
    value = observation.get('rpc') if type(observation) is dict and observation.get('error') is error else None
    if type(value) is not dict: value = {}
    number = lambda field: value[field] if type(value.get(field)) is int and 0 <= value[field] <= MAX_FRAME else None
    result = {'stage': value.get('stage') if isinstance(value.get('stage'), str) and value['stage'] in IO_STAGES else 'unavailable',
              'command': value.get('command') if isinstance(value.get('command'), str) and value['command'] in COMMANDS else None,
              'requestId': number('requestId'), 'bytesSent': number('bytesSent'),
              'receivedBytes': number('receivedBytes'), 'frameBytes': number('frameBytes'),
              'responseCorrelated': value.get('responseCorrelated') if type(value.get('responseCorrelated')) is bool else None,
              'nativeError': None}
    closed = value.get('secondaryCloseExceptionType')
    result['secondaryCloseExceptionType'] = closed if isinstance(closed, str) and closed in {'OSError', 'ValueError', 'TypeError', 'RuntimeError', 'OtherException'} else None
    native = value.get('nativeError')
    if native_owner_confirmed and result['responseCorrelated'] is True and isinstance(native, dict):
        code = native.get('code')
        detail = native.get('detail')
        allowed_details = {'ReferenceError: ' + symbol + ' is not defined' for symbol in QA_SYMBOLS}
        accepted_detail = result['command'] == 'WebDriver:ExecuteAsyncScript' and code == 'javascript error' and isinstance(detail, str) and detail in allowed_details
        result['nativeError'] = {'code': code if isinstance(code, str) and code in NATIVE_ERROR_CODES else 'UNRECOGNIZED',
                                 'detail': detail if accepted_detail else None,
                                 'detailWithheld': not accepted_detail}
    return result


class RpcFailureScope:
    def __init__(self, profile, output, record, receipt, number, artifact, process_probe):
        self.profile = Path(profile); self.output = Path(output); self.record = record
        self.receipt = receipt; self.failure_observation = None
        self.identity = dict(record['profileIdentity']); self.probe = process_probe
        self.context = {**{key: receipt[key] for key in ['floorpHead', 'run', 'attempt']},
                        'case': record['case'], 'nonce': record['nonce'], 'boot': number}
        self.runtime = {'version': artifact['version'], 'buildID': artifact['buildId']}
        if not isinstance(self.runtime['version'], str) or re.fullmatch('[0-9.]{1,32}', self.runtime['version']) is None or not isinstance(self.runtime['buildID'], str) or re.fullmatch('[0-9]{14}', self.runtime['buildID']) is None:
            raise ValueError('Bounded expected RPC Runtime identity required')

    def process(self):
        value = self.probe()
        if not isinstance(value, dict) or value.get('status') not in PROCESS_STATES:
            raise ValueError('Bounded RPC process ownership result required')
        result = {'status': value['status'], 'root': None, 'portOwnerConfirmed': False}
        root = value.get('root')
        if result['status'] == 'VERIFIED_LIVE_NATIVE':
            if not isinstance(root, dict) or type(root.get('pid')) is not int or root['pid'] <= 0 or root.get('session') != root['pid'] or type(root.get('session')) is not int or not isinstance(root.get('start'), str) or not root['start'].isdigit() or len(root['start']) > 32:
                raise ValueError('Owned native PID/start/session required')
            if root.get('profile') != self.identity['realpath']:
                raise ValueError('Owned native RPC profile differs')
            result['root'] = {key: root[key] for key in ['pid', 'start', 'session']}
            result['portOwnerConfirmed'] = value.get('portOwnerConfirmed') is True
        return result

    def save(self, stage, error):
        if stage not in STAGES:
            raise ValueError('Known RPC diagnostic operation required')
        verify_profile(self.profile, self.identity, self.context)
        before = self.process()
        after = self.process()
        if before != after:
            process = {'status': 'OWNERSHIP_REJECTED', 'root': None, 'portOwnerConfirmed': False}
        else:
            process = before
        verify_profile(self.profile, self.identity, self.context)
        diagnostic = {'schema': 'qa3-workspace-restart-owned-rpc-failure-v1',
                      'scope': 'ORDINARY_DEVELOPMENT_E2E', 'status': 'FAIL_DIAGNOSTIC_ONLY',
                      'syntheticProfileOnly': True, **self.context,
                      'profileIdentity': {key: self.identity[key] for key in ['realpath', 'device', 'inode', 'markerSha256']},
                      'operation': stage, 'originalExceptionType': exception_type(error),
                      'originalErrno': safe_errno(error),
                      'processOwnership': process, 'runtimeExpected': self.runtime,
                      'ownershipProofKind': 'HOST_MARKER_AND_PROCESS_ONLY', 'nativeProfDVerifiedByDiagnostic': False,
                      'rpc': safe_rpc_failure(error, process['portOwnerConfirmed'], self.failure_observation),
                      'rpcMetadataAvailability': 'AVAILABLE' if type(self.failure_observation) is dict and type(self.failure_observation.get('rpc')) is dict else 'UNAVAILABLE',
                      'nativeSnapshotCollected': False, 'nativeSnapshotAvailability': 'UNAVAILABLE_HOST_DIAGNOSTIC_ONLY',
                      'arbitraryErrorTextWithheld': True, 'retryPerformed': False,
                      'nativeProductQualification': 'UNVERIFIED', 'compileProfileQualification': 'UNVERIFIED',
                      'publicationAuthorized': False}
        encoded = (json.dumps(diagnostic, indent=2) + '\n').encode()
        if len(encoded) > MAX_RPC_DIAGNOSTIC:
            raise ValueError('Owned RPC diagnostic byte bound')
        name = self.context['case'] + '-boot' + str(self.context['boot']) + '-rpc-failure-diagnostic.json'
        fd = os.open(self.output, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            target = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=fd)
            with os.fdopen(target, 'wb') as stream:
                stream.write(encoded)
        finally:
            os.close(fd)
        self.record['rpcFailureDiagnosticEvidenceFile'] = name

    def check(self, stage):
        verify_profile(self.profile, self.identity, self.context)
        if stage != 'listener-startup':
            process = self.process()
            if process['status'] != 'VERIFIED_LIVE_NATIVE' or process['portOwnerConfirmed'] is not True:
                raise ValueError('Owned native RPC listener is no longer confirmed')

    def record_failure(self, stage, error):
        try:
            if type(self.failure_observation) is not dict or self.failure_observation.get('error') is not error:
                self.failure_observation = {'error': error, 'rpc': None}
            self.receipt['_rpcFailureRecord'] = self.failure_observation
            self.save(stage, error)
        except BaseException as diagnostic_error:
            self.record['rpcDiagnosticError'] = 'RPC diagnostic unavailable: ' + exception_type(diagnostic_error)

    def note_rpc_failure(self, error, value):
        # Caller-owned state; no getter/setter/repr/str is invoked on the error.
        observation = {'error': error, 'rpc': value if type(value) is dict else None}
        self.failure_observation = observation
        self.receipt['_rpcFailureRecord'] = observation

    @contextmanager
    def guard(self, stage):
        try:
            self.check(stage)
            yield
        except BaseException as error:
            self.record_failure(stage, error)
            raise
