# SPDX-License-Identifier: MPL-2.0
"""Deterministic in-memory fixture replay. No model, native or filesystem adapter."""

from .agent_types import ContractError, Observation, ReplayContext
from .protocol import (
    MAX_OUTPUT_BYTES, MAX_STEPS, MAX_WIRE_BYTES, ORACLE_ID, POLICY_ID,
    REQUIRED_ACTIONS, canonical, context_digest,
    decode_action, decode_observation, digest, redirect_case, require, wire_lines,
)
from tools.final_web_panel_http import validate_redirect_trace, validate_ua_values


def _oracle(context: ReplayContext, step: int, mobile: bool,
            action_id: str, observed: Observation) -> tuple[list[str], str | None]:
    findings: list[str] = []
    if observed.mobile_enabled != mobile:
        findings.append("MOBILE_STATE")
    if action_id != "web-panel.navigate-two-hop":
        unexpected = observed.redirect or observed.normal_tab_uas or observed.other_panel_uas
        return findings, "UNEXPECTED_HTTP" if unexpected else None
    # Absence is incomplete evidence; a present but wrong HTTP assertion is a finding.
    incomplete = (len(observed.redirect) != 3 or len(observed.normal_tab_uas) != 1
                  or len(observed.other_panel_uas) != 1)
    trace = [{"nonce": event.nonce, "caseId": event.case_id, "hop": event.hop,
              "url": event.url, "host": event.host, "method": event.method,
              "status": event.status, "location": event.location,
              "userAgent": event.user_agent} for event in observed.redirect]
    if len(trace) == 3:
        try:
            validate_redirect_trace(redirect_case(context, step, mobile), trace)
        except RuntimeError:
            findings.append("REDIRECT_ORACLE")
    for code, values in (("NORMAL_TAB_UA", observed.normal_tab_uas),
                         ("OTHER_PANEL_UA", observed.other_panel_uas)):
        if values:
            try:
                validate_ua_values(list(values), context.desktop_ua, code)
            except RuntimeError:
                findings.append(code)
    return findings, "MISSING_HTTP" if incomplete else None


def _wire_digest(raw: bytes) -> str | None:
    return digest(raw) if type(raw) is bytes and len(raw) <= MAX_WIRE_BYTES else None


def _bounded_output(value: dict[str, object]) -> dict[str, object]:
    require(len(canonical(value)) <= MAX_OUTPUT_BYTES, "OUTPUT_BOUND")
    return value


