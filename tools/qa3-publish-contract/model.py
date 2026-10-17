# SPDX-License-Identifier: MPL-2.0
"""Pure, nonauthoritative publication contract model. No publisher is imported.

Every input is an offline fixture/declaration, including snapshots. This module
does not authenticate an issuer, approve a release, or grant publication rights.
The production v2 bundle verifier and existing publishers are not changed.
"""
import hashlib
import json
import re

TARGETS = {"linux-x86_64", "linux-aarch64", "windows-x86_64", "mac-x86_64", "mac-aarch64"}
SUITE = "qa3-full-final-artifact-v1"


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def decode_contract(data):
    """Bounded input boundary for callers; no file/network access is performed."""
    require(type(data) is bytes and 0 < len(data) <= 4 * 1024**2, "bounded contract bytes required")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate contract JSON key")
            result[key] = value
        return result
    def constant(_):
        raise ValueError("non-finite contract JSON rejected")
    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=unique, parse_constant=constant)
    except (UnicodeError, RecursionError) as error:
        raise ValueError("invalid bounded contract JSON") from error
    require(isinstance(value, dict), "contract object required")
    return value


def identical(actual, expected):
    # Python's True == 1 and 1.0 == 1 are not JSON contract equivalence.
    return digest(actual) == digest(expected)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def closed(value, keys):
    require(isinstance(value, dict) and set(value) == set(keys), "closed contract schema differs")


def integer(value):
    return type(value) is int and 0 < value < 2**53


def hex_value(value, size):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{" + str(size) + "}", value) is not None


def reference(value):
    closed(value, {"id", "sha256"})
    require(isinstance(value["id"], str) and re.fullmatch(r"[a-z0-9][a-z0-9._/-]{0,127}", value["id"]) and hex_value(value["sha256"], 64), "immutable evidence reference required")


def artifact(value, name, limit):
    closed(value, {"id", "name", "digest", "bytes"})
    require(integer(value["id"]) and value["name"] == name and isinstance(value["digest"], str) and re.fullmatch(r"sha256:[0-9a-f]{64}", value["digest"]) and integer(value["bytes"]) and value["bytes"] <= limit, "exact original provider artifact identity/size required")


def evaluation_key(subject, execution):
    # Deliberately excludes C/run, host, controller, policy version, output path
    # and store namespace. None may erase the first result of the same artifact.
    return digest({"repositoryId": subject["repositoryId"], "memberSha512": subject["members"][execution["member"]]["sha512"], "target": execution["target"], "suite": SUITE})


def check_policy(policy):
    closed(policy, {"schema", "stage", "suite", "controllerHead", "gateRun", "environment", "approverIds", "issuer", "backendQualificationSha256", "snapshotTTL", "approvalTTL", "executions"})
    require(policy["schema"] == "qa3-publish-contract-policy-v1" and policy["stage"] == "contract-model" and policy["suite"] == SUITE, "only the offline contract model is supported")
    require(hex_value(policy["controllerHead"], 40) and integer(policy["gateRun"]), "fixed controller and gating run required")
    require(policy["environment"] == "release" and isinstance(policy["approverIds"], list) and 0 < len(policy["approverIds"]) <= 1000 and all(integer(x) for x in policy["approverIds"]) and len(set(policy["approverIds"])) == len(policy["approverIds"]), "manual approval policy incomplete")
    require(isinstance(policy["issuer"], str) and bool(policy["issuer"]) and hex_value(policy["backendQualificationSha256"], 64), "trusted issuer/backend qualification reference required")
    require(integer(policy["snapshotTTL"]) and policy["snapshotTTL"] <= 60 and integer(policy["approvalTTL"]) and policy["approvalTTL"] <= 3600, "bounded freshness policy required")
    executions = policy["executions"]
    require(isinstance(executions, list) and 5 <= len(executions) <= 64, "required execution catalog incomplete")
    seen = set()
    for execution in executions:
        closed(execution, {"target", "member", "caseIds"})
        require(execution["target"] in TARGETS and isinstance(execution["member"], str), "unknown target/member")
        key = (execution["target"], execution["member"])
        require(key not in seen, "duplicate required execution")
        seen.add(key)
        cases = execution["caseIds"]
        require(isinstance(cases, list) and 0 < len(cases) <= 10000 and all(isinstance(x, str) and re.fullmatch(r"[a-z0-9][a-z0-9._/-]{0,127}", x) for x in cases) and len(set(cases)) == len(cases), "fixed nonempty case IDs required")
    require({e["target"] for e in executions} == TARGETS, "all five native targets required")


