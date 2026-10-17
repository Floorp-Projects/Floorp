# SPDX-License-Identifier: MPL-2.0

import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import claim_store
from claim_store import ClaimStore, EVIDENCE_NAMES
import history
from test_prepare import host_fixture_authority

REAL_ANCESTOR_AUTHORITY = history.ancestor_authority
HOST_ANCESTORS = {(Path(name).stat().st_dev, Path(name).stat().st_ino) for name in ["/", tempfile.gettempdir()]}


def host_ancestor_authority(info):
    if (info.st_dev, info.st_ino) not in HOST_ANCESTORS:
        REAL_ANCESTOR_AUTHORITY(info)


@contextlib.contextmanager
def fixture_audit_authority(store):
    with patch("history.require_root"), patch("history.load_claim_store", return_value=store), patch("history.ancestor_authority", side_effect=host_ancestor_authority):
        yield


def encode(value):
    return (json.dumps(value, sort_keys=True) + "\n").encode()


class HistoryFixture:
    def __init__(self, root):
        self.root = root
        self.directory = root / "claims"
        self.directory.mkdir(mode=0o700)
        self.output = root / "original-output"
        self.output.mkdir(mode=0o700)
        binding = {"storeId": "host-fixture-not-qualified", "directory": str(self.directory), "filesystemType": "nfs4", "source": "host-fixture-only:/not-a-server", "namespace": "/host-fixture"}
        self.qualification = root / "qualification.json"
        self.qualification.write_bytes(encode({"schema": "qa3-durable-claim-store-qualification-v1", **binding, "status": "PASS", "checks": {"crossClientExclusiveCreate": True, "serverRestartDurability": True, "originalEvidenceRetention": True}}))
        self.policy = root / "policy.json"
        self.policy.write_bytes(encode({"schema": "qa3-durable-claim-store-policy-v1", **binding, "qualificationPath": str(self.qualification), "qualificationSha256": hashlib.sha256(self.qualification.read_bytes()).hexdigest()}))
        binding.update({"policySha256": hashlib.sha256(self.policy.read_bytes()).hexdigest(), "qualificationSha256": hashlib.sha256(self.qualification.read_bytes()).hexdigest()})
        self.store = ClaimStore(self.directory, binding)
        self.request = {"schema": "qa3-final-workspace-request-v1", "executionRun": 123, "executionAttempt": 1, "controllerHead": "a" * 40, "floorpHead": "b" * 40, "bundleRun": 456, "bundleAttempt": 1, "bundleArtifact": {"id": 789, "name": "floorp-release-bundle-v2", "digest": "sha256:" + "c" * 64}, "runtimeHead": "d" * 40, "runtimeRun": 987, "runtimeAttempt": 1, "target": "linux-x86_64"}
        self.subject = {"request": self.request, "manifestSetId": "host-fixture-no-product", "member": {"name": "host-fixture.tar.xz", "size": 1, "sha512": "e" * 128}, "expectedAppBuildID": "20261015000000", "expectedPlatformBuildID": "20261015000000", "expectedBuildID2": "host-fixture", "expectedFirefoxVersion": "158.0.1", "compileProfileQualification": "UNVERIFIED", "publicationAuthorized": False, "binarySha256": "f" * 64, "binaryRelativePath": "floorp"}
        self.key = self.store.consume(self.request, self.output)
        provider = {"run": {"id": 456, "head_sha": self.request["floorpHead"], "run_attempt": 1, "status": "completed", "conclusion": "success", "repository": {"full_name": "Floorp-Projects/Floorp"}}, "artifact": {**self.request["bundleArtifact"], "expired": False, "workflow_run": {"id": 456, "head_sha": self.request["floorpHead"]}}}
        execution = {"run": {"id": 123, "head_sha": self.request["controllerHead"], "run_attempt": 1, "status": "in_progress", "conclusion": None, "repository": {"id": 437505831, "full_name": "Floorp-Projects/Floorp"}}}
        self.worker = {"schema": "qa3-final-workspace-worker-v1", "subject": self.subject, "status": "PASS_WORKSPACE_BEHAVIOR_ONLY", "ownedCleanupComplete": True, "originalError": None, "cleanupError": None, "publicationAuthorized": False, "nativeProductQualification": "UNVERIFIED"}
        for name, value in [("original-provider-snapshot.json", provider), ("original-execution-snapshot.json", execution), ("worker-result.json", self.worker)]:
            (self.output / name).write_bytes(encode(value))
        for name in ["browser.stdout", "browser.stderr", "service.log"]:
            (self.output / name).write_bytes(b"host fixture only; no native execution\n")
        (self.output / "input-inventory.json").write_bytes(b'{"hostFixtureOnly":true}\n')
        api = {name: hashlib.sha256((self.output / name).read_bytes()).hexdigest() for name in ["original-provider-snapshot.json", "original-execution-snapshot.json"]}
        self.result = {"schema": "qa3-final-workspace-root-v2", "subject": self.subject, "unit": "qa3-workspace-" + "0" * 32 + ".service", "status": "PENDING_DURABLE_COMMIT", "executionStatus": "PASS_WORKSPACE_BEHAVIOR_ONLY", "publicationAuthorized": False, "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED", "claimScope": "QUALIFIED_SHARED_DURABLE_STORE", "claimStore": binding, "apiSnapshots": api, "consumerBootSha256": "1" * 64, "originalExecutionClaim": self.key, "scopeEmpty": True, "evidenceCollectionErrors": {}, "evidenceIncomplete": False, "originalError": None, "cleanupError": None, "cancellation": None, "observedWallSeconds": 0.1, "invoiceConfirmed": False}
        self.write_root()

    def write_root(self):
        (self.output / "root-result.json").write_bytes(encode(self.result))

    def commit(self):
        return self.store.commit(self.key, self.output, self.result)

    def audit(self, key=None):
        with fixture_audit_authority(self.store):
            return history.audit_history(self.policy, self.key if key is None else key)

    def terminal_path(self):
        return self.directory / (self.key + ".terminal.json")

    def replace_evidence_json(self, name, value):
        self.replace_evidence_raw(name, encode(value))

    def replace_evidence_raw(self, name, raw):
        target = self.directory / (self.key + ".evidence") / name
        target.write_bytes(raw)
        terminal = json.loads(self.terminal_path().read_bytes())
        terminal["evidence"][name] = {"bytes": target.stat().st_size, "sha256": hashlib.sha256(target.read_bytes()).hexdigest()}
        self.terminal_path().write_bytes(encode(terminal))

    def bytes_and_modes(self):
        return {str(p.relative_to(self.root)): (hashlib.sha256(p.read_bytes()).hexdigest(), p.stat().st_mode) for p in self.root.rglob("*") if p.is_file()}


