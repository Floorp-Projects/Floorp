# SPDX-License-Identifier: MPL-2.0
import ast
import builtins
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import checkpoint
import failure_excerpt as fe
from qa_core import Failure
from support import Runner


def traceback_11(code='checkpoint-context-mismatch', module='qa_core'):
    lines = ['Traceback (most recent call last):',
        '  File "/private/PRIVATE-PATH/checkpoint.py", line 655, in <module>',
        '    main()',
        '  File "/private/PRIVATE-PATH/checkpoint.py", line 647, in main',
        '    result = restore(plan, source, task, sdk)',
        '  File "/private/PRIVATE-PATH/checkpoint.py", line 580, in restore',
        '    require(False, "PRIVATE-TOKEN=SECRET")',
        '  File "/private/PRIVATE-PATH/qa_core.py", line 16, in require',
        '    raise Failure(code)', '    ^^^^^^^^^^^^^^^^^^^', module+'.Failure: '+code]
    body = '\n'.join(lines)+'\n'
    lines[1] = lines[1].replace('/private/', '/private/'+'P'*(911-len(body.encode())))
    return '\n'.join(lines)+'\n'


class CheckpointFailureExcerptTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        fe._checkpoint_codes.cache_clear()
        self.addCleanup(fe._checkpoint_codes.cache_clear)

    def path(self, content):
        path = self.root/'checkpoint-restore.private.log'
        path.write_bytes(content.encode() if isinstance(content, str) else content)
        return path

    def assert_private_absent(self, value):
        encoded = json.dumps(value)
        for token in ('PRIVATE', 'SECRET', '/private/', 'locals', 'nonce', 'https://'):
            self.assertNotIn(token, encoded)

    def test_known_require_codes_and_both_exact_failure_modules_have_fixed_phase_and_types(self):
        for module in ('qa_core', 'support'):
            for code, phase in (('checkpoint-context-mismatch', 'restore-validation'),
                    ('checkpoint-reference-content', 'reference-validation'),
                    ('checkpoint-payload-content', 'payload-validation'),
                    ('checkpoint-manifest-hash', 'manifest-validation'),
                    ('checkpoint-deadline', 'resource-bound'),
                    ('checkpoint-known-build-survivors', 'build-boundary'),
                    ('checkpoint-path', 'checkpoint-validation')):
                with self.subTest(module=module, code=code):
                    records = fe.classify(module+'.Failure: '+code+'\n')
                    self.assertEqual(records, [{'kind': 'checkpoint-failure', 'failureCode': code,
                        'phase': phase, 'phaseSource': 'fixed-failure-code', 'exceptionKind': 'failure',
                        'exceptionType': module+'.Failure', 'errno': None}])
                    self.assert_private_absent(records)

    def test_vocabulary_reads_literal_require_constants_without_import_or_execution(self):
        source = self.root/'source.py'
        source.write_text('raise RuntimeError("PRIVATE-MUST-NOT-EXECUTE")\n'
            'require(False, "checkpoint-path")\n'
            'require(False, "checkpoint-deadline")\n'
            'require(False, "PRIVATE")\n'
            'require(False, "checkpoint-" + "dynamic")\n'
            'other(False, "checkpoint-unknown")\n'
            'value = "checkpoint-not-a-require"\n')
        with mock.patch.object(fe, 'CHECKPOINT_SOURCES', (source,)):
            self.assertEqual(fe._checkpoint_codes(), frozenset(('checkpoint-path', 'checkpoint-deadline')))
            self.assertEqual(fe.classify('qa_core.Failure: checkpoint-unknown'), [])
            self.assertEqual(fe.classify('qa_core.Failure: checkpoint-path')[0]['failureCode'], 'checkpoint-path')

    def test_fixed_transfer_source_codes_are_available_without_importing_helpers(self):
        original_import = builtins.__import__
        def guarded(name, *args, **kwargs):
            if name in ('checkpoint_replay', 'checkpoint_transfer'):
                raise AssertionError('helper module must not be imported for diagnostics')
            return original_import(name, *args, **kwargs)
        with mock.patch.object(builtins, '__import__', side_effect=guarded):
            for code in ('checkpoint-transfer-role', 'checkpoint-run-provenance',
                         'checkpoint-artifact-provenance', 'checkpoint-api-deadline'):
                record = fe.classify('qa_core.Failure: '+code+'\n')[0]
                self.assertEqual(record['failureCode'], code)
                self.assertEqual(record['phase'], 'transfer-validation')
        self.assertEqual([path.name for path in fe.CHECKPOINT_SOURCES],
                         ['checkpoint.py', 'checkpoint_replay.py', 'checkpoint_transfer.py'])

    def test_replay_and_transfer_ast_codes_reject_private_paths_extras_and_unknown_prefix_members(self):
        sources = [self.root/name for name in ('checkpoint.py', 'checkpoint_replay.py', 'checkpoint_transfer.py')]
        for source, code in zip(sources, ('checkpoint-path', 'checkpoint-replay-envelope', 'checkpoint-transfer-role')):
            source.write_text('raise RuntimeError("PRIVATE-MUST-NOT-EXECUTE")\nrequire(False, '+repr(code)+')\n')
        with mock.patch.object(fe, 'CHECKPOINT_SOURCES', tuple(sources)):
            for code, phase in (('checkpoint-replay-envelope', 'replay-validation'),
                                ('checkpoint-transfer-role', 'transfer-validation')):
                for module in ('qa_core', 'support'):
                    path = self.path(traceback_11(code, module)+'PRIVATE nonce=SECRET https://signed.invalid/PRIVATE\n')
                    for value in (fe.excerpt(path), checkpoint.full_log_receipt(path)):
                        self.assertEqual(value['matchingRecords'], 1)
                        self.assertEqual(value['records'][0]['failureCode'], code)
                        self.assertEqual(value['records'][0]['phase'], phase)
                        self.assert_private_absent(value)
                for line in ('qa_core.Failure: '+code+' /private/PRIVATE',
                             '/private/PRIVATE/'+code, 'checkpoint_replay.Failure: '+code,
                             'qa_core.Failure: '+code+' {"nonce":"SECRET"}'):
                    self.assertEqual(fe.classify(line), [])
            self.assertEqual(fe.classify('qa_core.Failure: checkpoint-replay-private-unknown'), [])
            self.assertEqual(fe.classify('support.Failure: checkpoint-transfer-private-unknown'), [])

    def test_missing_malformed_or_symlink_helper_does_not_erase_other_fixed_source_codes(self):
        core = self.root/'checkpoint.py'; core.write_text('require(False, "checkpoint-path")\n')
        helper = self.root/'checkpoint_replay.py'
        for state in ('missing', 'malformed', 'symlink', 'oversized'):
            with self.subTest(state=state):
                if helper.exists() or helper.is_symlink(): helper.unlink()
                if state == 'malformed': helper.write_text('PRIVATE invalid Python!')
                if state == 'symlink': helper.symlink_to(core)
                if state == 'oversized': helper.write_text('P'*(fe.CHECKPOINT_SOURCE_LIMIT+1))
                fe._checkpoint_codes.cache_clear()
                with mock.patch.object(fe, 'CHECKPOINT_SOURCES', (core, helper)):
                    self.assertEqual(fe.classify('qa_core.Failure: checkpoint-path')[0]['failureCode'], 'checkpoint-path')
                    self.assertEqual(fe.classify('qa_core.Failure: checkpoint-replay-private-unknown'), [])

    def test_combined_source_vocabulary_still_has_the_existing_256_code_bound(self):
        sources = [self.root/name for name in ('a.py', 'b.py')]
        for source, start, stop in ((sources[0], 0, 129), (sources[1], 129, 257)):
            source.write_text('\n'.join('require(False, "checkpoint-code-'+str(i)+'")' for i in range(start, stop)))
        with mock.patch.object(fe, 'CHECKPOINT_SOURCES', tuple(sources)):
            self.assertEqual(fe._checkpoint_codes(), frozenset())
            self.assertEqual(fe.classify('qa_core.Failure: checkpoint-code-0'), [])

    def test_unknown_failure_false_module_private_extras_and_nonexact_lines_are_not_markers(self):
        known = 'qa_core.Failure: checkpoint-context-mismatch'
        lines = ['qa_core.Failure: checkpoint-private-unknown', 'qa_core.Failure: PRIVATE',
            'support.Failure: checkpoint-context-mismatch PRIVATE',
            'fake.Failure: checkpoint-context-mismatch', '__main__.Failure: checkpoint-context-mismatch',
            'qa_core.extra.Failure: checkpoint-context-mismatch', 'qacore.Failure: checkpoint-context-mismatch',
            'prefix '+known, ' '+known, known+' ', known+'\nPRIVATE', known+'\x00',
            known+' [Errno 13]', known+' {"nonce":"SECRET"}', known+'\t',
            'PermissionError: [Errno 13] PRIVATE', 'qa_core.Failure: checkpoint-context-mismatch-extra']
        for line in lines:
            with self.subTest(line=line):
                self.assertEqual(fe.classify(line), [])
        oversized = known+'P'*300
        self.assertEqual(fe.classify(oversized), [])

    def test_exact_11_line_911_byte_traceback_is_retained_by_excerpt_fullscan_and_stage_log_route(self):
        text = traceback_11()
        self.assertEqual(len(text.splitlines()), 11); self.assertEqual(len(text.encode()), 911)
        path = self.path(text)
        tail = fe.excerpt(path)
        full = checkpoint.full_log_receipt(path)
        self.assertTrue(tail['available']); self.assertEqual(tail['matchingRecords'], 1)
        self.assertEqual(tail['records'][0]['tailLine'], 11)
        self.assertTrue(full['complete']); self.assertEqual(full['lines'], 11)
        self.assertEqual(full['bytes'], 911)
        self.assertEqual(full['sha256'], hashlib.sha256(text.encode()).hexdigest())
        self.assertEqual(full['diagnosticCounts'], {'checkpoint-failure': 1})
        self.assertEqual(full['records'][0]['line'], 11)
        for value in (tail, full):
            self.assertEqual(value['records'][0]['failureCode'], 'checkpoint-context-mismatch')
            self.assert_private_absent(value)
        self.assertIs(checkpoint.full_log_receipt.__globals__['classify'], fe.classify)
        tree = ast.parse(Path(__file__).with_name('pipeline.py').read_text())
        routes = [node for node in ast.walk(tree) if isinstance(node, ast.Assign) and
            any(isinstance(target, ast.Subscript) and isinstance(target.slice, ast.Constant) and
                target.slice.value == 'checkpointStageLogs' for target in node.targets)]
        self.assertEqual(len(routes), 1)
        self.assertTrue(any(isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and
                            node.func.id == 'full_log_receipt' for node in ast.walk(routes[0])))
        self.assertIn('checkpoint-restore', [node.value for node in ast.walk(routes[0]) if isinstance(node, ast.Constant)])

    def test_private_traceback_frames_and_following_metadata_never_enter_projection(self):
        text = traceback_11(module='support')+'PRIVATE locals nonce=SECRET https://signed.invalid/PRIVATE\n'
        for value in (fe.excerpt(self.path(text)), checkpoint.full_log_receipt(self.root/'checkpoint-restore.private.log')):
            self.assertEqual(value['records'][0]['exceptionType'], 'support.Failure')
            self.assertEqual(value['matchingRecords'], 1)
            self.assertNotIn('frames', value['records'][0])
            self.assert_private_absent(value)

    def test_unterminated_or_tail_start_fragment_is_not_a_complete_failure(self):
        line = 'qa_core.Failure: checkpoint-context-mismatch'
        path = self.path(line)
        self.assertEqual(fe.excerpt(path)['matchingRecords'], 0)
        self.assertFalse(checkpoint.full_log_receipt(path)['complete'])
        path = self.path(b'PRIVATE'*30000+line.encode()+b'\n')
        tail = fe.excerpt(path)
        self.assertTrue(tail['tailTruncated']); self.assertEqual(tail['matchingRecords'], 0)
        self.assert_private_absent(tail)

    def test_oversized_line_and_full_stream_are_not_promoted_to_valid_tracebacks(self):
        path = self.path('PRIVATE'*(2*1024**2//7+1)+'qa_core.Failure: checkpoint-context-mismatch\n')
        tail = fe.excerpt(path); full = checkpoint.full_log_receipt(path)
        self.assertEqual(tail['matchingRecords'], 0); self.assertFalse(full['complete'])
        self.assert_private_absent(tail); self.assert_private_absent(full)
        with mock.patch('checkpoint.os.fstat', return_value=SimpleNamespace(st_mode=0o100600, st_size=256*1024**2+1)):
            self.assertFalse(checkpoint.full_log_receipt(path)['complete'])

    def test_missing_symlink_fifo_or_unavailable_source_remain_unknown(self):
        target = self.path(traceback_11())
        link = self.root/'link'; link.symlink_to(target)
        fifo = self.root/'fifo'; os.mkfifo(fifo)
        for path in (self.root/'missing', link, fifo):
            self.assertFalse(fe.excerpt(path)['available'])
            self.assertFalse(checkpoint.full_log_receipt(path)['complete'])
        for source in (self.root/'missing-source', link, fifo):
            fe._checkpoint_codes.cache_clear()
            with mock.patch.object(fe, 'CHECKPOINT_SOURCES', (source,)):
                self.assertEqual(fe.classify('qa_core.Failure: checkpoint-context-mismatch'), [])

    def test_malformed_oversized_or_excessive_source_does_not_execute_or_accept_codes(self):
        source = self.root/'source.py'
        for text in ('not valid python! PRIVATE', 'P'*(fe.CHECKPOINT_SOURCE_LIMIT+1),
                     '\n'.join('require(False, "checkpoint-code-'+str(i)+'")' for i in range(257))):
            with self.subTest(size=len(text)):
                source.write_text(text); fe._checkpoint_codes.cache_clear()
                with mock.patch.object(fe, 'CHECKPOINT_SOURCES', (source,)):
                    self.assertEqual(fe.classify('qa_core.Failure: checkpoint-context-mismatch'), [])

    def test_first_and_last_16_selection_and_tail_budget_are_unchanged(self):
        line = 'qa_core.Failure: checkpoint-context-mismatch\n'
        path = self.path(line*25)
        for value, key in ((fe.excerpt(path), 'tailLine'), (checkpoint.full_log_receipt(path), 'line')):
            self.assertEqual(value['matchingRecords'], 25); self.assertTrue(value['recordsTruncated'])
            self.assertEqual([row[key] for row in value['records']], list(range(1, 9))+list(range(18, 26)))
            self.assertTrue(all(row['failureCode'] == 'checkpoint-context-mismatch' for row in value['records']))
        path = self.path(line+'PRIVATE ordinary progress\n'*10000+line)
        tail = fe.excerpt(path); full = checkpoint.full_log_receipt(path)
        self.assertTrue(tail['tailTruncated']); self.assertLessEqual(tail['tailBytesRead'], 131072)
        self.assertEqual(tail['matchingRecords'], 1); self.assertEqual(full['matchingRecords'], 2)
        self.assert_private_absent(tail); self.assert_private_absent(full)

    def test_runner_before_after_and_fullscan_keep_marker_without_replacing_primary(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        registry = mock.Mock(); registry.outside_group_rss.return_value = 0
        registry.finish.side_effect = PermissionError(13, 'PRIVATE-CLEANUP')
        runner.native_registry = registry
        activity = {'sampleComplete': True, 'rssMiB': 1, 'cpuSeconds': 0, 'processes': 1, 'states': ['R'], '_pids': ()}
        script = 'import sys\nsys.stdout.write('+repr(traceback_11())+')\nsys.stdout.flush()\nraise SystemExit(1)'
        with mock.patch.object(runner, 'group_activity', return_value=activity), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            with self.assertRaises(PermissionError):
                runner.run('checkpoint-restore', [sys.executable, '-c', script], self.root, os.environ.copy(), 360)
        self.assertEqual(runner.failure['code'], 'nonzero-exit')
        self.assertEqual(json.loads((self.root/'first-failure.json').read_text())['code'], 'nonzero-exit')
        record = runner.records[-1]
        self.assertEqual(record['cleanupFailure'], 'PermissionError')
        self.assertEqual(record['groupCleanup'], 'verified-gone'); self.assertEqual(record['leaderReap']['status'], 'reaped')
        for phase in ('beforeCleanup', 'afterCleanup'):
            self.assertEqual(record['failureEvidence'][phase]['records'][0]['failureCode'], 'checkpoint-context-mismatch')
        full = checkpoint.full_log_receipt(self.root/'checkpoint-restore.private.log')
        self.assertEqual(full['records'][0]['failureCode'], 'checkpoint-context-mismatch')
        emitted = [json.loads(line)['failureExcerpt'] for line in output.getvalue().splitlines() if 'failureExcerpt' in line]
        self.assertEqual([row['phase'] for row in emitted], ['before-cleanup', 'after-cleanup'])
        self.assert_private_absent(record); self.assert_private_absent(full); self.assertNotIn('PRIVATE', output.getvalue())


if __name__ == '__main__':
    unittest.main()