def check_subject(subject):
    closed(subject, {"repositoryId", "floorpHead", "runtimeHead", "runtimeRun", "runtimeAttempt", "runtimeManifestArtifact", "runtimeBuildID", "runtimeSourceKey", "buildPolicyKey", "producerRun", "producerAttempt", "bundleArtifact", "manifestSetId", "members"})
    require(type(subject["repositoryId"]) is int and subject["repositoryId"] == 437505831 and integer(subject["producerRun"]) and type(subject["producerAttempt"]) is int and subject["producerAttempt"] == 1, "original Floorp producer required")
    require(all(hex_value(subject[k], 40) for k in ("floorpHead", "runtimeHead")) and all(hex_value(subject[k], 64) for k in ("runtimeSourceKey", "buildPolicyKey", "manifestSetId")), "exact source/build/manifest identities required")
    require(integer(subject["runtimeRun"]) and type(subject["runtimeAttempt"]) is int and subject["runtimeAttempt"] == 1 and isinstance(subject["runtimeBuildID"], str) and re.fullmatch(r"[0-9]{14}", subject["runtimeBuildID"]), "original Runtime run/attempt/BuildID required")
    artifact(subject["bundleArtifact"], "floorp-release-bundle-v2", 2 * 1024**3)
    artifact(subject["runtimeManifestArtifact"], "floorp-runtime-build-manifest-v2", 4 * 1024**2)
    require(subject["bundleArtifact"]["id"] != subject["runtimeManifestArtifact"]["id"], "distinct artifact identities cannot alias")
    members = subject["members"]
    require(isinstance(members, dict) and len(members) == 16 and all(isinstance(k, str) and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", k) for k in members), "closed sixteen-member digest inventory required")
    for value in members.values():
        closed(value, {"bytes", "sha512"})
        require(integer(value["bytes"]) and value["bytes"] <= 1024**3 and hex_value(value["sha512"], 128), "exact bounded member bytes/digest required")
    require(sum(value["bytes"] for value in members.values()) <= 4 * 1024**3, "full bundle expansion bound exceeded")


def check_snapshot(snapshot, subject, candidate, policy, now):
    closed(snapshot, {"schema", "subjectDigest", "policyDigest", "observedAt", "afterApprovalId", "producer", "closedProductionBundleVerified", "finalSignatureAndNativeRecordsVerified", "entries", "history"})
    require(snapshot["schema"] == "qa3-publish-contract-snapshot-v1" and snapshot["subjectDigest"] == digest(subject) and snapshot["policyDigest"] == digest(policy), "snapshot subject/policy differs")
    require(integer(snapshot["observedAt"]) and snapshot["observedAt"] <= now and snapshot["closedProductionBundleVerified"] is True and snapshot["finalSignatureAndNativeRecordsVerified"] is True, "original production bundle/signature/native verification missing")
    producer = snapshot["producer"]
    closed(producer, {"id", "head", "attempt", "status", "conclusion"})
    require(identical(producer, {"id": subject["producerRun"], "head": subject["floorpHead"], "attempt": 1, "status": "completed", "conclusion": "success"}), "completed original producer missing")
    entries = snapshot["entries"]
    refs = candidate["evidence"]
    expected = [refs[k] for k in ("source", "qualification", "compileProfile", "sidecar")] + list(refs["products"].values())
    require(isinstance(entries, dict) and len({x["id"] for x in expected}) == len(expected) and len({x["sha256"] for x in expected}) == len(expected) and set(entries) == {x["id"] for x in expected}, "exact authenticated evidence inventory required")
    for ref in expected:
        entry = entries[ref["id"]]
        closed(entry, {"sha256", "kind", "status", "issuer", "expiresAt", "revoked", "subjectDigest", "sourceKey", "buildPolicyKey", "run", "attempt", "details"})
        require(entry["sha256"] == ref["sha256"] and entry["issuer"] == policy["issuer"] and integer(entry["expiresAt"]) and entry["expiresAt"] > now and entry["revoked"] is False and entry["subjectDigest"] == digest(subject), "evidence changed, expired, revoked or foreign")
        require(entry["sourceKey"] == subject["runtimeSourceKey"] and entry["buildPolicyKey"] == subject["buildPolicyKey"] and integer(entry["run"]) and type(entry["attempt"]) is int and entry["attempt"] == 1, "source/build/original attempt differs")
    source = entries[refs["source"]["id"]]
    require(source["kind"] == "source" and source["status"] == "PASS_SOURCE_ONLY" and source["run"] == subject["producerRun"] and identical(source["details"], {"floorpHead": subject["floorpHead"], "executedCases": 109, "originalFailures": 0, "skipped": 0}), "complete same-run source evidence missing")
    qualification = entries[refs["qualification"]["id"]]
    require(qualification["kind"] == "qualification" and qualification["status"] == "QUALIFIED", "Debug applicability qualification missing")
    require(identical(qualification["details"], {"productRuntimeHead": subject["runtimeHead"], "targets": sorted(TARGETS), "diagnosticChangesReviewed": True, "sourceEquivalenceVerified": True}), "Debug/product source applicability differs")
    profile = entries[refs["compileProfile"]["id"]]
    require(profile["kind"] == "compile-profile" and profile["status"] == "QUALIFIED" and profile["run"] == subject["runtimeRun"] and identical(profile["details"], {"profileChainVerified": True, "manifestDigest": subject["runtimeManifestArtifact"]["digest"], "runtimeBuildID": subject["runtimeBuildID"], "modes": {target: "production-opt" if target == "linux-aarch64" else "pgo-use" for target in sorted(TARGETS)}}), "PGO/profile or ARM production-opt qualification missing")
    sidecar = entries[refs["sidecar"]["id"]]
    require(sidecar["kind"] == "sidecar" and sidecar["status"] == "PASS_FINAL_ARTIFACT_QA" and sidecar["run"] != subject["producerRun"] and identical(sidecar["details"], {"unsigned": False, "complete": True}), "independent complete final QA sidecar missing")
    history = snapshot["history"]
    keys = {evaluation_key(subject, e) for e in policy["executions"]}
    require(isinstance(history, dict) and len(keys) == len(policy["executions"]) and set(history) == keys, "first evaluation history coverage incomplete")
    for execution in policy["executions"]:
        key = execution["target"] + "/" + execution["member"]
        value = entries[refs["products"][key]["id"]]
        require(value["kind"] == "product" and value["status"] == "PASS" and value["run"] != subject["producerRun"], "product native result missing")
        expected_details = {"target": execution["target"], "member": execution["member"], "memberSha512": subject["members"][execution["member"]]["sha512"], "runtimeRun": subject["runtimeRun"], "runtimeBuildID": subject["runtimeBuildID"], "caseResults": [{"id": case, "status": "PASS", "executions": 1} for case in execution["caseIds"]], "cleanupVerified": True, "nativeIdentityVerified": True}
        require(identical(value["details"], expected_details), "required native cases, identity or cleanup incomplete")
        original = history[evaluation_key(subject, execution)]
        closed(original, {"originalRun", "originalAttempt", "originalStatus", "originalEvidenceSha256", "creationAnchor", "complete", "backendQualificationSha256"})
        require(original["complete"] is True and hex_value(original["creationAnchor"], 64) and original["backendQualificationSha256"] == policy["backendQualificationSha256"] and integer(original["originalRun"]) and original["originalRun"] == value["run"] and type(original["originalAttempt"]) is int and original["originalAttempt"] == 1 and original["originalStatus"] == "PASS" and original["originalEvidenceSha256"] == value["sha256"], "original first FAIL/missing history cannot be recovered by a later PASS")


def assess(policy, candidate, pre, approval, post, now):
    result = {"schema": "qa3-publish-contract-decision-v1", "stage": "contract-model", "decision": "BLOCK", "externalAuthentication": "REQUIRED", "publicationAuthorized": False, "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED"}
    try:
        require(integer(now), "explicit bounded evaluation time required")
        check_policy(policy)
        closed(candidate, {"schema", "controllerHead", "subject", "evidence"})
        require(candidate["schema"] == "qa3-publish-contract-candidate-v1" and candidate["controllerHead"] == policy["controllerHead"], "candidate controller differs")
        subject, refs = candidate["subject"], candidate["evidence"]
        check_subject(subject)
        closed(refs, {"source", "qualification", "compileProfile", "sidecar", "products"})
        for key in ("source", "qualification", "compileProfile", "sidecar"):
            reference(refs[key])
        expected_keys = {e["target"] + "/" + e["member"] for e in policy["executions"]}
        require(isinstance(refs["products"], dict) and set(refs["products"]) == expected_keys and all(e["member"] in subject["members"] for e in policy["executions"]), "all target/member executions must bind the final bundle")
        for ref in refs["products"].values(): reference(ref)
        check_snapshot(pre, subject, candidate, policy, now)
        check_snapshot(post, subject, candidate, policy, now)
        closed(approval, {"schema", "id", "subjectDigest", "policyDigest", "preSnapshotDigest", "gatingRun", "attempt", "environment", "approverId", "approvedAt", "decision"})
        require(approval["schema"] == "qa3-publish-contract-approval-v1" and integer(approval["id"]) and approval["subjectDigest"] == digest(subject) and approval["policyDigest"] == digest(policy) and approval["preSnapshotDigest"] == digest(pre), "approval binds different evidence")
        require(integer(approval["gatingRun"]) and approval["gatingRun"] == policy["gateRun"] and type(approval["attempt"]) is int and approval["attempt"] == 1 and approval["environment"] == policy["environment"] and type(approval["approverId"]) is int and approval["approverId"] in policy["approverIds"] and approval["decision"] == "approved", "original manual approval missing")
        require(integer(approval["approvedAt"]) and pre["observedAt"] < approval["approvedAt"] < post["observedAt"] <= now and approval["approvedAt"] - pre["observedAt"] < policy["approvalTTL"] and now - approval["approvedAt"] < policy["approvalTTL"] and now - post["observedAt"] < policy["snapshotTTL"], "fresh post-approval revalidation required")
        require(pre["afterApprovalId"] is None and integer(post["afterApprovalId"]) and post["afterApprovalId"] == approval["id"], "post snapshot does not follow this approval")
        require(all(identical(pre[k], post[k]) for k in ("subjectDigest", "policyDigest", "producer", "closedProductionBundleVerified", "finalSignatureAndNativeRecordsVerified", "entries", "history")), "evidence/history changed after approval")
        result["decision"] = "READY_FOR_AUTHORITATIVE_REVALIDATION"
    except (ValueError, TypeError, KeyError, OverflowError, RecursionError):
        # No arbitrary fixture content, secret or path is copied into errors.
        result["reason"] = "required contract evidence missing, changed or invalid"
    return result
