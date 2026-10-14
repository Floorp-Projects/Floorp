"""Fault injection uses local socketpairs and marked temporary fixtures only."""
import copy
import json
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import run
from protocol import Marionette, MAX_FRAME
from rpc_diagnostics import RpcFailureScope, MAX_RPC_DIAGNOSTIC, failure_text

SECRET = 'SYNTHETIC_SECRET_MUST_NOT_BE_SAVED'


def frame(value):
    data = json.dumps(value).encode()
    return str(len(data)).encode() + b':' + data


class OwnedFixtures(unittest.TestCase):
    def scope(self, probe=None):
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        root = Path(temp.name); profile = root / 'profile'; profile.mkdir(mode=0o700)
        output = root / 'output'; output.mkdir(mode=0o700)
        marker = {'nonce': 'a' * 32, 'case': 'normal', 'head': 'b' * 40}
        (profile / 'qa3-restart-profile.json').write_text(json.dumps(marker))
        record = {'case': 'normal', 'nonce': 'a' * 32, 'profileIdentity': run.profile_identity(profile)}
        receipt = {'floorpHead': 'b' * 40, 'run': 123, 'attempt': 1}
        proof = {'status': 'VERIFIED_LIVE_NATIVE', 'root': {'pid': 321, 'start': '55', 'session': 321,
                 'profile': str(profile.resolve())}, 'portOwnerConfirmed': True}
        scope = RpcFailureScope(profile, output, record, receipt, 1,
                                {'version': '157.0.1', 'buildId': '20261007125927'}, probe or (lambda: copy.deepcopy(proof)))
        return scope, record, profile, output, proof

    def client(self, response=None, scope=None):
        a, b = socket.socketpair(); a.setblocking(False)
        self.addCleanup(a.close); self.addCleanup(b.close)
        c = Marionette.__new__(Marionette); c.sock = a; c.index = 0; c.pump = lambda: None
        c.buffer = bytearray(frame(response) if response is not None else b''); c.pending_phase_trace = None
        c.trace = self.traces.append; c.failure_sink = scope.note_rpc_failure if scope is not None else None
        return c, b

    def setUp(self):
        self.traces = []

    def saved(self, record, output):
        data = (output / record['rpcFailureDiagnosticEvidenceFile']).read_bytes()
        self.assertLessEqual(len(data), MAX_RPC_DIAGNOSTIC)
        self.assertNotIn(SECRET.encode(), data + b''.join(self.traces))
        value = json.loads(data)
        self.assertEqual(value['status'], 'FAIL_DIAGNOSTIC_ONLY')
        self.assertFalse(value['nativeSnapshotCollected']); self.assertFalse(value['publicationAuthorized'])
        self.assertFalse(value['nativeProfDVerifiedByDiagnostic']); self.assertFalse(value['retryPerformed'])
        return value

    def assert_safe_receipt_text(self, error, scope):
        text = failure_text(error, scope.receipt.get('_rpcFailureRecord'))
        self.assertNotIn(SECRET, text)
        return {'originalError': text, 'firstError': text, 'status': 'FAIL_DEVELOPMENT_ONLY'}


