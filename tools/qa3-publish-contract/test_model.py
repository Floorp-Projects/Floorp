# SPDX-License-Identifier: MPL-2.0
"""Offline contract fixtures only. No real qualification or publisher is used."""
import copy
import json
import unittest
from unittest.mock import Mock

import model


def fixture():
    members = ["floorp-linux-x86_64.tar.xz", "floorp-linux-aarch64.tar.xz", "floorp-windows.exe", "floorp-windows.zip", "floorp-mac.dmg"] + [f"metadata-{n}.json" for n in range(11)]
    executions = [{"target": target, "member": member, "caseIds": ["fixture/startup", "fixture/native-input", "fixture/cleanup"]} for target, member in [("linux-x86_64", members[0]), ("linux-aarch64", members[1]), ("windows-x86_64", members[2]), ("windows-x86_64", members[3]), ("mac-x86_64", members[4]), ("mac-aarch64", members[4])]]
    policy = {"schema": "qa3-publish-contract-policy-v1", "stage": "contract-model", "suite": model.SUITE, "controllerHead": "a" * 40, "gateRun": 900, "environment": "release", "approverIds": [42], "issuer": "fixture-issuer", "backendQualificationSha256": "d" * 64, "snapshotTTL": 60, "approvalTTL": 600, "executions": executions}
    subject = {"repositoryId": 437505831, "floorpHead": "a" * 40, "runtimeHead": "b" * 40, "runtimeRun": 122, "runtimeAttempt": 1, "runtimeManifestArtifact": {"id": 1001, "name": "floorp-runtime-build-manifest-v2", "digest": "sha256:" + "1" * 64, "bytes": 512}, "runtimeBuildID": "20261014104253", "runtimeSourceKey": "c" * 64, "buildPolicyKey": "d" * 64, "producerRun": 123, "producerAttempt": 1, "bundleArtifact": {"id": 1000, "name": "floorp-release-bundle-v2", "digest": "sha256:" + "e" * 64, "bytes": 512}, "manifestSetId": "f" * 64, "members": {name: {"bytes": 100 + n, "sha512": model.digest(name) * 2} for n, name in enumerate(members)}}
    ref = lambda name: {"id": name.replace("compileProfile", "compile-profile"), "sha256": model.digest(name)}
    refs = {name: ref(name) for name in ("source", "qualification", "compileProfile", "sidecar")}
    refs["products"] = {e["target"] + "/" + e["member"]: ref("product/" + e["target"] + "/" + e["member"]) for e in executions}
    candidate = {"schema": "qa3-publish-contract-candidate-v1", "controllerHead": policy["controllerHead"], "subject": subject, "evidence": refs}
    def entry(reference, kind, status, run, details):
        return {"sha256": reference["sha256"], "kind": kind, "status": status, "issuer": policy["issuer"], "expiresAt": 2000, "revoked": False, "subjectDigest": model.digest(subject), "sourceKey": subject["runtimeSourceKey"], "buildPolicyKey": subject["buildPolicyKey"], "run": run, "attempt": 1, "details": details}
    entries = {
        "source": entry(refs["source"], "source", "PASS_SOURCE_ONLY", 123, {"floorpHead": subject["floorpHead"], "executedCases": 109, "originalFailures": 0, "skipped": 0}),
        "qualification": entry(refs["qualification"], "qualification", "QUALIFIED", 120, {"productRuntimeHead": subject["runtimeHead"], "targets": sorted(model.TARGETS), "diagnosticChangesReviewed": True, "sourceEquivalenceVerified": True}),
        "compile-profile": entry(refs["compileProfile"], "compile-profile", "QUALIFIED", 122, {"profileChainVerified": True, "manifestDigest": subject["runtimeManifestArtifact"]["digest"], "runtimeBuildID": subject["runtimeBuildID"], "modes": {target: "production-opt" if target == "linux-aarch64" else "pgo-use" for target in sorted(model.TARGETS)}}),
        "sidecar": entry(refs["sidecar"], "sidecar", "PASS_FINAL_ARTIFACT_QA", 124, {"unsigned": False, "complete": True}),
    }
    history = {}
    for n, execution in enumerate(executions):
        reference = refs["products"][execution["target"] + "/" + execution["member"]]
        entries[reference["id"]] = entry(reference, "product", "PASS", 200 + n, {"target": execution["target"], "member": execution["member"], "memberSha512": subject["members"][execution["member"]]["sha512"], "runtimeRun": subject["runtimeRun"], "runtimeBuildID": subject["runtimeBuildID"], "caseResults": [{"id": case, "status": "PASS", "executions": 1} for case in execution["caseIds"]], "cleanupVerified": True, "nativeIdentityVerified": True})
        history[model.evaluation_key(subject, execution)] = {"originalRun": 200 + n, "originalAttempt": 1, "originalStatus": "PASS", "originalEvidenceSha256": reference["sha256"], "creationAnchor": model.digest("anchor/" + reference["id"]), "complete": True, "backendQualificationSha256": policy["backendQualificationSha256"]}
    pre = {"schema": "qa3-publish-contract-snapshot-v1", "subjectDigest": model.digest(subject), "policyDigest": model.digest(policy), "observedAt": 900, "afterApprovalId": None, "producer": {"id": 123, "head": subject["floorpHead"], "attempt": 1, "status": "completed", "conclusion": "success"}, "closedProductionBundleVerified": True, "finalSignatureAndNativeRecordsVerified": True, "entries": entries, "history": history}
    approval = {"schema": "qa3-publish-contract-approval-v1", "id": 77, "subjectDigest": model.digest(subject), "policyDigest": model.digest(policy), "preSnapshotDigest": model.digest(pre), "gatingRun": 900, "attempt": 1, "environment": "release", "approverId": 42, "approvedAt": 950, "decision": "approved"}
    post = {**copy.deepcopy(pre), "observedAt": 990, "afterApprovalId": 77}
    return [policy, candidate, pre, approval, post, 1000]


