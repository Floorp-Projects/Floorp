# SPDX-License-Identifier: MPL-2.0

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat

from prepare import file_digest, read_json, read_regular

REPOSITORY_ID = 437505831
SUITE = "nativeHiddenTabTests-enabled-six-v1"
SHARED_FILESYSTEMS = {"nfs", "nfs4", "ceph", "fuse.glusterfs"}
EVIDENCE_NAMES = {"root-result.json", "worker-result.json", "browser.stdout", "browser.stderr", "service.log", "input-inventory.json", "original-provider-snapshot.json", "original-execution-snapshot.json"}


def protected(path, directory=False):
    if not path.is_absolute() or ".." in path.parts:
        raise ValueError("Claim authority path is not canonical and absolute")
    info = path.lstat()
    wanted = stat.S_ISDIR if directory else stat.S_ISREG
    if not wanted(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o022:
        raise ValueError("Claim policy/store evidence is not protected by its trusted supervisor")
    for ancestor in [path, *path.parents]:
        if ancestor.is_symlink():
            raise ValueError("Claim authority path contains a symlink")
        if ancestor != path:
            parent = ancestor.lstat()
            sticky_root = parent.st_uid == 0 and parent.st_mode & stat.S_ISVTX
            if parent.st_uid not in {0, os.geteuid()} or parent.st_mode & 0o022 and not sticky_root:
                raise ValueError("Claim authority has a mutable ancestor")


def mount_identity(directory, text):
    candidates = []
    decode = lambda value: re.sub(r"\\([0-7]{3})", lambda m: chr(int(m[1], 8)), value)
    for line in text.splitlines():
        before, after = line.split(" - ", 1)
        fields, filesystem = before.split(), after.split()
        mountpoint = Path(decode(fields[4]))
        if directory == mountpoint or mountpoint in directory.parents:
            root = PurePosixPath(decode(fields[3]))
            if not root.is_absolute() or ".." in root.parts:
                raise ValueError("Claim mount has an unsafe shared namespace")
            namespace = str(root / directory.relative_to(mountpoint))
            candidates.append((len(mountpoint.parts), filesystem[0], decode(filesystem[1]), namespace))
    if not candidates:
        raise ValueError("Claim store mount cannot be identified")
    deepest = max(value[0] for value in candidates)
    selected = [value for value in candidates if value[0] == deepest]
    if len(selected) != 1:
        raise ValueError("Claim store has ambiguous overmounted namespaces")
    _, kind, source, namespace = selected[0]
    return {"filesystemType": kind, "source": source, "namespace": namespace}


def load_claim_store(policy_path, mountinfo_path=Path("/proc/self/mountinfo")):
    # This path comes from the trusted root caller, never a candidate/request ZIP.
    protected(policy_path)
    policy = read_json(policy_path)
    if set(policy) != {"schema", "storeId", "directory", "filesystemType", "source", "namespace", "qualificationPath", "qualificationSha256"} or policy["schema"] != "qa3-durable-claim-store-policy-v1" or not re.fullmatch(r"[a-z0-9][a-z0-9._-]{0,63}", policy["storeId"]):
        raise ValueError("Durable shared claim policy is not accepted")
    directory = Path(policy["directory"])
    qualification_path = Path(policy["qualificationPath"])
    if not directory.is_absolute() or not qualification_path.is_absolute():
        raise ValueError("Claim policy paths must be absolute")
    protected(directory, directory=True)
    if directory.stat().st_mode & 0o077:
        raise ValueError("Durable claim directory must be private")
    protected(qualification_path)
    qualification = read_json(qualification_path)
    binding = {key: policy[key] for key in ["storeId", "directory", "filesystemType", "source", "namespace"]}
    if policy["filesystemType"] not in SHARED_FILESYSTEMS or mount_identity(directory, mountinfo_path.read_text()) != {key: policy[key] for key in ["filesystemType", "source", "namespace"]}:
        raise ValueError("Claim store is not the qualified shared filesystem")
    if file_digest(qualification_path) != policy["qualificationSha256"] or qualification != {"schema": "qa3-durable-claim-store-qualification-v1", **binding, "status": "PASS", "checks": {"crossClientExclusiveCreate": True, "serverRestartDurability": True, "originalEvidenceRetention": True}}:
        raise ValueError("External shared-store qualification is absent or mismatched")
    return ClaimStore(directory, {**binding, "policySha256": file_digest(policy_path), "qualificationSha256": policy["qualificationSha256"]})


def stable_key(identity):
    return hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def durable_file(directory, name, data):
    # O_EXCL is the arbitration point; a partial write stays consumed forever.
    if Path(name).name != name:
        raise ValueError("Durable evidence member is not flat")
    descriptor = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        file = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=descriptor)
        with os.fdopen(file, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


class ClaimStore:
    def __init__(self, directory, binding):
        self.directory = directory
        self.binding = binding

    def consume(self, request, output):
        protected(self.directory, directory=True)
        if type(request["executionRun"]) is not int or request["executionRun"] < 1 or type(request["executionAttempt"]) is not int or request["executionAttempt"] != 1:
            raise ValueError("Original execution identity is invalid")
        execution = {"repositoryId": REPOSITORY_ID, "executionRun": request["executionRun"], "attempt": 1, "target": request["target"]}
        package = {"repositoryId": REPOSITORY_ID, "bundleRun": request["bundleRun"], "bundleAttempt": request["bundleAttempt"], "artifactId": request["bundleArtifact"]["id"], "artifactDigest": request["bundleArtifact"]["digest"], "target": request["target"], "suite": SUITE}
        execution_key, package_key = stable_key(execution), stable_key(package)
        record = {"schema": "qa3-durable-execution-claim-v1", "store": self.binding, "execution": execution, "package": package, "request": request, "originalRootResult": str(output / "root-result.json"), "status": "CONSUMED_NO_RETRY"}
        data = (json.dumps(record, sort_keys=True) + "\n").encode()
        # Both keys are durable before spawn; failure of either never releases either.
        durable_file(self.directory, "execution-" + execution_key + ".json", data)
        durable_file(self.directory, "package-" + package_key + ".json", data)
        return execution_key

    def incomplete(self, execution_key, root_result, error):
        record = {"schema": "qa3-durable-claim-incomplete-v1", "executionKey": execution_key, "store": self.binding, "subject": root_result["subject"], "status": "FAIL", "originalError": str(error), "publicationAuthorized": False, "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED"}
        name = execution_key + ".incomplete.json"
        try:
            durable_file(self.directory, name, (json.dumps(record, sort_keys=True) + "\n").encode())
        except FileExistsError:
            retained = read_json(self.directory / name)
            if retained.get("schema") != record["schema"] or retained.get("executionKey") != execution_key or retained.get("subject") != record["subject"] or retained.get("status") != "FAIL":
                raise ValueError("Original incomplete durable record is invalid")
            return retained
        return record

    def commit(self, execution_key, output, root_result, check_cancellation=lambda: None):
        try:
            return self._commit(execution_key, output, root_result, check_cancellation)
        except Exception as error:
            try:
                self.incomplete(execution_key, root_result, error)
            except Exception as retention_error:
                raise ValueError(f"Original durable commit failure: {error}; incomplete retention failure: {retention_error}") from error
            raise

    def _commit(self, execution_key, output, root_result, check_cancellation):
        protected(self.directory, directory=True)
        if any((self.directory / (execution_key + suffix)).exists() for suffix in [".incomplete.json", ".terminal.json"]):
            raise FileExistsError("Original durable execution is already sealed or incomplete")
        check_cancellation()
        evidence = self.directory / (execution_key + ".evidence")
        evidence.mkdir(mode=0o700)
        inventory = {}
        for name in sorted(EVIDENCE_NAMES):
            check_cancellation()
            path = output / name
            if not path.exists():
                if root_result["executionStatus"] == "PASS_WORKSPACE_BEHAVIOR_ONLY":
                    raise ValueError("Successful execution lacks original evidence")
                continue
            data = read_regular(path, 8 * 1024 * 1024)
            durable_file(evidence, name, data)
            inventory[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        if "root-result.json" not in inventory:
            raise ValueError("Original root result must be retained")
        if root_result.get("evidenceIncomplete"):
            raise ValueError("Original evidence collection is incomplete; available raw bytes retained")
        terminal = {"schema": "qa3-durable-claim-terminal-v1", "store": self.binding, "executionKey": execution_key, "subject": root_result["subject"], "status": root_result["executionStatus"], "evidence": inventory, "publicationAuthorized": False, "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED"}
        # The terminal seal is authority only after all bytes and directory entries sync.
        check_cancellation()
        durable_file(self.directory, execution_key + ".terminal.json", (json.dumps(terminal, sort_keys=True) + "\n").encode())
        check_cancellation()
        return terminal