class TransportFaults(OwnedFixtures):
    def test_close_exception_class_getter_cannot_replace_original(self):
        class HostileClass(type):
            def __getattribute__(self, name):
                if name == '__name__': raise RuntimeError(SECRET)
                return super().__getattribute__(name)
        class CloseError(Exception, metaclass=HostileClass): pass
        scope, record, _, output, _ = self.scope(); original = OSError(22, SECRET)
        class BrokenSocket:
            def setblocking(self, value): raise original
            def close(self): raise CloseError(SECRET)
        with patch('protocol.socket.create_connection', return_value=BrokenSocket()):
            with self.assertRaises(OSError) as caught:
                with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, scope.note_rpc_failure)
        self.assertIs(caught.exception, original); self.assert_safe_receipt_text(original, scope)
        self.assertEqual(self.saved(record, output)['rpc']['secondaryCloseExceptionType'], 'OtherException')
    def test_connection_refusal_keeps_original_errno_object_and_one_dial(self):
        scope, record, _, output, _ = self.scope(); original = ConnectionRefusedError(111, SECRET)
        with patch('protocol.socket.create_connection', side_effect=original) as dial:
            with self.assertRaises(ConnectionRefusedError) as caught:
                with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, scope.note_rpc_failure)
        self.assertIs(caught.exception, original); dial.assert_called_once_with(('127.0.0.1', 2828), timeout=5)
        self.assert_safe_receipt_text(original, scope)
        value = self.saved(record, output)
        self.assertEqual(value['originalErrno'], 111); self.assertEqual(value['rpc']['stage'], 'connect')
        self.assertEqual(self.traces, [])

    def test_setblocking_and_close_fault_do_not_replace_original(self):
        scope, record, _, output, _ = self.scope(); original = OSError(22, SECRET)
        class BrokenSocket:
            def setblocking(self, value): raise original
            def close(self): raise RuntimeError(SECRET)
        with patch('protocol.socket.create_connection', return_value=BrokenSocket()):
            with self.assertRaises(OSError) as caught:
                with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, scope.note_rpc_failure)
        self.assertIs(caught.exception, original)
        value = self.saved(record, output)
        self.assertEqual(value['rpc']['secondaryCloseExceptionType'], 'RuntimeError')

    def test_initialization_faults_drop_raw_error_stack_capabilities_and_reply(self):
        for step in ['handshake', 'new-session', 'context', 'timeouts']:
            with self.subTest(step=step):
                scope, record, _, output, _ = self.scope()
                a, b = socket.socketpair(); self.addCleanup(a.close); self.addCleanup(b.close)
                payload = frame({'marionetteProtocol': 3, 'unexpected': SECRET})
                error = {'error': 'javascript error', 'message': SECRET, 'stacktrace': SECRET, 'prefs': SECRET}
                for ident in range(3):
                    payload += frame([1, ident, error if ident == {'new-session': 0, 'context': 1, 'timeouts': 2}.get(step) else None, {'capabilities': SECRET}])
                if step == 'handshake': payload = frame({'marionetteProtocol': True, 'unexpected': SECRET})
                b.sendall(payload)
                with patch('protocol.socket.create_connection', return_value=a) as dial:
                    with self.assertRaises(ValueError) as caught:
                        with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, scope.note_rpc_failure)
                self.assertNotIn(SECRET, str(caught.exception)); self.assertEqual(a.fileno(), -1)
                self.assert_safe_receipt_text(caught.exception, scope)
                dial.assert_called_once()
                value = self.saved(record, output)
                self.assertEqual(value['rpc']['stage'], 'handshake' if step == 'handshake' else 'native-error')
                if step != 'handshake':
                    self.assertEqual(value['rpc']['nativeError']['code'], 'javascript error')
                    self.assertTrue(value['rpc']['nativeError']['detailWithheld'])

    def test_handshake_eof_and_timeout_record_stage_without_retry(self):
        for failure in [EOFError(SECRET), TimeoutError(SECRET)]:
            with self.subTest(failure=type(failure).__name__):
                scope, record, _, output, _ = self.scope(); a, b = socket.socketpair()
                self.addCleanup(a.close); self.addCleanup(b.close)
                with patch('protocol.socket.create_connection', return_value=a), patch.object(Marionette, 'read', side_effect=failure):
                    with self.assertRaises(type(failure)) as caught:
                        with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, scope.note_rpc_failure)
                self.assertIs(caught.exception, failure); self.assertEqual(a.fileno(), -1)
                self.assertEqual(self.saved(record, output)['rpc']['stage'], 'handshake')

    def test_write_and_partial_reply_deadlines_close_and_keep_failure(self):
        for stage in ['write', 'reply']:
            with self.subTest(stage=stage):
                scope, record, _, output, _ = self.scope(); c, _ = self.client(scope=scope)
                if stage == 'reply': c.buffer.extend(b'200:partial-SECRET')
                with self.assertRaises(TimeoutError) as caught:
                    with scope.guard('phase'): c.command('WebDriver:ExecuteAsyncScript', {}, seconds=0 if stage == 'write' else 0.001, trace_response=False)
                self.assertEqual(c.sock.fileno(), -1); self.assertEqual(c.index, 1)
                value = self.saved(record, output); self.assertEqual(value['rpc']['stage'], stage)
                self.assertEqual(self.traces, []); self.assertIsNone(c.pending_phase_trace)
                self.assertEqual(caught.exception.qa3_rpc_failure['stage'], stage)

    def test_wrong_boolean_float_or_foreign_ids_drop_native_error_details(self):
        for ident in [False, 0.0, 1]:
            with self.subTest(ident=ident):
                scope, record, _, output, _ = self.scope()
                c, _ = self.client([1, ident, {'error': 'javascript error', 'message': SECRET}, None], scope)
                with self.assertRaisesRegex(ValueError, 'identity mismatch'):
                    with scope.guard('phase'): c.phase('synthetic', {})
                value = self.saved(record, output)
                self.assertFalse(value['rpc']['responseCorrelated']); self.assertIsNone(value['rpc']['nativeError'])
                self.assertEqual(self.traces, []); self.assertIsNone(c.pending_phase_trace)

    def test_known_reference_error_requires_correlated_async_javascript_error(self):
        for command, code, message, allowed in [
                ('WebDriver:ExecuteAsyncScript', 'javascript error', 'ReferenceError: restartOracle is not defined', True),
                ('WebDriver:ExecuteAsyncScript', 'javascript error', 'ReferenceError: restartOracle is not defined ' + SECRET, False),
                ('WebDriver:NewSession', 'javascript error', 'Services is not defined', False),
                ('WebDriver:ExecuteAsyncScript', 'unknown error', 'Services is not defined', False),
                ('WebDriver:ExecuteAsyncScript', SECRET, 'Services is not defined', False)]:
            with self.subTest(command=command, code=code, allowed=allowed):
                scope, record, _, output, _ = self.scope()
                c, _ = self.client([1, 0, {'error': code, 'message': message, 'stacktrace': SECRET}, None], scope)
                with self.assertRaises(ValueError):
                    with scope.guard('phase'): c.command(command, {}, trace_response=False)
                native = self.saved(record, output)['rpc']['nativeError']
                self.assertEqual(native['detailWithheld'], not allowed)
                self.assertEqual(native['detail'], 'ReferenceError: restartOracle is not defined' if allowed else None)
                self.assertEqual(self.traces, [])

    def test_missing_phase_result_and_oversized_request_close_without_raw_trace(self):
        for mode in ['missing', 'oversized']:
            with self.subTest(mode=mode):
                scope, record, _, output, _ = self.scope(); c, _ = self.client([1, 0, None, {'secret': SECRET}], scope)
                with self.assertRaises(ValueError):
                    with scope.guard('phase'): c.phase('x' * MAX_FRAME if mode == 'oversized' else 'synthetic', {})
                self.saved(record, output); self.assertEqual(c.sock.fileno(), -1)
                self.assertEqual(self.traces, []); self.assertIsNone(c.pending_phase_trace)

    def test_initialization_success_trace_is_closed_metadata_not_capabilities(self):
        a, b = socket.socketpair(); self.addCleanup(a.close); self.addCleanup(b.close)
        b.sendall(frame({'marionetteProtocol': 3, 'private': SECRET}) + b''.join(frame([1, i, None, {'capabilities': SECRET}]) for i in range(3)))
        with patch('protocol.socket.create_connection', return_value=a): c = Marionette(2828, lambda: None, self.traces.append)
        self.assertEqual(len(self.traces), 4); self.assertNotIn(SECRET.encode(), b''.join(self.traces))
        self.assertIsNone(c.pending_phase_trace)

    def test_trace_sink_failure_preserves_original_object_and_saves_no_response_body(self):
        scope, record, _, output, _ = self.scope(); a, b = socket.socketpair()
        self.addCleanup(a.close); self.addCleanup(b.close); b.sendall(frame({'marionetteProtocol': 3, 'secret': SECRET}))
        original = RuntimeError(SECRET)
        with patch('protocol.socket.create_connection', return_value=a):
            with self.assertRaises(RuntimeError) as caught:
                with scope.guard('initialization'): Marionette(2828, lambda: None, lambda data: (_ for _ in ()).throw(original), scope.note_rpc_failure)
        self.assertIs(caught.exception, original); self.saved(record, output)


