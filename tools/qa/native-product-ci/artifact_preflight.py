#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Read-only transport preflight. NEVER emits a cohort or a native-pass verdict.
Approval must come from reviewed default-branch configuration, not PR contents.
No credentials, archive extraction, source execution, or network operations here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import zipfile

RUNTIME = "Floorp-Projects/Floorp-Runtime"
FLOORP = "Floorp-Projects/Floorp"
PROFILE = "runtime-native-debug-artifact-v1"
FILES = {"runtime.tar.xz", "test-support.tar.gz", "producer.json"}
MAX_BYTES = 20 * 1024**3


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def validate_metadata(pin, artifact, run, pr, floorp_sha):
    require(pin.get("schema") == "floorp-debug-artifact-pin-v1", "unapproved pin schema")
    for key in ("artifact_id", "run_id", "run_attempt", "workflow_id"):
        require(type(pin.get(key)) is int and pin[key] > 0, "invalid immutable ID: " + key)
    for key, size in (("runtime_sha", 40), ("artifact_sha256", 64)):
        require(re.fullmatch("[0-9a-f]{%d}" % size, pin.get(key, "")), "invalid immutable digest: " + key)
    require(re.fullmatch("[0-9]{14}", pin.get("build_id", "")), "invalid BuildID")
    require(pin.get("repository") == RUNTIME and pin.get("profile") == PROFILE, "foreign artifact profile")
    require(pin.get("workflow_path") in {
        ".github/workflows/daily-build.yml", ".github/workflows/wrapper-build-linux.yml"
    }, "unapproved workflow")
    expected_name = f"native-debug-linux-x86_64-v1-{pin['runtime_sha']}-{pin['build_id']}-{pin['run_id']}-{pin['run_attempt']}"
    require(pin.get("artifact_name") == expected_name, "artifact name is not fully source/run bound")
    require(artifact.get("id") == pin["artifact_id"] and artifact.get("name") == expected_name
            and artifact.get("expired") is False and artifact.get("digest") == "sha256:" + pin["artifact_sha256"], "artifact identity changed/expired")
    require(type(artifact.get("size_in_bytes")) is int and 0 < artifact["size_in_bytes"] <= MAX_BYTES, "artifact size budget")
    require(artifact.get("workflow_run", {}).get("id") == pin["run_id"]
            and artifact["workflow_run"].get("head_sha") == pin["runtime_sha"], "artifact run/source mismatch")
    require(run.get("id") == pin["run_id"] and run.get("run_attempt") == pin["run_attempt"]
            and run.get("workflow_id") == pin["workflow_id"] and run.get("path") == pin["workflow_path"]
            and run.get("head_sha") == pin["runtime_sha"] and run.get("head_branch") == "nora-0.2.0"
            and run.get("repository", {}).get("full_name") == RUNTIME
            and (run.get("event") == "workflow_dispatch" or (run.get("event") == "schedule" and pin["workflow_path"] == ".github/workflows/daily-build.yml"))
            and run.get("status") == "completed" and run.get("conclusion") == "success", "unapproved/incomplete producer run")
    head = pr.get("head", {})
    require(re.fullmatch("[0-9a-f]{40}", floorp_sha or "") and head.get("sha") == floorp_sha
            and head.get("repo", {}).get("full_name") == FLOORP and head["repo"].get("fork") is False
            and pr.get("state") == "open" and pr.get("base", {}).get("ref") == "main"
            and pr["base"].get("repo", {}).get("full_name") == FLOORP, "PR must be an exact open same-repository head")
    return {
        "schema": "qa3-hosted-context-v1", "profile": PROFILE,
        "executionRepository": RUNTIME, "controllerHead": pin["runtime_sha"],
        "runtimeRecipe": pin["runtime_sha"], "run": pin["run_id"],
        "attempt": pin["run_attempt"], "buildID": pin["build_id"], "publicationAuthorized": False,
    }


def inspect_wrapper(path, pin, context, host):
    path = Path(path)
    require(path.is_file() and not path.is_symlink() and 0 < path.stat().st_size <= MAX_BYTES, "unsafe wrapper path/size")
    require(digest(path) == pin["artifact_sha256"], "outer artifact digest mismatch")
    with zipfile.ZipFile(path) as archive:
        rows = archive.infolist()
        require(len(rows) == 3 and {r.filename for r in rows} == FILES, "unexpected/duplicate wrapper entries")
        require(sum(r.file_size for r in rows) <= MAX_BYTES, "expanded wrapper size budget")
        for row in rows:
            require(not row.is_dir() and not row.flag_bits & 1
                    and stat.S_IFMT(row.external_attr >> 16) in {0, stat.S_IFREG}, "unsupported wrapper member type")
        require(archive.getinfo("producer.json").file_size <= 16 * 1024**2, "producer receipt size budget")
        value = json.loads(archive.read("producer.json"))
        require(value.get("schema") == "qa3-hosted-debug-producer-v1"
                and value.get("status") == "PACKAGED_NOT_CONSUMED"
                and value.get("context") == context and value.get("publicationAuthorized") is False
                and value.get("consumerVerdict") == "UNVERIFIED", "foreign/incomplete/prequalified producer")
        require(value.get("productProfile") == pin.get("product_profile") and isinstance(pin.get("product_profile"), dict) and pin["product_profile"].get("schema") == "floorp-native-debug-product-profile-v1" and pin["product_profile"].get("productTestsExecuted") is False, "compiled product patch profile missing/different")
        require(value.get("host") == host and host.get("os") == "ubuntu"
                and host.get("version") == "24.04" and host.get("machine") == "x86_64"
                and host.get("pythonABI") == "cp312" and host.get("libc", [None])[0] == "glibc", "unqualified host ABI")
        for name, key in (("runtime.tar.xz", "primarySha256"), ("test-support.tar.gz", "supportSha256")):
            h = hashlib.sha256()
            with archive.open(name) as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    h.update(block)
            require(value.get(key) == h.hexdigest(), "inner archive digest mismatch")
    # Deep QA3 validation, full support coverage and PR UI assembly still required.
    return {"schema": "floorp-debug-artifact-preflight-v1", "status": "TRANSPORT_ONLY_NOT_NATIVE_EVIDENCE",
            "runtime_sha": pin["runtime_sha"], "artifact_id": pin["artifact_id"],
            "artifact_sha256": pin["artifact_sha256"], "build_id": pin["build_id"],
            "native_execution": "UNVERIFIED", "product_assembly": "UNVERIFIED"}


def main():
    p = argparse.ArgumentParser()
    for name in ("pin", "artifact", "run", "pr", "host", "wrapper", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--floorp-sha", required=True)
    a = p.parse_args()
    read = lambda path: json.loads(path.read_text())
    pin = read(a.pin)
    context = validate_metadata(pin, read(a.artifact), read(a.run), read(a.pr), a.floorp_sha)
    result = inspect_wrapper(a.wrapper, pin, context, read(a.host))
    result["floorp_sha"] = a.floorp_sha
    with a.output.open("x") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")


if __name__ == "__main__":
    main()
