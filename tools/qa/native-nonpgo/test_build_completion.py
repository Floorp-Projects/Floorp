# SPDX-License-Identifier: MPL-2.0
import copy
import json
import os
from pathlib import Path
import tempfile
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

    def test_file_size_line_size_whole_size_and_marker_caps(self):
        for setting, cap, raw in (('MAX_BYTES', 1, jsonl([marker()])),
                                 ('MAX_LINE_BYTES', 256, jsonl([marker(text='x' * 300)])),
                                 ('MAX_WHOLE_BYTES', 32, json.dumps(whole([marker()])).encode()),
                                 ('MAX_MARKERS', 1, jsonl([marker(), marker()]))):
            with self.subTest(setting=setting):
                self.write(raw)
                with mock.patch.object(bc, setting, cap):
                    self.unknown(bc.collect(self.task, self.snapshot))

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