class OwnershipAndPersistence(OwnedFixtures):
    def test_exception_attribute_getters_setters_and_str_cannot_break_receipt_or_original(self):
        class HostileError(Exception):
            def __getattribute__(self, name):
                if name in ['qa3_rpc_failure', 'qa3_owned_rpc_failure', 'errno']: raise RuntimeError(SECRET)
                return super().__getattribute__(name)
            def __setattr__(self, name, value):
                if name.startswith('qa3_'): raise RuntimeError(SECRET)
                return super().__setattr__(name, value)
            def __str__(self): raise RuntimeError(SECRET)
        scope, record, _, output, _ = self.scope(); original = HostileError(SECRET)
        with patch('protocol.socket.create_connection', side_effect=original):
            with self.assertRaises(HostileError) as caught:
                with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, scope.note_rpc_failure)
        self.assertIs(caught.exception, original)
        self.assert_safe_receipt_text(original, scope); self.saved(record, output)
        self.assertIn('UNAVAILABLE', failure_text(original))

    def test_metadata_callback_failure_uses_caller_owned_unavailable_record(self):
        scope, record, _, output, _ = self.scope(); original = TimeoutError(SECRET)
        def broken_sink(error, value): raise RuntimeError(SECRET)
        a, b = socket.socketpair(); self.addCleanup(a.close); self.addCleanup(b.close)
        with patch('protocol.socket.create_connection', return_value=a), patch.object(Marionette, 'read', side_effect=original):
            with self.assertRaises(TimeoutError) as caught:
                with scope.guard('initialization'): Marionette(2828, lambda: None, self.traces.append, broken_sink)
        self.assertIs(caught.exception, original)
        value = self.saved(record, output); self.assertEqual(value['rpc']['stage'], 'unavailable')
        self.assert_safe_receipt_text(original, scope)

    def test_malformed_caller_metadata_and_raising_nonrpc_str_fall_back_without_throwing(self):
        class Unprintable(Exception):
            def __str__(self): raise RuntimeError(SECRET)
        original = Unprintable(SECRET)
        self.assertIn('UNAVAILABLE', failure_text(original))
        for value in [[], {'error': None, 'rpc': SECRET}, {'error': original, 'rpc': {'stage': [], 'command': {}}}]:
            self.assertNotIn(SECRET, failure_text(original, value))

    def test_changed_marker_or_symlink_profile_records_no_foreign_details(self):
        for mode in ['marker', 'profile-link', 'marker-link']:
            with self.subTest(mode=mode):
                scope, record, profile, output, _ = self.scope()
                marker = profile / 'qa3-restart-profile.json'
                if mode == 'marker': marker.write_text(SECRET)
                elif mode == 'marker-link':
                    target = profile.parent / 'foreign-marker'; target.write_text(SECRET); marker.unlink(); marker.symlink_to(target)
                else:
                    renamed = profile.with_name('foreign'); profile.rename(renamed); profile.symlink_to(renamed)
                original = TimeoutError(SECRET); scope.record_failure('phase', original)
                self.assertNotIn('rpcFailureDiagnosticEvidenceFile', record); self.assertIn('rpcDiagnosticError', record)
                self.assertNotIn(SECRET, json.dumps(record)); self.assertEqual(list(output.iterdir()), [])

    def test_pid_reuse_or_missing_owner_never_dumps_a_process_or_native_error(self):
        for status in ['OWNERSHIP_REJECTED', 'OWNED_NATIVE_EXITED', 'NO_OWNED_NATIVE', 'PROBE_UNAVAILABLE']:
            with self.subTest(status=status):
                scope, record, _, output, _ = self.scope(lambda: {'status': status, 'root': {'secret': SECRET}})
                error = ValueError(SECRET); error.qa3_rpc_failure = {'stage': 'native-error', 'command': 'WebDriver:ExecuteAsyncScript', 'responseCorrelated': True, 'nativeError': {'code': 'javascript error', 'detail': SECRET}}
                scope.note_rpc_failure(error, error.qa3_rpc_failure); scope.record_failure('phase', error); value = self.saved(record, output)
                self.assertIsNone(value['processOwnership']['root']); self.assertIsNone(value['rpc']['nativeError'])

    def test_process_changes_between_probes_withholds_owned_details(self):
        scope, record, _, output, proof = self.scope(); calls = []
        def probe():
            calls.append(1)
            return proof if len(calls) == 1 else {'status': 'OWNERSHIP_REJECTED', 'root': {'secret': SECRET}}
        scope.probe = probe; scope.record_failure('phase', TimeoutError(SECRET))
        value = self.saved(record, output); self.assertEqual(value['processOwnership']['status'], 'OWNERSHIP_REJECTED')
        self.assertIsNone(value['processOwnership']['root'])

    def test_marker_mutation_during_probe_rejects_persistence(self):
        scope, record, profile, output, proof = self.scope()
        def probe(): (profile / 'qa3-restart-profile.json').write_text(SECRET); return proof
        scope.probe = probe; scope.record_failure('phase', TimeoutError(SECRET))
        self.assertEqual(list(output.iterdir()), []); self.assertIn('rpcDiagnosticError', record)

    def test_existing_output_or_sink_error_keeps_original_and_separate_io_failure(self):
        scope, record, _, output, _ = self.scope(); target = output / 'normal-boot1-rpc-failure-diagnostic.json'
        target.write_text('original owned file'); original = TimeoutError(SECRET)
        with self.assertRaises(TimeoutError) as caught:
            with scope.guard('phase'): raise original
        self.assertIs(caught.exception, original); self.assertEqual(target.read_text(), 'original owned file')
        self.assertIn('rpcDiagnosticError', record); self.assertNotIn('rpcFailureDiagnosticEvidenceFile', record)
        self.assert_safe_receipt_text(original, scope)

    def test_size_bound_failure_preserves_original_and_safe_receipt_text(self):
        scope, record, _, output, _ = self.scope(); original = RuntimeError(SECRET)
        with patch('rpc_diagnostics.MAX_RPC_DIAGNOSTIC', 1):
            with self.assertRaises(RuntimeError) as caught:
                with scope.guard('phase'): raise original
        self.assertIs(caught.exception, original); self.assertEqual(list(output.iterdir()), [])
        self.assertIn('rpcDiagnosticError', record); self.assert_safe_receipt_text(original, scope)

    def test_guard_rejects_foreign_owner_before_any_operation(self):
        scope, record, _, output, _ = self.scope(lambda: {'status': 'OWNERSHIP_REJECTED'})
        called = []
        with self.assertRaisesRegex(ValueError, 'no longer confirmed'):
            with scope.guard('initialization'): called.append('dial')
        self.assertEqual(called, []); self.saved(record, output)

    def test_diagnostic_flag_cannot_qualify_a_pass_case(self):
        from test_contract import Contract
        receipt = Contract().receipt(); receipt['cases'][0]['rpcFailureDiagnosticEvidenceFile'] = 'fixture.json'
        with self.assertRaisesRegex(ValueError, 'cannot qualify'): run.validate_receipt(receipt)
        receipt = Contract().receipt(); receipt['rpcClientCloseError'] = 'fixture close error'
        with self.assertRaisesRegex(ValueError, 'cannot become a pass'): run.validate_receipt(receipt)

    def test_real_proc_descriptor_reads_only_the_unit_spawned_python_generation(self):
        scope, _, profile, _, _ = self.scope()
        child = subprocess.Popen([sys.executable, '-c', 'import sys;sys.stdout.write("ready\\n");sys.stdout.flush();sys.stdin.buffer.read(1)', '--profile', str(profile.resolve()), SECRET],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        def cleanup():
            if child.poll() is None:
                child.stdin.close()
                try: child.wait(timeout=5)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
            if not child.stdin.closed: child.stdin.close()
            for stream in [child.stdout, child.stderr]: stream.close()
        self.addCleanup(cleanup)
        self.assertTrue(select.select([child.stdout], [], [], 5)[0], 'Owned unit process readiness deadline')
        self.assertEqual(child.stdout.readline(), b'ready\n')
        table = run.legacy.process_table(); owned = {child.pid: table[child.pid]['start']}
        expected = {'pid': child.pid, 'start': owned[child.pid], 'session': child.pid}
        value = run.rpc_process_ownership(run.legacy.process_table, owned, Path(sys.executable), profile, expected)
        self.assertEqual(value['status'], 'VERIFIED_LIVE_NATIVE'); self.assertEqual(value['root']['pid'], child.pid)
        self.assertNotIn(SECRET, json.dumps(value)); self.assertFalse(value['portOwnerConfirmed'])
        child.stdin.write(b'x'); child.stdin.flush(); child.wait(timeout=5)
        value = run.rpc_process_ownership(run.legacy.process_table, owned, Path(sys.executable), profile, expected)
        self.assertEqual(value['status'], 'OWNED_NATIVE_EXITED')

    def test_process_probe_reads_only_owned_generations_and_rejects_profile_sid_reuse(self):
        scope, _, profile, _, _ = self.scope(); binary = profile.parent / 'owned-native'
        table = {321: {'start': '55', 'session': 321, 'state': 'S'}, 999: {'start': 'foreign', 'session': 999, 'state': 'S'}}
        owned = {321: '55'}; seen = []
        def inspect(pid): seen.append(pid); return binary.resolve(), [b'--profile', str(profile.resolve()).encode()]
        owner = {'pid': 321, 'start': '55', 'session': 321, 'profile': str(profile.resolve())}
        result = run.rpc_process_ownership(lambda: table, owned, binary, profile, owner, inspect, lambda: owner)
        self.assertEqual(result['status'], 'VERIFIED_LIVE_NATIVE'); self.assertEqual(seen, [321])
        for changed in ['generation', 'profile', 'session']:
            with self.subTest(changed=changed):
                rows = copy.deepcopy(table)
                if changed == 'generation': rows[321]['start'] = 'reused'
                elif changed == 'session': rows[321]['session'] = 999
                reader = inspect if changed != 'profile' else lambda pid: (binary.resolve(), [b'--profile', SECRET.encode()])
                value = run.rpc_process_ownership(lambda: rows, owned, binary, profile, owner, reader)
                self.assertNotEqual(value['status'], 'VERIFIED_LIVE_NATIVE'); self.assertNotIn(SECRET, json.dumps(value))


if __name__ == '__main__': unittest.main()
