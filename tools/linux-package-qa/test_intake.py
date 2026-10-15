# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
import os
import stat
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import intake
import producer


def index_fixture():
    def artifact(number, name):
        return {"id": number, "name": name, "digest": "sha256:" + hashlib.sha256(name.encode()).hexdigest()}
    return {
        "schema": "qa3-linux-production-package-index-v1", "target": "linux-x86_64",
        "floorp": {"repository": "Floorp-Projects/Floorp", "head_sha": "a" * 40, "workflow_run_id": 123, "attempt": 1},
        "source": {"artifact": artifact(1001, "qa3-source-required-123-1"), "receiptSha256": "1" * 64},
        "runtime": {"repository": "Floorp-Projects/Floorp-Runtime", "head_sha": "b" * 40, "workflow_run_id": 122,
                    "manifestArtifact": artifact(1002, "floorp-runtime-build-manifest-v2"),
                    "nativeArtifact": artifact(1003, "floorp-linux-x86_64-moz-artifact"),
                    "provenanceArtifact": artifact(1004, "floorp-runtime-provenance-v2"), "expectedBuildID": "20261014104253"},
        "packageArtifact": artifact(1005, "noraneko-linux-x86_64-installer"),
        "nativeRecordArtifact": artifact(1006, "floorp-native-verification-linux-x86_64-v2"),
        "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED", "publicationAuthorized": False,
    }


def context_fixture():
    index = index_fixture()
    identity = {"id": 1007, "name": "qa3-linux-package-index-123-1", "digest": "sha256:" + "7" * 64}
    request = {"schema": "qa3-linux-package-intake-request-v1", "controllerHead": "a" * 40, "executionRun": 124,
               "producerRun": 123, "floorpHead": "a" * 40, "runtimeRun": 122, "runtimeHead": "b" * 40,
               "indexArtifact": identity, "target": "linux-x86_64"}
    def run(number, head, repo, repository_id, path, active=False):
        return {"id": number, "head_sha": head, "run_attempt": 1, "repository": {"id": repository_id, "full_name": repo},
                "head_repository": {"id": repository_id}, "path": path, "status": "in_progress" if active else "completed",
                "conclusion": None if active else "success", "event": "workflow_dispatch"}
    jobs = [{"id": n + 1, "name": name, "run_id": 123, "run_attempt": 1, "status": "completed", "conclusion": "success"} for n, name in enumerate(sorted(intake.REQUIRED_JOBS))]
    provider = {
        "producer": run(123, "a" * 40, "Floorp-Projects/Floorp", 437505831, ".github/workflows/qa3-linux-package-producer.yml"),
        "consumer": run(124, "a" * 40, "Floorp-Projects/Floorp", 437505831, ".github/workflows/qa3-linux-package-intake.yml", True),
        "runtime": run(122, "b" * 40, "Floorp-Projects/Floorp-Runtime", 914860957, ".github/workflows/daily-build.yml"),
        "producerJobPages": [{"total_count": len(jobs), "jobs": jobs}],
        "artifacts": {"index": {**identity, "expired": False, "workflow_run": {"id": 123, "head_sha": "a" * 40}, "size_in_bytes": 200},
                      **{name: {} for name in ["source", "provenance", "package", "nativeRecord", "manifest", "runtimeNative"]}},
        "runtimeRestSnapshot": {},
    }
    return index, request, provider


