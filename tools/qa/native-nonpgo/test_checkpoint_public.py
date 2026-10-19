# SPDX-License-Identifier: MPL-2.0
import copy
import json
import os
from pathlib import Path
import unittest
from unittest import mock

import checkpoint_public as public
import checkpoint_replay
from qa_core import Failure


class PublicRestoreTests(unittest.TestCase):
    def setUp(self):
        self.stages = [{'stage': name, 'exitCode': 0, 'reason': None, 'failurePhase': None,
                        'groupCleanup': 'verified-gone', 'leaderReap': {'status': 'reaped', 'exitCode': 0},
                        'cleanupFailure': None} for name in public.REQUIRED_STAGES]
        self.boundary = {'contract': public.BOUNDARY_CONTRACT, 'status': 'passed', 'node': public.NODE,
                         'archiveSHA256': 'a'*64, 'depInfoSHA256': 'b'*64,
                         'knownBuildSurvivors': 0, 'interrupted': False,
                         'knownBuildIdentityCheck': {'status': 'passed', 'remainingLive': 0,
                             'remainingZombies': 0, 'observedBirths': 48}, 'stages': self.stages}
        self.value = {'buildDate': '20261019035607', 'archiveSHA256': 'c'*64,
                      'manifestSHA256': 'd'*64, 'qualification': public.NORMAL_QUALIFICATION,
                      'replay': None, 'freshObjdir': False, 'sameJob': False,
                      'producerBoundary': self.boundary}

    def assert_public_failure(self, code, value=None):
        with self.assertRaises(Failure) as caught:
            public.public_restore(self.value if value is None else value)
        self.assertEqual(str(caught.exception), 'checkpoint-public-'+code)
        self.assertNotIn('PRIVATE-SENTINEL', str(caught.exception))

    def replay(self):
        config = checkpoint_replay.load_config()
        current = {'repository': checkpoint_replay.REPOSITORY, 'run': '39199999999', 'attempt': '2',
                   'head': 'e'*40, 'fixture': 'f'*40}
        patch = mock.patch.dict(os.environ, {checkpoint_replay.ENV_KEYS[k]:v for k,v in current.items()})
        patch.start()
        self.addCleanup(patch.stop)
        self.value.update(qualification=checkpoint_replay.CONTRACT, archiveSHA256=config['origin']['sha256'],
                          manifestSHA256=config['origin']['manifestSHA256'],
                          replay={'contract': config['contract'], 'origin': copy.deepcopy(config['origin']),
                                  'currentExecution': current})
        return config

    def test_minimal_normal_restore_has_only_the_fixed_public_fields(self):
        result = public.public_restore(self.value)
        self.assertEqual(set(result), {'buildDate', 'archiveSHA256', 'manifestSHA256', 'qualification',
                                     'replay', 'freshObjdir', 'sameJob', 'producerBoundary'})
        self.assertEqual(result['qualification'], self.value['qualification'])
        self.assertIsNone(result['replay'])
        self.assertEqual(result['producerBoundary'], {key:value for key,value in self.boundary.items()
            if key != 'stages'} | {'stageCount': 5, 'allStagesNaturalZero': True, 'allExactReapedZero': True,
                                 'allGroupsGone': True, 'noCleanupFailures': True})
        plan = json.loads(Path(public.__file__).with_name('plan.json').read_text())
        self.assertEqual(public.NORMAL_QUALIFICATION, plan['checkpoint']['qualification'])

    def test_unknown_nested_paths_tags_features_logs_measurements_are_omitted(self):
        sentinel = {'nested': ['/PRIVATE-SENTINEL', {'tag': 'PRIVATE-SENTINEL'}]}
        self.value.update(unknown=sentinel, logs=sentinel)
        self.boundary.update(archive='/PRIVATE-SENTINEL/archive', depInfo='/PRIVATE-SENTINEL/dep',
                             profile=sentinel, features=sentinel, resourceScope=sentinel,
                             lastActivity=sentinel, stageCount=999, allStagesNaturalZero=False)
        self.boundary['knownBuildIdentityCheck'].update(measurement=sentinel, scope=sentinel)
        for row in self.stages:
            row.update(lastActivity=sentinel, tags=sentinel, log=sentinel)
            row['leaderReap']['unknown'] = sentinel
        result = public.public_restore(self.value)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))
        self.assertNotIn('measurement', result['producerBoundary']['knownBuildIdentityCheck'])
        self.assertNotIn('stages', result['producerBoundary'])
        self.assertEqual(result['producerBoundary']['stageCount'], 5)
        self.assertIs(result['producerBoundary']['allStagesNaturalZero'], True)

    def test_additional_preceding_stages_contribute_only_to_aggregate_proof(self):
        extra = copy.deepcopy(self.stages[0])
        extra.update(stage='/PRIVATE-SENTINEL/unknown-stage', lastActivity={'tag': 'PRIVATE-SENTINEL'})
        self.stages.insert(0, extra)
        result = public.public_restore(self.value)
        self.assertEqual(result['producerBoundary']['stageCount'], 6)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))

    def test_top_types_hashes_date_and_false_scope_flags_are_required(self):
        for key, value in [('buildDate', '/PRIVATE-SENTINEL'), ('buildDate', 20261019035607),
                           ('archiveSHA256', 'PRIVATE-SENTINEL'), ('manifestSHA256', None),
                           ('freshObjdir', 0), ('freshObjdir', True), ('sameJob', 0), ('sameJob', None)]:
            self.assert_public_failure('restore-fields', {**self.value, key:value})
        for value in ([], False, 'PRIVATE-SENTINEL'):
            self.assert_public_failure('restore-shape', value)

    def test_only_the_two_known_qualification_values_are_accepted(self):
        for value in (None, [], 'PRIVATE-SENTINEL', 'verified-origin-checkpoint-restore-only'):
            self.assert_public_failure('qualification', {**self.value, 'qualification': value})
        self.assert_public_failure('unexpected-replay', {**self.value, 'replay': {'PRIVATE-SENTINEL': True}})

    def test_boundary_contract_status_node_hashes_and_typed_zero_are_required(self):
        for key, value in [('contract', 'PRIVATE-SENTINEL'), ('status', 'failed'), ('node', '/PRIVATE-SENTINEL'),
                           ('archiveSHA256', None), ('depInfoSHA256', 'PRIVATE-SENTINEL'),
                           ('knownBuildSurvivors', False), ('knownBuildSurvivors', 0.0),
                           ('knownBuildSurvivors', 1), ('interrupted', 0), ('interrupted', True)]:
            self.assert_public_failure('boundary', {**self.value, 'producerBoundary': {**self.boundary, key:value}})
        self.assert_public_failure('boundary', {**self.value, 'producerBoundary': None})

    def test_identity_aggregate_requires_passed_typed_zeros_and_positive_birth_count(self):
        check = self.boundary['knownBuildIdentityCheck']
        for key, value in [('status', 'PRIVATE-SENTINEL'), ('remainingLive', False), ('remainingLive', None),
                           ('remainingZombies', 0.0), ('remainingZombies', 1), ('observedBirths', True),
                           ('observedBirths', 0), ('observedBirths', -1), ('observedBirths', '48')]:
            boundary = {**self.boundary, 'knownBuildIdentityCheck': {**check, key:value}}
            self.assert_public_failure('identity-check', {**self.value, 'producerBoundary':boundary})
        self.assert_public_failure('identity-check', {**self.value, 'producerBoundary':
                                  {**self.boundary, 'knownBuildIdentityCheck':None}})

    def test_every_required_stage_field_must_be_present(self):
        for key in public.STAGE_KEYS:
            stages = copy.deepcopy(self.stages)
            stages[0].pop(key)
            self.assert_public_failure('stage-shape', {**self.value, 'producerBoundary':{**self.boundary, 'stages':stages}})
        for stages in ([], {}, None, [None], [{**self.stages[0], 'stage':None}]):
            self.assert_public_failure('stage-shape', {**self.value, 'producerBoundary':{**self.boundary, 'stages':stages}})

    def test_required_stage_names_must_each_appear_once(self):
        for stages in (self.stages[:-1], self.stages+[self.stages[-1]]):
            self.assert_public_failure('required-stages', {**self.value, 'producerBoundary':{**self.boundary, 'stages':stages}})

    def test_false_float_string_and_nonzero_exit_codes_never_become_natural_zero(self):
        for exit_code in (False, True, 0.0, '0', None, 1, -9):
            stages = copy.deepcopy(self.stages)
            stages[0]['exitCode'] = exit_code
            self.assert_public_failure('stage-completion', {**self.value, 'producerBoundary':{**self.boundary, 'stages':stages}})

    def test_reap_group_cleanup_and_natural_completion_must_all_be_proven(self):
        for changes in ({'reason':'PRIVATE-SENTINEL'}, {'failurePhase':'PRIVATE-SENTINEL'},
                        {'cleanupFailure':{'PRIVATE-SENTINEL': True}}, {'groupCleanup':None},
                        {'leaderReap':None}, {'leaderReap':{'status':'reaped', 'exitCode':False}},
                        {'leaderReap':{'status':'reaped', 'exitCode':0.0}},
                        {'leaderReap':{'status':'unreaped', 'exitCode':0}},
                        {'leaderReap':{'status':'reaped'}}):
            stages = copy.deepcopy(self.stages)
            stages[0].update(changes)
            self.assert_public_failure('stage-completion', {**self.value, 'producerBoundary':{**self.boundary, 'stages':stages}})

    def test_fixed_origin_replay_preserves_only_typed_public_origin_and_current_execution(self):
        self.replay()
        result = public.public_restore(self.value)
        self.assertEqual(result['replay'], self.value['replay'])
        self.assertIsNot(result['replay']['origin'], self.value['replay']['origin'])
        self.assertIsNot(result['replay']['currentExecution'], self.value['replay']['currentExecution'])
        self.assertEqual(result['qualification'], checkpoint_replay.CONTRACT)

    def test_replay_shape_origin_keysets_and_types_cannot_hide_private_fields(self):
        self.replay()
        replay = self.value['replay']
        for value in (None, {}, {**replay, 'contract':'PRIVATE-SENTINEL'},
                      {**replay, 'private':'PRIVATE-SENTINEL'}):
            self.assert_public_failure('replay-shape', {**self.value, 'replay':value})
        for changes in ({'bytes':True}, {'run':1}, {'artifact':0}, {'private':'PRIVATE-SENTINEL'}):
            value = {**replay, 'origin':{**replay['origin'], **changes}}
            self.assert_public_failure('replay-origin', {**self.value, 'replay':value})

    def test_replay_payload_hashes_and_build_date_remain_bound_to_fixed_origin(self):
        self.replay()
        for changes in ({'buildDate':'20261019000000'}, {'archiveSHA256':'0'*64},
                        {'manifestSHA256':'0'*64}):
            self.assert_public_failure('replay-payload', {**self.value, **changes})

    def test_replay_current_execution_schema_and_real_environment_binding_are_required(self):
        self.replay()
        replay = self.value['replay']
        current = replay['currentExecution']
        for key in checkpoint_replay.EXECUTION_KEYS:
            value = {**replay, 'currentExecution': {k:v for k,v in current.items() if k != key}}
            self.assert_public_failure('current-execution', {**self.value, 'replay':value})
        for changes in ({'head':'/PRIVATE-SENTINEL'}, {'fixture':None}, {'run':True}, {'attempt':'0'},
                        {'repository':'PRIVATE-SENTINEL'}, {'private':'PRIVATE-SENTINEL'}):
            value = {**replay, 'currentExecution':{**current, **changes}}
            self.assert_public_failure('current-execution', {**self.value, 'replay':value})
        for key, env in ((key, checkpoint_replay.ENV_KEYS[key]) for key in checkpoint_replay.EXECUTION_KEYS):
            with mock.patch.dict(os.environ, {env:'PRIVATE-SENTINEL'}):
                self.assert_public_failure('current-environment')
            with mock.patch.dict(os.environ):
                os.environ.pop(env, None)
                self.assert_public_failure('current-environment')

    def test_same_run_or_origin_fixture_replay_is_not_a_reviewed_new_execution(self):
        config = self.replay()
        for key in ('run', 'fixture'):
            replay = copy.deepcopy(self.value['replay'])
            replay['currentExecution'][key] = config['origin'][key]
            with mock.patch.dict(os.environ, {checkpoint_replay.ENV_KEYS[key]:config['origin'][key]}):
                self.assert_public_failure('current-environment', {**self.value, 'replay':replay})

    def test_config_read_failure_is_replaced_with_fixed_public_code(self):
        self.replay()
        with mock.patch.object(public, 'load_config', side_effect=Failure('PRIVATE-SENTINEL')):
            self.assert_public_failure('replay-config')

    def test_projection_success_and_failure_never_mutate_or_alias_private_inputs(self):
        self.replay()
        before, environment = copy.deepcopy(self.value), dict(os.environ)
        result = public.public_restore(self.value)
        result['producerBoundary']['knownBuildIdentityCheck']['observedBirths'] = 1
        result['replay']['origin']['artifact'] = 1
        self.assertEqual(self.value, before)
        self.assertEqual(dict(os.environ), environment)
        broken = copy.deepcopy(self.value)
        broken['producerBoundary']['stages'][0]['exitCode'] = False
        snapshot = copy.deepcopy(broken)
        self.assert_public_failure('stage-completion', broken)
        self.assertEqual(broken, snapshot)
        self.assertEqual(dict(os.environ), environment)


if __name__ == '__main__':
    unittest.main()
