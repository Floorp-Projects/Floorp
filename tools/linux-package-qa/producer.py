# SPDX-License-Identifier: MPL-2.0
"""Index the same-run Linux package; never issue product qualification."""
import json
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "final-workspace-qa"))
from prepare import unique_object  # noqa: E402


def identifier(value):
    if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]*", value) or int(value) > 2**53 - 1:
        raise ValueError("identity must be a canonical positive safe integer")
    return int(value)


def artifact(outputs, prefix, expected_name, digest_prefix=True):
    name = outputs[prefix + "name"]
    number = identifier(outputs[prefix + "id"])
    digest = outputs[prefix + "digest"]
    if not digest_prefix:
        digest = "sha256:" + digest
    if name != expected_name or not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        raise ValueError("exact Linux producer artifact contract differs")
    return {"id": number, "name": name, "digest": digest}


def validate_index(index):
    def closed(value, keys):
        if not isinstance(value, dict) or set(value) != set(keys):
            raise ValueError("closed Linux index schema differs")

    def identity(value, name):
        closed(value, {"id", "name", "digest"})
        if type(value["id"]) is not int or not 0 < value["id"] < 2**53 or value["name"] != name or not isinstance(value["digest"], str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", value["digest"]):
            raise ValueError("invalid Linux index artifact identity")

    closed(index, {"schema", "target", "floorp", "source", "runtime", "packageArtifact", "nativeRecordArtifact", "nativeProductQualification", "compileProfileQualification", "publicationAuthorized"})
    if index["schema"] != "qa3-linux-production-package-index-v1" or index["target"] != "linux-x86_64" or index["publicationAuthorized"] is not False or index["nativeProductQualification"] != "UNVERIFIED" or index["compileProfileQualification"] != "UNVERIFIED":
        raise ValueError("single Linux index cannot issue qualification")
    floorp = index["floorp"]
    closed(floorp, {"repository", "head_sha", "workflow_run_id", "attempt"})
    if floorp["repository"] != "Floorp-Projects/Floorp" or type(floorp["attempt"]) is not int or floorp["attempt"] != 1 or type(floorp["workflow_run_id"]) is not int or not 0 < floorp["workflow_run_id"] < 2**53 or not isinstance(floorp["head_sha"], str) or not re.fullmatch(r"[0-9a-f]{40}", floorp["head_sha"]):
        raise ValueError("invalid original Floorp index cohort")
    source = index["source"]
    closed(source, {"artifact", "receiptSha256"})
    identity(source["artifact"], f"qa3-source-required-{floorp['workflow_run_id']}-1")
    if not isinstance(source["receiptSha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", source["receiptSha256"]):
        raise ValueError("source receipt hash is invalid")
    runtime = index["runtime"]
    closed(runtime, {"repository", "head_sha", "workflow_run_id", "manifestArtifact", "nativeArtifact", "provenanceArtifact", "expectedBuildID"})
    if runtime["repository"] != "Floorp-Projects/Floorp-Runtime" or type(runtime["workflow_run_id"]) is not int or not 0 < runtime["workflow_run_id"] < 2**53 or not isinstance(runtime["head_sha"], str) or not re.fullmatch(r"[0-9a-f]{40}", runtime["head_sha"]) or not isinstance(runtime["expectedBuildID"], str) or not re.fullmatch(r"[0-9]{14}", runtime["expectedBuildID"]):
        raise ValueError("invalid Runtime index cohort")
    for value, name in (
        (runtime["manifestArtifact"], "floorp-runtime-build-manifest-v2"),
        (runtime["nativeArtifact"], "floorp-linux-x86_64-moz-artifact"),
        (runtime["provenanceArtifact"], "floorp-runtime-provenance-v2"),
        (index["packageArtifact"], "noraneko-linux-x86_64-installer"),
        (index["nativeRecordArtifact"], "floorp-native-verification-linux-x86_64-v2"),
    ):
        identity(value, name)
    all_artifacts = [source["artifact"], runtime["manifestArtifact"], runtime["nativeArtifact"], runtime["provenanceArtifact"], index["packageArtifact"], index["nativeRecordArtifact"]]
    if len({x["id"] for x in all_artifacts}) != len(all_artifacts) or len({x["digest"] for x in all_artifacts}) != len(all_artifacts):
        raise ValueError("distinct producer inputs cannot alias")
    return index


def make_index(needs, environment):
    if environment.get("GITHUB_REPOSITORY_ID") != "437505831" or environment.get("GITHUB_REPOSITORY") != "Floorp-Projects/Floorp" or environment.get("GITHUB_RUN_ATTEMPT") != "1":
        raise ValueError("Linux producer must be the original Floorp run")
    run = identifier(environment["GITHUB_RUN_ID"])
    head = environment["GITHUB_SHA"]
    if not re.fullmatch(r"[0-9a-f]{40}", head):
        raise ValueError("Linux producer source must be a full SHA")
    if set(needs) != {"source-qa", "runtime", "package-linux", "native-identity"} or any(v.get("result") != "success" for v in needs.values()):
        raise ValueError("every required producer job must succeed originally")
    source, runtime, package, native = [needs[k]["outputs"] for k in ("source-qa", "runtime", "package-linux", "native-identity")]
    if source.get("verdict") != "PASS_SOURCE_ONLY" or not re.fullmatch(r"[0-9a-f]{64}", source.get("receipt_sha256", "")):
        raise ValueError("same-run required source evidence missing")
    if runtime["runtime_repository"] != "Floorp-Projects/Floorp-Runtime" or not re.fullmatch(r"[0-9a-f]{40}", runtime["runtime_head_sha"]) or not re.fullmatch(r"[0-9]{14}", runtime["expected_build_id"]):
        raise ValueError("Runtime cohort identity is invalid")
    source_artifact = artifact({"name": f"qa3-source-required-{run}-1", "id": source["artifact_id"], "digest": source["artifact_digest"]}, "", f"qa3-source-required-{run}-1", False)
    index = {
        "schema": "qa3-linux-production-package-index-v1", "target": "linux-x86_64",
        "floorp": {"repository": "Floorp-Projects/Floorp", "head_sha": head, "workflow_run_id": run, "attempt": 1},
        "source": {"artifact": source_artifact, "receiptSha256": source["receipt_sha256"]},
        "runtime": {
            "repository": runtime["runtime_repository"], "head_sha": runtime["runtime_head_sha"],
            "workflow_run_id": identifier(runtime["runtime_workflow_run_id"]),
            "manifestArtifact": artifact(runtime, "runtime_manifest_artifact_", "floorp-runtime-build-manifest-v2"),
            "nativeArtifact": artifact(runtime, "linux_artifact_", "floorp-linux-x86_64-moz-artifact"),
            "provenanceArtifact": artifact(runtime, "provenance_artifact_", "floorp-runtime-provenance-v2"),
            "expectedBuildID": runtime["expected_build_id"],
        },
        "packageArtifact": artifact(package, "installer_artifact_", "noraneko-linux-x86_64-installer"),
        "nativeRecordArtifact": artifact(native, "record_artifact_", "floorp-native-verification-linux-x86_64-v2"),
        "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED",
        "publicationAuthorized": False,
    }
    return validate_index(index)


if __name__ == "__main__":
    result = make_index(json.loads(os.environ["QA3_PRODUCER_INPUTS"], object_pairs_hook=unique_object), os.environ)
    with Path(sys.argv[1]).open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
