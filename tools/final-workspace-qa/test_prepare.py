# SPDX-License-Identifier: MPL-2.0

import io
import json
import os
from pathlib import Path
import stat
import signal
import tarfile
import tempfile
import unittest
import zipfile

from prepare import file_digest, read_json, safe_path, unpack_bundle, unpack_linux, validate_provider
from run_linux import PROPERTIES, validate_properties, claim_execution, Cancellation


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
    def test_same_original_execution_cannot_be_reclaimed_by_new_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            request = {"controllerHead": "a" * 40, "executionRun": 123, "executionAttempt": 1, "target": "linux-x86_64"}
            claim = claim_execution(request, root / "first-evidence", root / "claims")
            self.assertEqual(read_json(claim)["originalRootResult"], str(root / "first-evidence/root-result.json"))
            with self.assertRaises(FileExistsError):
                claim_execution(request, root / "different-evidence", root / "claims")

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


if __name__ == "__main__":
    unittest.main()