class RetainedHistoryContracts(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qa3-history-host-")
        self.addCleanup(temporary.cleanup)
        authority = patch("claim_store.protected", host_fixture_authority)
        authority.start()
        self.addCleanup(authority.stop)
        self.fixture = HistoryFixture(Path(temporary.name))

    def assert_blocked(self, result, observation=None):
        self.assertEqual(result["gateVerdict"], "BLOCK")
        self.assertFalse(result["publicationAuthorized"])
        self.assertEqual(result["nativeProductQualification"], "UNVERIFIED")
        self.assertEqual(result["compileProfileQualification"], "UNVERIFIED")
        self.assertEqual(result["originalSupervisorFinalOutcome"], "UNVERIFIED")
        self.assertEqual(result["contentStableFirstEvaluationAuthority"], "NOT_ESTABLISHED")
        if observation:
            self.assertEqual(result["auditStatus"], observation, result)

    def test_retained_terminal_is_observed_read_only_and_never_gate_success(self):
        f = self.fixture
        f.commit()
        before = f.bytes_and_modes()
        result = f.audit()
        self.assert_blocked(result, "CONSISTENT_RETAINED_TERMINAL")
        self.assertEqual(result["observedTerminalStatus"], "PASS_WORKSPACE_BEHAVIOR_ONLY")
        self.assertEqual(len(result["observedFiles"]), 13)
        self.assertEqual(f.bytes_and_modes(), before)

    def test_actual_audit_entry_has_no_writer_process_or_filesystem_mutation(self):
        f = self.fixture
        f.commit()
        original_open = os.open
        def read_only(path, flags, *args, **kwargs):
            self.assertFalse(flags & (os.O_CREAT | os.O_TRUNC | os.O_APPEND | os.O_WRONLY | os.O_RDWR))
            return original_open(path, flags, *args, **kwargs)
        forbidden = AssertionError("host audit must not mutate or launch")
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch("history.os.open", side_effect=read_only))
            for name in ["consume", "commit", "incomplete"]:
                stack.enter_context(patch.object(ClaimStore, name, side_effect=forbidden))
            for name in ["os.mkdir", "os.unlink", "os.rename", "subprocess.Popen", "socket.socket"]:
                stack.enter_context(patch(name, side_effect=forbidden))
            result = f.audit()
        self.assert_blocked(result, "CONSISTENT_RETAINED_TERMINAL")

    def test_original_failed_terminal_accepts_retained_subset_and_stays_failure(self):
        f = self.fixture
        for p in f.output.iterdir():
            p.unlink()
        f.result.update(executionStatus="FAIL", originalError="host first failure")
        f.write_root()
        f.commit()
        result = f.audit()
        self.assert_blocked(result, "CONSISTENT_RETAINED_TERMINAL")
        self.assertEqual(result["observedTerminalStatus"], "FAIL")
        self.assertIn(f.key + ".evidence/root-result.json", result["observedFiles"])

    def test_late_cancellation_incomplete_dominates_retained_pass_terminal(self):
        f = self.fixture
        original = claim_store.durable_file
        def late_cancel(directory, name, data):
            original(directory, name, data)
            if name.endswith(".terminal.json"):
                raise InterruptedError("host cancellation after terminal sync")
        with patch("claim_store.durable_file", side_effect=late_cancel), self.assertRaises(InterruptedError):
            f.commit()
        result = f.audit()
        self.assert_blocked(result, "INCOMPLETE_OBSERVED")
        self.assertTrue(result["terminalAlsoPresent"])
        self.assertTrue(result["retainedRootAvailable"])
        self.assertTrue(result["retainedRootMatchesEmbedded"])
        self.assertEqual(result["originalErrorSha256"], hashlib.sha256(b"host cancellation after terminal sync").hexdigest())
        self.assertEqual(json.loads(f.terminal_path().read_bytes())["status"], "PASS_WORKSPACE_BEHAVIOR_ONLY")

    def test_consumed_claim_without_terminal_cannot_be_recovered_as_pass(self):
        self.assert_blocked(self.fixture.audit(), "CONSUMED_WITHOUT_TERMINAL")

    def test_public_entry_requires_root_before_loading_any_policy(self):
        with patch("history.os.geteuid", return_value=1000), patch("history.load_claim_store") as loader:
            result = history.audit_history(self.fixture.policy, self.fixture.key)
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertEqual(result["reason"], "TRUSTED_ROOT_SUPERVISOR_REQUIRED")
        loader.assert_not_called()

    def test_real_ancestor_authority_rejects_foreign_or_mutable_owners(self):
        for uid, mode in [(65534, 0o40755), (os.geteuid(), 0o40777)]:
            with self.subTest(uid=uid, mode=mode), self.assertRaises(history.AuditBlocked):
                REAL_ANCESTOR_AUTHORITY(SimpleNamespace(st_uid=uid, st_mode=mode))

    def test_missing_or_local_policy_never_provisions_a_store(self):
        f = self.fixture
        missing = f.root / "not-created" / "policy.json"
        with patch("history.require_root"):
            result = history.audit_history(missing, f.key)
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertFalse(missing.parent.exists())
        policy = json.loads(f.policy.read_bytes())
        policy["filesystemType"] = "overlay"
        f.policy.write_bytes(encode(policy))
        with patch("history.require_root"):
            result = history.audit_history(f.policy, f.key)
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")

    def test_invalid_key_rejected_before_store_load(self):
        for key in ["../escape", "a" * 63, "A" * 64, True, 1]:
            with self.subTest(key=key), patch("history.require_root"), patch("history.load_claim_store") as loader:
                self.assert_blocked(history.audit_history(self.fixture.policy, key), "BLOCKED_INCONSISTENT_OR_UNREADABLE")
                loader.assert_not_called()

    def test_partial_or_foreign_package_claim_cannot_form_history(self):
        f = self.fixture
        package, = f.directory.glob("package-*.json")
        original = package.read_bytes()
        for raw in [b"", original + b" ", encode({"foreign": True})]:
            with self.subTest(rawBytes=len(raw)):
                package.write_bytes(raw)
                self.assert_blocked(f.audit(), "BLOCKED_INCONSISTENT_OR_UNREADABLE")

    def test_closed_duplicate_nonfinite_and_boolean_claims_are_rejected(self):
        f = self.fixture
        execution = f.directory / ("execution-" + f.key + ".json")
        original = json.loads(execution.read_bytes())
        unknown = copy.deepcopy(original); unknown["authenticated"] = True
        boolean = copy.deepcopy(original); boolean["execution"]["attempt"] = True
        retry = copy.deepcopy(original); retry["request"]["executionAttempt"] = 2
        raws = [encode(unknown), encode(boolean), encode(retry), b'{"schema":1,"schema":2}', b'{"schema":NaN}']
        package, = f.directory.glob("package-*.json")
        for raw in raws:
            with self.subTest(rawBytes=len(raw)):
                execution.write_bytes(raw); package.write_bytes(raw)
                self.assert_blocked(f.audit(), "BLOCKED_INCONSISTENT_OR_UNREADABLE")

    def test_unsafe_record_types_and_links_are_not_followed(self):
        f = self.fixture
        execution = f.directory / ("execution-" + f.key + ".json")
        raw = execution.read_bytes()
        outside = f.root / "outside-fixture.json"
        outside.write_bytes(raw)
        for kind in ["symlink", "hardlink", "fifo"]:
            with self.subTest(kind=kind):
                execution.unlink()
                if kind == "symlink": execution.symlink_to(outside)
                elif kind == "hardlink": os.link(outside, execution)
                else: os.mkfifo(execution, mode=0o600)
                self.assert_blocked(f.audit(), "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        execution.unlink(); execution.write_bytes(raw); execution.chmod(0o600)

    def test_symlinked_ancestor_of_authority_is_rejected(self):
        f = self.fixture
        alias = f.root / "aliased-claims"
        alias.symlink_to(f.directory, target_is_directory=True)
        store = ClaimStore(alias, f.store.binding)
        with fixture_audit_authority(store):
            result = history.audit_history(f.policy, f.key)
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")

    def test_record_and_raw_evidence_bounds_fail_closed(self):
        f = self.fixture
        execution = f.directory / ("execution-" + f.key + ".json")
        with execution.open("wb") as stream:
            stream.truncate(history.JSON_LIMIT + 1)
        self.assertEqual(f.audit()["reason"], "FILE_BYTE_BOUND")

    def test_pass_needs_all_evidence_and_exact_retained_bytes(self):
        f = self.fixture
        f.commit()
        target = f.directory / (f.key + ".evidence/browser.stdout")
        original = target.read_bytes()
        target.write_bytes(b"changed host evidence")
        self.assert_blocked(f.audit(), "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        target.write_bytes(original)
        target.unlink()
        self.assert_blocked(f.audit(), "BLOCKED_INCONSISTENT_OR_UNREADABLE")

    def test_extra_evidence_and_inventory_escape_are_rejected(self):
        f = self.fixture
        f.commit()
        extra = f.directory / (f.key + ".evidence/unexpected.json")
        extra.write_bytes(b"{}"); extra.chmod(0o600)
        self.assertEqual(f.audit()["reason"], "EVIDENCE_NAMESPACE")
        extra.unlink()
        terminal = json.loads(f.terminal_path().read_bytes())
        terminal["evidence"]["../escape"] = {"bytes": 0, "sha256": "0" * 64}
        f.terminal_path().write_bytes(encode(terminal))
        self.assert_blocked(f.audit(), "BLOCKED_INCONSISTENT_OR_UNREADABLE")

    def test_root_failure_or_worker_contradiction_cannot_look_consistent(self):
        f = self.fixture
        f.commit()
        root = copy.deepcopy(f.result); root["originalError"] = "first failure"
        f.replace_evidence_json("root-result.json", root)
        self.assertEqual(f.audit()["reason"], "ROOT_ORIGINAL_FAILURE")
        f.replace_evidence_json("root-result.json", f.result)
        worker = copy.deepcopy(f.worker); worker["subject"]["request"]["executionAttempt"] = True
        f.replace_evidence_json("worker-result.json", worker)
        self.assertEqual(f.audit()["reason"], "WORKER_CONTRADICTION")

    def test_file_change_after_hash_is_not_accepted(self):
        f = self.fixture
        f.commit()
        original = history.Snapshot.read
        def change(snapshot, directory, name, label, *args, **kwargs):
            data = original(snapshot, directory, name, label, *args, **kwargs)
            if label.endswith("/browser.stdout"):
                target = f.directory / (f.key + ".evidence/browser.stdout")
                with target.open("ab") as stream: stream.write(b"late change")
            return data
        with patch.object(history.Snapshot, "read", change): result = f.audit()
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertEqual(result["reason"], "FILE_CHANGED")

    def test_incomplete_created_during_read_blocks_the_snapshot(self):
        f = self.fixture
        f.commit()
        original = history.Snapshot.read
        def late_marker(snapshot, directory, name, label, *args, **kwargs):
            data = original(snapshot, directory, name, label, *args, **kwargs)
            if label == "terminal":
                f.store.incomplete(f.key, f.result, ValueError("late host cancellation"), f.output)
            return data
        with patch.object(history.Snapshot, "read", late_marker): result = f.audit()
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertIn(result["reason"], ["DIRECTORY_CONTENTS_CHANGED", "ABSENT_RECORD_APPEARED"])

    def test_qualification_change_and_close_failure_do_not_erase_first_error(self):
        f = self.fixture
        f.qualification.write_bytes(b"{}")
        actual_close = os.close
        failed = False
        def close_once(descriptor):
            nonlocal failed
            actual_close(descriptor)
            if not failed:
                failed = True
                raise OSError("host close fault after real close")
        with patch("history.os.close", side_effect=close_once): result = f.audit()
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertEqual(result["reason"], "QUALIFICATION_CHANGED")
        self.assertEqual(result["cleanupError"], "DESCRIPTOR_CLOSE_FAILED")

    def test_qualification_numeric_truth_alias_is_not_a_closed_boolean_proof(self):
        f = self.fixture
        qualification = json.loads(f.qualification.read_bytes())
        qualification["checks"]["serverRestartDurability"] = 1
        f.qualification.write_bytes(encode(qualification))
        digest = hashlib.sha256(f.qualification.read_bytes()).hexdigest()
        policy = json.loads(f.policy.read_bytes()); policy["qualificationSha256"] = digest
        f.policy.write_bytes(encode(policy))
        f.store.binding.update(qualificationSha256=digest, policySha256=hashlib.sha256(f.policy.read_bytes()).hexdigest())
        result = f.audit()
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertEqual(result["reason"], "QUALIFICATION_SCHEMA_OR_TYPED_CHECKS")

    def test_malformed_original_snapshot_stays_a_bounded_block_report(self):
        f = self.fixture
        f.commit()
        bad = {"run": []}
        f.replace_evidence_json("original-execution-snapshot.json", bad)
        terminal = json.loads(f.terminal_path().read_bytes())
        root = copy.deepcopy(f.result)
        root["apiSnapshots"]["original-execution-snapshot.json"] = terminal["evidence"]["original-execution-snapshot.json"]["sha256"]
        f.replace_evidence_json("root-result.json", root)
        result = f.audit()
        self.assert_blocked(result, "BLOCKED_INCONSISTENT_OR_UNREADABLE")
        self.assertEqual(result["reason"], "SNAPSHOT_RUN_OBJECT")

    def test_float_overflow_in_unreferenced_original_fields_is_rejected(self):
        f = self.fixture
        f.commit()
        for name, value in [("root-result.json", f.result), ("worker-result.json", f.worker)]:
            with self.subTest(name=name):
                raw = encode(value).rstrip()[:-1] + b',"hostOverflow":1e400}\n'
                f.replace_evidence_raw(name, raw)
                self.assertEqual(f.audit()["reason"], "INVALID_OR_DUPLICATE_JSON")
                f.replace_evidence_json(name, value)

    def test_incomplete_embedded_root_must_match_available_original_root_bytes(self):
        f = self.fixture
        f.store.incomplete(f.key, f.result, ValueError("host commit failure"), f.output)
        marker = f.directory / (f.key + ".incomplete.json")
        record = json.loads(marker.read_bytes())
        record["originalRootResult"]["originalError"] = "host forged different root"
        record["originalError"] = "host forged different root"
        marker.write_bytes(encode(record))
        self.assertEqual(f.audit()["reason"], "INCOMPLETE_ROOT_BYTES_DIFFER")

    def test_incomplete_cannot_replace_first_error_with_later_commit_error(self):
        f = self.fixture
        f.result.update(executionStatus="FAIL", originalError="first host failure", cleanupError="later cleanup failure")
        f.write_root()
        f.store.incomplete(f.key, f.result, ValueError("last commit failure"), f.output)
        marker = f.directory / (f.key + ".incomplete.json")
        record = json.loads(marker.read_bytes()); record["originalError"] = record["commitError"]
        marker.write_bytes(encode(record))
        self.assertEqual(f.audit()["reason"], "FIRST_ERROR_PRIORITY")

    def test_incomplete_missing_root_is_explicitly_unavailable_not_verified(self):
        f = self.fixture
        (f.output / "root-result.json").unlink()
        f.store.incomplete(f.key, f.result, ValueError("host root retention failure"), f.output)
        result = f.audit()
        self.assert_blocked(result, "INCOMPLETE_OBSERVED")
        self.assertFalse(result["retainedRootAvailable"])
        self.assertIsNone(result["retainedRootMatchesEmbedded"])

    def test_cli_reports_only_and_exits_nonzero_even_for_consistent_retention(self):
        f = self.fixture
        f.commit()
        stream = io.StringIO()
        with fixture_audit_authority(f.store), contextlib.redirect_stdout(stream):
            code = history.main(["--policy", str(f.policy), "--execution-key", f.key])
        self.assertEqual(code, 2)
        self.assert_blocked(json.loads(stream.getvalue()), "CONSISTENT_RETAINED_TERMINAL")


if __name__ == "__main__":
    unittest.main()
