# SPDX-License-Identifier: MPL-2.0

"""Read retained v1 pilot history. Every result remains BLOCK for a gate."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import time

from claim_store import EVIDENCE_NAMES, REPOSITORY_ID, SUITE, load_claim_store, stable_key
from prepare import unique_object, validate_execution, validate_provider

JSON_LIMIT = 4 * 1024**2
FILE_LIMIT = 8 * 1024**2
TOTAL_LIMIT = 96 * 1024**2
MAX_ID = 2**53 - 1
HEX64 = re.compile(r"[0-9a-f]{64}")
REQUEST_KEYS = {"schema", "controllerHead", "executionRun", "executionAttempt", "floorpHead", "bundleRun", "bundleAttempt", "bundleArtifact", "runtimeHead", "runtimeRun", "runtimeAttempt", "target"}
CLAIM_KEYS = {"schema", "store", "execution", "package", "request", "originalRootResult", "status"}
TERMINAL_KEYS = {"schema", "store", "executionKey", "subject", "status", "evidence", "publicationAuthorized", "nativeProductQualification", "compileProfileQualification"}
INCOMPLETE_KEYS = {"schema", "executionKey", "store", "subject", "status", "originalError", "commitError", "originalRootResult", "availableEvidence", "retentionErrors", "publicationAuthorized", "nativeProductQualification", "compileProfileQualification"}


class AuditBlocked(ValueError):
    pass


def require(condition, code):
    if not condition:
        raise AuditBlocked(code)


def closed(value, keys, code):
    require(type(value) is dict and set(value) == keys, code)


def equal_json(left, right):
    return json.dumps(left, sort_keys=True, separators=(",", ":"), allow_nan=False) == json.dumps(right, sort_keys=True, separators=(",", ":"), allow_nan=False)


def require_root():
    require(os.geteuid() == 0, "TRUSTED_ROOT_SUPERVISOR_REQUIRED")


def ancestor_authority(info):
    sticky_root = info.st_uid == 0 and info.st_mode & stat.S_ISVTX
    require(info.st_uid in {0, os.geteuid()} and (not info.st_mode & 0o022 or sticky_root), "ANCESTOR_AUTHORITY")


def parse(raw):
    def invalid_constant(_):
        raise AuditBlocked("NONFINITE_JSON")
    def finite_float(value):
        parsed = float(value)
        require(math.isfinite(parsed), "NONFINITE_JSON")
        return parsed
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object, parse_constant=invalid_constant, parse_float=finite_float)
    except (ValueError, UnicodeError, RecursionError) as error:
        raise AuditBlocked("INVALID_OR_DUPLICATE_JSON") from error


def signature(info):
    return (info.st_dev, info.st_ino, info.st_mode, info.st_uid, info.st_gid, info.st_nlink, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


class Snapshot:
    def __init__(self):
        self.fds, self.files, self.directories, self.absences = [], [], [], []
        self.inventory = {}
        self.total = 0
        self.deadline = time.monotonic() + 30

    def check_time(self):
        require(time.monotonic() < self.deadline, "AUDIT_DEADLINE")

    def own(self, descriptor):
        self.fds.append(descriptor)
        require(len(self.fds) <= 128, "DESCRIPTOR_BOUND")
        return descriptor

    def close(self):
        failed = False
        for descriptor in reversed(self.fds):
            try:
                os.close(descriptor)
            except OSError:
                failed = True
        self.fds.clear()
        return not failed

    def directory(self, parent, name, private=False):
        self.check_time()
        before = os.stat(name, dir_fd=parent, follow_symlinks=False)
        require(stat.S_ISDIR(before.st_mode), "DIRECTORY_TYPE")
        if private:
            require(before.st_uid == os.geteuid() and not before.st_mode & 0o077, "DIRECTORY_AUTHORITY")
        else:
            ancestor_authority(before)
        descriptor = self.own(os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=parent))
        require(signature(os.fstat(descriptor)) == signature(before), "DIRECTORY_CHANGED")
        self.directories.append((parent, name, descriptor, before, private))
        return descriptor

    def absolute_directory(self, path, private=False):
        path = Path(path)
        require(path.is_absolute() and ".." not in path.parts, "ABSOLUTE_PATH_REQUIRED")
        descriptor = self.own(os.open("/", os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC))
        ancestor_authority(os.fstat(descriptor))
        for index, part in enumerate(path.parts[1:]):
            descriptor = self.directory(descriptor, part, private and index == len(path.parts) - 2)
        return descriptor

    def present(self, directory, name):
        try:
            os.stat(name, dir_fd=directory, follow_symlinks=False)
            return True
        except FileNotFoundError:
            self.absences.append((directory, name))
            return False

    def read(self, directory, name, label, limit=JSON_LIMIT, keep=True, private=True):
        self.check_time()
        require(Path(name).name == name and name not in {"", ".", ".."}, "FLAT_MEMBER_REQUIRED")
        before = os.stat(name, dir_fd=directory, follow_symlinks=False)
        require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1, "REGULAR_SINGLE_LINK_REQUIRED")
        require(before.st_uid == os.geteuid() and not before.st_mode & (0o077 if private else 0o022), "FILE_AUTHORITY")
        require(0 <= before.st_size <= limit, "FILE_BYTE_BOUND")
        descriptor = self.own(os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC, dir_fd=directory))
        require(signature(os.fstat(descriptor)) == signature(before), "FILE_CHANGED")
        digest, chunks, size = hashlib.sha256(), [], 0
        while True:
            self.check_time()
            data = os.read(descriptor, min(128 * 1024, limit + 1 - size))
            if not data:
                break
            size += len(data)
            self.total += len(data)
            require(size <= limit and self.total <= TOTAL_LIMIT, "TOTAL_BYTE_BOUND")
            digest.update(data)
            if keep:
                chunks.append(data)
        require(size == before.st_size and signature(os.fstat(descriptor)) == signature(before), "FILE_CHANGED")
        self.files.append((directory, name, descriptor, before))
        self.inventory[label] = {"bytes": size, "sha256": digest.hexdigest()}
        return b"".join(chunks) if keep else None

    def absolute_file(self, path, label):
        path = Path(path)
        parent = self.absolute_directory(path.parent)
        return self.read(parent, path.name, label, private=False)

    def members(self, directory):
        names = set()
        with os.scandir(directory) as entries:
            for entry in entries:
                names.add(entry.name)
                require(len(names) <= len(EVIDENCE_NAMES) and entry.name in EVIDENCE_NAMES, "EVIDENCE_NAMESPACE")
        return names

    def finish(self):
        self.check_time()
        for parent, name, descriptor, before in self.files:
            require(signature(os.fstat(descriptor)) == signature(before), "FILE_CHANGED")
            require(signature(os.stat(name, dir_fd=parent, follow_symlinks=False)) == signature(before), "FILE_PATH_CHANGED")
        for parent, name, descriptor, before, watch_contents in self.directories:
            current = os.fstat(descriptor)
            at_path = os.stat(name, dir_fd=parent, follow_symlinks=False)
            expected = signature(before)
            require(signature(current)[:5] == expected[:5] and signature(at_path)[:5] == expected[:5], "DIRECTORY_PATH_CHANGED")
            if watch_contents:
                require(signature(current) == expected and signature(at_path) == expected, "DIRECTORY_CONTENTS_CHANGED")
        for parent, name in self.absences:
            try:
                os.stat(name, dir_fd=parent, follow_symlinks=False)
            except FileNotFoundError:
                continue
            raise AuditBlocked("ABSENT_RECORD_APPEARED")


def validate_request(request):
    closed(request, REQUEST_KEYS, "REQUEST_KEYS")
    require(request["schema"] == "qa3-final-workspace-request-v1" and request["target"] == "linux-x86_64", "REQUEST_SCOPE")
    for name in ["executionRun", "bundleRun", "runtimeRun"]:
        require(type(request[name]) is int and 1 <= request[name] <= MAX_ID, "REQUEST_RUN")
    for name in ["executionAttempt", "bundleAttempt", "runtimeAttempt"]:
        require(type(request[name]) is int and request[name] == 1, "ORIGINAL_ATTEMPT_REQUIRED")
    for name in ["controllerHead", "floorpHead", "runtimeHead"]:
        require(type(request[name]) is str and re.fullmatch(r"[0-9a-f]{40}", request[name]), "REQUEST_HEAD")
    artifact = request["bundleArtifact"]
    closed(artifact, {"id", "name", "digest"}, "ARTIFACT_KEYS")
    require(type(artifact["id"]) is int and 1 <= artifact["id"] <= MAX_ID, "ARTIFACT_ID")
    require(type(artifact["name"]) is str and re.fullmatch(r"[A-Za-z0-9._-]{1,128}", artifact["name"]), "ARTIFACT_NAME")
    require(type(artifact["digest"]) is str and re.fullmatch(r"sha256:[0-9a-f]{64}", artifact["digest"]), "ARTIFACT_DIGEST")
    require(request["executionRun"] != request["bundleRun"], "INDEPENDENT_CONSUMER_REQUIRED")


def validate_claim(record, binding, key):
    closed(record, CLAIM_KEYS, "CLAIM_KEYS")
    require(record["schema"] == "qa3-durable-execution-claim-v1" and record["status"] == "CONSUMED_NO_RETRY" and record["store"] == binding, "CLAIM_BINDING")
    request = record["request"]
    validate_request(request)
    execution = {"repositoryId": REPOSITORY_ID, "executionRun": request["executionRun"], "attempt": 1, "target": request["target"]}
    package = {"repositoryId": REPOSITORY_ID, "bundleRun": request["bundleRun"], "bundleAttempt": 1, "artifactId": request["bundleArtifact"]["id"], "artifactDigest": request["bundleArtifact"]["digest"], "target": request["target"], "suite": SUITE}
    require(equal_json(record["execution"], execution) and equal_json(record["package"], package) and stable_key(execution) == key, "CLAIM_IDENTITY")
    original = record["originalRootResult"]
    require(type(original) is str and len(original) <= 4096 and "\x00" not in original and "\\" not in original and Path(original).is_absolute() and ".." not in Path(original).parts and Path(original).name == "root-result.json", "ORIGINAL_RESULT_PATH")
    return request, stable_key(package)


def validate_nonpublication(record, binding, key, request):
    require(record["store"] == binding and record["executionKey"] == key, "RESULT_BINDING")
    subject = record["subject"]
    require(type(subject) is dict and equal_json(subject.get("request"), request), "SUBJECT_BINDING")
    require(subject.get("publicationAuthorized") is False and subject.get("compileProfileQualification") == "UNVERIFIED", "SUBJECT_QUALIFICATION")
    require(record["publicationAuthorized"] is False and record["nativeProductQualification"] == record["compileProfileQualification"] == "UNVERIFIED", "RESULT_QUALIFICATION")


def read_evidence(snapshot, store_fd, key, suffix, inventory, complete):
    require(type(inventory) is dict and set(inventory) <= EVIDENCE_NAMES, "INVENTORY_KEYS")
    name = key + suffix
    if not snapshot.present(store_fd, name):
        require(not inventory, "EVIDENCE_DIRECTORY_MISSING")
        return {}
    directory = snapshot.directory(store_fd, name, private=True)
    names = snapshot.members(directory)
    require(set(inventory) <= names and (not complete or names == set(inventory)), "INVENTORY_MEMBERS")
    parsed = {}
    for member, expected in sorted(inventory.items()):
        closed(expected, {"bytes", "sha256"}, "INVENTORY_MEMBER_KEYS")
        require(type(expected["bytes"]) is int and 0 <= expected["bytes"] <= FILE_LIMIT and type(expected["sha256"]) is str and HEX64.fullmatch(expected["sha256"]), "INVENTORY_MEMBER_VALUE")
        keep = member in {"root-result.json", "worker-result.json", "original-provider-snapshot.json", "original-execution-snapshot.json"}
        label = name + "/" + member
        raw = snapshot.read(directory, member, label, JSON_LIMIT if keep else FILE_LIMIT, keep=keep)
        require(snapshot.inventory[label] == expected, "EVIDENCE_DIGEST")
        if keep:
            parsed[member] = parse(raw)
    return parsed


def observe(snapshot, store, key):
    directory = snapshot.absolute_directory(store.directory, private=True)
    claim_raw = snapshot.read(directory, "execution-" + key + ".json", "execution-claim")
    claim = parse(claim_raw)
    request, package_key = validate_claim(claim, store.binding, key)
    package_raw = snapshot.read(directory, "package-" + package_key + ".json", "package-claim")
    require(claim_raw == package_raw, "CLAIM_BYTES_DIFFER")
    incomplete_name, terminal_name = key + ".incomplete.json", key + ".terminal.json"
    incomplete_present = snapshot.present(directory, incomplete_name)
    terminal_present = snapshot.present(directory, terminal_name)
    if incomplete_present:
        record = parse(snapshot.read(directory, incomplete_name, "incomplete"))
        closed(record, INCOMPLETE_KEYS, "INCOMPLETE_KEYS")
        require(record["schema"] == "qa3-durable-claim-incomplete-v1" and record["status"] == "FAIL", "INCOMPLETE_STATUS")
        validate_nonpublication(record, store.binding, key, request)
        require(type(record["originalRootResult"]) is dict and equal_json(record["originalRootResult"].get("subject"), record["subject"]), "INCOMPLETE_ROOT_BINDING")
        require(type(record["originalError"]) is str and 0 < len(record["originalError"]) <= JSON_LIMIT and type(record["commitError"]) is str and type(record["retentionErrors"]) is dict, "INCOMPLETE_ERRORS")
        root = record["originalRootResult"]
        require(root.get("schema") == "qa3-final-workspace-root-v2" and root.get("status") == "PENDING_DURABLE_COMMIT" and root.get("claimStore") == store.binding and root.get("originalExecutionClaim") == key, "INCOMPLETE_ROOT_IDENTITY")
        require(root.get("publicationAuthorized") is False and root.get("nativeProductQualification") == root.get("compileProfileQualification") == "UNVERIFIED", "INCOMPLETE_ROOT_QUALIFICATION")
        require(all(root.get(name) is None or type(root[name]) is str for name in ["originalError", "cleanupError"]), "INCOMPLETE_ROOT_ERROR_TYPES")
        require(record["originalError"] == (root.get("originalError") or root.get("cleanupError") or record["commitError"]), "FIRST_ERROR_PRIORITY")
        evidence = read_evidence(snapshot, directory, key, ".incomplete-evidence", record["availableEvidence"], complete=False)
        root_available = "root-result.json" in evidence
        require(not root_available or equal_json(evidence["root-result.json"], root), "INCOMPLETE_ROOT_BYTES_DIFFER")
        return {"auditStatus": "INCOMPLETE_OBSERVED", "reason": "INCOMPLETE_PRECEDES_ANY_TERMINAL", "terminalAlsoPresent": terminal_present, "originalErrorSha256": hashlib.sha256(record["originalError"].encode()).hexdigest(), "retainedRootAvailable": root_available, "retainedRootMatchesEmbedded": True if root_available else None}
    if not terminal_present:
        return {"auditStatus": "CONSUMED_WITHOUT_TERMINAL", "reason": "ORIGINAL_TERMINAL_ABSENT"}
    terminal = parse(snapshot.read(directory, terminal_name, "terminal"))
    closed(terminal, TERMINAL_KEYS, "TERMINAL_KEYS")
    require(terminal["schema"] == "qa3-durable-claim-terminal-v1" and terminal["status"] in {"FAIL", "PASS_WORKSPACE_BEHAVIOR_ONLY"}, "TERMINAL_STATUS")
    validate_nonpublication(terminal, store.binding, key, request)
    require("root-result.json" in terminal["evidence"], "ROOT_EVIDENCE_REQUIRED")
    passed = terminal["status"] == "PASS_WORKSPACE_BEHAVIOR_ONLY"
    require(not passed or set(terminal["evidence"]) == EVIDENCE_NAMES, "COMPLETE_EVIDENCE_REQUIRED")
    evidence = read_evidence(snapshot, directory, key, ".evidence", terminal["evidence"], complete=True)
    root = evidence["root-result.json"]
    require(type(root) is dict and root.get("schema") == "qa3-final-workspace-root-v2" and root.get("status") == "PENDING_DURABLE_COMMIT", "ROOT_SCHEMA_OR_PENDING_STATUS")
    require(equal_json(root.get("subject"), terminal["subject"]) and root.get("claimStore") == store.binding and root.get("originalExecutionClaim") == key and root.get("executionStatus") == terminal["status"], "ROOT_BINDING")
    require(root.get("publicationAuthorized") is False and root.get("nativeProductQualification") == root.get("compileProfileQualification") == "UNVERIFIED", "ROOT_QUALIFICATION")
    if passed:
        require(root.get("scopeEmpty") is True and root.get("evidenceIncomplete") is False and root.get("evidenceCollectionErrors") == {}, "ROOT_CLEANUP_OR_EVIDENCE")
        require(all(root.get(name) is None for name in ["originalError", "cleanupError", "cancellation", "stateCleanupError", "stagingCleanupError"]), "ROOT_ORIGINAL_FAILURE")
        worker = evidence["worker-result.json"]
        require(type(worker) is dict and worker.get("schema") == "qa3-final-workspace-worker-v1" and equal_json(worker.get("subject"), terminal["subject"]) and worker.get("status") == terminal["status"] and worker.get("ownedCleanupComplete") is True and worker.get("originalError") is None and worker.get("cleanupError") is None and worker.get("publicationAuthorized") is False and worker.get("nativeProductQualification") == "UNVERIFIED", "WORKER_CONTRADICTION")
        api = root.get("apiSnapshots")
        closed(api, {"original-provider-snapshot.json", "original-execution-snapshot.json"}, "API_SNAPSHOT_KEYS")
        require(all(api[name] == terminal["evidence"][name]["sha256"] for name in api), "API_SNAPSHOT_DIGEST")
        for name, run_key in [("original-provider-snapshot.json", "bundleRun"), ("original-execution-snapshot.json", "executionRun")]:
            value = evidence[name]
            closed(value, {"run", "artifact"} if run_key == "bundleRun" else {"run"}, "SNAPSHOT_KEYS")
            run = value["run"]
            require(type(run) is dict, "SNAPSHOT_RUN_OBJECT")
            require(type(run.get("id")) is int and run["id"] == request[run_key] and type(run.get("run_attempt")) is int and run["run_attempt"] == 1, "SNAPSHOT_ORIGINAL_IDENTITY")
            require(type(run.get("repository")) is dict, "SNAPSHOT_REPOSITORY_OBJECT")
            if run_key == "bundleRun":
                artifact = value["artifact"]
                require(type(artifact) is dict and type(artifact.get("id")) is int and artifact["id"] == request["bundleArtifact"]["id"], "SNAPSHOT_ARTIFACT_ID")
                producer = artifact.get("workflow_run")
                require(type(producer) is dict and type(producer.get("id")) is int and producer["id"] == request["bundleRun"], "SNAPSHOT_ARTIFACT_RUN")
        validate_provider(evidence["original-provider-snapshot.json"], request)
        validate_execution(evidence["original-execution-snapshot.json"], request)
    return {"auditStatus": "CONSISTENT_RETAINED_TERMINAL", "reason": "ORIGINAL_SUPERVISOR_AND_CI_FINAL_OUTCOME_UNAUTHENTICATED", "observedTerminalStatus": terminal["status"]}


def audit_history(policy_path, execution_key):
    result = {"schema": "qa3-pilot-history-audit-v1", "auditStatus": "BLOCKED_INCONSISTENT_OR_UNREADABLE", "reason": "NOT_OBSERVED", "historyScope": "v1-execution-and-package-artifact-only", "gateVerdict": "BLOCK", "originalSupervisorFinalOutcome": "UNVERIFIED", "providerAuthentication": "NOT_ESTABLISHED_BY_THIS_AUDIT", "contentStableFirstEvaluationAuthority": "NOT_ESTABLISHED", "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED", "publicationAuthorized": False}
    snapshot = Snapshot()
    try:
        require_root()
        require(type(execution_key) is str and HEX64.fullmatch(execution_key), "EXECUTION_KEY")
        store = load_claim_store(Path(policy_path))
        policy_raw = snapshot.absolute_file(policy_path, "policy")
        require(hashlib.sha256(policy_raw).hexdigest() == store.binding["policySha256"], "POLICY_CHANGED")
        policy = parse(policy_raw)
        binding = {name: store.binding[name] for name in ["storeId", "directory", "filesystemType", "source", "namespace"]}
        require(equal_json(policy, {"schema": "qa3-durable-claim-store-policy-v1", **binding, "qualificationPath": policy["qualificationPath"], "qualificationSha256": store.binding["qualificationSha256"]}), "POLICY_SCHEMA_OR_BINDING")
        qualification_raw = snapshot.absolute_file(Path(policy["qualificationPath"]), "qualification")
        require(hashlib.sha256(qualification_raw).hexdigest() == store.binding["qualificationSha256"], "QUALIFICATION_CHANGED")
        qualification = parse(qualification_raw)
        require(equal_json(qualification, {"schema": "qa3-durable-claim-store-qualification-v1", **binding, "status": "PASS", "checks": {"crossClientExclusiveCreate": True, "serverRestartDurability": True, "originalEvidenceRetention": True}}), "QUALIFICATION_SCHEMA_OR_TYPED_CHECKS")
        observation = observe(snapshot, store, execution_key)
        require(load_claim_store(Path(policy_path)).binding == store.binding, "STORE_POLICY_CHANGED")
        snapshot.finish()
        result.update(observation)
        result["observedFiles"] = snapshot.inventory
        result["readBytes"] = snapshot.total
    except AuditBlocked as error:
        result["reason"] = str(error)
    except (OSError, ValueError, KeyError, TypeError, RecursionError) as error:
        result["reason"] = "POLICY_OR_ORIGINAL_EVIDENCE_UNREADABLE"
        result["errorType"] = type(error).__name__
    finally:
        if not snapshot.close():
            if result["auditStatus"] != "BLOCKED_INCONSISTENT_OR_UNREADABLE":
                result["auditStatus"] = "BLOCKED_INCONSISTENT_OR_UNREADABLE"
                result["reason"] = "DESCRIPTOR_CLOSE_FAILED"
            result["cleanupError"] = "DESCRIPTOR_CLOSE_FAILED"
    return result


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--execution-key", required=True)
    args = parser.parse_args(arguments)
    print(json.dumps(audit_history(args.policy, args.execution_key), sort_keys=True))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