class ContractTests(unittest.TestCase):
    def assess_without_public_mutations(self, args):
        # Model a caller's entire external mutation surface; never use real APIs.
        publisher = Mock(spec=["create_draft", "create_tag", "upload_assets", "update_updater", "portable", "ppa"])
        result = model.assess(*args)
        if result["publicationAuthorized"]:
            for name in ["create_draft", "create_tag", "upload_assets", "update_updater", "portable", "ppa"]:
                getattr(publisher, name)()
        self.assertEqual(publisher.mock_calls, [])
        self.assertEqual(result["stage"], "contract-model")
        self.assertEqual(result["externalAuthentication"], "REQUIRED")
        self.assertFalse(result["publicationAuthorized"])
        self.assertEqual(result["nativeProductQualification"], "UNVERIFIED")
        self.assertEqual(result["compileProfileQualification"], "UNVERIFIED")
        return result

    def block(self, args):
        self.assertEqual(self.assess_without_public_mutations(args)["decision"], "BLOCK")

    def both(self, change):
        args = fixture()
        change(args[2]); change(args[4])
        args[3]["preSnapshotDigest"] = model.digest(args[2])
        return args

    def test_complete_offline_model_never_authorizes_or_calls_a_publisher(self):
        args = fixture()
        args[:5] = [model.decode_contract(json.dumps(value).encode()) for value in args[:5]]
        self.assertEqual(self.assess_without_public_mutations(args)["decision"], "READY_FOR_AUTHORITATIVE_REVALIDATION")

    def test_raw_json_rejects_duplicates_nonfinite_unsafe_size_and_nonobjects(self):
        for data in [b'{"id":1,"id":2}', b'{"entries":{"id":1,"id":2}}', b'{"value":NaN}', b'[]', b'\xff', b'x' * (4 * 1024**2 + 1)]:
            with self.subTest(prefix=data[:20]), self.assertRaises(ValueError): model.decode_contract(data)

    def test_missing_unknown_and_self_asserted_authenticated_inputs_block(self):
        for n in range(5):
            args = fixture(); args[n] = None
            with self.subTest(input=n): self.block(args)
        for n in range(5):
            args = fixture(); args[n]["authenticated"] = True
            with self.subTest(extra=n): self.block(args)
        args = fixture(); args[0]["stage"] = "production"; self.block(args)

    def test_all_five_native_targets_all_artifact_kinds_and_closed_bundle_required(self):
        args = fixture(); args[0]["executions"] = [e for e in args[0]["executions"] if e["target"] != "linux-aarch64"]; self.block(args)
        args = fixture(); del args[1]["evidence"]["products"]["windows-x86_64/floorp-windows.zip"]; self.block(args)
        for flag in ["closedProductionBundleVerified", "finalSignatureAndNativeRecordsVerified"]:
            with self.subTest(flag=flag): self.block(self.both(lambda s: s.update({flag: False})))
        args = fixture(); del args[1]["subject"]["members"]["metadata-0.json"]; self.block(args)
        args = fixture(); args[1]["subject"]["bundleArtifact"]["name"] += "-unsigned"; self.block(args)
        self.block(self.both(lambda s: s["entries"]["sidecar"]["details"].update(unsigned=True)))

    def test_source_debug_applicability_profile_and_arm_opt_remain_distinct(self):
        changes = [
            lambda s: s["entries"]["source"]["details"].update(executedCases=108),
            lambda s: s["entries"]["source"]["details"].update(executedCases=106),
            lambda s: s["entries"]["source"]["details"].update(skipped=1),
            lambda s: s["entries"]["source"].update(run=124),
            lambda s: s["entries"]["qualification"].update(status="PASS_DEBUG_HARNESS_ONLY"),
            lambda s: s["entries"]["qualification"]["details"].update(productRuntimeHead="0" * 40),
            lambda s: s["entries"]["qualification"]["details"].update(diagnosticChangesReviewed=False),
            lambda s: s["entries"]["compile-profile"]["details"].update(profileChainVerified=False),
            lambda s: s["entries"]["compile-profile"]["details"]["modes"].update({"linux-aarch64": "pgo-use"}),
        ]
        for n, change in enumerate(changes):
            with self.subTest(change=n): self.block(self.both(change))

    def test_skip_retry_missing_case_cleanup_identity_or_bool_coercion_blocks(self):
        product = "product/linux-x86_64/floorp-linux-x86_64.tar.xz"
        changes = [
            lambda s: s["entries"][product].update(status="SKIP"),
            lambda s: s["entries"][product].update(attempt=2),
            lambda s: s["entries"][product]["details"]["caseResults"].pop(),
            lambda s: s["entries"][product]["details"]["caseResults"][0].update(status="SKIP"),
            lambda s: s["entries"][product]["details"]["caseResults"][0].update(executions=2),
            lambda s: s["entries"][product]["details"].update(cleanupVerified=False),
            lambda s: s["entries"][product]["details"].update(nativeIdentityVerified=False),
            lambda s: s["entries"][product]["details"].update(cleanupVerified=1),
            lambda s: s["entries"][product]["details"]["caseResults"][0].update(executions=True),
            lambda s: s["entries"]["source"]["details"].update(originalFailures=False),
        ]
        for n, change in enumerate(changes):
            with self.subTest(change=n): self.block(self.both(change))

    def test_first_failure_missing_history_new_c_or_unknown_backend_never_recovers(self):
        args = fixture(); first = next(iter(args[2]["history"]))
        changes = [
            lambda s: s["history"][first].update(originalStatus="FAIL"),
            lambda s: s["history"][first].update(originalStatus="INCOMPLETE"),
            lambda s: s["history"][first].update(originalRun=999),
            lambda s: s["history"][first].update(originalAttempt=2),
            lambda s: s["history"][first].update(complete=False),
            lambda s: s["history"][first].update(backendQualificationSha256="0" * 64),
            lambda s: s["history"].pop(first),
        ]
        for n, change in enumerate(changes):
            with self.subTest(change=n): self.block(self.both(change))
        changed = fixture(); subject, execution = changed[1]["subject"], changed[0]["executions"][0]
        key = model.evaluation_key(subject, execution)
        subject["producerRun"] = 999; subject["bundleArtifact"]["id"] = 9999
        self.assertEqual(model.evaluation_key(subject, execution), key, "repack/new producer cannot reset first history for identical member bytes")
        renamed = {**execution, "member": "renamed.tar.xz"}; subject["members"][renamed["member"]] = subject["members"][execution["member"]]
        self.assertEqual(model.evaluation_key(subject, renamed), key)

    def test_approval_binding_stale_post_approval_change_and_expiry_equality_block(self):
        for field, value in [("decision", "pending"), ("approverId", 7), ("gatingRun", 901), ("attempt", 2), ("environment", "other"), ("preSnapshotDigest", "0" * 64), ("approvedAt", 990)]:
            args = fixture(); args[3][field] = value
            with self.subTest(field=field): self.block(args)
        for change in [lambda a: a[4].update(observedAt=940), lambda a: a[4].update(observedAt=940, afterApprovalId=None), lambda a: a[4].update(observedAt=1001), lambda a: a[4].update(afterApprovalId=78), lambda a: a[4]["entries"]["qualification"].update(revoked=True), lambda a: a[4]["entries"]["sidecar"].update(sha256="0" * 64), lambda a: a[4]["history"][next(iter(a[4]["history"]))].update(creationAnchor="0" * 64)]:
            args = fixture(); change(args); self.block(args)
        self.block(self.both(lambda s: s["entries"]["qualification"].update(expiresAt=1000)))
        args = fixture(); args[5] = 1050; self.block(args)
        args = fixture(); args[0]["approvalTTL"] = 50; self.block(args)
        args = fixture(); args[2]["observedAt"] = 100; args[3]["preSnapshotDigest"] = model.digest(args[2]); self.block(args)


if __name__ == "__main__": unittest.main()
