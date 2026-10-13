# SPDX-License-Identifier: MPL-2.0

import io
import json
import os
from pathlib import Path
import stat
import signal
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from prepare import file_digest, read_json, safe_path, unpack_bundle, unpack_linux, validate_provider, validate_execution
from run_linux import PROPERTIES, validate_properties, Cancellation, run
from claim_store import ClaimStore, load_claim_store, durable_file, EVIDENCE_NAMES


def host_fixture_authority(path, directory=False):
    # Container ancestors may map root to nobody. This checks only private host
    # fixtures; it cannot qualify root ownership, shared storage or an OS worker.
    if not path.is_absolute() or not path.is_relative_to(Path(tempfile.gettempdir())):
        raise ValueError("Authority stub is restricted to temporary host fixtures")
    info = path.lstat()
    wanted = stat.S_ISDIR if directory else stat.S_ISREG
    if not wanted(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o022 or any(p.is_symlink() for p in [path, *path.parents]):
        raise ValueError("Unsafe host fixture authority")


class ArchiveContracts(unittest.TestCase):
    def test_reject_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "request.json"
            path.write_text('{"run":1,"run":2}')
            with self.assertRaises(ValueError):
                read_json(path)

    def test_reject_unsafe_paths(self):
        for name in ["../escape", "/absolute", "x//y", "x/./y", "x/../y", "x\\y"]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                safe_path(name)

    def bundle(self, path, unsafe=None):
        with zipfile.ZipFile(path, "w") as archive:
            for index in range(16):
                info = zipfile.ZipInfo(f"fixture-{index}")
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                if index == 0 and unsafe:
                    unsafe(info)
                archive.writestr(info, b"host-contract-only")

    def test_bundle_identity_and_safe_flat_members(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "bundle.zip"
            self.bundle(archive)
            unpack_bundle(archive, root / "extracted", "sha256:" + file_digest(archive))
            self.assertEqual(len(list((root / "extracted").iterdir())), 16)

    def test_wrong_outer_artifact_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "bundle.zip"
            self.bundle(archive)
            with self.assertRaises(ValueError):
                unpack_bundle(archive, root / "extracted", "sha256:" + "0" * 64)

    def test_reject_outer_symlink_and_traversal(self):
        for unsafe in [lambda i: setattr(i, "external_attr", (stat.S_IFLNK | 0o777) << 16), lambda i: setattr(i, "filename", "../escape")]:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive = root / "bundle.zip"
                self.bundle(archive, unsafe)
                with self.assertRaises(ValueError):
                    unpack_bundle(archive, root / "extracted", "sha256:" + file_digest(archive))

    def tar(self, path, entries):
        with tarfile.open(path, "w:xz") as archive:
            for name, kind, data in entries:
                member = tarfile.TarInfo(name)
                member.type = kind
                member.mode = 0o755 if name.endswith("floorp") else 0o644
                if kind == tarfile.REGTYPE:
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
                else:
                    member.linkname = data
                    archive.addfile(member)

    def test_internal_symlink_and_regular_hardlink_preserve_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "native.tar.xz"
            self.tar(archive, [("floorp/floorp", tarfile.REGTYPE, b"fixture ELF"), ("floorp/floorp-bin", tarfile.LNKTYPE, "floorp/floorp"), ("floorp/lib-link", tarfile.SYMTYPE, "floorp")])
            unpack_linux(archive, root / "native")
            self.assertEqual((root / "native/floorp-bin").read_bytes(), b"fixture ELF")
            self.assertEqual((root / "native/lib-link").read_bytes(), b"fixture ELF")

    def test_reject_package_escape_link_and_duplicate(self):
        cases = [[("other/floorp", tarfile.REGTYPE, b"x")], [("floorp/escape", tarfile.SYMTYPE, "../../outside")], [("floorp/device", tarfile.CHRTYPE, "")], [("floorp/x", tarfile.REGTYPE, b"a"), ("floorp/x", tarfile.REGTYPE, b"b")], [("floorp/link", tarfile.SYMTYPE, "target"), ("floorp/link/x", tarfile.REGTYPE, b"x")]]
        for entries in cases:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive = root / "native.tar.xz"
                self.tar(archive, entries)
                with self.assertRaises(ValueError):
                    unpack_linux(archive, root / "native")


class SupervisorContracts(unittest.TestCase):
    def test_stop_failure_still_collects_original_worker_bytes_and_journal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); prepared = root / "prepared"; inputs = prepared / "input"; inputs.mkdir(parents=True)
            state = root / "owned-state"; state.mkdir()
            original = b'{"status":"FAIL","originalError":"first native case failure"}'
            (state / "worker-result.json").write_bytes(original)
            for name in ["browser.stdout", "browser.stderr"]: (state / name).write_bytes(b"original native diagnostics")
            receipt = {"subject": {"request": {"executionRun": 123}}, "apiSnapshots": {}}
            output = root / "output"; operations = []
            class HostStore:
                binding = {"storeId": "host-stop-fixture-not-native-qualified"}
                def consume(self, _request, _output): return "host-fixture-key"
                def commit(self, _key, path, result, check):
                    check()
                    self.result = result
                    self.raw = (path / "worker-result.json").read_bytes()
                    return {"status": result["executionStatus"]}
            store = HostStore()
            def operation(args, timeout=30):
                operations.append(args[0])
                if args[0] == "/usr/bin/systemd-run": raise ValueError("Original service startup failure")
                if args[0] == "/usr/bin/systemctl": raise ValueError("Original stop failure")
                if args[0] == "/usr/bin/journalctl": return "original service journal\n"
                raise AssertionError("Unexpected operation")
            actual_path = Path
            # /run/<unit> resolves to a private host fixture; no systemd job runs.
            fake_id = type("HostId", (), {"hex": "fixture"})()
            state.rename(root / "qa3-workspace-fixture"); state = root / "qa3-workspace-fixture"
            def mapped_path(value): return root if value == "/run" else actual_path(value)
            with patch("run_linux.os.geteuid", return_value=0), patch("run_linux.verify_prepared", return_value=receipt), patch("run_linux.load_claim_store", return_value=store), patch("run_linux.uuid.uuid4", return_value=fake_id), patch("run_linux.Path", side_effect=mapped_path), patch("run_linux.command", side_effect=operation):
                with self.assertRaises(ValueError): run(prepared, output, root / "policy")
            self.assertEqual(store.raw, original)
            self.assertEqual((output / "service.log").read_text(), "original service journal\n")
            self.assertFalse(store.result["scopeEmpty"])
            self.assertEqual(store.result["originalError"], "Original service startup failure")
            self.assertEqual(store.result["cleanupError"], "Original stop failure")
            self.assertTrue(state.exists())
            self.assertEqual(operations, ["/usr/bin/systemd-run", "/usr/bin/systemctl", "/usr/bin/journalctl"])

    def test_post_claim_setup_failure_is_retained_without_attempting_service(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            request = {"executionRun": 123}
            receipt = {"subject": {"request": request}, "apiSnapshots": {}}
            events = []
            class HostStore:
                binding = {"storeId": "host-fault-fixture-not-native-qualified"}
                def consume(self, _request, _output):
                    events.append("consume")
                    return "host-fixture-key"
                def commit(self, _key, output, result, check_cancellation):
                    check_cancellation()
                    events.append("retain")
                    self.retained = (output / "root-result.json").read_bytes()
                    return {"status": result["executionStatus"]}
            store = HostStore()
            previous = signal.getsignal(signal.SIGTERM)
            with patch("run_linux.os.geteuid", return_value=0), patch("run_linux.verify_prepared", return_value=receipt), patch("run_linux.load_claim_store", return_value=store), patch("run_linux.shutil.copytree", side_effect=OSError("Original host staging failure")), patch("run_linux.command") as operation:
                with self.assertRaises(ValueError):
                    run(root / "prepared", root / "output", root / "policy")
                operation.assert_not_called()
            self.assertEqual(events, ["consume", "retain"])
            result = json.loads(store.retained)
            self.assertEqual(result["originalError"], "Original host staging failure")
            self.assertEqual(result["executionStatus"], "FAIL")
            self.assertEqual(result["status"], "PENDING_DURABLE_COMMIT")
            self.assertEqual(signal.getsignal(signal.SIGTERM), previous)

    def test_same_original_execution_cannot_be_reclaimed_by_new_output(self):
        with tempfile.TemporaryDirectory() as directory, patch("claim_store.protected", host_fixture_authority):
            root = Path(directory)
            request = {"controllerHead": "a" * 40, "executionRun": 123, "executionAttempt": 1, "target": "linux-x86_64", "bundleRun": 456, "bundleAttempt": 1, "bundleArtifact": {"id": 789, "name": "floorp-release-bundle-v2", "digest": "sha256:" + "b" * 64}}
            directory = root / "claims"
            directory.mkdir(mode=0o700)
            store = ClaimStore(directory, {"storeId": "host-unit-fixture-not-native-qualified"})
            claim = store.consume(request, root / "first-evidence")
            self.assertEqual(read_json(directory / ("execution-" + claim + ".json"))["originalRootResult"], str(root / "first-evidence/root-result.json"))
            with self.assertRaises(FileExistsError):
                store.consume(request, root / "different-evidence")

    def test_real_signal_preserves_cancel_and_allows_cleanup_then_restores_handler(self):
        previous = signal.getsignal(signal.SIGTERM)
        cancellation = Cancellation()
        cancellation.install()
        try:
            with self.assertRaises(InterruptedError):
                os.kill(os.getpid(), signal.SIGTERM)
            first = cancellation.message
            cancellation.cleaning = True
            os.kill(os.getpid(), signal.SIGINT)
            self.assertEqual(cancellation.message, first)
        finally:
            cancellation.restore()
        self.assertEqual(signal.getsignal(signal.SIGTERM), previous)

    def valid(self):
        return {**PROPERTIES, "ControlGroup": "/system.slice/qa3-workspace-fixture.service", "RuntimeMaxUSec": "20min", "CPUQuotaPerSecUSec": "4s"}

    def test_accept_exact_bounded_worker(self):
        validate_properties(self.valid(), "qa3-workspace-fixture.service")

    def test_reject_weakened_effective_isolation(self):
        for field in PROPERTIES:
            record = self.valid()
            record[field] = "changed"
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate_properties(record, "qa3-workspace-fixture.service")

    def test_reject_foreign_scope_and_missing_runtime_cap(self):
        for change in [{"ControlGroup": "/system.slice/other.service"}, {"RuntimeMaxUSec": "infinity"}, {"CPUQuotaPerSecUSec": "infinity"}]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate_properties({**self.valid(), **change}, "qa3-workspace-fixture.service")


class ProviderContracts(unittest.TestCase):
    def request(self):
        return {"bundleRun": 2, "bundleAttempt": 1, "floorpHead": "a" * 40, "bundleArtifact": {"id": 3, "name": "fixture-bundle", "digest": "sha256:" + "b" * 64}}

    def snapshot(self):
        request = self.request()
        return {"run": {"id": 2, "head_sha": request["floorpHead"], "run_attempt": 1, "status": "completed", "conclusion": "success", "repository": {"full_name": "Floorp-Projects/Floorp"}}, "artifact": {**request["bundleArtifact"], "expired": False, "workflow_run": {"id": 2, "head_sha": request["floorpHead"]}}}

    def test_exact_original_provider_subject(self):
        validate_provider(self.snapshot(), self.request())

    def test_reject_retry_failed_foreign_or_expired_provider_subjects(self):
        for side, field, value in [("run", "run_attempt", 2), ("run", "conclusion", "failure"), ("run", "head_sha", "c" * 40), ("artifact", "digest", "sha256:" + "c" * 64), ("artifact", "expired", True), ("artifact", "workflow_run", {"id": 999, "head_sha": "a" * 40})]:
            snapshot = self.snapshot()
            snapshot[side][field] = value
            with self.subTest(side=side, field=field), self.assertRaises(ValueError):
                validate_provider(snapshot, self.request())

    def test_original_consumer_is_authenticated_separately_from_producer(self):
        request = {"executionRun": 42, "executionAttempt": 1, "controllerHead": "c" * 40}
        run = {"id": 42, "head_sha": "c" * 40, "run_attempt": 1, "status": "in_progress", "conclusion": None, "repository": {"id": 437505831, "full_name": "Floorp-Projects/Floorp"}}
        validate_execution({"run": run}, request)
        for change in [{"id": 43}, {"head_sha": "d" * 40}, {"run_attempt": 2}, {"status": "completed", "conclusion": "success"}, {"repository": {"id": 1, "full_name": "Floorp-Projects/Floorp"}}]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate_execution({"run": {**run, **change}}, request)


class DurableClaimContracts(unittest.TestCase):
    def setUp(self):
        authority = patch("claim_store.protected", host_fixture_authority)
        authority.start()
        self.addCleanup(authority.stop)

    def request(self):
        return {"executionRun": 123, "executionAttempt": 1, "controllerHead": "a" * 40, "bundleRun": 456, "bundleAttempt": 1, "bundleArtifact": {"id": 789, "name": "floorp-release-bundle-v2", "digest": "sha256:" + "b" * 64}, "target": "linux-x86_64"}

    def store(self, root):
        directory = root / "claims"
        directory.mkdir(mode=0o700)
        return ClaimStore(directory, {"storeId": "host-unit-fixture-not-native-qualified"})

    def test_fresh_process_cannot_reissue_after_controller_or_output_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            store = self.store(root)
            store.consume(self.request(), root / "first")
            script = "import json,sys,claim_store; from pathlib import Path; from test_prepare import host_fixture_authority; claim_store.protected=host_fixture_authority; s=claim_store.ClaimStore(Path(sys.argv[1]),{}); r=json.loads(sys.argv[2]);\ntry: s.consume(r,Path(sys.argv[3]))\nexcept FileExistsError: sys.exit(17)"
            changed = {**self.request(), "controllerHead": "c" * 40}
            result = subprocess.run([sys.executable, "-B", "-c", script, str(store.directory), json.dumps(changed), str(root / "second")], cwd=Path(__file__).parent, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 17, result.stderr)

    def test_same_package_cannot_escape_through_new_execution_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root)
            store.consume(self.request(), root / "first")
            with self.assertRaises(FileExistsError):
                store.consume({**self.request(), "executionRun": 124}, root / "second")

    def test_two_processes_have_exactly_one_atomic_winner(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root)
            script = "import json,sys,claim_store; from pathlib import Path; from test_prepare import host_fixture_authority; claim_store.protected=host_fixture_authority;\ntry: claim_store.ClaimStore(Path(sys.argv[1]),{}).consume(json.loads(sys.argv[2]),Path(sys.argv[3]))\nexcept FileExistsError: sys.exit(17)"
            processes = [subprocess.Popen([sys.executable, "-B", "-c", script, str(store.directory), json.dumps(self.request()), str(root / str(i))], cwd=Path(__file__).parent, stdout=subprocess.PIPE, stderr=subprocess.PIPE) for i in range(2)]
            for process in processes:
                process.communicate(timeout=5)
            self.assertEqual(sorted(p.returncode for p in processes), [0, 17])

    def test_sync_failure_blocks_issuance_and_keeps_partial_claim_consumed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root)
            with patch("claim_store.os.fsync", side_effect=OSError("Host fixture fsync failure")), self.assertRaises(OSError):
                store.consume(self.request(), root / "first")
            with self.assertRaises(FileExistsError):
                store.consume(self.request(), root / "second")

    def test_original_failed_bytes_survive_ephemeral_output_removal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root); output = root / "first"; output.mkdir()
            key = store.consume(self.request(), output)
            result = {"executionStatus": "FAIL", "subject": {"request": self.request()}, "originalError": "first original failure"}
            original = json.dumps(result).encode(); (output / "root-result.json").write_bytes(original)
            terminal = store.commit(key, output, result)
            (output / "root-result.json").unlink(); output.rmdir()
            self.assertEqual((store.directory / (key + ".evidence/root-result.json")).read_bytes(), original)
            self.assertEqual(terminal["status"], "FAIL")
            with self.assertRaises(FileExistsError):
                store.commit(key, root / "second", result)

    def test_cancellation_during_terminal_sync_keeps_original_incomplete_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root); output = root / "original"; output.mkdir()
            key = store.consume(self.request(), output)
            result = {"executionStatus": "PASS_WORKSPACE_BEHAVIOR_ONLY", "subject": {"request": self.request()}, "originalError": None}
            for name in EVIDENCE_NAMES - {"root-result.json"}:
                (output / name).write_bytes(b"host cancellation fixture; no product executed")
            (output / "root-result.json").write_text(json.dumps(result))
            cancellation = Cancellation(); cancellation.cleaning = True
            actual_write = durable_file
            def interrupt(directory, name, data):
                actual_write(directory, name, data)
                if name.endswith(".terminal.json"):
                    cancellation.handle(signal.SIGTERM, None)
            with patch("claim_store.durable_file", side_effect=interrupt), self.assertRaises(InterruptedError):
                store.commit(key, output, result, cancellation.check)
            marker = read_json(store.directory / (key + ".incomplete.json"))
            self.assertEqual(marker["originalError"], cancellation.message)
            self.assertEqual(marker["status"], "FAIL")
            self.assertEqual(read_json(store.directory / (key + ".terminal.json"))["status"], "PASS_WORKSPACE_BEHAVIOR_ONLY")
            store.incomplete(key, result, ValueError("Later failure"))
            self.assertEqual(read_json(store.directory / (key + ".incomplete.json")), marker)

    def test_terminal_sync_failure_cannot_return_success_and_retains_incomplete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root); output = root / "original"; output.mkdir()
            key = store.consume(self.request(), output)
            result = {"executionStatus": "FAIL", "subject": {"request": self.request()}}
            (output / "root-result.json").write_text(json.dumps(result))
            actual_sync = os.fsync
            def fail_terminal(descriptor):
                if os.readlink(f"/proc/self/fd/{descriptor}").endswith(".terminal.json"):
                    raise OSError("Original host terminal sync failure")
                actual_sync(descriptor)
            with patch("claim_store.os.fsync", side_effect=fail_terminal), self.assertRaises(OSError):
                store.commit(key, output, result)
            self.assertTrue((store.directory / (key + ".terminal.json")).exists())
            self.assertEqual(read_json(store.directory / (key + ".incomplete.json"))["originalError"], "Original host terminal sync failure")

    def test_missing_failure_evidence_is_explicitly_incomplete_and_keeps_available_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root); output = root / "original"; output.mkdir()
            key = store.consume(self.request(), output)
            result = {"executionStatus": "FAIL", "subject": {"request": self.request()}, "evidenceIncomplete": True}
            original = json.dumps(result).encode(); (output / "root-result.json").write_bytes(original)
            with self.assertRaises(ValueError): store.commit(key, output, result)
            self.assertEqual((store.directory / (key + ".evidence/root-result.json")).read_bytes(), original)
            self.assertFalse((store.directory / (key + ".terminal.json")).exists())
            self.assertEqual(read_json(store.directory / (key + ".incomplete.json"))["status"], "FAIL")

    def test_missing_or_local_store_cannot_qualify_and_no_directory_is_provisioned(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root)
            policy = {"schema": "qa3-durable-claim-store-policy-v1", "storeId": "unit-fixture", "directory": str(store.directory), "filesystemType": "ext4", "source": "/dev/fixture", "namespace": "/claims", "qualificationPath": str(root / "qualification.json"), "qualificationSha256": "0" * 64}
            path = root / "policy.json"; path.write_text(json.dumps(policy)); (root / "qualification.json").write_text("{}")
            with self.assertRaises(ValueError): load_claim_store(path)
            policy["directory"] = str(root / "not-provisioned"); path.write_text(json.dumps(policy))
            with self.assertRaises(FileNotFoundError): load_claim_store(path)
            self.assertFalse((root / "not-provisioned").exists())

    def test_policy_binding_and_external_qualification_are_both_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); store = self.store(root)
            binding = {"storeId": "unit-fixture", "directory": str(store.directory), "filesystemType": "nfs4", "source": "unit-test-server:/export", "namespace": "/claims"}
            proof = {"schema": "qa3-durable-claim-store-qualification-v1", **binding, "status": "PASS", "checks": {"crossClientExclusiveCreate": True, "serverRestartDurability": True, "originalEvidenceRetention": True}}
            proof_path = root / "qualification.json"; proof_path.write_text(json.dumps(proof))
            policy = {"schema": "qa3-durable-claim-store-policy-v1", **binding, "qualificationPath": str(proof_path), "qualificationSha256": file_digest(proof_path)}
            path = root / "policy.json"; path.write_text(json.dumps(policy))
            mount = root / "unit-mountinfo"; mount.write_text(f"1 0 0:1 / {root} rw - nfs4 unit-test-server:/export rw\n")
            self.assertEqual(load_claim_store(path, mount).binding["storeId"], "unit-fixture")
            # Same server/export and local path, but a different bind-mounted subtree.
            mount.write_text(f"1 0 0:1 /alternate {root} rw - nfs4 unit-test-server:/export rw\n")
            with self.assertRaises(ValueError): load_claim_store(path, mount)
            mount.write_text(f"1 0 0:1 / {root} rw - nfs4 unit-test-server:/export rw\n2 1 0:1 /alternate {root} rw - nfs4 unit-test-server:/export rw\n")
            with self.assertRaises(ValueError): load_claim_store(path, mount)
            mount.write_text(f"1 0 0:1 / {root} rw - nfs4 unit-test-server:/export rw\n")
            proof["checks"]["serverRestartDurability"] = False; proof_path.write_text(json.dumps(proof))
            policy["qualificationSha256"] = file_digest(proof_path); path.write_text(json.dumps(policy))
            with self.assertRaises(ValueError): load_claim_store(path, mount)


if __name__ == "__main__":
    unittest.main()
