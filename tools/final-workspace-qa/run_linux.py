# SPDX-License-Identifier: MPL-2.0

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
import uuid

from prepare import file_digest, read_json, read_regular, validate_provider, validate_execution
from claim_store import load_claim_store

PROPERTIES = {
    "DynamicUser": "yes",
    "PrivateNetwork": "yes",
    "PrivateTmp": "yes",
    "ProtectHome": "yes",
    "ProtectSystem": "strict",
    "ProtectControlGroups": "yes",
    "NoNewPrivileges": "yes",
    "MemoryMax": str(8 * 1024 ** 3),
    "MemorySwapMax": "0",
    "TasksMax": "512",
    "KillMode": "control-group",
}

class Cancellation:
    def __init__(self):
        self.message = None
        self.cleaning = False
        self.previous = {}

    def install(self):
        for signum in [signal.SIGTERM, signal.SIGINT]:
            self.previous[signum] = signal.signal(signum, self.handle)

    def handle(self, signum, _frame):
        self.message = self.message or f"Original supervisor cancelled by signal {signum}"
        if not self.cleaning:
            raise InterruptedError(self.message)

    def restore(self):
        for signum, previous in self.previous.items():
            signal.signal(signum, previous)

    def check(self):
        if self.message is not None:
            raise InterruptedError(self.message)


def command(args, timeout=30):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
    if len(result.stdout) + len(result.stderr) > 2 * 1024 * 1024:
        raise ValueError("Supervisor diagnostics exceeded the bound")
    if result.returncode:
        raise ValueError(f"Original supervisor operation failed: {result.stderr.decode(errors='replace')[:2048]}")
    return result.stdout.decode()


def properties(unit):
    keys = [*PROPERTIES, "ControlGroup", "ActiveState", "SubState", "ExecMainStatus", "Result", "RuntimeMaxUSec", "CPUQuotaPerSecUSec"]
    text = command(["/usr/bin/systemctl", "show", unit, "--property=" + ",".join(keys)])
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)


def validate_properties(record, unit):
    if any(record.get(key) != value for key, value in PROPERTIES.items()):
        raise ValueError("Effective ownership, memory or isolation policy differs")
    if record.get("ControlGroup") not in ("/system.slice/" + unit, ""):
        raise ValueError("Supervisor cgroup identity differs")
    if record.get("RuntimeMaxUSec") not in ("20min", "1200000000") or record.get("CPUQuotaPerSecUSec") not in ("4s", "4000000"):
        raise ValueError("Effective runtime or CPU bound differs")


def scope_empty(group):
    if not group:
        return True
    root = Path("/sys/fs/cgroup") / group.lstrip("/")
    if not root.exists():
        return True
    return not any(path.read_text().strip() for path in root.rglob("cgroup.procs"))


def input_hashes(directory):
    files = {}
    for path in sorted(directory.rglob("*")):
        if path.is_symlink():
            files[str(path.relative_to(directory))] = {"symlink": os.readlink(path)}
        elif path.is_file():
            files[str(path.relative_to(directory))] = {"sha256": file_digest(path), "bytes": path.stat().st_size, "mode": path.stat().st_mode & 0o777}
        elif path.is_dir():
            files[str(path.relative_to(directory))] = {"directory": True}
    return files


def verify_prepared(prepared):
    info = prepared.lstat()
    if info.st_uid != 0 or info.st_mode & 0o077 or prepared.is_symlink():
        raise ValueError("Preparation is not protected root-issued evidence")
    for path in prepared.rglob("*"):
        if path.lstat().st_uid != 0:
            raise ValueError("Preparation contains inputs not issued by root")
    receipt = read_json(prepared / "prepared.json")
    inputs = prepared / "input"
    if receipt.get("schema") != "qa3-final-workspace-prepared-v1" or receipt.get("status") != "PREPARED_NOT_EXECUTED" or receipt.get("publicationAuthorized") is not False:
        raise ValueError("Preparation receipt is not accepted")
    subject = read_json(inputs / "subject.json")
    snapshots = {name: file_digest(prepared / name) for name in ["original-provider-snapshot.json", "original-execution-snapshot.json"]}
    if receipt.get("apiSnapshots") != snapshots:
        raise ValueError("Original independent API snapshots changed")
    validate_provider(read_json(prepared / "original-provider-snapshot.json"), subject["request"])
    validate_execution(read_json(prepared / "original-execution-snapshot.json"), subject["request"])
    repository = Path(__file__).resolve().parents[2]
    head = command(["git", "-C", str(repository), "rev-parse", "HEAD"]).strip()
    if subject["request"]["controllerHead"] != head or command(["git", "-C", str(repository), "status", "--porcelain", "--", "tools/final-workspace-qa", "tools/src/browser_launcher.ts", "tools/src/defines.ts", "tools/src/utils.ts", "tools/src/release_provenance.ts", "deno.json", "deno.lock"]).strip():
        raise ValueError("Supervisor controller code is not the frozen request subject")
    if receipt.get("controllerConfiguration") != {name: file_digest(repository / name) for name in ["deno.json", "deno.lock"]}:
        raise ValueError("Controller dependency configuration differs from preparation")
    if input_hashes(inputs) != receipt["inputInventory"]:
        raise ValueError("Complete final member or support inventory changed")
    if subject != receipt["subject"] or read_json(inputs / "support-binding.json") != receipt["support"]:
        raise ValueError("Prepared subject or source binding changed")
    for path, digest in [("worker.js", receipt["workerSha256"]), ("deno", receipt["denoSha256"]), ("browser-payload.js", receipt["support"]["payloadSha256"]), ("browser/" + subject["binaryRelativePath"], subject["binarySha256"])]:
        if file_digest(inputs / path) != digest:
            raise ValueError("Prepared executable or test bytes changed")
    return receipt


