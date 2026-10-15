# SPDX-License-Identifier: MPL-2.0
"""Synthetic-provider integration test: real validators, no product execution."""
import hashlib
import io
import json
import os
import subprocess
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import intake
from test_intake import context_fixture
import test_required_source as source_fixture


def fixture(root):
    index, request, provider = context_fixture()
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=intake.ROOT, text=True).strip()
    request["floorpHead"] = request["controllerHead"] = index["floorp"]["head_sha"] = head
    for name in ("producer", "consumer"):
        provider[name]["head_sha"] = head
    artifacts = root / "archives"; artifacts.mkdir()
    def archive(name, files):
        path = artifacts / (name + ".zip")
        with zipfile.ZipFile(path, "w") as stream:
            for filename, data in files.items(): stream.writestr(filename, data)
        return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
    source = source_fixture.RequiredSourceTests().receipt()
    source["sourceHead"] = source["expectedSourceHead"] = head
    source_bytes = (json.dumps(source, indent=2) + "\n").encode()
    source_files = {name: b"synthetic contract fixture\n" for name in intake.SOURCE_FILES}
    source_files["source/source-receipt.json"] = source_bytes
    source_files["contracts.exit-code"] = b"0\n"
    index["source"]["receiptSha256"] = hashlib.sha256(source_bytes).hexdigest()
    index["source"]["artifact"]["digest"] = archive("source", source_files)
    build = index["runtime"]["expectedBuildID"]
    uuid = "019f9000-0000-7000-8000-000000000001"
    elf = b"\x7fELF\x02\x01" + b"\x00" * 12 + (62).to_bytes(2, "little")
    tar_bytes = io.BytesIO()
    with tarfile.open(fileobj=tar_bytes, mode="w:xz") as stream:
        folder = tarfile.TarInfo("floorp"); folder.type = tarfile.DIRTYPE; folder.mode = 0o755; stream.addfile(folder)
        for name, data in {"floorp": elf, "application.ini": f"[App]\nVersion=158.0.1\nBuildID={build}\n".encode(), "browser/buildid2": uuid.encode()}.items():
            member = tarfile.TarInfo("floorp/" + name); member.size = len(data); member.mode = 0o755 if name == "floorp" else 0o644
            stream.addfile(member, io.BytesIO(data))
    index["packageArtifact"]["digest"] = archive("package", {"floorp-linux-x86_64.tar.xz": tar_bytes.getvalue()})
    definitions = [("Windows", "x86_64", "floorp-windows-x86_64-moz-artifact"), ("Linux", "x86_64", "floorp-linux-x86_64-moz-artifact"), ("Linux", "aarch64", "floorp-linux-aarch64-moz-artifact"), ("Darwin", "universal", "floorp-mac-universal-moz-artifact")]
    targets = [{"platform": platform, "arch": arch, "artifact_name": name, "artifact_id": 1003 if n == 1 else 3000 + n,
                "artifact_digest": index["runtime"]["nativeArtifact"]["digest"] if n == 1 else "sha256:" + hashlib.sha256(name.encode()).hexdigest(), "expected_build_id": build}
               for n, (platform, arch, name) in enumerate(definitions)]
    manifest = {"schema_version": 2, "repository": "Floorp-Projects/Floorp-Runtime", "head_sha": "b" * 40,
                "workflow_run_id": 122, "run_created_at": "2026-10-14T10:42:53Z", "run_attempt": 1,
                "expected_build_id": build, "targets": targets}
    index["runtime"]["manifestArtifact"]["digest"] = archive("manifest", {"runtime-build-manifest-v2.json": json.dumps(manifest).encode()})
    normalized = {**manifest, "workflow_path": ".github/workflows/daily-build.yml", "manifest_artifact": index["runtime"]["manifestArtifact"]}
    index["runtime"]["provenanceArtifact"]["digest"] = archive("provenance", {"normalized-runtime-provenance-v2.json": json.dumps(normalized).encode()})
    package = index["packageArtifact"]
    record = {"schema_version": 2, "target_key": "linux", "native_arch": "x86_64", "firefox_version": "158.0.1", "app_build_id": build, "platform_build_id": build, "build_id2": uuid,
              "runtime": {"repository": manifest["repository"], "head_sha": "b" * 40, "workflow_run_id": 122,
                          "artifact_id": 1003, "artifact_digest": index["runtime"]["nativeArtifact"]["digest"], "expected_build_id": build},
              "floorp": {key: value for key, value in index["floorp"].items() if key != "attempt"},
              "floorp_package": {"artifact_id": package["id"], "artifact_name": package["name"], "artifact_digest": package["digest"], "unsigned": False},
              "verification": {"status": "verified", "method": "full-version"}}
    index["nativeRecordArtifact"]["digest"] = archive("nativeRecord", {"native-verification.json": json.dumps(record).encode()})
    request["indexArtifact"]["digest"] = archive("index", {"qa3-linux-package-index.json": json.dumps(index).encode()})
    identities = {"index": request["indexArtifact"], "source": index["source"]["artifact"], "package": package,
                  "manifest": index["runtime"]["manifestArtifact"], "runtimeNative": index["runtime"]["nativeArtifact"],
                  "provenance": index["runtime"]["provenanceArtifact"], "nativeRecord": index["nativeRecordArtifact"]}
    for name, identity in identities.items():
        runtime_input = name in ("manifest", "runtimeNative")
        provider["artifacts"][name] = {**identity, "expired": False, "workflow_run": {"id": 122 if runtime_input else 123, "head_sha": "b" * 40 if runtime_input else head},
                                        "size_in_bytes": 99 if name == "runtimeNative" else (artifacts / (name + ".zip")).stat().st_size}
    def rest_artifact(identity):
        return {**identity, "expired": False, "workflow_run_id": 122, "workflow_run_head_sha": "b" * 40}
    provider["runtimeRestSnapshot"] = {"schema_version": 2, "repository": manifest["repository"], "workflow_path": ".github/workflows/daily-build.yml",
                                      "run": {"id": 122, "status": "completed", "conclusion": "success", "head_sha": "b" * 40, "run_attempt": 1, "created_at": manifest["run_created_at"]},
                                      "manifest_artifact": rest_artifact(index["runtime"]["manifestArtifact"]),
                                      "target_artifacts": [rest_artifact({"id": t["artifact_id"], "name": t["artifact_name"], "digest": t["artifact_digest"]}) for t in targets]}
    request_path = root / "request.json"; provider_path = root / "provider.json"
    request_path.write_text(json.dumps(request)); provider_path.write_text(json.dumps(provider))
    return request_path, provider_path, artifacts, head


class SyntheticIntakeIntegrationTests(unittest.TestCase):
    def test_original_archives_pass_real_validators_but_never_qualify_a_product(self):
        deno = Path(os.environ["QA3_DENO_FOR_CONTRACTS"])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); request, provider, archives, head = fixture(root)
            def synthetic_git(args, **kwargs):
                if args == ["git", "rev-parse", "HEAD"]: return head + "\n"
                if args == ["git", "status", "--porcelain", "--untracked-files=all"]: return b""
                raise AssertionError("unexpected subprocess: " + repr(args))
            with patch.object(intake.subprocess, "check_output", side_effect=synthetic_git):
                result = intake.intake(request, provider, archives, deno, root / "out")
            self.assertEqual(result["status"], "INPUTS_MATCH_SNAPSHOTS_NOT_EXECUTED", result.get("originalError"))
            self.assertEqual(result["nativeProductQualification"], "UNVERIFIED")
            self.assertFalse(result["publicationAuthorized"])
            self.assertNotIn("manifestSetId", result)
            self.assertEqual(len(list((root / "out").glob("*/.provider-original.zip"))), 6)


if __name__ == "__main__": unittest.main()