class LinuxIndexTests(unittest.TestCase):
    def test_index_is_closed_and_cannot_claim_product_qualification(self):
        producer.validate_index(index_fixture())
        for key, value in [("target", "linux-aarch64"), ("schema", "qa3-final-workspace-request-v1"), ("publicationAuthorized", True), ("nativeProductQualification", "PASS"), ("extra", True)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                producer.validate_index({**index_fixture(), key: value})
        for field, value in [("attempt", 2), ("attempt", True), ("head_sha", "a" * 7), ("repository", "other/Floorp")]:
            index = index_fixture(); index["floorp"][field] = value
            with self.assertRaises(ValueError): producer.validate_index(index)
        index = index_fixture(); index["packageArtifact"]["name"] += "-unsigned"
        with self.assertRaises(ValueError): producer.validate_index(index)
        index = index_fixture(); index["nativeRecordArtifact"]["id"] = index["packageArtifact"]["id"]
        with self.assertRaises(ValueError): producer.validate_index(index)

    def test_complete_original_producer_and_active_independent_consumer_are_required(self):
        _, request, provider = context_fixture()
        intake.validate_context(request, provider)
        for run_name in ("producer", "runtime", "consumer"):
            for field, value in [("run_attempt", 2), ("run_attempt", True), ("head_sha", "c" * 40), ("conclusion", "failure"), ("path", ".github/workflows/foreign.yml")]:
                changed = copy.deepcopy(provider); changed[run_name][field] = value
                with self.subTest(run=run_name, field=field), self.assertRaises(ValueError):
                    intake.validate_context(request, changed)
        with self.assertRaises(ValueError): intake.validate_context({**request, "executionRun": request["producerRun"]}, provider)
        changed = copy.deepcopy(provider); changed["artifacts"]["index"]["expired"] = True
        with self.assertRaises(ValueError): intake.validate_context(request, changed)
        with self.assertRaises(ValueError): intake.validate_context({**request, "indexArtifact": {**request["indexArtifact"], "extra": True}}, provider)

    def test_partial_duplicate_foreign_failed_or_skipped_jobs_never_satisfy_intake(self):
        _, request, provider = context_fixture()
        for verdict in ("failure", "skipped", "cancelled", "timed_out", None):
            changed = copy.deepcopy(provider); changed["producerJobPages"][0]["jobs"][0]["conclusion"] = verdict
            with self.assertRaises(ValueError): intake.validate_context(request, changed)
        changed = copy.deepcopy(provider); changed["producerJobPages"][0]["jobs"].pop()
        with self.assertRaises(ValueError): intake.validate_context(request, changed)
        changed = copy.deepcopy(provider); changed["producerJobPages"] *= 2
        with self.assertRaises(ValueError): intake.validate_context(request, changed)
        changed = copy.deepcopy(provider); changed["producerJobPages"][0]["jobs"][0]["run_id"] = 999
        with self.assertRaises(ValueError): intake.validate_context(request, changed)


class BoundedZipTests(unittest.TestCase):
    def test_original_reads_reject_symlinks_oversize_and_fifos_without_blocking(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); path = root / "value.json"; path.write_bytes(b"{}")
            link = root / "link"; link.symlink_to(path)
            with self.assertRaises((OSError, ValueError)): intake.read_once(link)
            with self.assertRaises(ValueError): intake.read_once(path, 1)
            pipe = root / "pipe"; os.mkfifo(pipe)
            script = """import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import intake
for operation in (lambda: intake.read_once(Path(sys.argv[2])), lambda: intake.freeze_archive(Path(sys.argv[2]), Path(sys.argv[3]), 100)):
    try: operation()
    except ValueError: pass
    else: raise AssertionError('FIFO was accepted')
"""
            result = subprocess.run([sys.executable, "-I", "-B", "-c", script, str(Path(intake.__file__).parent), str(pipe), str(root / "copy")], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=3)
            self.assertEqual(result.returncode, 0, result.stderr.decode())

    def test_only_exact_regular_members_with_original_provider_digest_expand(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); archive = root / "original.zip"
            with zipfile.ZipFile(archive, "w") as stream: stream.writestr("one.json", b"{}")
            digest = "sha256:" + hashlib.sha256(archive.read_bytes()).hexdigest()
            intake.unpack_exact(archive, root / "good", digest, {"one.json"}, 100, 100)
            self.assertEqual((root / "good/one.json").read_bytes(), b"{}")
            for n, (names, member_limit, total_limit, expected) in enumerate([({"other"}, 100, 100, digest), ({"one.json"}, 1, 100, digest), ({"one.json"}, 100, 1, digest), ({"one.json"}, 100, 100, "sha256:" + "0" * 64)]):
                with self.assertRaises(ValueError): intake.unpack_exact(archive, root / f"bad{n}", expected, names, member_limit, total_limit)

    def test_original_path_swap_after_digest_cannot_replace_the_extracted_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); archive = root / "original.zip"; other = root / "other.zip"
            with zipfile.ZipFile(archive, "w") as stream: stream.writestr("one", b"original A")
            with zipfile.ZipFile(other, "w") as stream: stream.writestr("one", b"substitute B")
            original_bytes = archive.read_bytes()
            expected = "sha256:" + hashlib.sha256(original_bytes).hexdigest()
            digest = intake.file_digest
            def swap(path, *args):
                result = digest(path, *args)
                archive.write_bytes(other.read_bytes())
                return result
            with patch.object(intake, "file_digest", side_effect=swap):
                intake.unpack_exact(archive, root / "out", expected, {"one"}, 100, 100)
            self.assertEqual((root / "out/one").read_bytes(), b"original A")
            self.assertEqual((root / "out/.provider-original.zip").read_bytes(), original_bytes)
            self.assertNotEqual(archive.read_bytes(), original_bytes)

    def test_snapshot_path_swap_cannot_change_the_parsed_or_retained_original(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); _, request, provider = context_fixture()
            request_bytes = json.dumps(request).encode(); provider_bytes = json.dumps(provider).encode()
            request_path = root / "request.json"; provider_path = root / "provider.json"
            request_path.write_bytes(request_bytes); provider_path.write_bytes(provider_bytes)
            validate = intake.validate_context
            def swap(actual_request, actual_provider):
                validate(actual_request, actual_provider)
                request_path.write_text('{"substitute": true}')
                provider_path.write_text('{"substitute": true}')
            with patch.object(intake, "validate_context", side_effect=swap):
                result = intake.intake(request_path, provider_path, root / "archives", root / "deno", root / "out")
            self.assertEqual(result["status"], "BLOCK")
            self.assertEqual((root / "out/request.json").read_bytes(), request_bytes)
            self.assertEqual((root / "out/provider.json").read_bytes(), provider_bytes)
            self.assertEqual(result["snapshots"]["request"], hashlib.sha256(request_bytes).hexdigest())

    def test_duplicate_extra_traversal_and_symlink_members_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for n, names in enumerate([("one", "one"), ("one", "extra"), ("../one",), ("one",)]):
                archive = root / f"{n}.zip"
                with zipfile.ZipFile(archive, "w") as stream:
                    for name in names:
                        member = zipfile.ZipInfo(name)
                        member.external_attr = (stat.S_IFLNK | 0o777) << 16 if n == 3 else (stat.S_IFREG | 0o600) << 16
                        stream.writestr(member, b"{}")
                digest = "sha256:" + hashlib.sha256(archive.read_bytes()).hexdigest()
                with self.assertRaises(ValueError): intake.unpack_exact(archive, root / f"out{n}", digest, {"one"} if n != 2 else {"../one"}, 100, 100)
            self.assertFalse((root.parent / "one").exists())

    def test_missing_input_retains_a_block_receipt_without_browser_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = intake.intake(root / "missing.json", root / "provider.json", root / "archives", root / "deno", root / "out")
            self.assertEqual(result["status"], "BLOCK")
            self.assertFalse(result["publicationAuthorized"])
            self.assertIn("originalError", json.loads((root / "out/intake-result.json").read_text()))


if __name__ == "__main__": unittest.main()
