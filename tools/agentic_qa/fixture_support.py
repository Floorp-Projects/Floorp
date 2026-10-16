# SPDX-License-Identifier: MPL-2.0
"""Synthetic test data only; these declarations are not browser observations."""

from urllib.parse import urlsplit

from .agent_types import ReplayContext, SourceBindings
from .protocol import REQUIRED_ACTIONS, canonical, redirect_case


def fixture_context() -> ReplayContext:
    return ReplayContext("synthetic-web-panel-1", "http://127.0.0.1:2829",
                         "Synthetic desktop UA", "Synthetic mobile UA",
                         SourceBindings("a" * 40, "b" * 40))


def fixture_records(context: ReplayContext) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    actions, observations = [], []
    mobile = False
    for step, action_id in enumerate(REQUIRED_ACTIONS, 1):
        envelope = {"schemaVersion": 1, "runId": context.run_id,
                    "step": step, "actionId": action_id}
        actions.append({**envelope, "kind": "action"})
        if action_id == "web-panel.mobile-on":
            mobile = True
        elif action_id == "web-panel.mobile-off":
            mobile = False
        observed = {**envelope, "kind": "observation", "mobileEnabled": mobile,
                    "redirect": [], "normalTabUas": [], "otherPanelUas": []}
        if action_id == "web-panel.navigate-two-hop":
            case = redirect_case(context, step, mobile)
            observed["redirect"] = [
                {"nonce": case.nonce, "caseId": case.case_id, "hop": hop,
                 "url": case.url(hop), "host": urlsplit(case.origin).netloc,
                 "method": "GET", "status": 302 if hop < 2 else 200,
                 "location": case.url(hop + 1) if hop < 2 else None,
                 "userAgent": case.expected_ua} for hop in range(3)]
            observed["normalTabUas"] = [context.desktop_ua]
            observed["otherPanelUas"] = [context.desktop_ua]
        observations.append(observed)
    return actions, observations


def jsonl(records: list[dict[str, object]]) -> bytes:
    return b"".join(canonical(record) + b"\n" for record in records)
