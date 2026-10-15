# SPDX-License-Identifier: MPL-2.0
"""Capture original GitHub REST metadata. No archive or browser is downloaded.

This is a controller operation, never a browser-worker capability. A candidate
index selects metadata; it is not authenticated until the original archive is
checked by the separate intake. Successful capture does not authorize launch.
"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import subprocess
import time

from intake import unique_object, validate_artifact, validate_context
from producer import validate_index

ROOT = Path(__file__).resolve().parents[2]
TARGET_NAMES = [
    "floorp-windows-x86_64-moz-artifact", "floorp-linux-x86_64-moz-artifact",
    "floorp-linux-aarch64-moz-artifact", "floorp-mac-universal-moz-artifact",
]
ENDPOINT = re.compile(
    r"/repos/Floorp-Projects/(?:Floorp|Floorp-Runtime)/actions/"
    r"(?:runs/[1-9][0-9]*(?:/(?:jobs|artifacts)\?per_page=100&page=[1-9][0-9]*)?|artifacts/[1-9][0-9]*)"
)


def parse(data):
    if not 0 < len(data) <= 4 * 1024**2:
        raise ValueError("bounded original JSON required")
    def reject_constant(_):
        raise ValueError("non-finite JSON is not an original API value")
    return json.loads(data.decode("utf-8"), object_pairs_hook=unique_object, parse_constant=reject_constant)


class GitHubRest:
    def __init__(self, token, directory):
        if not isinstance(token, str) or not token or any(c in token for c in "\r\n"):
            raise ValueError("existing controller API credential unavailable")
        self.token = token
        self.directory = directory
        directory.mkdir(mode=0o700)
        self.deadline = time.monotonic() + 120
        self.count = 0
        self.bytes = 0
        self.responses = []
        self.retention_errors = []
        self.close_errors = []

    def retain(self, endpoint, data, complete):
        path = self.directory / f"{self.count:03}.json"
        with path.open("xb") as stream:
            stream.write(data)
        self.bytes += len(data)
        self.responses.append({"endpoint": endpoint, "file": path.name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), "jsonValidated": False, "completeBody": complete})

    def get(self, endpoint):
        if not ENDPOINT.fullmatch(endpoint):
            raise ValueError("metadata-only fixed GitHub API endpoint required")
        remaining = self.deadline - time.monotonic()
        if remaining <= 0 or self.count >= 50 or self.bytes >= 32 * 1024**2:
            raise ValueError("metadata acquisition deadline or request bound")
        self.count += 1
        connection = http.client.HTTPSConnection("api.github.com", timeout=min(20, remaining))
        chunks = []
        primary_error = None
        try:
            connection.request("GET", endpoint, headers={
                "Authorization": "Bearer " + self.token,
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "Floorp-QA3-metadata",
            })
            response = connection.getresponse()
            if response.status != 200:
                # Do not retain a redirect URL, token, arbitrary error body or retry.
                raise ValueError(f"original metadata API HTTP {response.status}; no retry")
            known_length = type(response.length) is int and response.length >= 0
            chunked = response.chunked is True
            if not known_length and not chunked:
                raise ValueError("unframed original metadata response rejected")
            received = 0
            read_limit = min(4 * 1024**2 + 1, 32 * 1024**2 - self.bytes + 1)
            while received < read_limit:
                if time.monotonic() > self.deadline:
                    raise ValueError("metadata acquisition deadline exceeded")
                chunk = response.read1(min(65536, read_limit - received))
                if not chunk:
                    break
                chunks.append(chunk)
                received += len(chunk)
            data = b"".join(chunks)
            if known_length and response.length != 0 or chunked and not response.isclosed():
                raise ValueError("incomplete original HTTP metadata body")
            if time.monotonic() > self.deadline:
                raise ValueError("metadata acquisition deadline exceeded")
        except BaseException as error:
            primary_error = error
            if chunks:
                try:
                    self.retain(endpoint, b"".join(chunks), False)
                except OSError as error:
                    self.retention_errors.append(type(error).__name__)
            raise
        finally:
            try:
                connection.close()
            except BaseException as error:
                self.close_errors.append(type(error).__name__)
                if primary_error is None:
                    # A received body remains evidence even when cleanup fails.
                    try:
                        self.retain(endpoint, data, len(data) < read_limit)
                    except OSError as retention_error:
                        self.retention_errors.append(type(retention_error).__name__)
                    raise ValueError("metadata connection cleanup failed; no retry") from None
        self.retain(endpoint, data, len(data) < read_limit)
        if self.bytes > 32 * 1024**2:
            raise ValueError("total metadata byte bound exceeded")
        value = parse(data)
        self.responses[-1]["jsonValidated"] = True
        return value


def pages(client, endpoint, key):
    result, identifiers, total = [], set(), None
    for page in range(1, 11):
        value = client.get(f"{endpoint}?per_page=100&page={page}")
        observed, items = value.get("total_count"), value.get(key)
        if type(observed) is not int or not 0 < observed <= 1000 or not isinstance(items, list) or not 0 < len(items) <= 100:
            raise ValueError("incomplete metadata page or total")
        if total is None:
            total = observed
        if observed != total:
            raise ValueError("metadata pagination changed total")
        for item in items:
            number = item.get("id")
            if type(number) is not int or number < 1 or number in identifiers:
                raise ValueError("duplicate or invalid paginated identity")
            identifiers.add(number)
        result.append(value)
        if len(identifiers) == total:
            return result
        if len(identifiers) > total or len(items) != 100:
            raise ValueError("partial original metadata pagination")
    raise ValueError("metadata pagination exceeds closed bound")


def capture(request, index, client, environment):
    validate_index(index)
    expected = {
        "GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": "Floorp-Projects/Floorp",
        "GITHUB_REPOSITORY_ID": "437505831", "GITHUB_RUN_ATTEMPT": "1",
        "GITHUB_RUN_ID": str(request.get("executionRun")), "GITHUB_SHA": request.get("controllerHead"),
    }
    if any(environment.get(k) != v for k, v in expected.items()):
        raise ValueError("capture must run in its original active controller context")
    if index["floorp"]["head_sha"] != request.get("floorpHead") or index["floorp"]["workflow_run_id"] != request.get("producerRun") or index["runtime"]["head_sha"] != request.get("runtimeHead") or index["runtime"]["workflow_run_id"] != request.get("runtimeRun"):
        raise ValueError("declared index and requested cohort differ")
    floorp, runtime = "/repos/Floorp-Projects/Floorp/actions", "/repos/Floorp-Projects/Floorp-Runtime/actions"
    # ID syntax is checked before it can select even a metadata endpoint.
    for key in ("executionRun", "producerRun", "runtimeRun"):
        if type(request.get(key)) is not int or not 0 < request[key] < 2**53:
            raise ValueError("canonical original run identity required")
    provider = {
        "producer": client.get(f"{floorp}/runs/{request['producerRun']}"),
        "consumer": client.get(f"{floorp}/runs/{request['executionRun']}"),
        "runtime": client.get(f"{runtime}/runs/{request['runtimeRun']}"),
        "producerJobPages": pages(client, f"{floorp}/runs/{request['producerRun']}/jobs", "jobs"),
    }
    inventories = {
        "floorp": pages(client, f"{floorp}/runs/{request['producerRun']}/artifacts", "artifacts"),
        "runtime": pages(client, f"{runtime}/runs/{request['runtimeRun']}/artifacts", "artifacts"),
    }
    mappings = {kind: {a["id"]: a for p in original for a in p["artifacts"]} for kind, original in inventories.items()}
    identities = {"index": request["indexArtifact"], "source": index["source"]["artifact"], "package": index["packageArtifact"], "nativeRecord": index["nativeRecordArtifact"], "provenance": index["runtime"]["provenanceArtifact"], "manifest": index["runtime"]["manifestArtifact"], "runtimeNative": index["runtime"]["nativeArtifact"]}
    artifacts = {}
    for name, identity in identities.items():
        runtime_input = name in {"manifest", "runtimeNative"}
        actual = mappings["runtime" if runtime_input else "floorp"].get(identity["id"])
        if actual is None:
            raise ValueError("declared artifact missing from complete original inventory")
        validate_artifact(actual, identity, request["runtimeRun"] if runtime_input else request["producerRun"], request["runtimeHead"] if runtime_input else request["floorpHead"])
        artifacts[name] = actual
    def rest_artifact(actual):
        return {**{k: actual[k] for k in ("id", "name", "digest", "expired")}, "workflow_run_id": actual["workflow_run"]["id"], "workflow_run_head_sha": actual["workflow_run"]["head_sha"]}
    targets = []
    for name in TARGET_NAMES:
        selected = [a for a in mappings["runtime"].values() if a.get("name") == name]
        if len(selected) != 1:
            raise ValueError("exact four Runtime target metadata records required")
        value = selected[0]
        validate_artifact(value, {k: value[k] for k in ("id", "name", "digest")}, request["runtimeRun"], request["runtimeHead"])
        if not isinstance(value["digest"], str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", value["digest"]):
            raise ValueError("original provider target digest missing")
        targets.append(rest_artifact(value))
    provider["artifacts"] = artifacts
    provider["runtimeRestSnapshot"] = {
        "schema_version": 2, "repository": "Floorp-Projects/Floorp-Runtime", "workflow_path": ".github/workflows/daily-build.yml",
        "run": {k: provider["runtime"][k] for k in ("id", "status", "conclusion", "head_sha", "run_attempt", "created_at")},
        "manifest_artifact": rest_artifact(artifacts["manifest"]), "target_artifacts": targets,
    }
    validate_context(request, provider)
    return provider, inventories


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(mode=0o700, exist_ok=False)
    receipt = {"schema": "qa3-linux-metadata-capture-v1", "status": "BLOCK", "archiveBytesAcquired": False, "nativeLaunched": False, "providerAuthentication": "UNVERIFIED", "nativeProductQualification": "UNVERIFIED", "publicationAuthorized": False}
    try:
        request = parse(os.environ.get("QA3_INTAKE_REQUEST", "").encode())
        index = parse(os.environ.get("QA3_INDEX", "").encode())
        for name, declaration in (("request", request), ("index", index)):
            (args.output / (name + "-declared.json")).write_text(json.dumps(declaration, indent=2) + "\n")
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        if head != request["controllerHead"] or subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=all"], cwd=ROOT):
            raise ValueError("metadata controller checkout is not exact and clean")
        client = GitHubRest(os.environ.get("GH_TOKEN"), args.output / "original-api")
        provider, inventories = capture(request, index, client, os.environ)
        raw = (json.dumps(provider, indent=2) + "\n").encode()
        with (args.output / "provider.json").open("xb") as stream: stream.write(raw)
        (args.output / "inventories.json").write_text(json.dumps(inventories, indent=2) + "\n")
        receipt.update({"status": "AUTHENTICATED_METADATA_CAPTURED_NOT_EXECUTED", "providerAuthentication": "GITHUB_REST_CONTROLLER", "providerSha256": hashlib.sha256(raw).hexdigest(), "request": request, "indexBytesAuthenticated": False})
    except Exception as error:
        receipt["originalError"] = type(error).__name__ + (": " + str(error) if isinstance(error, ValueError) else ": original capture failed")
    if "client" in locals():
        receipt["responses"] = client.responses
        receipt["retentionErrors"] = client.retention_errors
        receipt["closeErrors"] = client.close_errors
    (args.output / "capture-result.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({k: receipt[k] for k in ("status", "archiveBytesAcquired", "nativeLaunched", "publicationAuthorized")}))
    return 0 if receipt["status"] == "AUTHENTICATED_METADATA_CAPTURED_NOT_EXECUTED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
