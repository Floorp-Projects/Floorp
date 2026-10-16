# SPDX-License-Identifier: MPL-2.0
"""Known-bad/fixed same-policy replay, contamination and first-finding regressions."""

from copy import deepcopy
from dataclasses import replace
import hashlib
import unittest
from unittest.mock import patch

from tools.agentic_qa.agent_types import ContractError, SourceBindings
from tools.agentic_qa.fixture_support import fixture_context, fixture_records, jsonl
from tools.agentic_qa.protocol import MAX_OUTPUT_BYTES, MAX_STEPS, canonical
from tools.agentic_qa.replay import compare_synthetic_pair, regression_candidate, replay


class ReplayTests(unittest.TestCase):
    def setUp(self):
        self.context = fixture_context()
        self.actions, self.observations = fixture_records(self.context)

    def run_fixture(self, actions=None, observations=None):
        return replay(self.context, jsonl(self.actions if actions is None else actions),
                      jsonl(self.observations if observations is None else observations))

    def bad(self):
        changed = deepcopy(self.observations)
        changed[1]["redirect"][1]["userAgent"] = self.context.desktop_ua
        return changed

    def test_complete_on_off_on_is_fixture_only_and_binds_original_bytes(self):
        result = self.run_fixture()
        self.assertEqual(result["status"], "FIXTURE_PASS")
        self.assertIs(result["complete"], True)
        self.assertEqual(result["nativeExecution"], "NOT_EXECUTED")
        self.assertEqual(result["executionKind"], "offline-synthetic-fixture")
        self.assertEqual(result["productQualification"], "UNVERIFIED")
        self.assertEqual(result["workerQualification"], "UNVERIFIED")
        self.assertIs(result["publicationAuthorized"], False)
        self.assertIs(result["releaseEvidence"], False)
        self.assertEqual(result["actionWireSha256"], hashlib.sha256(jsonl(self.actions)).hexdigest())
        for index, entry in enumerate(result["steps"]):
            self.assertEqual(entry["observationSha256"],
                             hashlib.sha256(jsonl([self.observations[index]])).hexdigest())
        self.assertLessEqual(len(canonical(result)), MAX_OUTPUT_BYTES)

    def test_observe_and_acknowledged_toggles_cannot_satisfy_missing_phases(self):
        for length in (1, 2, 4, 5):
            result = self.run_fixture(self.actions[:length], self.observations[:length])
            self.assertEqual(result["status"], "INCOMPLETE")
            self.assertEqual(result["incompletionReason"], "MISSING_PHASE")
        actions = [{**self.actions[0], "actionId": "observe"}]
        observations = [{**self.observations[0], "actionId": "observe", "mobileEnabled": False}]
        self.assertEqual(self.run_fixture(actions, observations)["status"], "INCOMPLETE")

    def test_unknown_sequence_or_reordered_action_is_incomplete(self):
        for action_id in ("web-panel.mobile-off", "web-panel.navigate-two-hop"):
            actions = deepcopy(self.actions)
            actions[0]["actionId"] = action_id
            self.assertEqual(self.run_fixture(actions)["incompletionReason"], "POLICY_SEQUENCE")

    def test_wrong_ua_on_every_hop_in_every_phase_is_finding(self):
        for phase in (1, 3, 5):
            for hop in range(3):
                changed = deepcopy(self.observations)
                changed[phase]["redirect"][hop]["userAgent"] = "Synthetic wrong UA"
                with self.subTest(phase=phase, hop=hop):
                    result = self.run_fixture(observations=changed)
                    self.assertEqual(result["status"], "FINDING")
                    self.assertEqual(result["firstFinding"]["code"], "REDIRECT_ORACLE")

    def test_expected_each_phase_ua_and_case_are_fixed_independently_of_generator(self):
        phases = ((1, True, "Synthetic mobile UA", "web-panel-ua.redirect.mobile-on"),
                  (3, False, "Synthetic desktop UA", "web-panel-ua.redirect.mobile-off"),
                  (5, True, "Synthetic mobile UA", "web-panel-ua.redirect.mobile-on"))
        for phase, mobile, ua, case_id in phases:
            with self.subTest(phase=phase):
                observed = self.observations[phase]
                self.assertIs(observed["mobileEnabled"], mobile)
                self.assertEqual([event["userAgent"] for event in observed["redirect"]], [ua] * 3)
                self.assertEqual([event["caseId"] for event in observed["redirect"]], [case_id] * 3)
                self.assertEqual(observed["normalTabUas"], ["Synthetic desktop UA"])
                self.assertEqual(observed["otherPanelUas"], ["Synthetic desktop UA"])
        self.assertEqual(self.run_fixture()["status"], "FIXTURE_PASS")

    def test_both_independent_roles_cannot_leak_in_any_phase(self):
        for phase in (1, 3, 5):
            for role, code in (("normalTabUas", "NORMAL_TAB_UA"),
                               ("otherPanelUas", "OTHER_PANEL_UA")):
                changed = deepcopy(self.observations)
                changed[phase][role] = [self.context.mobile_ua]
                with self.subTest(phase=phase, role=role):
                    self.assertEqual(self.run_fixture(observations=changed)["firstFinding"]["code"], code)

    def test_ignored_toggle_is_finding_even_when_later_http_is_missing(self):
        changed = deepcopy(self.observations)
        changed[0]["mobileEnabled"] = False
        self.assertEqual(self.run_fixture(observations=changed)["firstFinding"]["code"], "MOBILE_STATE")
        changed[1]["mobileEnabled"] = False
        changed[1]["redirect"] = []
        result = self.run_fixture(observations=changed)
        self.assertEqual(result["status"], "FINDING")
        self.assertFalse(result["complete"])
        self.assertEqual(result["firstFinding"]["step"], 1)
        self.assertEqual(result["incompletionReason"], "MISSING_HTTP")

    def test_each_phase_requires_fresh_nonce_and_exact_chain(self):
        changes = [("nonce", "f" * 32), ("caseId", "foreign-case"), ("hop", 2),
                   ("url", "https://example.invalid/"), ("host", "127.0.0.1:2830"),
                   ("method", "POST"), ("status", 200), ("location", None)]
        for field, value in changes:
            changed = deepcopy(self.observations)
            changed[1]["redirect"][0][field] = value
            with self.subTest(field=field):
                self.assertEqual(self.run_fixture(observations=changed)["status"], "FINDING")
        changed = deepcopy(self.observations)
        changed[5]["redirect"] = deepcopy(changed[1]["redirect"])
        self.assertEqual(self.run_fixture(observations=changed)["firstFinding"]["step"], 6)
        for trace in (self.observations[1]["redirect"][::-1],
                      [self.observations[1]["redirect"][0]] * 3):
            changed = deepcopy(self.observations)
            changed[1]["redirect"] = trace
            self.assertEqual(self.run_fixture(observations=changed)["status"], "FINDING")

    def test_missing_extra_and_role_evidence_is_incomplete(self):
        for phase in (1, 3, 5):
            for field in ("redirect", "normalTabUas", "otherPanelUas"):
                changed = deepcopy(self.observations)
                changed[phase][field] = []
                with self.subTest(phase=phase, field=field):
                    self.assertEqual(self.run_fixture(observations=changed)["status"], "INCOMPLETE")
        changed = deepcopy(self.observations)
        changed[0]["normalTabUas"] = [self.context.desktop_ua]
        self.assertEqual(self.run_fixture(observations=changed)["incompletionReason"], "UNEXPECTED_HTTP")
        changed = deepcopy(self.observations)
        changed[1]["redirect"].append(changed[1]["redirect"][0])
        self.assertEqual(self.run_fixture(observations=changed)["status"], "INCOMPLETE")

    def test_present_http_failure_survives_missing_independent_peer_evidence(self):
        cases = (("normalTabUas", "REDIRECT_ORACLE"),
                 ("otherPanelUas", "REDIRECT_ORACLE"),
                 ("redirect", "NORMAL_TAB_UA"),
                 ("otherPanelUas", "NORMAL_TAB_UA"),
                 ("redirect", "OTHER_PANEL_UA"),
                 ("normalTabUas", "OTHER_PANEL_UA"))
        for missing, code in cases:
            changed = deepcopy(self.observations)
            changed[1][missing] = []
            if code == "REDIRECT_ORACLE":
                changed[1]["redirect"][0]["userAgent"] = "Synthetic wrong UA"
            else:
                role = "normalTabUas" if code == "NORMAL_TAB_UA" else "otherPanelUas"
                changed[1][role] = [self.context.mobile_ua]
            with self.subTest(missing=missing, code=code):
                result = self.run_fixture(observations=changed)
                self.assertEqual(result["status"], "FINDING")
                self.assertFalse(result["complete"])
                self.assertEqual(result["incompletionReason"], "MISSING_HTTP")
                self.assertEqual(result["firstFinding"]["code"], code)

    def test_wrong_run_step_action_duplicate_and_swapped_observations_rejected(self):
        for field, value in (("runId", "foreign-run"), ("step", 1), ("actionId", "observe")):
            changed = deepcopy(self.observations)
            changed[1][field] = value
            with self.subTest(field=field):
                self.assertEqual(self.run_fixture(observations=changed)["incompletionReason"],
                                 "OBSERVATION_CORRELATION")
        for changed in (self.observations[:1] + self.observations[:1] + self.observations[2:],
                        self.observations[::-1]):
            self.assertEqual(self.run_fixture(observations=changed)["status"], "INCOMPLETE")
        actions = deepcopy(self.actions)
        actions[1]["step"] = 1
        self.assertEqual(self.run_fixture(actions)["incompletionReason"], "ACTION_CORRELATION")

    def test_first_finding_survives_clean_followups_and_later_channel_errors(self):
        bad = self.bad()
        baseline = self.run_fixture(observations=bad)["firstFinding"]
        cases = [jsonl(bad), jsonl(bad[:-1]), jsonl(bad)[:-1],
                 jsonl(bad + [bad[-1]]), jsonl(bad[:-1]) + b'{"bad":"synthetic-secret-sentinel"}\n']
        for raw in cases:
            result = replay(self.context, jsonl(self.actions), raw)
            self.assertEqual(result["status"], "FINDING")
            self.assertEqual(result["firstFinding"], baseline)
        actions = jsonl(self.actions[:-1]) + b'{"shell":"synthetic-secret-sentinel"}\n'
        self.assertEqual(replay(self.context, actions, jsonl(bad))["firstFinding"], baseline)

    def test_empty_partial_missing_extra_blank_and_schema_skip_cannot_pass(self):
        for raw in (b"", jsonl(self.observations)[:-1], jsonl(self.observations[:-1]),
                    jsonl(self.observations + [self.observations[-1]]),
                    jsonl(self.observations[:2]) + b"\n" + jsonl(self.observations[2:])):
            self.assertEqual(replay(self.context, jsonl(self.actions), raw)["status"], "INCOMPLETE")
        for field in ("skip", "retry", "attempt", "workerQualified", "cleanupError"):
            changed = deepcopy(self.observations)
            changed[1][field] = True
            self.assertEqual(self.run_fixture(observations=changed)["status"], "INCOMPLETE")

    def test_step_budget_retains_earlier_finding(self):
        actions, observations = deepcopy(self.actions), self.bad()
        for step in range(7, MAX_STEPS + 2):
            actions.append({**self.actions[0], "actionId": "observe", "step": step})
            observations.append({**self.observations[0], "actionId": "observe", "step": step})
        result = self.run_fixture(actions, observations)
        self.assertEqual(result["status"], "FINDING")
        self.assertEqual(result["incompletionReason"], "STEP_BOUND")
        self.assertLessEqual(len(canonical(result)), MAX_OUTPUT_BYTES)

    def test_candidate_retains_full_sequence_exact_bindings_and_human_review(self):
        bad_wire = jsonl(self.bad())
        candidate = regression_candidate(self.context, jsonl(self.actions), bad_wire)
        self.assertEqual(candidate["actions"], [{"step": a["step"], "actionId": a["actionId"]}
                                                for a in self.actions])
        self.assertEqual(candidate["observationWireSha256"], hashlib.sha256(bad_wire).hexdigest())
        self.assertTrue(candidate["humanReviewRequired"])
        self.assertFalse(candidate["publicationAuthorized"])
        self.assertEqual(candidate, regression_candidate(self.context, jsonl(self.actions), bad_wire))
        for raw in (jsonl(self.observations), jsonl(self.bad()[:-1])):
            with self.assertRaises(ContractError):
                regression_candidate(self.context, jsonl(self.actions), raw)

    def test_observe_does_not_shorten_candidate_and_exact_byte_changes_are_bound(self):
        actions, observations = deepcopy(self.actions), self.bad()
        actions.append({**actions[-1], "actionId": "observe", "step": 7})
        observations.append({**observations[0], "actionId": "observe", "step": 7})
        candidate = regression_candidate(self.context, jsonl(actions), jsonl(observations))
        self.assertEqual(len(candidate["actions"]), 7)
        self.assertEqual(candidate["actions"][-1]["actionId"], "observe")
        original = jsonl(self.actions)
        spaced = original.replace(b'{', b'{ ', 1)
        receipt = replay(self.context, spaced, jsonl(self.observations))
        self.assertEqual(receipt["status"], "FIXTURE_PASS")
        self.assertNotEqual(receipt["actionWireSha256"], self.run_fixture()["actionWireSha256"])
        self.assertEqual(receipt["actionWireSha256"], hashlib.sha256(spaced).hexdigest())

    def test_bad_and_fixed_cannot_change_ua_context_or_declarations(self):
        for context in (replace(self.context, mobile_ua="Synthetic changed expected UA"),
                        replace(self.context, run_id="different-run"),
                        replace(self.context, sources=SourceBindings("c" * 40, "b" * 40))):
            with self.assertRaises(ContractError):
                compare_synthetic_pair(context, jsonl(self.actions), jsonl(self.bad()),
                                       jsonl(self.observations))

    def test_bad_fixed_pair_holds_context_actions_and_oracle_fixed(self):
        pair = compare_synthetic_pair(self.context, jsonl(self.actions),
                                      jsonl(self.bad()), jsonl(self.observations))
        self.assertFalse(pair["actualProductFixVerified"])
        self.assertTrue(pair["humanReviewRequired"])
        self.assertEqual(pair["candidate"]["firstFinding"]["code"], "REDIRECT_ORACLE")
        with self.assertRaises(ContractError):
            compare_synthetic_pair(self.context, jsonl(self.actions), jsonl(self.bad()), jsonl(self.bad()))
        changed_context = replace(self.context, mobile_ua=self.context.desktop_ua)
        self.assertEqual(replay(changed_context, jsonl(self.actions), jsonl(self.bad()))["status"], "INCOMPLETE")
        changed = deepcopy(self.observations)
        changed[1]["expectedUa"] = self.context.desktop_ua
        self.assertEqual(self.run_fixture(observations=changed)["status"], "INCOMPLETE")

    def test_signature_stable_across_same_oracle_failures_and_no_secret_reflection(self):
        bad = self.bad()
        baseline = self.run_fixture(observations=bad)["firstFinding"]["signature"]
        bad[1]["redirect"][1]["userAgent"] = "synthetic-secret-sentinel"
        result = self.run_fixture(observations=bad)
        self.assertEqual(result["firstFinding"]["signature"], baseline)
        self.assertNotIn(b"synthetic-secret-sentinel", canonical(result))
        candidate = regression_candidate(self.context, jsonl(self.actions), jsonl(bad))
        self.assertNotIn(b"synthetic-secret-sentinel", canonical(candidate))
        self.assertNotIn(self.context.mobile_ua.encode(), canonical(result))

    def test_replay_does_not_call_environment_process_or_network_adapters(self):
        def forbidden(*_args, **_kwargs):
            raise AssertionError("Unexpected adapter access")
        with patch("os.getenv", forbidden), patch("subprocess.Popen", forbidden), \
                patch("socket.socket", forbidden):
            self.assertEqual(self.run_fixture()["status"], "FIXTURE_PASS")


if __name__ == "__main__":
    unittest.main()
