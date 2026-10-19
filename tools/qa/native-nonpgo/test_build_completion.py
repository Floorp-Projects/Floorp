# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import time
import tracemalloc
import unittest
from unittest import mock

import build_completion as bc


META = {'product': 'mach', 'version': 27, 'preprocessedProfileVersion': 47}
THREAD = {'type': 'thread', 'processName': 'mach', 'pid': '0', 'tid': 0}
NAME = 'profile_log_20261019_120000.json'


def marker(name='Object', text='MacWebAppService.o', start=1, end=2):
    return {'type': 'marker', 'name': name, 'startTime': start, 'endTime': end,
            'data': {'type': 'Text', 'text': text}}


def phase(name='compile', start=0, end=5):
    return {'type': 'marker', 'name': 'Phase', 'startTime': start, 'endTime': end,
            'category': 1, 'data': {'type': 'Phase', 'phase': name}}


def jsonl(rows):
    return b''.join((json.dumps(row, separators=(',', ':')) + '\n').encode()
                    for row in [{'type': 'meta', **META}, THREAD, *rows])


def whole(rows):
    strings, names = [], []
    for row in rows:
        if row['name'] not in strings:
            strings.append(row['name'])
        names.append(strings.index(row['name']))
    markers = {'name': names, 'length': len(rows),
        'data': [row['data'] for row in rows], 'startTime': [row['startTime'] for row in rows],
        'endTime': [row['endTime'] for row in rows],
        'phase': [0 if row['endTime'] is None else 1 for row in rows],
        'category': [row.get('category', 0) for row in rows], 'stack': [None] * len(rows)}
    return {'meta': dict(META), 'libs': [], 'counters': [],
            'threads': [{'processName': 'mach', 'stringArray': strings, 'markers': markers}]}


class CompletionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='floorp-build-completion-test-')
        self.addCleanup(self.temp.cleanup)
        self.task = Path(self.temp.name).resolve()
        self.directory = self.task.joinpath(*bc.PARTS)
        self.directory.mkdir(parents=True)
        self.snapshot = bc.start_snapshot(self.task)
        self.assertEqual(self.snapshot['status'], 'captured')

    def write(self, raw, name=NAME):
        path = self.directory / name
        path.write_bytes(raw)
        return path

    def result(self, rows):
        self.write(jsonl(rows))
        return bc.collect(self.task, self.snapshot)

    def unknown(self, value):
        self.assertEqual(value['status'], 'unconfirmed')
        self.assertFalse(value['completeCoverage'])
        self.assertIsNone(value['finishedTierMarkers'])
        self.assertIsNone(value['finishedTargetMarkers'])
        self.assertIsNone(value['lastObservedFinishedTier'])
        self.assertTrue(all(target['status'] == 'unconfirmed' for target in value['targets'].values()))
        self.assertNotIn('PRIVATE_SENTINEL', json.dumps(value))
        self.assertEqual(value['currentBuildPhase'], 'UNKNOWN')
        self.assertEqual(value['durationToCompletion'], 'UNKNOWN')

    def test_current_upstream_jsonl_shape_counts_only_finished_markers(self):
        rows = [phase('compile', 2, 20), phase('export', 0, 4), marker(),
                marker('Program', 'xpcshell', 7, 8), marker('SharedLib', 'XUL', 8, 9),
                marker('StaticLib', 'private-library-name', 3, 4), marker('Rust', 'private-rust-name', 4, 5)]
        value = self.result(rows)
        self.assertEqual(value['status'], 'observed')
        self.assertTrue(value['completeCoverage'])
        self.assertEqual(value['profileFormat'], 'jsonl')
        self.assertEqual(value['lastObservedFinishedTier'], {'tier': 'compile', 'startTimeMs': 2, 'endTimeMs': 20})
        self.assertEqual(value['finishedTierMarkers']['compile'], 1)
        self.assertEqual(value['finishedTargetMarkers'], dict.fromkeys(bc.KINDS, 1))
        self.assertTrue(all(row['status'] == 'observed-finished' for row in value['targets'].values()))
        self.assertNotIn('private-library-name', json.dumps(value))
        self.assertFalse(value['provesBuildCompletion'])
        self.assertFalse(value['provesCacheHit'])

    def test_whole_profile_uses_max_end_and_ignores_actual_teardown(self):
        data = whole([phase('compile', 5, 50), phase('export', 1, 2), marker('Rust', 'crate', 3, 4),
                      phase('teardown', 60, 70), marker('Program', 'xpcshell', 10, 11)])
        self.write(json.dumps(data, separators=(',', ':')).encode())
        value = bc.collect(self.task, self.snapshot)
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(value['profileFormat'], 'whole-profile')
        self.assertEqual(value['lastObservedFinishedTier']['tier'], 'compile')
        self.assertEqual(value['lastObservedFinishedTier']['endTimeMs'], 50)
        self.assertNotIn('teardown', value['finishedTierMarkers'])

    def test_complete_empty_marker_coverage_is_not_cache_or_build_proof(self):
        value = self.result([])
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(value['finishedTargetMarkers'], dict.fromkeys(bc.KINDS, 0))
        self.assertIsNone(value['lastObservedFinishedTier'])
        self.assertTrue(all(t['status'] == 'unconfirmed' for t in value['targets'].values()))
        self.assertFalse(value['provesCacheHit'])

    def test_instant_and_unended_markers_never_count_finished(self):
        value = self.result([phase(end=None), marker(end=None), marker('Program', 'xpcshell', end=None)])
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(sum(value['finishedTargetMarkers'].values()), 0)
        self.assertEqual(sum(value['finishedTierMarkers'].values()), 0)
        self.assertIsNone(value['lastObservedFinishedTier'])

    def test_target_aliases_paths_and_wrong_classes_do_not_bind(self):
        sentinel = '/Users/PRIVATE_SENTINEL/XUL'
        rows = [marker('SharedLib', sentinel), marker('Program', 'dir/xpcshell'),
                marker('Object', 'prefixMacWebAppService.o'), marker('Object', 'XUL')]
        rows[0]['data']['unknownNested'] = {'pid': 424242, 'argv': sentinel}
        value = self.result(rows)
        self.assertEqual(value['status'], 'observed')
        self.assertTrue(all(t['status'] == 'unconfirmed' for t in value['targets'].values()))
        self.assertNotIn('PRIVATE_SENTINEL', json.dumps(value))
        self.assertNotIn('424242', json.dumps(value))

    def test_latest_completed_kind_and_target_use_max_end_not_row_order(self):
        value = self.result([marker(start=4, end=9), marker(start=1, end=3)])
        self.assertEqual(value['latestFinishedTargetByKind']['Object']['endTimeMs'], 9)
        self.assertEqual(value['targets']['MacWebAppService.o']['endTimeMs'], 9)

    def test_boolean_nonfinite_negative_and_reversed_times_fail_closed(self):
        for start, end in ((True, 2), (0, False), (-1, 2), (2, 1), (0, float('nan')),
                           (0, float('inf')), (0, 'PRIVATE_SENTINEL')):
            with self.subTest(start=start, end=end):
                self.write(jsonl([marker(start=start, end=end)]))
                self.unknown(bc.collect(self.task, self.snapshot))

    def test_invalid_relevant_schema_even_unended_fails_closed(self):
        for row in (phase('PRIVATE_SENTINEL', end=None), marker('Phase', 'bad', end=None),
                    {**marker(), 'data': {'type': 'Phase', 'phase': 'compile'}},
                    {**marker(), 'data': {'type': 'Text', 'text': False}}):
            with self.subTest(row=row):
                self.write(jsonl([row]));self.unknown(bc.collect(self.task, self.snapshot))

    def test_metadata_is_exact_source_product_and_versions(self):
        for key, value in (('product', 'PRIVATE_SENTINEL'), ('version', 26),
                           ('version', True), ('preprocessedProfileVersion', 48)):
            with self.subTest(key=key, value=value):
                rows = [{'type': 'meta', **META, key: value}, THREAD, marker()]
                self.write(b''.join((json.dumps(x) + '\n').encode() for x in rows))
                self.unknown(bc.collect(self.task, self.snapshot))

    def test_malformed_resource_row_cannot_hide_between_valid_targets(self):
        self.write(jsonl([marker()]) + b'{"type":"marker","name":\n' + jsonl([marker()]))
        self.unknown(bc.collect(self.task, self.snapshot))

    def test_truncated_final_line_even_valid_json_is_unknown(self):
        self.write(jsonl([marker()]).rstrip(b'\n'))
        self.unknown(bc.collect(self.task, self.snapshot))

    def test_invalid_utf8_duplicate_keys_and_overflow_numbers_fail_closed(self):
        for raw in (b'\xff\n', b'{"type":"marker","type":"marker"}\n',
                    b'{"type":"marker","name":"Resource","startTime":0,"endTime":1,"data":{"type":"CPU","x":1e999}}\n'):
            with self.subTest(raw=raw):
                self.write(jsonl([]) + raw);self.unknown(bc.collect(self.task, self.snapshot))

    def test_unknown_record_type_or_missing_thread_fails_closed(self):
        for raw in (jsonl([]) + b'{"type":"PRIVATE_SENTINEL"}\n',
                    (json.dumps({'type': 'meta', **META}) + '\n').encode()):
            with self.subTest(raw=raw):
                self.write(raw);self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_column_lengths_indexes_phase_and_bool_length_fail_closed(self):
        initial = whole([marker()])
        for update in ({'length': True}, {'name': [True]}, {'name': [99]},
                       {'endTime': []}, {'phase': [2]}, {'unknown': []}):
            with self.subTest(update=update):
                data = copy.deepcopy(initial);data['threads'][0]['markers'].update(update)
                self.write(json.dumps(data).encode());self.unknown(bc.collect(self.task, self.snapshot))

    def test_missing_or_only_preexisting_profile_is_unknown_not_zero(self):
        self.unknown(bc.collect(self.task, self.snapshot))
        self.write(jsonl([marker()]));snapshot = bc.start_snapshot(self.task)
        self.unknown(bc.collect(self.task, snapshot))

    def test_snapshot_can_precede_creation_of_expected_directories(self):
        with tempfile.TemporaryDirectory(prefix='floorp-build-completion-new-') as temp:
            task = Path(temp).resolve();snapshot = bc.start_snapshot(task)
            directory = task.joinpath(*bc.PARTS);directory.mkdir(parents=True)
            (directory / NAME).write_bytes(jsonl([marker()]))
            self.assertEqual(bc.collect(task, snapshot)['status'], 'observed')

    def test_new_but_stale_profile_is_unknown(self):
        path = self.write(jsonl([marker()]));os.utime(path, ns=(1, 1))
        result = bc.collect(self.task, self.snapshot)
        self.unknown(result);self.assertEqual(result['reason'], 'build-completion-profile-stale')

    def test_multiple_new_profiles_are_ambiguous(self):
        self.write(jsonl([marker()]));self.write(jsonl([marker()]), 'profile_log_20261019_120001.json')
        result = bc.collect(self.task, self.snapshot)
        self.unknown(result);self.assertEqual(result['reason'], 'build-completion-profile-ambiguous')

    def test_symlink_file_and_parent_escape_are_rejected_without_following(self):
        external = self.task / 'PRIVATE_SENTINEL';external.write_bytes(jsonl([marker()]))
        (self.directory / NAME).symlink_to(external)
        self.unknown(bc.collect(self.task, self.snapshot))
        (self.directory / NAME).unlink()
        moved = self.task / 'old-build';self.directory.rename(moved);self.directory.symlink_to(moved, target_is_directory=True)
        self.unknown(bc.collect(self.task, self.snapshot))

    def test_parent_replacement_changes_binding(self):
        self.directory.rename(self.directory.with_name('previous'))
        self.directory.mkdir();self.write(jsonl([marker()]))
        result = bc.collect(self.task, self.snapshot)
        self.unknown(result);self.assertEqual(result['reason'], 'build-completion-directory-changed')

    def test_file_owner_nonregular_and_hardlink_fail_closed(self):
        path = self.write(jsonl([marker()]))
        with mock.patch.object(bc.os, 'geteuid', return_value=os.geteuid() + 1):
            self.unknown(bc.collect(self.task, self.snapshot))
        os.link(path, self.task / 'alias');self.unknown(bc.collect(self.task, self.snapshot));path.unlink()
        os.mkfifo(path);self.unknown(bc.collect(self.task, self.snapshot))

    def test_file_size_line_size_value_size_and_marker_caps(self):
        for setting, cap, raw in (('MAX_BYTES', 1, jsonl([marker()])),
                                 ('MAX_LINE_BYTES', 256, jsonl([marker(text='x' * 300)])),
                                 ('MAX_LINE_BYTES', 256, json.dumps(whole([marker(text='x' * 300)])).encode()),
                                 ('MAX_MARKERS', 1, jsonl([marker(), marker()]))):
            with self.subTest(setting=setting):
                self.write(raw)
                with mock.patch.object(bc, setting, cap):
                    self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_larger_than_previous_16mib_cap_streams_with_bounded_memory(self):
        resource = marker('CPU', 'unused')
        resource['data'] = {'type': 'CPU', 'padding': 'PRIVATE_SENTINEL' + 'x' * (128 * 1024)}
        raw = json.dumps(whole([phase('compile'), marker(), *([resource] * 136)]),
                         separators=(',', ':')).encode()
        self.assertGreater(len(raw), 16 * 1024**2)
        self.write(raw)
        original, requests = bc.os.pread, []
        def bounded(fd, size, offset):
            requests.append(size)
            self.assertLessEqual(size, bc.MAX_LINE_BYTES)
            return original(fd, size, offset)
        tracemalloc.start()
        try:
            with mock.patch.object(bc.os, 'pread', side_effect=bounded):
                value = bc.collect(self.task, self.snapshot)
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(value['profileFormat'], 'whole-profile')
        self.assertEqual(value['profileBytes'], len(raw))
        self.assertEqual(value['finishedTargetMarkers']['Object'], 1)
        self.assertEqual(value['finishedTierMarkers']['compile'], 1)
        self.assertLess(peak, 8 * 1024**2)
        self.assertGreater(len(requests), 100)
        self.assertNotIn('PRIVATE_SENTINEL', json.dumps(value))

    def test_whole_ignored_large_subtree_is_streamed_and_validated(self):
        data = whole([marker()])
        data['ignored'] = ['x' * (128 * 1024)] * 136
        self.write(json.dumps(data).encode())
        self.assertEqual(bc.collect(self.task, self.snapshot)['status'], 'observed')
        raw = json.dumps(data).encode().replace(b'"ignored": [', b'"ignored": [1e999,', 1)
        self.write(raw)
        self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_offsets_do_not_depend_on_column_or_metadata_order(self):
        data = whole([marker(), phase(), marker('Program', 'xpcshell', end=None)])
        thread = data['threads'][0]
        thread['markers'] = dict(reversed(list(thread['markers'].items())))
        thread = dict(reversed(list(thread.items())))
        data = {'threads': [thread], 'meta': data['meta']}
        self.write(json.dumps(data).encode())
        value = bc.collect(self.task, self.snapshot)
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(value['finishedTargetMarkers']['Object'], 1)
        self.assertEqual(value['targets']['xpcshell']['status'], 'unconfirmed')

    def test_whole_chunk_boundaries_cover_escapes_utf8_numbers_and_null(self):
        data = whole([marker(start=1.25, end=2.5), marker('Program', 'xpcshell', end=None)])
        data['ignored'] = ['slash\\quote"unicode\u2603', None, True, False, -1.25e-12]
        self.write(json.dumps(data, ensure_ascii=False, separators=(',', ':')).encode())
        with mock.patch.object(bc, 'READ_BYTES', 7):
            value = bc.collect(self.task, self.snapshot)
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(value['targets']['MacWebAppService.o']['endTimeMs'], 2.5)
        self.assertEqual(value['targets']['xpcshell']['status'], 'unconfirmed')

    def test_whole_ignored_duplicate_keys_use_decoded_semantic_names(self):
        base = json.dumps(whole([marker()])).encode()[:-1]
        for tail in (b',"ignored":{"a":1,"\\u0061":2}}',
                     b',"ignored":{"\xc3\xa9":1,"\\u00e9":2}}'):
            with self.subTest(tail=tail):
                self.write(base + tail)
                self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_ignored_nonfinite_bad_numbers_and_utf8_fail_closed(self):
        base = json.dumps(whole([marker()])).encode()[:-1]
        for raw in (b'NaN', b'Infinity', b'1e999', b'01', b'1.', b'+1',
                    b'-.1', b'truefalse', b'"\xff"', b'"bad\\q"', b'"control\x01"'):
            with self.subTest(raw=raw):
                self.write(base + b',"ignored":' + raw + b'}')
                self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_truncated_or_trailing_document_never_aggregates_partial_markers(self):
        raw = json.dumps(whole([marker()])).encode()
        for changed in (raw[:-1], raw + b'{}', raw + b'\xff',
                        raw[:-1] + b',"ignored":[0,]}'):
            with self.subTest(changed=changed[-20:]):
                self.write(changed)
                with mock.patch.object(bc._Completed, 'marker') as aggregate:
                    self.unknown(bc.collect(self.task, self.snapshot))
                aggregate.assert_not_called()

    def test_whole_string_table_is_bounded_and_all_entries_are_strings(self):
        for strings in (['Object', False], ['Object'] * 4):
            data = whole([marker()]); data['threads'][0]['stringArray'] = strings
            self.write(json.dumps(data).encode())
            with mock.patch.object(bc, 'MAX_ROWS', 3):
                self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_category_stack_counts_and_ignored_values_are_validated(self):
        for key in ('category', 'stack'):
            data = whole([marker()]);data['threads'][0]['markers'][key] = []
            self.write(json.dumps(data).encode())
            self.unknown(bc.collect(self.task, self.snapshot))
        raw = json.dumps(whole([marker()])).encode().replace(b'"stack": [null]', b'"stack": [1e999]')
        self.write(raw);self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_structural_depth_key_and_per_element_limits_fail_closed(self):
        data = whole([marker()]); data['ignored'] = {'x' * (bc.MAX_KEY_BYTES + 1): 1}
        self.write(json.dumps(data).encode());self.unknown(bc.collect(self.task, self.snapshot))
        data['ignored'] = {str(i): 1 for i in range(bc.MAX_OBJECT_KEYS + 1)}
        self.write(json.dumps(data).encode());self.unknown(bc.collect(self.task, self.snapshot))
        nested = 1
        for _ in range(bc.MAX_DEPTH + 1): nested = [nested]
        data['ignored'] = nested
        self.write(json.dumps(data).encode());self.unknown(bc.collect(self.task, self.snapshot))
        data = whole([marker()]); data['threads'][0]['markers']['data'][0]['ignored'] = ['x' * 100000] * 11
        self.write(json.dumps(data).encode());self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_shares_original_processing_deadline_without_partial_counts(self):
        self.write(json.dumps(whole([marker()])).encode())
        with mock.patch.object(bc.time, 'monotonic', side_effect=[0, 31]):
            value = bc.collect(self.task, self.snapshot)
        self.unknown(value)
        self.assertEqual(value['reason'], 'build-completion-processing-limit')

    def test_whole_pread_failure_is_private_and_closes_descriptor(self):
        self.write(json.dumps(whole([marker()])).encode());descriptors = []
        def fail(fd, size, offset):
            descriptors.append(fd)
            raise OSError('PRIVATE_SENTINEL')
        with mock.patch.object(bc.os, 'pread', side_effect=fail):
            value = bc.collect(self.task, self.snapshot)
        self.unknown(value)
        self.assertEqual(value['reason'], 'build-completion-unavailable')
        self.assertEqual(len(set(descriptors)), 1)
        with self.assertRaises(OSError):os.fstat(descriptors[0])

    def test_whole_stability_recheck_discards_success_after_file_change(self):
        path = self.write(json.dumps(whole([marker()])).encode());original = bc._whole_stream
        def change(*args):
            value = original(*args)
            with path.open('ab') as stream:stream.write(b' ')
            return value
        with mock.patch.object(bc, '_whole_stream', side_effect=change):
            self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_fast_and_structural_cursors_agree_on_same_valid_and_invalid_values(self):
        initial = whole([marker(), marker('Program', 'xpcshell', end=None), phase()])
        cases = [initial]
        for update in ({'name': [False, 1, 2]}, {'phase': [0, 0, 1]},
                       {'data': [None, {'type': 'Text', 'text': 'xpcshell'}, {'type': 'Phase', 'phase': 'compile'}]}):
            value = copy.deepcopy(initial);value['threads'][0]['markers'].update(update);cases.append(value)
        for data in cases:
            self.write(json.dumps(data).encode())
            fast = bc.collect(self.task, self.snapshot)
            with mock.patch.object(bc._JSONStream, 'small_value', return_value=bc._UNDECODED), \
                    mock.patch.object(bc._JSONStream, 'array_batch', return_value=bc._UNDECODED):
                structural = bc.collect(self.task, self.snapshot)
            self.assertEqual(fast, structural)

    def test_whole_fast_numeric_or_literal_prefix_at_chunk_boundary_cannot_confirm(self):
        base = json.dumps(whole([marker()]), separators=(',', ':')).encode()[:-1]
        for token in (b'1e999', b'01', b'truefalse', b'nullfalse', b'1.2.3'):
            self.write(base + b',"ignored":' + token + b'}')
            for size in (1, 2, 3, 7):
                with self.subTest(token=token, size=size), mock.patch.object(bc, 'READ_BYTES', size):
                    self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_fast_bounds_match_structural_for_decoded_keys_and_scalar_depth(self):
        for text in ('\u2603' * 85, '\u2603' * 86):
            data = whole([marker()]);data['ignored'] = {text: 1}
            self.write(json.dumps(data).encode())
            fast = bc.collect(self.task, self.snapshot)
            with mock.patch.object(bc._JSONStream, 'small_value', return_value=bc._UNDECODED), \
                    mock.patch.object(bc._JSONStream, 'array_batch', return_value=bc._UNDECODED):
                self.assertEqual(fast, bc.collect(self.task, self.snapshot))
            self.assertEqual(fast['status'], 'observed' if len(text) == 85 else 'unconfirmed')
        data = whole([marker()]);nested = 1
        for _ in range(bc.MAX_DEPTH):nested = [nested]
        data['ignored'] = nested
        self.write(json.dumps(data).encode())
        fast = bc.collect(self.task, self.snapshot)
        with mock.patch.object(bc._JSONStream, 'small_value', return_value=bc._UNDECODED), \
                mock.patch.object(bc._JSONStream, 'array_batch', return_value=bc._UNDECODED):
            self.assertEqual(fast, bc.collect(self.task, self.snapshot))
        self.unknown(fast)

    def test_whole_fast_decoder_rejects_boolean_phase_even_with_valid_null_end(self):
        for end, phase_value in ((None, False), (2, True)):
            data = whole([marker(end=end)])
            data['threads'][0]['markers']['phase'] = [phase_value]
            self.write(json.dumps(data).encode())
            self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_one_million_small_resource_rows_remain_processable(self):
        resource = marker('CPU', 'unused')
        resource['data'] = {'type': 'CPU', 'user': 1.25, 'system': 2.5, 'idle': 97.5,
                            'cores': [1.0] * 8, 'PRIVATE_SENTINEL': 'x' * 64}
        raw = json.dumps(whole([phase(), marker(), *([resource] * 1000000)]),
                         separators=(',', ':')).encode()
        self.assertGreater(len(raw), 16 * 1024**2)
        self.write(raw)
        started = time.monotonic()
        value = bc.collect(self.task, self.snapshot)
        elapsed = time.monotonic() - started
        from unittest_receipt import DENSITY_ID, density_public
        diagnostic = density_public({'schema': 1, 'case': DENSITY_ID,
            'status': value['status'], 'reason': value['reason'], 'elapsedSeconds': elapsed,
            'inputBytes': len(raw), 'inputSHA256': hashlib.sha256(raw).hexdigest(),
            'resourceRows': 1000000, 'processingSeconds': bc.PROCESSING_SECONDS,
            'completeCoverage': value['completeCoverage'],
            'objectCount': (value['finishedTargetMarkers'] or {}).get('Object'),
            'compileCount': (value['finishedTierMarkers'] or {}).get('compile')})
        print(json.dumps({'buildCompletionDensityDiagnostic': diagnostic}), flush=True)
        self.assertEqual(value['status'], 'observed')
        self.assertEqual(value['finishedTargetMarkers']['Object'], 1)
        self.assertEqual(value['finishedTierMarkers']['compile'], 1)
        self.assertNotIn('PRIVATE_SENTINEL', json.dumps(value))

    def test_whole_batches_with_quoted_container_delimiters_match_generic_validation(self):
        resource = marker('CPU', 'unused')
        resource['data'] = {'type': 'CPU', 'quoted': '}, ], \", literal', 'nested': {'a': [1, 2]}}
        data = whole([marker(), *([resource] * 40)])
        self.write(json.dumps(data).encode())
        with mock.patch.object(bc, 'READ_BYTES', 127):
            fast = bc.collect(self.task, self.snapshot)
            with mock.patch.object(bc._JSONStream, 'small_value', return_value=bc._UNDECODED), \
                    mock.patch.object(bc._JSONStream, 'array_batch', return_value=bc._UNDECODED):
                self.assertEqual(fast, bc.collect(self.task, self.snapshot))
        self.assertEqual(fast['status'], 'observed')
        for tail in ('], comma,', {'nested': [1, 2]}, [1, 2], None):
            data = whole([marker()]);data['ignored'] = [1] * 20000 + [tail]
            data['following'] = {'array': [1, 2], 'text': '] ,'}
            self.write(json.dumps(data).encode())
            fast = bc.collect(self.task, self.snapshot)
            with mock.patch.object(bc._JSONStream, 'small_value', return_value=bc._UNDECODED), \
                    mock.patch.object(bc._JSONStream, 'array_batch', return_value=bc._UNDECODED):
                self.assertEqual(fast, bc.collect(self.task, self.snapshot))
            self.assertEqual(fast['status'], 'observed')
        base = json.dumps(whole([marker()])).encode()[:-1]
        for suffix in (b',"ignored":[{"a":"},","b":[1,2]},]}',
                       b',"ignored":[1,,2]}', b',"ignored":[,1]}', b',"ignored":[1,2 3]}'):
            self.write(base + suffix)
            self.unknown(bc.collect(self.task, self.snapshot))

    def test_whole_batch_row_cap_counts_all_values_not_only_last_element(self):
        data = whole([marker()]);data['ignored'] = [0, 0, 0, 0]
        self.write(json.dumps(data).encode())
        with mock.patch.object(bc, 'MAX_ROWS', 3):
            value = bc.collect(self.task, self.snapshot)
        self.unknown(value)
        self.assertEqual(value['reason'], 'build-completion-row-limit')

    def test_processing_budget_returns_unknown_without_partial_counts(self):
        self.write(jsonl([marker()]))
        with mock.patch.object(bc.time, 'monotonic', side_effect=[0, 31]):
            result = bc.collect(self.task, self.snapshot)
        self.unknown(result);self.assertEqual(result['reason'], 'build-completion-processing-limit')

    def test_resource_rows_do_not_consume_completed_marker_cap_but_have_total_bound(self):
        resource = {'type': 'marker', 'name': 'CPU', 'startTime': 0, 'endTime': 1,
                    'data': {'type': 'CPU', 'PRIVATE_SENTINEL': 'private-resource-value'}}
        self.write(jsonl([resource] * 20 + [marker()]))
        with mock.patch.object(bc, 'MAX_MARKERS', 1):
            result = bc.collect(self.task, self.snapshot)
        self.assertEqual(result['status'], 'observed')
        self.assertEqual(result['finishedTargetMarkers']['Object'], 1)
        self.assertNotIn('PRIVATE_SENTINEL', json.dumps(result))
        with mock.patch.object(bc, 'MAX_ROWS', 20):
            result = bc.collect(self.task, self.snapshot)
        self.unknown(result);self.assertEqual(result['reason'], 'build-completion-row-limit')

    def test_read_failure_preserves_fixed_private_error_code(self):
        self.write(jsonl([marker()]))
        with mock.patch.object(bc, '_parse', side_effect=OSError('PRIVATE_SENTINEL')):
            result = bc.collect(self.task, self.snapshot)
        self.unknown(result);self.assertEqual(result['reason'], 'build-completion-unavailable')

    def test_failed_stream_creation_closes_the_exact_open_descriptor(self):
        self.write(jsonl([marker()]));descriptors = []
        def fail(descriptor, *args, **kwargs):
            descriptors.append(descriptor)
            raise OSError('PRIVATE_SENTINEL')
        with mock.patch.object(bc.os, 'fdopen', side_effect=fail):
            self.unknown(bc.collect(self.task, self.snapshot))
        self.assertEqual(len(descriptors), 1)
        with self.assertRaises(OSError):os.fstat(descriptors[0])

    def test_changed_file_after_parse_discards_all_counts(self):
        path = self.write(jsonl([marker()]));original = bc._parse
        def change(*args):
            result = original(*args)
            with path.open('ab') as stream:
                stream.write(b'\n')
            return result
        with mock.patch.object(bc, '_parse', side_effect=change):
            self.unknown(bc.collect(self.task, self.snapshot))

    def test_snapshot_validation_and_input_are_not_mutated(self):
        self.write(jsonl([marker()]));original = copy.deepcopy(self.snapshot)
        self.assertEqual(bc.collect(self.task, self.snapshot)['status'], 'observed')
        self.assertEqual(self.snapshot, original)
        for value in (None, {}, {**original, 'schema': True}, {**original, 'directoryIdentities': []},
                      {**original, 'wallTimeNs': False}, {**original, 'profiles': {'PRIVATE_SENTINEL': []}}):
            with self.subTest(value=value):self.unknown(bc.collect(self.task, value))

    def test_directory_scan_cap_and_snapshot_failures_are_fixed(self):
        for i in range(3):(self.directory / str(i)).write_bytes(b'')
        with mock.patch.object(bc, 'MAX_ENTRIES', 2):
            result = bc.start_snapshot(self.task)
        self.assertEqual(result, {'schema': 1, 'status': 'unconfirmed', 'reason': 'build-completion-directory-limit'})
        with mock.patch.object(bc, '_directories', side_effect=OSError('PRIVATE_SENTINEL')):
            result = bc.start_snapshot(self.task)
        self.assertEqual(result, {'schema': 1, 'status': 'unconfirmed', 'reason': 'build-completion-snapshot-unavailable'})


if __name__ == '__main__':
    unittest.main()
