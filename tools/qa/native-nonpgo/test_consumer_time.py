# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
import os
from pathlib import Path
import unittest

import consumer_time as timing
from qa_core import Failure


class ConsumerTimeTests(unittest.TestCase):
    def setUp(self):
        self.path = Path(timing.__file__).with_name('plan.json')
        self.raw = self.path.read_bytes()
        self.plan = json.loads(self.raw)
        self.checksum = hashlib.sha256(self.raw).hexdigest()

    def select(self, **changes):
        values = {'plan': self.plan, 'plan_sha256': self.checksum, 'checkpoint_role': 'consumer',
                  'shared': True, 'restore_only': False}
        values.update(changes)
        return timing.select_consumer_time(**values)

    def assert_fixed_failure(self, code, **changes):
        with self.assertRaises(Failure) as caught:
            self.select(**changes)
        self.assertEqual(str(caught.exception), 'consumer-time-'+code)
        self.assertNotIn('PRIVATE-SENTINEL', str(caught.exception))

    def test_default_returns_only_copied_legacy_deadlines_and_no_evidence(self):
        result = timing.select_consumer_time(self.plan, self.checksum, 'consumer')
        self.assertEqual(set(result), {'deadlines','evidence'})
        self.assertEqual(result['deadlines'], dict(zip(timing.KEYS, (60,300,315,335,345))))
        self.assertIsNone(result['evidence'])
        result['deadlines']['consumerBuildDeadlineMinutes'] = 1
        self.assertEqual(self.plan['checkpoint']['consumerBuildDeadlineMinutes'],300)

    def test_default_does_not_restrict_producer_nonconsumer_roles_or_bind_a_new_plan(self):
        for role in (None, 'producer', 'other-legacy-role', False):
            for restore_only in (False, True):
                result = self.select(shared=False, checkpoint_role=role,
                                     plan_sha256='legacy-hash', restore_only=restore_only)
                self.assertIsNone(result['evidence'])
                self.assertEqual(result['deadlines']['consumerBuildDeadlineMinutes'],300)

    def test_default_reads_legacy_caps_without_applying_the_shared_profile_guards(self):
        plan = copy.deepcopy(self.plan)
        plan['checkpoint']['consumerBuildDeadlineMinutes'] = 299
        plan['limits']['jobMinutes'] = 720
        result = self.select(plan=plan, shared=False, plan_sha256='legacy-hash')
        self.assertEqual(result['deadlines']['consumerBuildDeadlineMinutes'],299)
        self.assertEqual(plan['limits']['jobMinutes'],720)
        self.assertIsNone(result['evidence'])

    def test_shared_consumer_uses_one_absolute_final_work_deadline(self):
        result = self.select()
        self.assertEqual(result['deadlines'], dict(zip(timing.KEYS, (60,345,345,345,345))))
        self.assertEqual(result['evidence']['profile'],'consumer-shared-final-deadline-v1')
        self.assertEqual(result['evidence']['schema'],1)
        self.assertEqual(result['evidence']['baselinePlanSHA256'],self.checksum)
        self.assertEqual(result['evidence']['deadlines'],result['deadlines'])
        self.assertEqual(result['evidence']['legacyDeadlines'],dict(zip(timing.KEYS,timing.LEGACY)))

    def test_evidence_budget_reserves_exactly_fifteen_minutes_within_platform(self):
        value=self.select()['evidence']
        self.assertEqual(value['platformMinutes'],360)
        self.assertEqual(value['sharedWorkDeadlineMinutes'],345)
        self.assertEqual(value['cleanupReserveMinutes'],15)
        self.assertEqual(value['sharedWorkDeadlineMinutes']+value['cleanupReserveMinutes'],
                         value['platformMinutes'])
        self.assertEqual(value['clock'],'fresh-job-monotonic-origin')
        self.assertEqual(value['deadlineReference'],'absolute-from-existing-job-origin')
        self.assertIs(value['clockResetAllowed'],False)
        self.assertIs(value['stageBudgetsAreAdditive'],False)
        self.assertIs(value['onlyExecutionTimingChanged'],True)
        self.assertIs(value['resourceAndCompilationInputsChanged'],False)

    def test_later_stages_have_remaining_job_time_not_a_fresh_345_minute_budget(self):
        value=self.select()['deadlines']
        for stage in timing.KEYS[1:4]:
            self.assertEqual(value[stage]-344,1)
            self.assertFalse(345<value[stage])
        self.assertEqual(set(value[stage] for stage in timing.KEYS[1:4]),{value['finalDeadlineMinutes']})

    def test_shared_profile_is_consumer_only(self):
        for role in (None,'producer','PRIVATE-SENTINEL','',False,1,[]):
            self.assert_fixed_failure('role',checkpoint_role=role)

    def test_restore_only_cannot_enable_shared_work_profile(self):
        self.assert_fixed_failure('restore-only',restore_only=True)

    def test_explicit_flags_are_strict_booleans_in_both_modes(self):
        for value in (0,1,None,'true','PRIVATE-SENTINEL',[]):
            self.assert_fixed_failure('flag',shared=value)
            self.assert_fixed_failure('flag',restore_only=value)
            self.assert_fixed_failure('flag',shared=False,restore_only=value)

    def test_shared_plan_hash_is_fixed_and_typed(self):
        for value in (None,True,0,'0'*64,'PRIVATE-SENTINEL'):
            self.assert_fixed_failure('plan-binding',plan_sha256=value)

    def test_all_five_legacy_caps_and_platform_cap_must_be_strict_integers(self):
        for key in (*timing.KEYS,'jobMinutes'):
            for value in (True,False,60.0,'60',None):
                plan=copy.deepcopy(self.plan)
                plan['limits' if key=='jobMinutes' else 'checkpoint'][key]=value
                self.assert_fixed_failure('caps',plan=plan)

    def test_changed_but_ordered_legacy_caps_cannot_silently_select_a_new_profile(self):
        for key,value in zip(timing.KEYS,(61,301,316,336,346)):
            plan=copy.deepcopy(self.plan);plan['checkpoint'][key]=value
            self.assert_fixed_failure('order' if key=='finalDeadlineMinutes' else 'caps',plan=plan)
        plan=copy.deepcopy(self.plan);plan['limits']['jobMinutes']=361
        self.assert_fixed_failure('order',plan=plan)

    def test_zero_negative_reversed_and_platform_or_cleanup_overruns_fail_closed(self):
        for key,value in [('consumerRestoreDeadlineMinutes',0),('consumerBuildDeadlineMinutes',-1),
                          ('consumerRestoreDeadlineMinutes',301),('consumerPackageDeadlineMinutes',299),
                          ('consumerNativeDeadlineMinutes',346),('finalDeadlineMinutes',335)]:
            plan=copy.deepcopy(self.plan);plan['checkpoint'][key]=value
            self.assert_fixed_failure('order',plan=plan)
        for platform in (0,345,359,375):
            plan=copy.deepcopy(self.plan);plan['limits']['jobMinutes']=platform
            self.assert_fixed_failure('order',plan=plan)

    def test_missing_plan_fields_fail_with_fixed_private_codes(self):
        for key in timing.KEYS:
            plan=copy.deepcopy(self.plan);plan['checkpoint'].pop(key)
            self.assert_fixed_failure('plan-shape',plan=plan)
        plan=copy.deepcopy(self.plan);plan['limits'].pop('jobMinutes')
        self.assert_fixed_failure('plan-shape',plan=plan)
        for plan in (None,{}, {'checkpoint':'PRIVATE-SENTINEL'}):
            self.assert_fixed_failure('plan-shape',plan=plan)

    def test_selector_never_changes_plan_resource_compilation_or_environment_inputs(self):
        snapshot=copy.deepcopy(self.plan);environment=dict(os.environ)
        result=self.select()
        self.assertEqual(self.plan,snapshot)
        self.assertEqual(self.path.read_bytes(),self.raw)
        self.assertEqual(hashlib.sha256(self.raw).hexdigest(),timing.BASELINE_PLAN_SHA256)
        self.assertEqual(dict(os.environ),environment)
        result['deadlines']['consumerBuildDeadlineMinutes']=1
        result['evidence']['deadlines']['consumerPackageDeadlineMinutes']=2
        result['evidence']['legacyDeadlines']['consumerRestoreDeadlineMinutes']=3
        self.assertEqual(self.plan,snapshot)
        self.assertEqual(self.select()['deadlines']['consumerBuildDeadlineMinutes'],345)

    def test_returned_deadlines_and_each_evidence_mapping_do_not_alias(self):
        result=self.select()
        self.assertIsNot(result['deadlines'],result['evidence']['deadlines'])
        self.assertIsNot(result['deadlines'],result['evidence']['legacyDeadlines'])
        result['deadlines']['consumerBuildDeadlineMinutes']=1
        self.assertEqual(result['evidence']['deadlines']['consumerBuildDeadlineMinutes'],345)
        result['evidence']['legacyDeadlines']['consumerBuildDeadlineMinutes']=2
        self.assertEqual(result['evidence']['deadlines']['consumerBuildDeadlineMinutes'],345)

    def test_failed_selection_keeps_inputs_unchanged_and_never_publishes_free_text(self):
        plan=copy.deepcopy(self.plan)
        plan['checkpoint']['consumerBuildDeadlineMinutes']='PRIVATE-SENTINEL'
        snapshot=copy.deepcopy(plan)
        self.assert_fixed_failure('caps',plan=plan)
        self.assertEqual(plan,snapshot)
        result=self.select()
        self.assertNotIn('PRIVATE-SENTINEL',json.dumps(result))
        self.assertNotIn('roots',result['evidence'])


if __name__=='__main__':
    unittest.main()
