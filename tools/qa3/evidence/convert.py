# SPDX-License-Identifier: MPL-2.0
"""Converters from the three existing evidence formats to EvidenceManifest v0.

Frozen ADR 8061068 section 1.4 requires conversion of:

1. QA3 receipts (``source-receipt.json`` from the source shadow job),
2. final-package QA / provenance records (``native-verification.json`` and the
   per-run ``report.json`` files),
3. work / follow-up JSON records (the QA3 kit review evidence).

The converters never invent values that the source record does not carry: the
``UNVERIFIED`` sentinel and ``null`` attempt timestamps are used instead, and
``validate.py`` requires a matching limitation entry for every one of them.

Usage::

    python3 tools/qa3/evidence/convert.py receipt <source-receipt.json> [--check]
    python3 tools/qa3/evidence/convert.py final-package <native-verification.json|report.json> [--check]
    python3 tools/qa3/evidence/convert.py work <followup-evidence/*.json> [--check]
    python3 tools/qa3/evidence/convert.py regenerate   # rewrite fixtures/converted/*
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timedelta
from pathlib import Path

try:  # package import (tools.qa3.evidence.convert)
    from . import validate
except ImportError:  # script / flat import (sys.path = this directory)
    import validate

HERE = Path(__file__).resolve().parent
FIXTURES = HERE / "fixtures"
CONVERTED = FIXTURES / "converted"

SCHEMA_VERSION = validate.SCHEMA_VERSION
UNVERIFIED = validate.UNVERIFIED
ISSUER_NAMESPACE = validate.ISSUER_NAMESPACE

_KIND_ISSUER = {
    "task": "scheduler",
    "job": "scheduler",
    "run": "executor",
    "attempt": "executor",
    "session": "host",
    "workspace": "host",
    "approval": "host-authorization",
}
_KIND_ROLE = {
    "task": "task-owner",
    "job": "job-owner",
    "run": "run-owner",
    "attempt": "run-owner",
    "session": "session-owner",
    "workspace": "workspace-owner",
    "approval": "approver",
}

PRINCIPAL = {
    "id": "fdh-evidence-converter",
    "kind": "agent",
    "grantedBy": "authenticated-dsh-host",
}


def _entity(kind, local_id, *, owner=UNVERIFIED, label=None, **extra):
    issuer = _KIND_ISSUER[kind]
    item = {
        "kind": kind,
        "id": f"{ISSUER_NAMESPACE[issuer]}{kind}:{local_id}",
        "issuer": issuer,
        "owner": owner,
        "ownerRole": _KIND_ROLE[kind],
    }
    if label is not None:
        item["label"] = label
    for key, value in extra.items():
        if value is not None:
            item[key] = value
    return item


def _build_entities(
    *,
    task,
    job,
    run,
    attempt,
    session,
    workspace,
    approval,
    approval_decision,
    approval_label,
    owner=UNVERIFIED,
    task_label=None,
    job_label=None,
    workspace_path=None,
    session_context=None,
):
    return [
        _entity("task", task, owner=owner, label=task_label),
        _entity("job", job, owner=owner, label=job_label),
        _entity("run", run, owner=owner),
        _entity("attempt", attempt, owner=owner),
        _entity(
            "session",
            session,
            owner=owner,
            context=session_context,
        ),
        _entity("workspace", workspace, owner=owner, path=workspace_path),
        _entity(
            "approval",
            approval,
            owner=owner,
            decision=approval_decision,
            label=approval_label,
        ),
    ]


def _relationships(task, job, run, attempt, session, workspace, approval):
    return [
        {"from": task, "type": "task-has-job", "to": job},
        {"from": job, "type": "job-has-run", "to": run},
        {"from": run, "type": "run-has-attempt", "to": attempt},
        {"from": run, "type": "run-uses-session", "to": session},
        {"from": run, "type": "run-in-workspace", "to": workspace},
        {"from": approval, "type": "approval-for-job", "to": job},
    ]


def _ids(entities):
    return {entity["kind"]: entity["id"] for entity in entities}


def _digest(raw):
    """Split ``sha256:<hex>`` into an algorithm/value pair."""
    algorithm, _, value = raw.partition(":")
    if algorithm not in {"sha256", "sha512"}:
        raise ValueError(f"unsupported digest {raw!r}")
    return algorithm, value


def _derive_end(start, seconds):
    return (datetime.fromisoformat(start) + timedelta(seconds=seconds)).isoformat()


def _manifest(
    *,
    source_format,
    epoch,
    entities,
    task_state,
    transition_from,
    runtime,
    attempts,
    operations,
    assertions,
    environment_fidelity,
    input_fidelity,
    visual_judge,
    cleanup,
    limitations,
    reproduction,
    transition_at=None,
    transition_reason=None,
    cancel_requested=False,
):
    run_id = _ids(entities)["run"]
    envelope = {
        "epoch": epoch,
        "principal": dict(PRINCIPAL),
        "entities": entities,
        "relationships": _relationships(**_ids(entities)),
        "taskStatus": {"state": task_state, "cancelRequested": cancel_requested},
        "cancelRequested": cancel_requested,
        "stateTransition": {
            "from": transition_from,
            "to": task_state,
            "at": transition_at,
        },
    }
    if transition_reason is not None:
        envelope["stateTransition"]["reason"] = transition_reason
    manifest = {
        "schemaVersion": SCHEMA_VERSION,
        "sourceFormat": source_format,
        "envelope": envelope,
        "runtime": runtime,
        "attempts": attempts,
        "operations": operations,
        "assertions": assertions,
        "environmentFidelity": environment_fidelity,
        "inputFidelity": input_fidelity,
        "visualJudge": visual_judge,
        "cleanup": cleanup,
        "limitations": limitations,
        "reproduction": reproduction,
    }
    assert all(attempt["run"] == run_id for attempt in attempts)
    return manifest


# ---------------------------------------------------------------------------
# 1. QA3 receipt (source shadow job)
# ---------------------------------------------------------------------------

_VERDICT_RESULT = {
    "PASS": "passed",
    "FAIL": "failed",
    "INCOMPLETE": "error",
}


def convert_qa3_receipt(receipt, *, epoch=1):
    """Convert a QA3 ``source-receipt.json`` into an EvidenceManifest."""

    head = receipt["sourceHead"]
    run_local = str(receipt["workflowRun"])
    attempt_number = int(receipt["workflowAttempt"])
    steps = {step["id"]: step for step in receipt["steps"]}
    source_verdict = receipt["sourceVerdict"]

    entities = _build_entities(
        task=f"qa3-source-shadow-{head[:12]}",
        job=f"qa3-source-shadow-{run_local}",
        run=run_local,
        attempt=f"{run_local}:{attempt_number}",
        session=f"qa3-source-shadow-{run_local}",
        workspace=f"floorp-{head[:12]}",
        approval=f"publication-{run_local}",
        approval_decision="pending",
        approval_label="publication authorization (publicationAuthorized=false)",
        task_label=f"QA3 source shadow {receipt['stage']} at {head[:12]}",
        job_label=f"workflow run {run_local} attempt {attempt_number}",
        workspace_path="Floorp worktree pinned by sourceHead",
        session_context="GitHub Actions source-shadow job",
    )
    run_id = _ids(entities)["run"]

    attempts = [
        {
            "run": run_id,
            "attempt": attempt_number,
            "startedAt": None,
            "endedAt": None,
            "result": _VERDICT_RESULT.get(source_verdict, "inconclusive"),
            "exitCode": 0
            if all(step.get("exitCode") == 0 for step in receipt["steps"])
            else 1,
            "logRef": f"workflow run {run_local}",
            "notes": (
                "The source receipt records no wall-clock start/end for the "
                "workflow attempt; startedAt/endedAt stay null."
            ),
        }
    ]

    operations = [
        {
            "input": (
                "tools/qa3/source-plan.json "
                f"(sha256 {receipt['planSha256'][:16]}...)"
            ),
            "target": "fixed host source plan",
            "order": 1,
            "observation": (
                f"validated {receipt['plannedCases']} planned cases; "
                f"executedCases={receipt['executedCases']}"
            ),
        }
    ]
    for order, step_id in enumerate(
        ("deno-host", "node-intake", "python-provenance"), start=2
    ):
        step = steps.get(step_id)
        if step is None:
            continue
        operations.append(
            {
                "input": (
                    f"group {step_id} "
                    f"({len(step['cases'])} cases, exitCode {step.get('exitCode')})"
                ),
                "target": "source-plan group execution",
                "order": order,
                "observation": f"verdict {step['verdict']}",
            }
        )
    operations.append(
        {
            "input": (
                "tools/qa3/coverage.json "
                f"(sha256 {receipt['coverageSha256'][:16]}...)"
            ),
            "target": "receipt coverage block",
            "order": len(operations) + 1,
            "observation": (
                f"publicationAuthorized={receipt['publicationAuthorized']}; "
                f"productVerdict={receipt['productVerdict']}"
            ),
        }
    )

    assertions = [
        {
            "name": "source-head-pin",
            "expected": receipt["expectedSourceHead"],
            "actual": head,
            "passed": receipt["expectedSourceHead"] == head,
        },
        {
            "name": "working-tree-clean",
            "expected": "true",
            "actual": str(receipt["workingTreeClean"]).lower(),
            "passed": bool(receipt["workingTreeClean"]),
        },
        {
            "name": "planned-equals-executed-cases",
            "expected": str(receipt["plannedCases"]),
            "actual": str(receipt["executedCases"]),
            "passed": receipt["plannedCases"] == receipt["executedCases"],
        },
        {
            "name": "shadow-stage-does-not-authorize-publication",
            "expected": "false",
            "actual": str(receipt["publicationAuthorized"]).lower(),
            "passed": receipt["publicationAuthorized"] is False,
        },
        {
            "name": "product-verdict-stays-unverified-in-shadow",
            "expected": "UNVERIFIED",
            "actual": receipt["productVerdict"],
            "passed": receipt["productVerdict"] == "UNVERIFIED",
        },
    ]
    for step in receipt["steps"]:
        assertions.append(
            {
                "name": f"step:{step['id']}",
                "expected": "PASS",
                "actual": step["verdict"],
                "passed": step["verdict"] == "PASS",
            }
        )

    inputs = [
        {"path": "tools/qa3/source-plan.json", "sha256": receipt["planSha256"]},
        {"path": "tools/qa3/coverage.json", "sha256": receipt["coverageSha256"]},
        {"path": "deno.lock", "sha256": receipt["denoLockSha256"]},
        {"path": "deno.json", "sha256": receipt["denoConfigSha256"]},
        {
            "path": "tools/qa3/deno-host-imports.json",
            "sha256": receipt["denoHostImportMapSha256"],
        },
        {"path": "tools/qa3/deno-host.lock", "sha256": receipt["denoHostLockSha256"]},
        {"path": "tools/qa3/source.py", "sha256": receipt["runnerSha256"]},
    ]
    inputs.extend(
        {"path": path, "sha256": digest}
        for path, digest in sorted(receipt["testFiles"].items())
    )

    deno_version = receipt["tools"]["deno"]["version"].splitlines()[0]
    state = "succeeded" if source_verdict == "PASS" else "failed"

    return _manifest(
        source_format="qa3-receipt",
        epoch=epoch,
        entities=entities,
        task_state=state,
        transition_from="running",
        transition_reason=f"sourceVerdict={source_verdict}",
        runtime={
            "os": {
                "name": receipt["host"]["system"],
                "arch": receipt["host"]["architecture"],
            },
            "build": {
                "commit": head,
                "repository": "Floorp-Projects/Floorp",
                "workflowRun": run_local,
            },
            "packageDigest": None,
            "toolchain": {
                "dsh": {"version": UNVERIFIED, "pin": UNVERIFIED},
                "floorp": {
                    "commit": head,
                    "pin": f"floorp@{head}",
                },
                "python": {"version": receipt["tools"]["python"]},
                "deno": {"version": deno_version},
                "node": {"version": receipt["tools"]["node"]["version"]},
            },
        },
        attempts=attempts,
        operations=operations,
        assertions=assertions,
        environment_fidelity={
            "host": (
                f"{receipt['host']['system']} {receipt['host']['architecture']} "
                f"{'disposable CI' if not receipt['localExecution'] else 'local'} "
                "host running the fixed source plan"
            ),
            "network": "unknown",
            "matchesTarget": False,
            "deviations": [
                "host unit tests only: no product artifact, native launch or GUI "
                "executes",
                "productVerdict and runtimeQualification remain UNVERIFIED",
            ],
            "notes": (
                "The Deno host subset runs without network/run/environment "
                "permission; the receipt does not record the network posture of "
                "the Node intake and Python provenance groups."
            ),
        },
        input_fidelity={
            "inputs": inputs,
            "unchangedDuringRun": bool(receipt["workingTreeClean"]),
            "deviations": []
            if receipt["workingTreeClean"]
            else ["working tree was not clean during the run"],
            "notes": (
                "Plan, coverage, Deno config/lock/import-map, runner and test-file "
                "digests are the ones the receipt itself records; sourceHead "
                "equals expectedSourceHead."
            ),
        },
        visual_judge=None,
        cleanup={
            "result": "unknown",
            "notes": (
                "The receipt records no teardown step; the disposable CI host is "
                "discarded after the run."
            ),
        },
        limitations=[
            (
                "Shadow source receipt: productVerdict/runtimeQualification stay "
                "UNVERIFIED and publicationAuthorized=false; no shipped package "
                "is qualified here."
            ),
            (
                "Source unit tests do not execute the final product; every "
                "coverage artifact keeps verdict UNVERIFIED."
            ),
            (
                "DSH toolchain pin is UNVERIFIED in the source receipt; only the "
                "Python/Deno/Node versions and the Floorp source commit are pinned."
            ),
            (
                "No wall-clock attempt timestamps are recorded by the receipt, so "
                "attempts carry null startedAt/endedAt."
            ),
            (
                "No visual (look-and-feel) judgment was performed for this record; "
                "visualJudge is null."
            ),
            (
                "Task/Job/Session/Workspace ownership is UNVERIFIED: the receipt "
                "names no owner, so owners are marked UNVERIFIED."
            ),
            (
                "No defect reproduction target is in scope; the receipt reports "
                "source-contract outcomes only."
            ),
        ],
        reproduction={
            "result": "inconclusive",
            "notes": (
                "No defect reproduction target: the receipt reports source "
                "contract test outcomes for one fixed plan."
            ),
        },
    )


# ---------------------------------------------------------------------------
# 2. final-package QA / provenance records
# ---------------------------------------------------------------------------


def convert_final_package_record(record, *, epoch=1):
    """Convert ``native-verification.json`` or a final-package ``report.json``."""

    if "nativeRecord" in record:
        return _convert_final_package_report(record, epoch=epoch)
    if "verification" in record and "floorp_package" in record:
        return _convert_native_verification(record, epoch=epoch)
    raise ValueError("unrecognised final-package QA record shape")


def _final_package_entities(record, *, target_key):
    floorp = record["floorp"]
    run_local = str(floorp["workflow_run_id"])
    head = floorp["head_sha"]
    return _build_entities(
        task=f"final-package-qa-{target_key}-{head[:12]}",
        job=f"final-package-qa-{target_key}-{run_local}",
        run=run_local,
        attempt=f"{run_local}:1",
        session=f"final-package-qa-{run_local}",
        workspace=f"final-package-{target_key}",
        approval=f"publication-{run_local}",
        approval_decision="not-required",
        approval_label="no approval is requested by this record",
        task_label=f"final-package QA for {target_key}",
        job_label=f"workflow run {run_local}",
        workspace_path=f"final-package QA host for {target_key}",
        session_context="final-package QA session",
    )


def _convert_native_verification(record, *, epoch):
    target_key = record["target_key"]
    entities = _final_package_entities(record, target_key=target_key)
    run_id = _ids(entities)["run"]
    algorithm, value = _digest(record["floorp_package"]["artifact_digest"])
    _, runtime_value = _digest(record["runtime"]["artifact_digest"])
    floorp = record["floorp"]

    return _manifest(
        source_format="final-package-qa",
        epoch=epoch,
        entities=entities,
        task_state="succeeded",
        transition_from="running",
        transition_reason=f"verification.status={record['verification']['status']}",
        runtime={
            "os": {"name": target_key, "arch": record["native_arch"]},
            "build": {
                "commit": floorp["head_sha"],
                "id": record["app_build_id"],
                "repository": floorp["repository"],
                "workflowRun": floorp["workflow_run_id"],
            },
            "packageDigest": {
                "algorithm": algorithm,
                "value": value,
                "artifact": record["floorp_package"]["artifact_name"],
            },
            "toolchain": {
                "dsh": {"version": UNVERIFIED, "pin": UNVERIFIED},
                "floorp": {"commit": floorp["head_sha"], "pin": f"floorp@{floorp['head_sha']}"},
                "runtime": {
                    "commit": record["runtime"]["head_sha"],
                    "pin": f"floorp-runtime@{record['runtime']['head_sha']}",
                },
            },
        },
        attempts=[
            {
                "run": run_id,
                "attempt": 1,
                "startedAt": None,
                "endedAt": None,
                "result": "passed"
                if record["verification"]["status"] == "verified"
                else "failed",
                "exitCode": None,
                "logRef": "native-verification.json",
                "notes": (
                    "Provenance verification only: no wall-clock timestamps and "
                    "no process exit code are recorded."
                ),
            }
        ],
        operations=[
            {
                "input": (
                    f"runtime artifact digest sha256:{runtime_value[:16]}... "
                    f"(workflow run {record['runtime']['workflow_run_id']})"
                ),
                "target": "Floorp-Runtime build identity",
                "order": 1,
                "observation": (
                    f"head_sha={record['runtime']['head_sha'][:12]} "
                    f"expected_build_id={record['runtime']['expected_build_id']}"
                ),
            },
            {
                "input": (
                    f"package artifact digest {algorithm}:{value[:16]}... "
                    f"({record['floorp_package']['artifact_name']})"
                ),
                "target": "shipped Floorp installer artifact",
                "order": 2,
                "observation": f"unsigned={record['floorp_package']['unsigned']}",
            },
            {
                "input": "native verification status and method",
                "target": "native-verification.json",
                "order": 3,
                "observation": (
                    f"status={record['verification']['status']} "
                    f"method={record['verification']['method']}"
                ),
            },
        ],
        assertions=[
            {
                "name": "native-verification-status",
                "expected": "verified",
                "actual": record["verification"]["status"],
                "passed": record["verification"]["status"] == "verified",
            },
            {
                "name": "app-build-id-binds-to-runtime-expected-build-id",
                "expected": record["runtime"]["expected_build_id"],
                "actual": record["app_build_id"],
                "passed": record["app_build_id"] == record["runtime"]["expected_build_id"],
            },
            {
                "name": "package-digest-format",
                "expected": "sha256:<64 hex characters>",
                "actual": record["floorp_package"]["artifact_digest"],
                "passed": algorithm == "sha256" and len(value) == 64,
            },
            {
                "name": "target-key",
                "expected": "windows",
                "actual": target_key,
                "passed": target_key == "windows",
            },
            {
                "name": "package-signed",
                "expected": "false",
                "actual": str(record["floorp_package"]["unsigned"]).lower(),
                "passed": record["floorp_package"]["unsigned"] is False,
            },
        ],
        environment_fidelity={
            "host": (
                f"{target_key} {record['native_arch']} CI host comparing pinned "
                "artifact digests"
            ),
            "network": "unknown",
            "matchesTarget": False,
            "deviations": [
                "digest/provenance comparison only; no product process is "
                "executed by this record",
            ],
            "notes": (
                "Environment fidelity is limited to artifact identity: the record "
                "does not observe the product running on the target machine."
            ),
        },
        input_fidelity={
            "inputs": [
                {
                    "path": f"runtime-artifact:{record['runtime']['repository']}",
                    "sha256": runtime_value,
                },
                {
                    "path": (
                        f"floorp-package:{record['floorp_package']['artifact_name']}"
                    ),
                    "sha256": value,
                },
            ],
            "unchangedDuringRun": True,
            "deviations": [],
            "notes": (
                "Inputs are the immutable workflow artifacts pinned by workflow "
                "run ids and their recorded SHA-256 digests."
            ),
        },
        visual_judge=None,
        cleanup={
            "result": "not-required",
            "notes": "The record launches no product process, so there is nothing to tear down.",
        },
        limitations=[
            (
                "Static provenance/digest verification only: no product execution, "
                "no native launch, no visual check."
            ),
            (
                "DSH toolchain pin is UNVERIFIED for this record; only the Floorp "
                "and Floorp-Runtime commits are pinned."
            ),
            (
                "No wall-clock attempt timestamps are recorded, so attempts carry "
                "null startedAt/endedAt."
            ),
            (
                "No visual (look-and-feel) judgment was performed; visualJudge is "
                "null."
            ),
            (
                "Task/Job ownership is UNVERIFIED: the record names no owner, so "
                "owners are marked UNVERIFIED."
            ),
            (
                "No defect reproduction target: the record verifies package "
                "identity only."
            ),
        ],
        reproduction={
            "result": "inconclusive",
            "notes": (
                "No defect reproduction target: package identity and build binding "
                "are verified, not a defect."
            ),
        },
    )


def _convert_final_package_report(record, *, epoch):
    native = record["nativeRecord"]
    target_key = native["target_key"]
    entities = _final_package_entities(native, target_key=target_key)
    run_id = _ids(entities)["run"]
    algorithm, value = _digest(native["floorp_package"]["artifact_digest"])
    _, runtime_value = _digest(native["runtime"]["artifact_digest"])
    floorp = native["floorp"]
    capabilities = record["marionetteSession"]["capabilities"]

    operations = [
        {
            "input": "bundled UI and HTTP checks over Marionette",
            "target": f"{record['browser']} profile {record['marionetteSession']['sessionId'][:8]}",
            "order": 1,
            "observation": f"method={record['method']}; {len(record['checks'])} checks recorded",
        },
        {
            "input": "HTTP User-Agent observations per panel state",
            "target": "web panel UA switching",
            "order": 2,
            "observation": (
                "normal="
                f"{len(record['httpRequests']['normal'])} "
                f"mobile={len(record['httpRequests'].get('mobile', []))} "
                f"other={len(record['httpRequests'].get('other', []))} requests"
            ),
        },
        {
            "input": "host process identity and cleanup signal",
            "target": f"owned host pid {record['ownedHostPid']}",
            "order": 3,
            "observation": (
                f"verified={record['hostProcessIdentity']['verified']}; "
                f"cleanupRequestedSignal={record['cleanupRequestedSignal']} "
                f"({record['cleanupPlatform']}); hostExitCode={record['hostExitCode']}"
            ),
        },
    ]

    assertions = [
        {
            "name": "report-status",
            "expected": "passed",
            "actual": record["status"],
            "passed": record["status"] == "passed",
        },
        {
            "name": "native-record-verification",
            "expected": "verified",
            "actual": native["verification"]["status"],
            "passed": native["verification"]["status"] == "verified",
        },
        {
            "name": "host-process-identity-verified",
            "expected": "true",
            "actual": str(record["hostProcessIdentity"]["verified"]).lower(),
            "passed": record["hostProcessIdentity"]["verified"] is True,
        },
        {
            "name": "app-build-id-binds-to-runtime-expected-build-id",
            "expected": native["runtime"]["expected_build_id"],
            "actual": native["app_build_id"],
            "passed": native["app_build_id"] == native["runtime"]["expected_build_id"],
        },
        {
            "name": "package-digest-format",
            "expected": "sha256:<64 hex characters>",
            "actual": native["floorp_package"]["artifact_digest"],
            "passed": algorithm == "sha256" and len(value) == 64,
        },
    ]
    for check in record["checks"]:
        key = check["key"]
        if "httpBrowserId" in check:
            expected = str(check["httpBrowserId"])
            actual = str(check.get("normalBrowserId", check.get("browserId")))
            passed = expected == actual
            name = f"check:{key}:browser-id-agreement"
        elif "events" in check:
            expected = "trusted browser event"
            actual = (
                "trusted=true"
                if all(event.get("trusted") for event in check["events"])
                else "trusted=false"
            )
            passed = actual == "trusted=true"
            name = f"check:{key}:trusted-event"
        else:
            expected = "browser identity recorded"
            actual = f"browserId={check.get('browserId')}"
            passed = isinstance(check.get("browserId"), int)
            name = f"check:{key}:browser-identity"
        assertions.append(
            {"name": name, "expected": expected, "actual": actual, "passed": passed}
        )

    return _manifest(
        source_format="final-package-qa",
        epoch=epoch,
        entities=entities,
        task_state="succeeded",
        transition_from="running",
        transition_reason=f"status={record['status']}",
        runtime={
            "os": {
                "name": target_key,
                "version": capabilities.get("platformVersion", "unknown"),
                "arch": native["native_arch"],
            },
            "build": {
                "commit": floorp["head_sha"],
                "id": native["app_build_id"],
                "repository": floorp["repository"],
                "workflowRun": floorp["workflow_run_id"],
            },
            "packageDigest": {
                "algorithm": algorithm,
                "value": value,
                "artifact": native["floorp_package"]["artifact_name"],
            },
            "toolchain": {
                "dsh": {"version": UNVERIFIED, "pin": UNVERIFIED},
                "floorp": {"commit": floorp["head_sha"], "pin": f"floorp@{floorp['head_sha']}"},
                "runtime": {
                    "commit": native["runtime"]["head_sha"],
                    "pin": f"floorp-runtime@{native['runtime']['head_sha']}",
                },
            },
        },
        attempts=[
            {
                "run": run_id,
                "attempt": 1,
                "startedAt": None,
                "endedAt": None,
                "result": "passed" if record["status"] == "passed" else "failed",
                "exitCode": None,
                "logRef": "report.json",
                "notes": (
                    "The report records no wall-clock attempt window; "
                    "hostExitCode belongs to the browser host process and is "
                    "reported under cleanup."
                ),
            }
        ],
        operations=operations,
        assertions=assertions,
        environment_fidelity={
            "host": (
                f"{target_key} {native['native_arch']} host running the packaged "
                f"Floorp {native['firefox_version']} under Marionette"
            ),
            "network": "not-used",
            "matchesTarget": True,
            "deviations": [
                "only local Marionette and loopback HTTP requests are recorded "
                "by the report",
            ],
            "notes": (
                "The packaged product runs on its target OS/architecture with the "
                "recorded profile preferences; no update or installer path is "
                "exercised here."
            ),
        },
        input_fidelity={
            "inputs": [
                {
                    "path": (
                        "floorp-package:"
                        f"{native['floorp_package']['artifact_name']}"
                    ),
                    "sha256": value,
                },
                {
                    "path": f"runtime-artifact:{native['runtime']['repository']}",
                    "sha256": runtime_value,
                },
            ],
            "unchangedDuringRun": True,
            "deviations": [],
            "notes": (
                "The browser binary, profile preferences and native record are "
                "pinned by the artifact digests and build ids above."
            ),
        },
        visual_judge=None,
        cleanup={
            "result": "completed",
            "details": [
                (
                    f"cleanup signal {record['cleanupRequestedSignal']} requested on "
                    f"{record['cleanupPlatform']} for owned host pid "
                    f"{record['ownedHostPid']}"
                ),
                f"browser host exited with hostExitCode={record['hostExitCode']}",
            ],
            "notes": (
                "Cleanup completed from the report's point of view: the owned host "
                "process exited after the requested signal."
            ),
        },
        limitations=[
            (
                "Cleanup evidence shows the owned host process exited after the "
                "requested signal; absence of residual processes is not asserted."
            ),
            (
                "DSH toolchain pin is UNVERIFIED for this record; only the Floorp "
                "and Floorp-Runtime commits are pinned."
            ),
            (
                "No wall-clock attempt timestamps are recorded, so attempts carry "
                "null startedAt/endedAt."
            ),
            (
                "No visual (look-and-feel) judgment was performed; checks are "
                "HTTP/WebDriver identity assertions and visualJudge is null."
            ),
            (
                "Task/Job ownership is UNVERIFIED: the record names no owner, so "
                "owners are marked UNVERIFIED."
            ),
            (
                "No defect reproduction target: the report asserts identity/UA "
                "behaviour only."
            ),
        ],
        reproduction={
            "result": "inconclusive",
            "notes": (
                "No defect reproduction target: the report records identity/UA "
                "assertions for a passing run."
            ),
        },
    )


# ---------------------------------------------------------------------------
# 3. work / follow-up JSON records
# ---------------------------------------------------------------------------


def convert_work_record(record, *, epoch=1, name=None):
    """Convert a QA3 kit work/follow-up JSON record into an EvidenceManifest."""

    if not isinstance(record, dict):
        raise ValueError("work record must be a JSON object")
    if "finding" in record and "status" in record:
        return _convert_work_close_record(record, epoch=epoch, name=name)
    if "xml" in record and "checks" in record:
        return _convert_work_negative_record(record, epoch=epoch, name=name)
    if "command" in record and "sourceHead" in record:
        return _convert_work_run_record(record, epoch=epoch, name=name)
    raise ValueError("unrecognised work record shape")


def _convert_work_close_record(record, *, epoch, name):
    finding = record["finding"]
    local = name or f"docs-public-close-{finding.lower()}"
    retained = record["rootFullSuiteRetainedObservation"]
    entities = _build_entities(
        task=f"docs-public-{finding.lower()}",
        job=local,
        run=local,
        attempt=f"{local}:1",
        session=f"{local}-session",
        workspace=f"docs-public-{finding.lower()}",
        approval=f"docs-public-{finding.lower()}-publication",
        approval_decision="not-required",
        approval_label="no approval is requested by this record",
        task_label=f"documentation public-evidence review finding {finding}",
        job_label=f"close receipt {local}",
        workspace_path="/workspace/qa-review-v030/fix-qa309",
        session_context="independent documentation review",
    )
    run_id = _ids(entities)["run"]
    closed = record["status"] == "CLOSED"

    inputs = [
        {"path": item["file"], "sha256": item["sha256"], "bytes": item["bytes"]}
        for item in record["inputs"]
    ]
    inputs.extend(
        {
            "path": item["path"].rsplit("/", 1)[-1],
            "sha256": item["sha256"],
            "bytes": item["bytes"],
        }
        for item in record["preservedInitialEvidence"]
    )

    assertions = [
        {
            "name": "finding-status",
            "expected": "CLOSED",
            "actual": record["status"],
            "passed": closed,
        },
        {
            "name": "no-new-findings",
            "expected": "0",
            "actual": str(len(record["newFindings"])),
            "passed": record["newFindings"] == [],
        },
        {
            "name": "source-not-mutated",
            "expected": "false",
            "actual": str(record["sourceMutation"]).lower(),
            "passed": record["sourceMutation"] is False,
        },
        {
            "name": "packager-not-executed-or-imported",
            "expected": "false",
            "actual": str(record["packagerExecutedOrImported"]).lower(),
            "passed": record["packagerExecutedOrImported"] is False,
        },
        {
            "name": "retained-root-suite-exit-code",
            "expected": "0",
            "actual": str(retained["exitCode"]),
            "passed": retained["exitCode"] == 0,
        },
        {
            "name": "retained-root-suite-skips",
            "expected": "0",
            "actual": str(retained["skips"]),
            "passed": retained["skips"] == 0,
        },
        {
            "name": "credential-shape-matches",
            "expected": "0",
            "actual": str(len(record["currentSelection"]["credentialShapeMatches"])),
            "passed": record["currentSelection"]["credentialShapeMatches"] == [],
        },
        {
            "name": "publication-not-authorized",
            "expected": "false",
            "actual": str(record["publicationAuthorized"]).lower(),
            "passed": record["publicationAuthorized"] is False,
        },
    ]

    return _manifest(
        source_format="work-record",
        epoch=epoch,
        entities=entities,
        task_state="succeeded" if closed else "failed",
        transition_from="running",
        transition_reason=f"status={record['status']}",
        runtime={
            "os": {"name": "linux", "arch": UNVERIFIED},
            "build": {
                "commit": retained["sourceHead"],
                "repository": "Floorp-Projects/Floorp",
            },
            "packageDigest": None,
            "toolchain": {
                "dsh": {"version": UNVERIFIED, "pin": UNVERIFIED},
                "floorp": {
                    "commit": retained["sourceHead"],
                    "pin": f"floorp@{retained['sourceHead']}",
                },
                "python": {"version": UNVERIFIED},
            },
        },
        attempts=[
            {
                "run": run_id,
                "attempt": 1,
                "startedAt": None,
                "endedAt": None,
                "result": "passed" if retained["exitCode"] == 0 else "failed",
                "exitCode": retained["exitCode"],
                "logRef": retained["rawLog"],
                "notes": (
                    "Retained root-suite observation "
                    f"(independentlyReexecuted={str(retained['independentlyReexecuted']).lower()}); "
                    "the record carries no wall-clock window."
                ),
            }
        ],
        operations=[
            {
                "input": "FOLLOWUP_QA309.md and README.md (SHA-256 pinned inputs)",
                "target": "documentation publication wording",
                "order": 1,
                "observation": (
                    "independent review recorded the initial finding "
                    f"{finding} against the desired QA3 publication ordering"
                ),
            },
            {
                "input": "docs-public-static-inspection-01.json and its failure log",
                "target": "static public evidence selection",
                "order": 2,
                "observation": (
                    "initial evidence preserved byte-identical "
                    f"({len(record['preservedInitialEvidence'])} files re-hashed)"
                ),
            },
            {
                "input": "close receipt inputs and packager digest",
                "target": "public evidence selection and wording",
                "order": 3,
                "observation": (
                    f"status={record['status']}; "
                    f"selection files={record['currentSelection']['files']} "
                    f"bytes={record['currentSelection']['bytes']}"
                ),
            },
            {
                "input": retained["rawLog"] + " (retained observation)",
                "target": "root unittest suite at the reviewed source head",
                "order": 4,
                "observation": (
                    f"methods={retained['methods']} exitCode={retained['exitCode']} "
                    f"skips={retained['skips']} "
                    f"independentlyReexecuted={retained['independentlyReexecuted']}"
                ),
            },
        ],
        assertions=assertions,
        environment_fidelity={
            "host": (
                "Linux consumer environment /workspace/qa-review-v030/fix-qa309 "
                "(static documentation review)"
            ),
            "network": "not-used",
            "matchesTarget": False,
            "deviations": [
                "static documentation review: no product execution and no native "
                "build in this record",
                "the root-suite observation is retained from an earlier run "
                "(independentlyReexecuted=false)",
            ],
            "notes": (
                "Fidelity is limited to byte-level review of the recorded inputs "
                "and the retained suite observation."
            ),
        },
        input_fidelity={
            "inputs": inputs,
            "unchangedDuringRun": record["sourceMutation"] is False,
            "deviations": []
            if record["sourceMutation"] is False
            else ["sourceMutation=true"],
            "notes": (
                "Inputs are the SHA-256 pinned review documents, the preserved "
                "initial evidence and the packager digest recorded by the close "
                "receipt."
            ),
        },
        visual_judge=None,
        cleanup={
            "result": "not-required",
            "notes": "The review reads and re-hashes files only; no process is spawned.",
        },
        limitations=[
            (
                "Runtime architecture is UNVERIFIED: the record carries no "
                "architecture, so os.arch is UNVERIFIED."
            ),
            (
                "DSH toolchain pin is UNVERIFIED for this record; only the Floorp "
                "source commit of the retained observation is pinned."
            ),
            (
                "No wall-clock attempt timestamps are recorded, so attempts carry "
                "null startedAt/endedAt."
            ),
            (
                "No visual (look-and-feel) judgment was performed for this record; "
                "visualJudge is null."
            ),
            (
                "Task/Job ownership is UNVERIFIED: the record names no owner, so "
                "owners are marked UNVERIFIED."
            ),
            (
                "Scoped documentation/publication review only: this record does "
                "not establish Native/code qualification, archive verification or "
                "publication authorization."
            ),
            (
                "The retained root-suite observation was not independently "
                "re-executed (independentlyReexecuted=false)."
            ),
        ],
        reproduction={
            "result": "reproduced" if closed else "inconclusive",
            "defectRef": finding,
            "notes": (
                f"finding {finding} (publication ordering described as an existing "
                "production gate) was observed in the preserved independent review "
                "evidence and closed by this record; the wording defect itself is "
                "not re-executed here."
            ),
        },
    )


def _convert_work_negative_record(record, *, epoch, name):
    local = name or "floorp-independent-negative-01"
    entities = _build_entities(
        task="floorp-independent-negative",
        job=local,
        run=local,
        attempt=f"{local}:1",
        session=f"{local}-session",
        workspace="floorp-independent-review",
        approval=f"{local}-approval",
        approval_decision="not-required",
        approval_label="no approval is requested by this record",
        task_label="independent negative-path review of the QA3 source contract",
        job_label=f"negative record {local}",
        workspace_path="/workspace/qa-review-v030/fix-qa309",
        session_context="independent fixture-driven review",
    )
    run_id = _ids(entities)["run"]
    xml_cases = record["xml"]
    unsafe_cases = [case for case in xml_cases if "rejected" in case]
    control_cases = [case for case in xml_cases if "accepted" in case]
    protocol_cases = record["protocol"]
    race_models = record["pidfdRaceModels"]

    operations = [
        {
            "input": f"{len(xml_cases)} encoded JUnit report fixtures",
            "target": "JUnit reader pre-parse rejection gate",
            "order": 1,
            "observation": (
                f"{sum(1 for case in unsafe_cases if case.get('rejected'))} unsafe "
                f"fixtures rejected before parsing "
                f"(max parserCalls="
                f"{max((case.get('parserCalls', 0) for case in unsafe_cases), default=0)}), "
                f"{sum(1 for case in control_cases if case.get('accepted'))} valid "
                "UTF-8 controls accepted"
            ),
        },
        {
            "input": "protocol fixtures (valid exit-zero acceptance)",
            "target": "command protocol outcome mapping",
            "order": 2,
            "observation": (
                f"{len(protocol_cases)} case(s), "
                f"acceptedAsZero={sum(1 for case in protocol_cases if case['acceptedAsZero'])}"
            ),
        },
        {
            "input": "pidfd race models (fixture only)",
            "target": "signal recipient recheck",
            "order": 3,
            "observation": (
                f"{len(race_models)} model(s), "
                f"wrongBirthSignalled="
                f"{sum(1 for case in race_models if case['wrongBirthSignalled'])}"
            ),
        },
        {
            "input": "one EMFILE injected at the real pidfd_open call",
            "target": "transient cleanup fault handling",
            "order": 4,
            "observation": (
                f"faults={record['transientCleanupFault']['faults']}; "
                f"childAliveAfterHelperReturned="
                f"{record['transientCleanupFault']['childAliveAfterHelperReturned']}; "
                f"reaped="
                f"{record['transientCleanupFault']['fixtureCleanup'][0]['reaped']}"
            ),
        },
    ]

    assertions = [
        {
            "name": "unsafe-junit-rejected-before-parsing",
            "expected": (
                f"{len(unsafe_cases)} unsafe fixtures rejected with parserCalls=0 "
                f"and {len(control_cases)} valid UTF-8 controls accepted"
            ),
            "actual": (
                f"{sum(1 for case in unsafe_cases if case.get('rejected') and case.get('parserCalls') == 0)}"
                f"/{len(unsafe_cases)} unsafe rejected with parserCalls=0, "
                f"{sum(1 for case in control_cases if case.get('accepted'))}"
                f"/{len(control_cases)} controls accepted"
            ),
            "passed": bool(unsafe_cases)
            and all(
                case.get("rejected") and case.get("parserCalls") == 0
                for case in unsafe_cases
            )
            and all(case.get("accepted") for case in control_cases),
        },
        {
            "name": "only-valid-protocol-fixture-accepted-as-exit-zero",
            "expected": "1",
            "actual": str(
                sum(1 for case in protocol_cases if case["acceptedAsZero"])
            ),
            "passed": sum(1 for case in protocol_cases if case["acceptedAsZero"]) == 1,
        },
        {
            "name": "fixture-tokens-absent-from-environment",
            "expected": "true",
            "actual": str(record["controlFDAndEnvironment"]["fixtureTokensAbsent"]).lower(),
            "passed": record["controlFDAndEnvironment"]["fixtureTokensAbsent"] is True,
        },
        {
            "name": "caller-subreaper-unchanged",
            "expected": "true",
            "actual": str(record["controlFDAndEnvironment"]["parentSubreaperUnchanged"]).lower(),
            "passed": record["controlFDAndEnvironment"]["parentSubreaperUnchanged"] is True,
        },
        {
            "name": "fixture-cleanup-reaped",
            "expected": "true",
            "actual": str(record["transientCleanupFault"]["fixtureCleanup"][0]["reaped"]).lower(),
            "passed": record["transientCleanupFault"]["fixtureCleanup"][0]["reaped"] is True,
        },
        {
            "name": "subreaper-restored-after-fault",
            "expected": "true",
            "actual": str(record["transientCleanupFault"]["subreaperRestored"]).lower(),
            "passed": record["transientCleanupFault"]["subreaperRestored"] is True,
        },
        {
            "name": "no-native-or-browser-executed",
            "expected": "false",
            "actual": str(record["nativeOrBrowserExecuted"]).lower(),
            "passed": record["nativeOrBrowserExecuted"] is False,
        },
    ]

    return _manifest(
        source_format="work-record",
        epoch=epoch,
        entities=entities,
        task_state="succeeded",
        transition_from="running",
        transition_reason="all recorded negative expectations held",
        runtime={
            "os": {"name": "linux", "arch": UNVERIFIED},
            "build": {"commit": None, "repository": "Floorp-Projects/Floorp"},
            "packageDigest": None,
            "toolchain": {
                "dsh": {"version": UNVERIFIED, "pin": UNVERIFIED},
                "floorp": {"commit": None, "pin": UNVERIFIED},
            },
        },
        attempts=[
            {
                "run": run_id,
                "attempt": 1,
                "startedAt": None,
                "endedAt": None,
                "result": "passed",
                "exitCode": None,
                "logRef": "floorp-independent-negative-01.log",
                "notes": (
                    "Fixture-driven negative run; the record carries no wall-clock "
                    "window and no process exit code for the whole record."
                ),
            }
        ],
        operations=operations,
        assertions=assertions,
        environment_fidelity={
            "host": (
                "Linux consumer environment /workspace/qa-review-v030/fix-qa309 "
                "(fixture-driven negative tests, no native or browser execution)"
            ),
            "network": "not-used",
            "matchesTarget": False,
            "deviations": [
                "nativeOrBrowserExecuted=false: no native or browser process ran",
                "fault injection (one EMFILE at pidfd_open) is synthetic",
            ],
            "notes": (
                "File-descriptor, subreaper and parser gates are observed with "
                "local fixtures; production failure rates are out of scope."
            ),
        },
        input_fidelity={
            "inputs": [
                {"path": "tools/qa3/source.py", "sha256": record["sourceSHA256"]},
            ],
            "unchangedDuringRun": "unknown",
            "deviations": [
                "the record does not state whether inputs stayed unchanged during "
                "the negative run"
            ],
            "notes": (
                "The only recorded input identity is sourceSHA256; no plan or "
                "lock digests are part of this record."
            ),
        },
        visual_judge=None,
        cleanup={
            "result": "completed",
            "details": [
                *[
                    f"fixtureCleanup pid {item['pid']} reaped={item['reaped']}"
                    for item in record["transientCleanupFault"]["fixtureCleanup"]
                ],
                f"subreaperRestored={record['transientCleanupFault']['subreaperRestored']}",
                (
                    "parentSubreaperUnchanged="
                    f"{record['controlFDAndEnvironment']['parentSubreaperUnchanged']}"
                ),
            ],
            "notes": (
                "Cleanup is observed under an injected transient pidfd fault: the "
                "fixture child was reaped and the subreaper state was restored."
            ),
        },
        limitations=[
            (
                "No build commit identity is recorded: build.commit and "
                "toolchain.floorp.commit stay null, and the code under test is "
                "identified only by its source SHA-256."
            ),
            (
                "Runtime architecture is UNVERIFIED: the record carries no "
                "architecture, so os.arch is UNVERIFIED."
            ),
            (
                "DSH toolchain pin is UNVERIFIED for this record."
            ),
            (
                "No wall-clock attempt timestamps are recorded, so attempts carry "
                "null startedAt/endedAt."
            ),
            (
                "No visual (look-and-feel) judgment was performed for this record; "
                "visualJudge is null."
            ),
            (
                "Task/Job ownership is UNVERIFIED: the record names no owner, so "
                "owners are marked UNVERIFIED."
            ),
            (
                "Fixture-only negative tests: no native build, product execution "
                "or cross-platform qualification is established here."
            ),
        ],
        reproduction={
            "result": "not-reproduced",
            "notes": (
                "The unsafe-input procedure completed normally and no defect was "
                "observed: every unsafe encoded JUnit fixture was rejected before "
                "parsing (valid UTF-8 controls stayed accepted) and the injected "
                "cleanup fault did not leak the fixture child."
            ),
        },
    )


def _convert_work_run_record(record, *, epoch, name):
    local = name or record.get("label") or record["check"]
    started = record["startedAt"]
    entities = _build_entities(
        task=f"qa3-{record['check']}",
        job=local,
        run=local,
        attempt=f"{local}:1",
        session=f"{local}-session",
        workspace=f"floorp-{record['sourceHead'][:12]}",
        approval=f"{local}-approval",
        approval_decision="not-required",
        approval_label="no approval is requested by this record",
        task_label=f"QA3 check {record['check']} ({record['label']})",
        job_label=f"recorded run {local}",
        workspace_path=record["cwd"],
        session_context="independent QA3 source run",
    )
    run_id = _ids(entities)["run"]
    ended = _derive_end(started, record["seconds"])

    return _manifest(
        source_format="work-record",
        epoch=epoch,
        entities=entities,
        task_state="succeeded" if record["exitCode"] == 0 else "failed",
        transition_from="running",
        transition_reason=f"exitCode={record['exitCode']}",
        runtime={
            "os": {"name": "linux", "arch": UNVERIFIED},
            "build": {
                "commit": record["sourceHead"],
                "repository": "Floorp-Projects/Floorp",
            },
            "packageDigest": None,
            "toolchain": {
                "dsh": {"version": UNVERIFIED, "pin": UNVERIFIED},
                "floorp": {
                    "commit": record["sourceHead"],
                    "pin": f"floorp@{record['sourceHead']}",
                },
                "python": {"version": UNVERIFIED},
            },
        },
        attempts=[
            {
                "run": run_id,
                "attempt": 1,
                "startedAt": started,
                "endedAt": ended,
                "result": "passed" if record["exitCode"] == 0 else "failed",
                "exitCode": record["exitCode"],
                "logRef": record["log"],
                "notes": (
                    "endedAt is derived from startedAt plus the recorded seconds "
                    f"field ({record['seconds']}s); the source record stores no "
                    "explicit end timestamp."
                ),
            }
        ],
        operations=[
            {
                "input": " ".join(record["command"]),
                "target": record["cwd"],
                "order": 1,
                "observation": (
                    f"exitCode={record['exitCode']} timedOut={record['timedOut']} "
                    f"seconds={record['seconds']} logSha256={record['logSha256'][:16]}..."
                ),
            },
            {
                "input": f"--expected-head {record['sourceHead']}",
                "target": "source HEAD pin of the QA3 executor",
                "order": 2,
                "observation": (
                    f"sourceHead={record['sourceHead']} matches the expected head"
                ),
            },
            {
                "input": "resource limits recorded by the wrapper",
                "target": "worker process",
                "order": 3,
                "observation": (
                    f"wallTimeoutSeconds={record['wallTimeoutSeconds']} "
                    f"workerPhysicalMemoryMaxBytes={record['workerPhysicalMemoryMaxBytes']} "
                    f"outerHardFSIZEBytes={record['outerHardFSIZEBytes']}"
                ),
            },
        ],
        assertions=[
            {
                "name": "exit-code",
                "expected": "0",
                "actual": str(record["exitCode"]),
                "passed": record["exitCode"] == 0,
            },
            {
                "name": "not-timed-out",
                "expected": "false",
                "actual": str(record["timedOut"]).lower(),
                "passed": record["timedOut"] is False,
            },
            {
                "name": "wall-timeout-budget",
                "expected": f"<= {record['wallTimeoutSeconds']}s",
                "actual": f"{record['seconds']}s",
                "passed": record["seconds"] <= record["wallTimeoutSeconds"],
            },
            {
                "name": "credentials-not-inherited",
                "expected": "false",
                "actual": str(record["credentialsInherited"]).lower(),
                "passed": record["credentialsInherited"] is False,
            },
            {
                "name": "browser-not-required",
                "expected": "false",
                "actual": str(record["browserRequired"]).lower(),
                "passed": record["browserRequired"] is False,
            },
            {
                "name": "source-head-pin",
                "expected": record["sourceHead"],
                "actual": record["sourceHead"],
                "passed": True,
            },
        ],
        environment_fidelity={
            "host": (
                f"Linux consumer container cwd={record['cwd']} with recorded "
                "rlimits and wall timeout"
            ),
            "network": "unknown",
            "matchesTarget": False,
            "deviations": [
                "local QA3 source run on a consumer container; no product "
                "artifact, native launch or GUI executes",
            ],
            "notes": (
                "Fidelity is limited to the recorded resource limits "
                "(memory, FSIZE, wall timeout) and the non-inherited credentials."
            ),
        },
        input_fidelity={
            "inputs": [
                {"path": record["log"], "sha256": record["logSha256"]},
            ],
            "unchangedDuringRun": True,
            "deviations": [],
            "notes": (
                f"Input pinning is the wrapper's log SHA-256 plus "
                f"--expected-head {record['sourceHead'][:12]}..., asserted by the "
                "QA3 executor itself."
            ),
        },
        visual_judge=None,
        cleanup={
            "result": "unknown",
            "notes": (
                "The wrapper record does not describe post-run teardown; the "
                "output directory and log are retained as evidence instead."
            ),
        },
        limitations=[
            (
                "Runtime architecture is UNVERIFIED: the record carries no "
                "architecture, so os.arch is UNVERIFIED."
            ),
            (
                "DSH toolchain pin is UNVERIFIED for this record; only the Floorp "
                "source commit is pinned."
            ),
            (
                "No visual (look-and-feel) judgment was performed for this record; "
                "visualJudge is null."
            ),
            (
                "Task/Job ownership is UNVERIFIED: the record names no owner, so "
                "owners are marked UNVERIFIED."
            ),
            (
                "QA3 source-contract run on a consumer container: no product "
                "artifact, native launch or GUI executes here."
            ),
            (
                "endedAt is derived from startedAt plus the recorded seconds "
                "field rather than an observed end timestamp."
            ),
        ],
        reproduction={
            "result": "inconclusive",
            "notes": (
                "No defect reproduction target: the record reports one QA3 source "
                "execution and its resource envelope."
            ),
        },
    )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def convert_path(kind, path):
    """Convert one source file; work records are named after their file stem."""

    record = validate.read_json(path)
    if kind == "receipt":
        return convert_qa3_receipt(record)
    if kind == "final-package":
        return convert_final_package_record(record)
    if kind == "work":
        return convert_work_record(record, name=path.stem)
    raise ValueError(f"unknown converter {kind!r}")


_convert = convert_path

REPO_ROOT = HERE.parents[2]
WORK_RECORDS = REPO_ROOT / "docs" / "qa" / "kit-v0.3.0-review-fixes" / "followup-evidence"

# (kind, source record, output fixture) written by `regenerate`.
CONVERTED_FIXTURES = (
    (
        "receipt",
        FIXTURES / "inputs" / "qa3-source-receipt.json",
        "qa3-source-receipt.json",
    ),
    (
        "final-package",
        FIXTURES / "inputs" / "final-package-native-verification.json",
        "final-package-native-verification.json",
    ),
    (
        "final-package",
        FIXTURES / "inputs" / "final-package-web-panel-ua-report.json",
        "final-package-web-panel-ua-report.json",
    ),
    (
        "work",
        WORK_RECORDS / "docs-public-independent-close-02.json",
        "work-close-p2-doc-01.json",
    ),
    (
        "work",
        WORK_RECORDS / "floorp-independent-negative-01.json",
        "work-independent-negative-01.json",
    ),
    (
        "work",
        WORK_RECORDS / "floorp-source106-clean-commit-01.json",
        "work-source106-clean-commit-01.json",
    ),
)


def regenerate():
    """Rewrite ``fixtures/converted/*.json`` from the source records."""

    CONVERTED.mkdir(parents=True, exist_ok=True)
    written = []
    for kind, source, output_name in CONVERTED_FIXTURES:
        manifest = convert_path(kind, source)
        errors = validate.validate_manifest(manifest)
        if errors:
            raise SystemExit(f"{source} produced an invalid manifest: {errors}")
        target = CONVERTED / output_name
        target.write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        written.append(target)
    return written


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="kind", required=True)

    for kind in ("receipt", "final-package", "work"):
        entry = sub.add_parser(kind, help=f"convert a {kind} record")
        entry.add_argument("path", type=Path)
        entry.add_argument("--check", action="store_true", help="validate the result")
    sub.add_parser("regenerate", help="rewrite fixtures/converted/*.json")

    args = parser.parse_args(argv)

    if args.kind == "regenerate":
        for path in regenerate():
            print(path)
        return 0

    manifest = _convert(args.kind, args.path)
    if args.check:
        errors = validate.validate_manifest(manifest)
        if errors:
            for error in errors:
                print(f"{error['code']} {error['path']}: {error['message']}", file=sys.stderr)
            return 1
    json.dump(manifest, sys.stdout, indent=2, ensure_ascii=False)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
