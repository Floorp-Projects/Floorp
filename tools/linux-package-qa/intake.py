# SPDX-License-Identifier: MPL-2.0
"""Read-only intake. It does not launch a browser or issue qualification.

The external caller must obtain snapshots through its authorized provider
connection. JSON in a candidate archive is never a provider snapshot.
"""
import argparse
import configparser
import hashlib
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

from producer import validate_index

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/final-workspace-qa"))
from prepare import file_digest, read_json, read_regular, safe_path, unique_object, unpack_linux  # noqa: E402
sys.path.insert(0, str(ROOT / "tools/qa3"))
from source import validate_required_receipt  # noqa: E402

GIB = 1024**3
SOURCE_FILES = {
    "contracts.log", "contracts.exit-code", "gate.json", "source/source-receipt.json",
    "source/deno-version.log", "source/node-version.log", "source/deno-host.log",
    "source/deno-host.xml", "source/node-intake.log", "source/python-provenance.log",
    "source/python-provenance.xml",
}
REQUIRED_JOBS = {
    "source-qa / source", "runtime / Validate immutable Floorp-Runtime provenance",
    "package-linux / main", "native-identity / Verify linux/x86_64 package", "index",
}


def read_once(path, limit=4 * 1024**2):
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK), "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= limit:
            raise ValueError("original JSON type or byte bound rejected")
        data = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
        if len(data) != info.st_size or len(data) > limit or (info.st_size, info.st_mtime_ns, info.st_ctime_ns) != (after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise ValueError("original JSON changed during bounded read")
        return data


def freeze_archive(archive, destination, maximum):
    with os.fdopen(os.open(archive, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK), "rb") as incoming:
        info = os.fstat(incoming.fileno())
        if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= maximum:
            raise ValueError("original archive type or byte bound rejected")
        count = 0
        with destination.open("xb") as outgoing:
            for chunk in iter(lambda: incoming.read(1024 * 1024), b""):
                count += len(chunk)
                if count > maximum:
                    raise ValueError("original archive grew beyond its byte bound")
                outgoing.write(chunk)
            outgoing.flush()
            os.fsync(outgoing.fileno())
        after = os.fstat(incoming.fileno())
        if count != info.st_size or (info.st_size, info.st_mtime_ns, info.st_ctime_ns) != (after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise ValueError("original archive changed during private copy")
    destination.chmod(0o400)
    return count


def unpack_exact(archive, output, expected_digest, names, member_limit, total_limit, expected_size=None):
    if ".provider-original.zip" in names:
        raise ValueError("reserved private archive name")
    output.mkdir(mode=0o700)
    frozen = output / ".provider-original.zip"
    byte_count = freeze_archive(archive, frozen, total_limit + 1024 * 1024)
    if (expected_size is not None and byte_count != expected_size) or "sha256:" + file_digest(frozen) != expected_digest:
        raise ValueError("original provider size or SHA-256 differs")
    with frozen.open("rb") as stream:
        info = os.fstat(stream.fileno())
        stream.seek(max(0, info.st_size - 65557))
        tail = stream.read()
    marker = tail.rfind(b"PK\x05\x06")
    if marker < 0 or len(tail) < marker + 22:
        raise ValueError("ZIP directory missing")
    footer = struct.unpack("<4s4H2IH", tail[marker:marker + 22])
    if footer[1:5] != (0, 0, len(names), len(names)) or footer[5] > 65536 or marker + 22 + footer[7] != len(tail):
        raise ValueError("ZIP directory is outside its closed bound")
    with zipfile.ZipFile(frozen) as source:
        members = source.infolist()
        if len(members) != len(names) or {m.filename for m in members} != names:
            raise ValueError("missing, duplicate or additional ZIP members")
        if sum(m.file_size for m in members) > total_limit:
            raise ValueError("ZIP expansion exceeds total byte bound")
        for member in members:
            path = safe_path(member.filename)
            mode = member.external_attr >> 16
            if member.is_dir() or member.flag_bits & 1 or (stat.S_IFMT(mode) and not stat.S_ISREG(mode)) or member.file_size > member_limit:
                raise ValueError("ZIP member type, encryption or byte bound rejected")
            destination = output.joinpath(*path.parts)
            destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            with source.open(member) as incoming, destination.open("xb") as outgoing:
                shutil.copyfileobj(incoming, outgoing, 1024 * 1024)


def complete_jobs(pages, expected_run):
    if not isinstance(pages, list) or not pages:
        raise ValueError("complete original job pages are required")
    total = pages[0].get("total_count")
    if type(total) is not int or not 0 < total <= 1000 or any(p.get("total_count") != total or not isinstance(p.get("jobs"), list) for p in pages):
        raise ValueError("job pagination total is invalid or changed")
    jobs = [j for page in pages for j in page["jobs"]]
    if len(jobs) != total or len({j.get("id") for j in jobs}) != total or any(type(j.get("id")) is not int or j["id"] < 1 or j.get("run_id") != expected_run or type(j.get("run_attempt")) is not int or j["run_attempt"] != 1 or j.get("status") != "completed" for j in jobs):
        raise ValueError("original job pagination is incomplete or duplicated")
    for name in REQUIRED_JOBS:
        selected = [j for j in jobs if j.get("name") == name]
        if len(selected) != 1 or selected[0].get("conclusion") != "success":
            raise ValueError("required producer job did not succeed: " + name)


def validate_run(run, number, head, repository, repository_id, path, active=False):
    if (
        type(run.get("id")) is not int or run.get("id") != number or run.get("head_sha") != head
        or type(run.get("run_attempt")) is not int or run["run_attempt"] != 1
        or run.get("repository", {}).get("id") != repository_id
        or run.get("repository", {}).get("full_name") != repository
        or run.get("head_repository", {}).get("id") != repository_id
        or run.get("path") != path
        or run.get("status") != ("in_progress" if active else "completed")
        or run.get("conclusion") != (None if active else "success")
    ):
        raise ValueError("authenticated original run identity or completion differs")


def validate_artifact(actual, expected, run, head):
    if (
        any(actual.get(key) != expected[key] for key in ("id", "name", "digest"))
        or actual.get("expired") is not False
        or actual.get("workflow_run", {}).get("id") != run
        or actual.get("workflow_run", {}).get("head_sha") != head
        or type(actual.get("size_in_bytes")) is not int or actual["size_in_bytes"] < 1
    ):
        raise ValueError("authenticated exact artifact identity differs or expired")


def validate_context(request, provider):
    keys = {"schema", "controllerHead", "executionRun", "producerRun", "floorpHead", "runtimeRun", "runtimeHead", "indexArtifact", "target"}
    if not isinstance(request, dict) or set(request) != keys or request["schema"] != "qa3-linux-package-intake-request-v1" or request["target"] != "linux-x86_64":
        raise ValueError("closed Linux intake request differs")
    for key in ("controllerHead", "floorpHead", "runtimeHead"):
        if not isinstance(request[key], str) or not re.fullmatch(r"[0-9a-f]{40}", request[key]):
            raise ValueError("intake requires full immutable source heads")
    for key in ("executionRun", "producerRun", "runtimeRun"):
        if type(request[key]) is not int or not 0 < request[key] < 2**53:
            raise ValueError("intake run ID is invalid")
    if request["controllerHead"] != request["floorpHead"] or request["executionRun"] == request["producerRun"]:
        raise ValueError("initial single-package intake requires same-source independent P and C")
    expected_index = request["indexArtifact"]
    if not isinstance(expected_index, dict) or set(expected_index) != {"id", "name", "digest"} or type(expected_index["id"]) is not int or not 0 < expected_index["id"] < 2**53 or expected_index["name"] != f"qa3-linux-package-index-{request['producerRun']}-1" or not isinstance(expected_index["digest"], str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", expected_index["digest"]):
        raise ValueError("exact producer index identity is required")
    if not isinstance(provider, dict) or set(provider) != {"producer", "consumer", "runtime", "producerJobPages", "artifacts", "runtimeRestSnapshot"}:
        raise ValueError("external authenticated provider snapshots are incomplete")
    validate_run(provider["producer"], request["producerRun"], request["floorpHead"], "Floorp-Projects/Floorp", 437505831, ".github/workflows/qa3-linux-package-producer.yml")
    validate_run(provider["consumer"], request["executionRun"], request["controllerHead"], "Floorp-Projects/Floorp", 437505831, ".github/workflows/qa3-linux-package-intake.yml", True)
    validate_run(provider["runtime"], request["runtimeRun"], request["runtimeHead"], "Floorp-Projects/Floorp-Runtime", 914860957, ".github/workflows/daily-build.yml")
    if provider["producer"].get("event") != "workflow_dispatch":
        raise ValueError("single-package producer event is outside its fixed policy")
    complete_jobs(provider["producerJobPages"], request["producerRun"])
    if set(provider["artifacts"]) != {"index", "source", "provenance", "package", "nativeRecord", "manifest", "runtimeNative"}:
        raise ValueError("authenticated artifact inventory is incomplete")
    validate_artifact(provider["artifacts"]["index"], expected_index, request["producerRun"], request["floorpHead"])


def intake(request_path, provider_path, archives, deno, output):
    if output.exists():
        raise ValueError("intake output is creation-only")
    output.mkdir(mode=0o700)
    receipt = {"schema": "qa3-linux-package-intake-v1", "status": "BLOCK", "providerAuthentication": "EXTERNAL_CALLER_REQUIRED", "nativeProductQualification": "UNVERIFIED", "compileProfileQualification": "UNVERIFIED", "publicationAuthorized": False}
    try:
        originals, snapshots = {}, {}
        for name, path in (("request", request_path), ("provider", provider_path)):
            raw = read_once(path)
            with (output / (name + ".json")).open("xb") as stream:
                stream.write(raw)
            snapshots[name] = hashlib.sha256(raw).hexdigest()
            originals[name] = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object)
        receipt["snapshots"] = snapshots
        request, provider = originals["request"], originals["provider"]
        validate_context(request, provider)
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        if head != request["controllerHead"] or subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=all"], cwd=ROOT):
            raise ValueError("controller and source checkout must be exact and clean")
        unpack_exact(archives / "index.zip", output / "index", request["indexArtifact"]["digest"], {"qa3-linux-package-index.json"}, 1024**2, 1024**2, provider["artifacts"]["index"]["size_in_bytes"])
        index = validate_index(read_json(output / "index/qa3-linux-package-index.json"))
        if index["floorp"]["head_sha"] != head or index["floorp"]["workflow_run_id"] != request["producerRun"] or index["runtime"]["head_sha"] != request["runtimeHead"] or index["runtime"]["workflow_run_id"] != request["runtimeRun"]:
            raise ValueError("index differs from the caller's authenticated expected cohort")
        identities = {"source": index["source"]["artifact"], "provenance": index["runtime"]["provenanceArtifact"], "package": index["packageArtifact"], "nativeRecord": index["nativeRecordArtifact"], "manifest": index["runtime"]["manifestArtifact"], "runtimeNative": index["runtime"]["nativeArtifact"]}
        for name, identity in identities.items():
            runtime_input = name in ("manifest", "runtimeNative")
            validate_artifact(provider["artifacts"][name], identity, request["runtimeRun"] if runtime_input else request["producerRun"], request["runtimeHead"] if runtime_input else head)
        layouts = {
            "source": (SOURCE_FILES, 16 * 1024**2, 64 * 1024**2),
            "provenance": ({"normalized-runtime-provenance-v2.json"}, 1024**2, 1024**2),
            "nativeRecord": ({"native-verification.json"}, 1024**2, 1024**2),
            "manifest": ({"runtime-build-manifest-v2.json"}, 1024**2, 1024**2),
            "package": ({"floorp-linux-x86_64.tar.xz"}, GIB, GIB),
        }
        for name, (names, member_limit, total_limit) in layouts.items():
            archive = archives / (name + ".zip")
            unpack_exact(archive, output / name, identities[name]["digest"], names, member_limit, total_limit, provider["artifacts"][name]["size_in_bytes"])
        source_receipt = output / "source/source/source-receipt.json"
        if file_digest(source_receipt) != index["source"]["receiptSha256"] or read_regular(output / "source/contracts.exit-code").strip() != b"0":
            raise ValueError("original required source receipt or contract command differs")
        validate_required_receipt(read_json(source_receipt), head, str(request["producerRun"]))
        materials = {
            "manifest": read_json(output / "manifest/runtime-build-manifest-v2.json"),
            "snapshot": provider["runtimeRestSnapshot"],
            "normalized": read_json(output / "provenance/normalized-runtime-provenance-v2.json"),
            "record": read_json(output / "nativeRecord/native-verification.json"),
            "floorp": {k: v for k, v in index["floorp"].items() if k != "attempt"},
            "package": {"artifact_id": identities["package"]["id"], "artifact_name": identities["package"]["name"], "artifact_digest": identities["package"]["digest"], "unsigned": False},
        }
        path = output / "materials.json"
        path.write_text(json.dumps(materials, indent=2) + "\n")
        checked = subprocess.run([str(deno), "run", "--no-config", "--no-lock", "--allow-read=" + str(path), str(ROOT / "tools/linux-package-qa/validate.ts"), str(path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
        (output / "validation.stdout").write_bytes(checked.stdout)
        (output / "validation.stderr").write_bytes(checked.stderr)
        if checked.returncode:
            raise ValueError("strict Runtime manifest and native record validation failed")
        validated = json.loads(checked.stdout)
        runtime, record = validated["runtime"], validated["record"]
        if runtime["workflow_run_id"] != request["runtimeRun"] or runtime["head_sha"] != request["runtimeHead"] or runtime["expected_build_id"] != index["runtime"]["expectedBuildID"] or runtime["manifest_artifact"] != identities["manifest"]:
            raise ValueError("fresh Runtime provenance differs from index")
        selected = next(t for t in runtime["targets"] if t["platform"] == "Linux" and t["arch"] == "x86_64")
        if any(selected["artifact_" + k] != identities["runtimeNative"][k] for k in ("id", "name", "digest")):
            raise ValueError("selected Runtime artifact differs from index")
        member = output / "package/floorp-linux-x86_64.tar.xz"
        member_identity = {"name": member.name, "size": member.stat().st_size, "sha512": file_digest(member, "sha512")}
        unpack_linux(member, output / "browser")
        binary = output / "browser/floorp"
        with binary.open("rb") as stream:
            elf = stream.read(20)
        if len(elf) != 20 or elf[:6] != b"\x7fELF\x02\x01" or int.from_bytes(elf[18:20], "little") != 62 or not binary.stat().st_mode & 0o111:
            raise ValueError("final member is not executable Linux x64 ELF")
        if read_regular(output / "browser/browser/buildid2", 256).decode().strip() != record["build_id2"]:
            raise ValueError("final packaged overlay build ID differs")
        app = configparser.ConfigParser(interpolation=None)
        app.read_string(read_regular(output / "browser/application.ini").decode())
        if app["App"]["BuildID"] != record["app_build_id"] or app["App"]["Version"] != record["firefox_version"]:
            raise ValueError("final application identity differs from original native record")
        receipt.update({"status": "INPUTS_MATCH_SNAPSHOTS_NOT_EXECUTED", "request": request, "index": index, "member": member_identity, "binarySha256": file_digest(binary), "record": record, "snapshots": snapshots})
    except Exception as error:
        receipt["originalError"] = str(error)
    with (output / "intake-result.json").open("x") as stream:
        stream.write(json.dumps(receipt, indent=2) + "\n")
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("request", "provider", "archives", "deno", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = intake(args.request, args.provider, args.archives, args.deno, args.output)
    print(json.dumps(result))
    sys.exit(0 if result["status"] == "INPUTS_MATCH_SNAPSHOTS_NOT_EXECUTED" else 1)
