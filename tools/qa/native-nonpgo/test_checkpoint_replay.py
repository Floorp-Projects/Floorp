# SPDX-License-Identifier: MPL-2.0
import copy
import datetime
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import checkpoint_replay as replay
from qa_core import Failure


class ReplayGuards(unittest.TestCase):
    def setUp(self):
        self.config = replay.load_config()
        self.origin = copy.deepcopy(self.config['origin'])
        self.now = datetime.datetime(2026, 10, 19, 12, tzinfo=datetime.timezone.utc)
        self.current = {
            'planSHA256': 'a'*64, 'roots': {name: '/private/fixture/'+name for name in replay.ROOT_KEYS},
            'fixture': 'c'*40, 'harness': 'd'*40, 'runtime': 'e'*40,
            'repository': replay.REPOSITORY, 'run': '39199999999', 'attempt': '1',
            'head': 'f'*40, 'image': '20260824.0482.1', 'pythonSHA256': 'b'*64}
        self.environment = {env: self.current[key] for key, env in replay.ENV_KEYS.items()}
        patch = mock.patch.dict(os.environ, self.environment)
        patch.start()
        self.addCleanup(patch.stop)
        self.receipt = {'contract': replay.CONTRACT, 'origin': copy.deepcopy(self.origin),
                        'currentExecution': {key: self.current[key] for key in replay.EXECUTION_KEYS}}
        self.manifest = copy.deepcopy(self.current)
        self.manifest.update({key: self.origin[key] for key in replay.HISTORIC_KEYS})
        self.run = {'id': int(self.origin['run']), 'repository': {'full_name': replay.REPOSITORY},
                    'head_sha': self.origin['head'], 'run_attempt': 1, 'event': 'push',
                    'status': 'completed', 'conclusion': 'failure', 'private': 'PRIVATE-SENTINEL'}
        self.jobs = {'total_count': 2, 'jobs': [
            {'id': self.origin['producerJob'], 'run_id': int(self.origin['run']),
             'name': 'checkpoint-producer', 'status': 'completed', 'conclusion': 'success',
             'labels': ['macos-15-intel']},
            {'id': 117903013507, 'run_id': int(self.origin['run']), 'name': 'checkpoint-consumer',
             'status': 'completed', 'conclusion': 'failure', 'labels': ['macos-15-intel']}]}
        self.artifact = {'id': self.origin['artifact'], 'name': 'checkpoint.tar.gz', 'expired': False,
            'size_in_bytes': self.origin['bytes'], 'digest': 'sha256:'+self.origin['sha256'],
            'workflow_run': {'id': int(self.origin['run']), 'head_sha': self.origin['head']},
            'created_at': self.origin['createdAt'], 'expires_at': self.origin['expiresAt'],
            'signed_url': 'https://PRIVATE-SENTINEL.invalid'}
        self.artifacts = {'total_count': 1, 'artifacts': [copy.deepcopy(self.artifact)]}

    def validate_origin(self, **changes):
        values = {'config': self.config, 'source_run': self.run, 'source_jobs': self.jobs,
                  'artifact': self.artifact, 'source_artifacts': self.artifacts, 'now': self.now}
        values.update(changes)
        return replay.validate_origin(**values)

    def assert_private_failure(self, code, call):
        with self.assertRaises(Failure) as failure:
            call()
        self.assertEqual(str(failure.exception), code)
        self.assertNotIn('PRIVATE-SENTINEL', str(failure.exception))

    def test_exact_config_is_a_fresh_value_and_not_a_mutable_origin_alias(self):
        self.assertEqual(self.config['origin'], replay.ORIGIN)
        self.assertIsNot(self.config['origin'], replay.ORIGIN)
        self.config['origin']['run'] = '1'
        self.assertEqual(replay.load_config()['origin'], replay.ORIGIN)

    def test_config_contract_schema_origin_and_build_date_are_fixed(self):
        cases = []
        for key, value, code in [('schema', True, 'shape'), ('contract', 'PRIVATE-SENTINEL', 'contract'),
                                 ('buildDate', '20261019000000', 'build-date')]:
            config = copy.deepcopy(self.config)
            config[key] = value
            cases.append((config, code))
        for key, value in [('run', '1'), ('attempt', 1), ('bytes', True),
                           ('manifestSHA256', '0'*64), ('sha256', '0'*64),
                           ('createdAt', '2026-10-19T05:20:30Z'), ('expiresAt', '2026-10-21T05:19:56Z')]:
            config = copy.deepcopy(self.config)
            config['origin'][key] = value
            cases.append((config, 'origin'))
        cases.extend([({**self.config, 'private': 'PRIVATE-SENTINEL'}, 'shape'),
                      ({**self.config, 'origin': {**self.origin, 'private': 'PRIVATE-SENTINEL'}}, 'origin')])
        for config, suffix in cases:
            with self.subTest(suffix=suffix):
                self.assert_private_failure('checkpoint-replay-config-'+suffix,
                    lambda: self.validate_origin(config=config))

    def test_config_read_errors_invalid_json_size_and_duplicate_keys_stay_fixed(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'PRIVATE-SENTINEL.json'
            self.assert_private_failure('checkpoint-replay-config-read', lambda: replay.load_config(path))
            for raw, code in [(b'PRIVATE-SENTINEL', 'read'), (b'\xff', 'read'),
                              (b' '*16385, 'shape'),
                              (b'{"schema":1,"schema":1}', 'shape')]:
                path.write_bytes(raw)
                self.assert_private_failure('checkpoint-replay-config-'+code, lambda: replay.load_config(path))

    def test_exact_completed_failed_run_with_successful_producer_and_sole_artifact_is_accepted(self):
        proof = self.validate_origin()
        self.assertTrue(proof['originVerified'])
        self.assertTrue(proof['producerJobVerified'])
        self.assertTrue(proof['soleArtifactVerified'])
        self.assertTrue(proof['unexpiredAtVerification'])
        self.assertFalse(proof['strictSameRunQualified'])
        self.assertFalse(proof['productQualification'])
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(proof))

    def test_wrong_run_repository_head_attempt_event_status_or_conclusion_rejects(self):
        for changes in ({'id': 1}, {'repository': {'full_name': 'PRIVATE-SENTINEL'}},
                        {'head_sha': '0'*40}, {'run_attempt': True}, {'run_attempt': 2},
                        {'event': 'workflow_dispatch'}, {'status': 'in_progress'},
                        {'conclusion': 'success'}, {'repository': None}):
            with self.subTest(changes=changes):
                self.assert_private_failure('checkpoint-replay-source-run',
                    lambda: self.validate_origin(source_run={**self.run, **changes}))

    def test_jobs_must_be_complete_unique_and_bound_to_the_origin_run(self):
        cases = [{'total_count': 3, 'jobs': self.jobs['jobs']}, {'total_count': 2, 'jobs': []},
                 {'total_count': True, 'jobs': self.jobs['jobs']}, {'total_count': 2, 'jobs': [None, None]}]
        for change in ({'run_id': 1}, {'id': self.origin['producerJob']}, {'id': True}):
            value = copy.deepcopy(self.jobs)
            value['jobs'][1].update(change)
            cases.append(value)
        for value in cases:
            with self.subTest(value=value):
                self.assert_private_failure('checkpoint-replay-source-jobs',
                    lambda: self.validate_origin(source_jobs=value))

    def test_exact_producer_identity_name_success_status_and_intel_labels_are_required(self):
        for changes in ({'id': 123}, {'name': 'PRIVATE-SENTINEL'}, {'conclusion': 'failure'},
                        {'status': 'in_progress'}, {'labels': ['macos-15']},
                        {'labels': ['macos-15-intel', 'self-hosted']}):
            jobs = copy.deepcopy(self.jobs)
            jobs['jobs'][0].update(changes)
            with self.subTest(changes=changes):
                self.assert_private_failure('checkpoint-replay-producer-job',
                    lambda: self.validate_origin(source_jobs=jobs))

    def test_artifact_id_origin_head_name_digest_size_times_and_expired_flag_are_fixed(self):
        changes = [{'id': 1}, {'name': 'PRIVATE-SENTINEL'}, {'expired': True}, {'expired': None},
                   {'size_in_bytes': True}, {'size_in_bytes': self.origin['bytes']+1},
                   {'digest': 'sha256:'+'0'*64}, {'workflow_run': {'id': 1, 'head_sha': self.origin['head']}},
                   {'workflow_run': {'id': int(self.origin['run']), 'head_sha': '0'*40}},
                   {'created_at': '2026-10-19T00:00:00Z'}, {'expires_at': '2026-10-21T05:19:56Z'}]
        for change in changes:
            with self.subTest(change=change):
                self.assert_private_failure('checkpoint-replay-artifact',
                    lambda: self.validate_origin(artifact={**self.artifact, **change}))

    def test_artifact_listing_must_be_sole_complete_and_match_the_individual_pin(self):
        cases = [{'total_count': 2, 'artifacts': [self.artifact]}, {'total_count': 1, 'artifacts': []},
                 {'total_count': 1, 'artifacts': [self.artifact, self.artifact]},
                 {'total_count': True, 'artifacts': [self.artifact]},
                 {'total_count': 1, 'artifacts': [{**self.artifact, 'digest': 'sha256:'+'0'*64}]},
                 {'total_count': 1, 'artifacts': [{**self.artifact, 'expired': True}]}]
        for value in cases:
            with self.subTest(value=value):
                self.assert_private_failure('checkpoint-replay-source-artifacts',
                    lambda: self.validate_origin(source_artifacts=value))

    def test_current_acquisition_time_rejects_expiry_boundary_and_future_creation(self):
        expires = datetime.datetime.fromisoformat(self.origin['expiresAt'].replace('Z', '+00:00'))
        created = datetime.datetime.fromisoformat(self.origin['createdAt'].replace('Z', '+00:00'))
        for now in (expires, expires+datetime.timedelta(seconds=1), created-datetime.timedelta(seconds=1)):
            self.assert_private_failure('checkpoint-replay-artifact-expired', lambda: self.validate_origin(now=now))
        self.assertTrue(self.validate_origin(now=expires-datetime.timedelta(microseconds=1))['originVerified'])
        for now in (datetime.datetime(2026, 10, 19), 'PRIVATE-SENTINEL', True):
            self.assert_private_failure('checkpoint-replay-clock', lambda: self.validate_origin(now=now))

    def test_exact_replay_receipt_binds_separate_actual_current_execution(self):
        proof = replay.validate_receipt(self.config, self.receipt, self.current)
        self.assertTrue(proof['currentExecutionBound'])
        self.assertTrue(proof['crossRun'])
        self.assertFalse(proof['strictSameRunQualified'])
        self.assertFalse(proof['productQualification'])
        self.assertNotIn('/private/', json.dumps(proof))

    def test_receipt_requires_explicit_contract_exact_keysets_and_origin_types(self):
        for value in (None, {}, {'origin': self.origin}, {**self.receipt, 'contract': False},
                      {**self.receipt, 'private': 'PRIVATE-SENTINEL'}):
            self.assert_private_failure('checkpoint-replay-receipt-shape',
                lambda: replay.validate_receipt(self.config, value, self.current))
        for key, value in (('bytes', True), ('attempt', 1), ('sha256', '0'*64), ('artifact', 1)):
            receipt = copy.deepcopy(self.receipt)
            receipt['origin'][key] = value
            self.assert_private_failure('checkpoint-replay-receipt-origin',
                lambda: replay.validate_receipt(self.config, receipt, self.current))
        value = copy.deepcopy(self.receipt)
        value['origin']['private'] = 'PRIVATE-SENTINEL'
        self.assert_private_failure('checkpoint-replay-receipt-origin',
            lambda: replay.validate_receipt(self.config, value, self.current))

    def test_current_execution_exact_projection_rejects_origin_spoofing_or_missing_fields(self):
        for key in replay.EXECUTION_KEYS:
            value = copy.deepcopy(self.receipt)
            value['currentExecution'][key] = 'PRIVATE-SENTINEL'
            self.assert_private_failure('checkpoint-replay-current-execution',
                lambda: replay.validate_receipt(self.config, value, self.current))
        for execution in ({}, None, {**self.receipt['currentExecution'], 'roots': 'PRIVATE-SENTINEL'}):
            value = {**self.receipt, 'currentExecution': execution}
            self.assert_private_failure('checkpoint-replay-current-execution',
                lambda: replay.validate_receipt(self.config, value, self.current))

    def test_current_context_is_bound_to_every_real_environment_field(self):
        for env in replay.ENV_KEYS.values():
            with self.subTest(env=env), mock.patch.dict(os.environ, {env: 'PRIVATE-SENTINEL'}):
                self.assert_private_failure('checkpoint-replay-current-environment',
                    lambda: replay.validate_receipt(self.config, self.receipt, self.current))
        for env in replay.ENV_KEYS.values():
            with mock.patch.dict(os.environ):
                os.environ.pop(env, None)
                self.assert_private_failure('checkpoint-replay-current-environment',
                    lambda: replay.validate_receipt(self.config, self.receipt, self.current))

    def test_same_run_and_unreviewed_origin_fixture_are_rejected_even_when_environment_matches(self):
        for key, env, suffix in (('run', 'GITHUB_RUN_ID', 'same-run'),
                                 ('fixture', 'FLOORP_FIXTURE_SHA', 'current-fixture')):
            context = {**self.current, key: self.origin[key]}
            with mock.patch.dict(os.environ, {env: self.origin[key]}):
                self.assert_private_failure('checkpoint-replay-'+suffix,
                    lambda: replay.validate_receipt(self.config, self.receipt, context))

    def test_complete_context_keysets_and_typed_hashes_are_mandatory(self):
        for key in replay.CONTEXT_KEYS:
            value = copy.deepcopy(self.current)
            value.pop(key)
            self.assert_private_failure('checkpoint-replay-context-keyset',
                lambda: replay.validate_receipt(self.config, self.receipt, value))
        self.assert_private_failure('checkpoint-replay-context-keyset',
            lambda: replay.compare_context({**self.manifest, 'private': 'PRIVATE-SENTINEL'}, self.current, self.config))
        for key, value in (('run', 123), ('attempt', True), ('head', 'PRIVATE-SENTINEL'),
                           ('planSHA256', 'a'*40), ('pythonSHA256', None), ('image', '/PRIVATE-SENTINEL')):
            context = {**self.current, key: value}
            self.assert_private_failure('checkpoint-replay-context-value',
                lambda: replay.validate_receipt(self.config, self.receipt, context))

    def test_roots_are_complete_typed_absolute_canonical_paths_without_being_projected(self):
        cases = [None, {}, {**self.current['roots'], 'private': '/PRIVATE-SENTINEL'}]
        for path in (False, ['PRIVATE-SENTINEL'], 'relative', '/private/../PRIVATE-SENTINEL',
                     '/private//PRIVATE-SENTINEL', '//private/PRIVATE-SENTINEL',
                     '/private/PRIVATE-SENTINEL\n', '/private/PRIVATE-SENTINEL\x7f',
                     '/private\\PRIVATE-SENTINEL'):
            cases.append({**self.current['roots'], 'source': path})
        for roots in cases:
            context = {**self.current, 'roots': roots}
            self.assert_private_failure('checkpoint-replay-context-roots',
                lambda: replay.validate_receipt(self.config, self.receipt, context))

    def test_context_comparison_allows_only_the_four_pinned_historic_values(self):
        proof = replay.compare_context(self.manifest, self.current, self.config)
        self.assertEqual(set(proof['checks']), replay.CONTEXT_KEYS)
        self.assertTrue(all(proof['checks'].values()))
        self.assertTrue(proof['remainingContextExact'])
        self.assertFalse(proof['strictSameRunQualified'])
        self.assertFalse(proof['productQualification'])
        self.assertNotIn('/private/', json.dumps(proof))
        for key in replay.HISTORIC_KEYS:
            value = {**self.manifest, key: '2' if key == 'attempt' else self.current[key]}
            self.assert_private_failure('checkpoint-replay-context-origin',
                lambda: replay.compare_context(value, self.current, self.config))

    def test_every_nonhistoric_context_value_still_matches_exactly(self):
        changes = {'planSHA256': 'b'*64, 'pythonSHA256': 'a'*64, 'harness': 'a'*40,
                   'runtime': 'a'*40, 'image': '20261019.0001.1',
                   'roots': {**self.manifest['roots'], 'obj': '/PRIVATE-SENTINEL'}}
        self.assertEqual(set(changes)|{'repository'}, replay.CONTEXT_KEYS-replay.HISTORIC_KEYS)
        for key, value in changes.items():
            with self.subTest(key=key):
                self.assert_private_failure('checkpoint-replay-context-incompatible',
                    lambda: replay.compare_context({**self.manifest, key: value}, self.current, self.config))
        self.assert_private_failure('checkpoint-replay-context-value',
            lambda: replay.compare_context({**self.manifest, 'repository': 'PRIVATE-SENTINEL'}, self.current, self.config))

    def test_success_and_failure_never_rewrite_contexts_receipts_or_environment(self):
        before = copy.deepcopy((self.config, self.manifest, self.current, self.receipt, dict(os.environ)))
        replay.validate_receipt(self.config, self.receipt, self.current)
        replay.compare_context(self.manifest, self.current, self.config)
        with self.assertRaises(Failure):
            replay.compare_context({**self.manifest, 'image': 'PRIVATE-SENTINEL'}, self.current, self.config)
        after = (self.config, self.manifest, self.current, self.receipt, dict(os.environ))
        self.assertEqual(before, after)

    def test_current_context_equality_alone_cannot_be_used_as_origin_replay(self):
        self.assert_private_failure('checkpoint-replay-context-origin',
            lambda: replay.compare_context(self.current, self.current, self.config))
        self.assert_private_failure('checkpoint-replay-receipt-shape',
            lambda: replay.validate_receipt(self.config, None, self.current))


if __name__ == '__main__':
    unittest.main()