def run(prepared, output, claim_policy):
    if os.geteuid() != 0 or not Path("/sys/fs/cgroup/cgroup.controllers").is_file():
        raise ValueError("A separately authorized root supervisor with cgroup v2 is required")
    if output.exists():
        raise ValueError("Evidence output is creation-only; reruns are forbidden")
    receipt = verify_prepared(prepared)
    claim_store = load_claim_store(claim_policy)
    output.mkdir(mode=0o700)
    unit = "qa3-workspace-" + uuid.uuid4().hex + ".service"
    state_name = unit.removesuffix(".service")
    state = Path("/run") / state_name
    result = {"schema": "qa3-final-workspace-root-v2", "subject": receipt["subject"], "unit": unit, "status": "PENDING_DURABLE_COMMIT", "executionStatus": "FAIL", "publicationAuthorized": False, "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED", "claimScope": "QUALIFIED_SHARED_DURABLE_STORE", "claimStore": claim_store.binding, "apiSnapshots": receipt["apiSnapshots"], "consumerBootSha256": hashlib.sha256(Path('/proc/sys/kernel/random/boot_id').read_bytes()).hexdigest()}
    try:
        claim = claim_store.consume(receipt["subject"]["request"], output)
    except Exception as error:
        # Do not mutate a competing winner's shared claim. Retain this caller's
        # issuance failure even when the shared backend cannot acknowledge sync.
        result["status"] = "FAIL"
        result["claimStatus"] = "NOT_ACKNOWLEDGED_NO_NATIVE_LAUNCH"
        result["originalError"] = str(error)
        with (output / "root-result.json").open("x") as stream:
            stream.write(json.dumps(result, indent=2) + "\n")
        raise
    result["originalExecutionClaim"] = claim
    original_error = None
    cleanup_error = None
    observed = None
    cancellation = Cancellation()
    staging = None
    service_attempted = False
    started = time.monotonic()
    try:
        cancellation.install()
        for name in receipt["apiSnapshots"]:
            (output / name).write_bytes(read_regular(prepared / name))
        staging = Path(tempfile.mkdtemp(prefix="qa3-workspace-input-"))
        inputs = staging / "input"
        shutil.copytree(prepared / "input", inputs, symlinks=True)
        for path in inputs.rglob("*"):
            if path.is_file() and not path.is_symlink():
                path.chmod(0o555 if path.stat().st_mode & 0o111 else 0o444)
        inputs.chmod(0o555)
        # Bind the complete member bytes, not just the selected executable.
        before = input_hashes(inputs)
        (output / "input-inventory.json").write_text(json.dumps(before, indent=2) + "\n")
        arguments = ["/usr/bin/systemd-run", "--quiet", "--no-block", "--unit=" + unit]
        for key, value in PROPERTIES.items():
            arguments.append("--property=" + key + "=" + value)
        arguments += [
            "--property=RuntimeMaxSec=1200", "--property=CPUQuota=400%",
            "--property=TimeoutStopSec=20", "--property=SendSIGKILL=yes",
            "--property=RuntimeDirectory=" + state_name, "--property=RuntimeDirectoryMode=0700",
            "--property=RuntimeDirectoryPreserve=yes", "--property=PrivateDevices=yes",
            "--property=TemporaryFileSystem=/workspace:ro", "--property=BindReadOnlyPaths=" + str(inputs) + ":/input",
            "--property=InaccessiblePaths=-/run/user -/run/secrets -/run/credentials -/var/run/docker.sock",
            "/usr/bin/env", "-i", "HOME=" + str(state), "TMPDIR=" + str(state),
            "PATH=/usr/bin:/bin", "LANG=C.UTF-8", "DENO_DIR=" + str(state / "deno-cache"),
            "/input/deno", "run", "--no-config", "--no-lock", "--cached-only",
            "--allow-run",
            "--allow-read=/input,/proc,/sys/fs/cgroup," + str(state),
            "--allow-write=" + str(state), "--allow-net=127.0.0.1", "--allow-env=HOME,TMPDIR,PATH,LANG,DENO_DIR",
            "/input/worker.js", str(state),
        ]
        service_attempted = True
        command(arguments)
        observed = properties(unit)
        validate_properties(observed, unit)
        result["effectiveProperties"] = observed
        while observed.get("ActiveState") in ("active", "activating", "deactivating"):
            if time.monotonic() - started > 1230:
                raise ValueError("Original native worker exceeded the root deadline")
            time.sleep(1)
            observed = properties(unit)
        result["finalProperties"] = observed
        if observed.get("ExecMainStatus") != "0" or observed.get("Result") != "success":
            raise ValueError("Original native service did not succeed")
        if not scope_empty(observed.get("ControlGroup")):
            raise ValueError("Final owned scope is not empty")
        worker = read_json(state / "worker-result.json")
        if worker.get("status") != "PASS_WORKSPACE_BEHAVIOR_ONLY" or worker.get("subject") != receipt["subject"] or worker.get("ownedCleanupComplete") is not True or worker.get("originalError") is not None or worker.get("cleanupError") is not None:
            raise ValueError("Original worker result or cleanup rejected")
        if input_hashes(inputs) != before:
            raise ValueError("Protected source, binary or support inventory changed")
        result["executionStatus"] = "PASS_WORKSPACE_BEHAVIOR_ONLY"
    except Exception as error:
        original_error = str(error)
    finally:
        cancellation.cleaning = True
        result["scopeEmpty"] = not service_attempted
        try:
            if service_attempted:
                command(["/usr/bin/systemctl", "stop", unit])
                final = properties(unit)
                if not scope_empty(final.get("ControlGroup")):
                    raise ValueError("Owned service scope survived stop")
                result["finalProperties"] = final
            result["scopeEmpty"] = True
        except Exception as error:
            cleanup_error = str(error)
        collection_errors = {}
        # Cleanup failure must never suppress the original worker's raw failure.
        for name in ["worker-result.json", "browser.stdout", "browser.stderr"]:
            try:
                path = state / name
                if path.exists():
                    (output / name).write_bytes(read_regular(path, 8 * 1024 * 1024))
                elif service_attempted:
                    raise ValueError("Original native evidence is absent")
            except Exception as error:
                collection_errors[name] = str(error)
        try:
            journal = command(["/usr/bin/journalctl", "--unit=" + unit, "--no-pager", "--output=cat"]) if service_attempted else "Native service was not attempted.\n"
            (output / "service.log").write_text(journal)
        except Exception as error:
            collection_errors["service.log"] = str(error)
        result["evidenceCollectionErrors"] = collection_errors
        result["evidenceIncomplete"] = bool(collection_errors)
        if collection_errors:
            cleanup_error = cleanup_error or "Original evidence collection is incomplete"
        try:
            if result["scopeEmpty"] and not collection_errors and state.exists():
                shutil.rmtree(state)
        except Exception as error:
            result["stateCleanupError"] = str(error)
            cleanup_error = cleanup_error or str(error)
        try:
            if staging is not None:
                shutil.rmtree(staging)
        except Exception as error:
            result["stagingCleanupError"] = str(error)
            cleanup_error = cleanup_error or str(error)
        original_error = original_error or cancellation.message
        result["originalError"] = original_error
        result["cancellation"] = cancellation.message
        result["cleanupError"] = cleanup_error
        result["observedWallSeconds"] = round(time.monotonic() - started, 3)
        result["invoiceConfirmed"] = False
        if original_error is not None or cleanup_error is not None:
            result["executionStatus"] = "FAIL"
        try:
            with (output / "root-result.json").open("x") as stream:
                stream.write(json.dumps(result, indent=2) + "\n")
            terminal = claim_store.commit(claim, output, result, cancellation.check)
            cancellation.check()
            with (output / "durable-result.json").open("x") as stream:
                stream.write(json.dumps(terminal, indent=2) + "\n")
            cancellation.check()
        except Exception as error:
            failure = {"schema": "qa3-final-workspace-durable-incomplete-v1", "status": "FAIL", "subject": receipt["subject"], "executionKey": claim, "originalError": result.get("originalError") or result.get("cleanupError") or str(error), "commitError": str(error), "cancellation": cancellation.message, "publicationAuthorized": False}
            try:
                failure["durableIncomplete"] = claim_store.incomplete(claim, result, error, output)
            except Exception as retention_error:
                failure["retentionError"] = str(retention_error)
            with (output / "durable-incomplete.json").open("x") as stream:
                stream.write(json.dumps(failure, indent=2) + "\n")
            raise
        finally:
            cancellation.restore()
    if terminal["status"] != "PASS_WORKSPACE_BEHAVIOR_ONLY":
        raise ValueError("Final workspace pilot failed; original evidence retained")
    return terminal


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--claim-policy", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.prepared.resolve(), args.output.resolve(), args.claim_policy.absolute())))