def replay(context: ReplayContext, action_wire: bytes,
           observation_wire: bytes) -> dict[str, object]:
    """Separate trusted-caller inputs; labels do not authenticate their producers."""
    receipt: dict[str, object] = {
        "schemaVersion": 1, "stage": "agentic-host-contract",
        "executionKind": "offline-synthetic-fixture", "policyId": POLICY_ID,
        "oracleId": ORACLE_ID, "status": "INCOMPLETE", "complete": False,
        "nativeExecution": "NOT_EXECUTED", "workerQualification": "UNVERIFIED",
        "productQualification": "UNVERIFIED", "publicationAuthorized": False,
        "releaseEvidence": False, "firstFinding": None, "incompletionReason": None,
        "actionWireSha256": _wire_digest(action_wire),
        "observationWireSha256": _wire_digest(observation_wire),
    }
    steps: list[dict[str, object]] = []
    receipt["steps"] = steps
    first_finding: dict[str, object] | None = None
    try:
        receipt["contextSha256"] = context_digest(context)
        receipt["declaredSources"] = {"floorp": context.sources.floorp,
                                      "runtime": context.sources.runtime}
        actions, observations = wire_lines(action_wire), wire_lines(observation_wire)
        mobile, progress = False, 0
        for index, raw_action in enumerate(actions):
            require(index < MAX_STEPS, "STEP_BOUND")
            # Retain the attempted identity even if its parsing/observation fails.
            entry: dict[str, object] = {"step": index + 1, "actionSha256": digest(raw_action)}
            steps.append(entry)
            action = decode_action(raw_action)
            entry["actionId"] = action.action_id
            require(action.run_id == context.run_id and action.step == index + 1,
                    "ACTION_CORRELATION")
            if action.action_id != "observe":
                require(progress < len(REQUIRED_ACTIONS)
                        and action.action_id == REQUIRED_ACTIONS[progress], "POLICY_SEQUENCE")
                progress += 1
            if action.action_id == "web-panel.mobile-on":
                mobile = True
            elif action.action_id == "web-panel.mobile-off":
                mobile = False
            require(index < len(observations), "MISSING_OBSERVATION")
            raw_observation = observations[index]
            entry["observationSha256"] = digest(raw_observation)
            observed = decode_observation(raw_observation)
            require((observed.run_id, observed.step, observed.action_id)
                    == (context.run_id, action.step, action.action_id), "OBSERVATION_CORRELATION")
            findings, incomplete = _oracle(context, action.step, mobile,
                                            action.action_id, observed)
            entry["oracleCodes"] = findings
            if findings and first_finding is None:
                code = findings[0]
                first_finding = {"step": action.step, "actionId": action.action_id,
                                 "code": code, "signature": digest(canonical([
                                     POLICY_ID, ORACLE_ID, code]))}
                receipt["firstFinding"] = first_finding
            require(incomplete is None, incomplete or "ORACLE_INCOMPLETE")
        require(len(observations) == len(actions), "EXTRA_OBSERVATION")
        require(progress == len(REQUIRED_ACTIONS), "MISSING_PHASE")
        receipt["complete"] = True
    except ContractError as error:
        receipt["incompletionReason"] = str(error)  # Fixed local codes only.
    receipt["status"] = ("FINDING" if first_finding is not None else
                         "FIXTURE_PASS" if receipt["complete"] else "INCOMPLETE")
    return _bounded_output(receipt)


def regression_candidate(context: ReplayContext, action_wire: bytes,
                         observation_wire: bytes) -> dict[str, object]:
    """Re-evaluate original inputs; never accept a caller-provided success receipt."""
    receipt = replay(context, action_wire, observation_wire)
    require(receipt["status"] == "FINDING" and receipt["complete"] is True,
            "CANDIDATE_REQUIRES_COMPLETE_FINDING")
    actions = [decode_action(raw) for raw in wire_lines(action_wire)]
    return _bounded_output({
        "schemaVersion": 1, "kind": "synthetic-regression-candidate",
        "policyId": POLICY_ID, "oracleId": ORACLE_ID,
        "contextSha256": receipt["contextSha256"],
        "actionWireSha256": receipt["actionWireSha256"],
        "observationWireSha256": receipt["observationWireSha256"],
        "declaredSources": receipt["declaredSources"],
        "firstFinding": receipt["firstFinding"],
        "actions": [{"step": action.step, "actionId": action.action_id} for action in actions],
        "executionKind": "offline-synthetic-fixture", "humanReviewRequired": True,
        "productQualification": "UNVERIFIED", "publicationAuthorized": False,
        "releaseEvidence": False,
    })


def compare_synthetic_pair(context: ReplayContext, action_wire: bytes,
                           bad_observations: bytes, fixed_observations: bytes) -> dict[str, object]:
    """One context/action/oracle for both sides; this cannot attest an actual code fix."""
    candidate = regression_candidate(context, action_wire, bad_observations)
    fixed = replay(context, action_wire, fixed_observations)
    require(fixed["status"] == "FIXTURE_PASS" and fixed["complete"] is True,
            "PAIR_FIXED_NOT_PASSING")
    return _bounded_output({"kind": "synthetic-regression-pair", "candidate": candidate,
            "fixedObservationWireSha256": fixed["observationWireSha256"],
            "humanReviewRequired": True, "actualProductFixVerified": False,
            "publicationAuthorized": False, "releaseEvidence": False})
