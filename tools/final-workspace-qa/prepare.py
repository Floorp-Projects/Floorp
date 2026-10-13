# SPDX-License-Identifier: MPL-2.0

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import tarfile
import zipfile

GIB = 1024 ** 3
ROOT = Path(__file__).resolve().parents[2]
HELPER = "browser-features/chrome/common/workspaces/test/workspace-last-tab-test-utils.ts"
MATERIALS = [
    HELPER,
    "browser-features/chrome/common/workspaces/test/workspace-last-tab-test-types.ts",
    "browser-features/chrome/common/workspaces/test/nativeLastTabWorkspacesEnabled.test.ts",
    "browser-features/chrome/common/workspaces/utils/workspaces-static-names.ts",
    "browser-features/chrome/test/utils/test_harness.ts",
    "bridge/startup/src/chrome_root.ts",
    "tools/src/builder.ts",
]


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key")
        result[key] = value
    return result


def read_json(path):
    return json.loads(read_regular(path).decode("utf-8"), object_pairs_hook=unique_object)


def read_regular(path, limit=4 * 1024 * 1024):
    path = Path(path)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
        raise ValueError("Unsafe regular file or byte bound")
    return path.read_bytes()


def file_digest(path, algorithm="sha256"):
    result = hashlib.new(algorithm)
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def validate_provider(snapshot, request):
    if not isinstance(snapshot, dict) or set(snapshot) != {"run", "artifact"}:
        raise ValueError("Trusted original provider snapshot requires run and artifact")
    run = snapshot["run"]
    artifact = snapshot["artifact"]
    if run.get("id") != request["bundleRun"] or run.get("head_sha") != request["floorpHead"] or run.get("run_attempt") != request["bundleAttempt"] or run.get("status") != "completed" or run.get("conclusion") != "success" or run.get("repository", {}).get("full_name") != "Floorp-Projects/Floorp":
        raise ValueError("Original producer repository/source/run/attempt did not succeed")
    expected = request["bundleArtifact"]
    if any(artifact.get(key) != expected[key] for key in ["id", "name", "digest"]) or artifact.get("expired") is not False or artifact.get("workflow_run", {}).get("id") != request["bundleRun"] or artifact.get("workflow_run", {}).get("head_sha") != request["floorpHead"]:
        raise ValueError("Original final artifact provider identity differs or expired")


def safe_path(name):
    if "\\" in name or "\x00" in name:
        raise ValueError("Unsafe archive path")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in ("", ".", "..") for part in name.split("/")):
        raise ValueError("Unsafe archive path")
    return path


def unpack_bundle(archive, directory, expected_digest):
    info = archive.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_size > 2 * GIB:
        raise ValueError("Final artifact type or size rejected")
    if "sha256:" + file_digest(archive) != expected_digest:
        raise ValueError("Original artifact bytes differ from trusted API digest")
    directory.mkdir(mode=0o700)
    with archive.open("rb") as file:
        file.seek(max(0, info.st_size - 65557))
        tail = file.read()
    marker = tail.rfind(b"PK\x05\x06")
    if marker < 0 or len(tail) < marker + 22:
        raise ValueError("Final bundle ZIP directory is missing")
    footer = struct.unpack("<4s4H2IH", tail[marker:marker + 22])
    if footer[1:5] != (0, 0, 16, 16) or footer[5] > 65536 or marker + 22 + footer[7] != len(tail):
        raise ValueError("Final bundle ZIP directory is outside its bound")
    with zipfile.ZipFile(archive) as source:
        members = source.infolist()
        if len(members) != 16 or len({m.filename for m in members}) != 16:
            raise ValueError("Final bundle requires exactly 16 unique files")
        total = 0
        for member in members:
            path = safe_path(member.filename)
            mode = member.external_attr >> 16
            if len(path.parts) != 1 or member.is_dir() or stat.S_ISLNK(mode) or (stat.S_IFMT(mode) and not stat.S_ISREG(mode)) or member.flag_bits & 1:
                raise ValueError("Bundle must contain safe flat regular files")
            total += member.file_size
            if member.file_size > GIB or total > 4 * GIB:
                raise ValueError("Final bundle expansion exceeds memory budget")
        for member in members:
            target = directory / member.filename
            with source.open(member) as incoming, target.open("xb") as outgoing:
                shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
            target.chmod(0o600)


def unpack_linux(archive, directory):
    directory.mkdir(mode=0o755)
    with tarfile.open(archive, "r:xz") as source:
        members = []
        for member in source:
            members.append(member)
            if len(members) > 30000:
                raise ValueError("Linux package member count exceeds bound")
        seen = set()
        total = 0
        links = []
        for member in members:
            name = member.name.rstrip("/")
            path = safe_path(name)
            if path.parts[0] != "floorp" or name in seen or not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise ValueError("Unsafe or duplicate final Linux package member")
            seen.add(name)
            total += member.size
            if total > 2 * GIB or member.size > GIB:
                raise ValueError("Linux package expansion exceeds bound")
            if member.issym():
                target = PurePosixPath(member.linkname)
                if target.is_absolute() or "\\" in member.linkname:
                    raise ValueError("External package symlink")
                resolved = os.path.normpath(str(path.parent / target))
                if not resolved.startswith("floorp/"):
                    raise ValueError("Package symlink escapes product")
                links.append((member, resolved))
            elif member.islnk():
                link = safe_path(member.linkname)
                if link.parts[0] != "floorp":
                    raise ValueError("External package hardlink")
                links.append((member, str(link)))
        link_names = {m.name for m, _ in links}
        for member in members:
            path = safe_path(member.name.rstrip("/"))
            if any(str(parent) in link_names for parent in path.parents):
                raise ValueError("Archive writes through a symlink")
            target = directory.joinpath(*path.parts[1:])
            if not path.parts[1:]:
                if not member.isdir():
                    raise ValueError("Package root is not a directory")
                continue
            target.parent.mkdir(parents=True, exist_ok=True, mode=0o755)
            if member.isdir():
                target.mkdir(exist_ok=True, mode=0o755)
            elif member.isfile():
                with source.extractfile(member) as incoming, target.open("xb") as outgoing:
                    shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
                target.chmod(0o755 if member.mode & 0o111 else 0o644)
        for member, resolved in links:
            target = directory.joinpath(*PurePosixPath(member.name).parts[1:])
            destination = directory.joinpath(*PurePosixPath(resolved).parts[1:])
            if not destination.exists() or destination.is_symlink():
                raise ValueError("Missing or chained package symlink")
            if member.islnk():
                if not destination.is_file():
                    raise ValueError("Package hardlink is not a regular file")
                shutil.copyfile(destination, target)
                target.chmod(destination.stat().st_mode & 0o777)
            else:
                target.symlink_to(member.linkname)


def git_bytes(repository, head, name):
    return subprocess.run(["git", "-C", str(repository), "show", f"{head}:{name}"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def build_support(repository, floorp_head, deno, output):
    if output.exists():
        raise ValueError("Support output is creation-only")
    output.mkdir(mode=0o700, parents=True)
    source = output / "source"
    source.mkdir()
    materials = []
    for name in MATERIALS:
        data = git_bytes(repository, floorp_head, name)
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        materials.append({"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    for path in (repository / "tools/final-workspace-qa").glob("*.ts"):
        target = source / "tools/final-workspace-qa" / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    helper = source / HELPER
    original = helper.read_text()
    old_import = 'from "../../../test/utils/test_harness.ts";'
    old_finally = "  } finally {\n"
    if original.count(old_import) != 1 or original.count(old_finally) != 1:
        raise ValueError("Pinned fixture instrumentation shape changed")
    proxy = source / "tools/final-workspace-qa/observed_harness.ts"
    relative = os.path.relpath(proxy, helper.parent).replace(os.sep, "/")
    transformed = original.replace(old_import, f'from "{relative}";')
    transformed = transformed.replace('  type TestCase,', '  type TestCase,\n  recordCaseException,')
    if '  recordCaseException,' not in transformed:
        raise ValueError("Exception observer import shape changed")
    transformed = transformed.replace(old_finally, "  } catch (error) {\n    recordCaseException(error);\n    throw error;\n  } finally {\n")
    helper.write_text(transformed)
    payload = output / "browser-payload.js"
    command = [str(deno), "bundle", "--no-config", "--no-lock", "--platform=browser", "--format=iife", "--output", str(payload), str(source / "tools/final-workspace-qa/browser.ts")]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90, env={**os.environ, "DENO_DIR": str(output / "deno-cache")})
    (output / "bundle.stdout").write_bytes(result.stdout)
    (output / "bundle.stderr").write_bytes(result.stderr)
    if result.returncode:
        raise ValueError("Browser test support compilation failed; original diagnostics retained")
    binding = {"schema": "qa3-workspace-support-v1", "floorpHead": floorp_head, "materials": materials, "transform": {"id": "observe-original-assertions-and-precleanup-exception-v1", "path": HELPER, "beforeSha256": hashlib.sha256(original.encode()).hexdigest(), "afterSha256": file_digest(helper)}, "payloadSha256": file_digest(payload)}
    (output / "support-binding.json").write_text(json.dumps(binding, indent=2) + "\n")
    return binding


def prepare(request_path, archive, provider_path, repository, deno, output):
    if os.geteuid() != 0:
        raise ValueError("Execution preparation must be issued by the trusted root supervisor")
    if output.exists():
        raise ValueError("Product preparation is creation-only")
    request = read_json(request_path)
    provider = read_json(provider_path)
    validate_provider(provider, request)
    if request.get("target") != "linux-x86_64" or request.get("executionAttempt") != 1 or request.get("bundleAttempt") != 1:
        raise ValueError("Required original native target/attempt missing")
    head = subprocess.check_output(["git", "-C", str(repository), "rev-parse", "HEAD"], text=True).strip()
    if request.get("controllerHead") != head:
        raise ValueError("Controller revision differs from trusted request")
    dirty = subprocess.check_output(["git", "-C", str(repository), "status", "--porcelain", "--", "tools/final-workspace-qa", "tools/src/browser_launcher.ts", "tools/src/defines.ts", "tools/src/utils.ts", "tools/src/release_provenance.ts", "deno.json", "deno.lock"], text=True)
    if dirty:
        raise ValueError("Controller or inherited validation inputs are not frozen")
    output.mkdir(mode=0o700)
    (output / "original-provider-snapshot.json").write_text(json.dumps(provider, indent=2) + "\n")
    bundle = output / "bundle"
    frozen_archive = output / "original-bundle.zip"
    if not stat.S_ISREG(archive.lstat().st_mode) or archive.stat().st_size > 2 * GIB:
        raise ValueError("Original bundle archive type or bound rejected")
    with archive.open("rb") as incoming, frozen_archive.open("xb") as outgoing:
        shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
    unpack_bundle(frozen_archive, bundle, request["bundleArtifact"]["digest"])
    frozen_request = output / "request.json"
    frozen_request.write_text(json.dumps(request))
    command = [str(deno), "run", "--no-config", "--no-lock", f"--allow-read={bundle},{frozen_request}", str(repository / "tools/final-workspace-qa/validate.ts"), str(frozen_request), str(bundle)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=240)
    (output / "validation.stdout").write_bytes(result.stdout)
    (output / "validation.stderr").write_bytes(result.stderr)
    if result.returncode:
        raise ValueError("Final production bundle validation failed; original diagnostics retained")
    subject = json.loads(result.stdout, object_pairs_hook=unique_object)
    support = output / "support"
    binding = build_support(repository, request["floorpHead"], deno, support)
    inputs = output / "input"
    inputs.mkdir(mode=0o755)
    member = bundle / subject["member"]["name"]
    if member.stat().st_size != subject["member"]["size"] or file_digest(member, "sha512") != subject["member"]["sha512"]:
        raise ValueError("Final selected member bytes changed")
    unpack_linux(member, inputs / "browser")
    binary = inputs / "browser/floorp"
    with binary.open("rb") as source:
        elf = source.read(20)
    if len(elf) != 20 or elf[:4] != b"\x7fELF" or elf[4:6] != b"\x02\x01" or int.from_bytes(elf[18:20], "little") != 62 or not binary.stat().st_mode & 0o111:
        raise ValueError("Final binary is not executable native Linux x64 ELF")
    build_id2 = read_regular(inputs / "browser/browser/buildid2", 256).decode().strip()
    if build_id2 != subject["expectedBuildID2"]:
        raise ValueError("Final packaged overlay BuildID differs")
    subject["binarySha256"] = file_digest(binary)
    subject["binaryRelativePath"] = str(binary.resolve().relative_to(inputs / "browser"))
    (inputs / "subject.json").write_text(json.dumps(subject, indent=2) + "\n")
    shutil.copyfile(support / "support-binding.json", inputs / "support-binding.json")
    shutil.copyfile(support / "browser-payload.js", inputs / "browser-payload.js")
    shutil.copyfile(deno, inputs / "deno")
    (inputs / "deno").chmod(0o755)
    worker = inputs / "worker.js"
    configuration = {name: file_digest(repository / name) for name in ["deno.json", "deno.lock"]}
    command = [str(deno), "bundle", f"--config={repository / 'deno.json'}", f"--lock={repository / 'deno.lock'}", "--frozen-lockfile", "--platform=deno", "--format=esm", "--output", str(worker), str(repository / "tools/final-workspace-qa/worker.ts")]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120, env={**os.environ, "DENO_DIR": str(output / "deno-cache")})
    (output / "worker-bundle.stdout").write_bytes(result.stdout)
    (output / "worker-bundle.stderr").write_bytes(result.stderr)
    if result.returncode:
        raise ValueError("Owned worker compilation failed; diagnostics retained")
    inventory = {}
    for path in sorted(inputs.rglob("*")):
        name = str(path.relative_to(inputs))
        inventory[name] = {"symlink": os.readlink(path)} if path.is_symlink() else {"directory": True} if path.is_dir() else {"sha256": file_digest(path), "bytes": path.stat().st_size, "mode": path.stat().st_mode & 0o777}
    if configuration != {name: file_digest(repository / name) for name in configuration}:
        raise ValueError("Controller dependency configuration changed during compilation")
    receipt = {"schema": "qa3-final-workspace-prepared-v1", "subject": subject, "support": binding, "inputInventory": inventory, "controllerConfiguration": configuration, "workerSha256": file_digest(worker), "denoSha256": file_digest(inputs / "deno"), "status": "PREPARED_NOT_EXECUTED", "publicationAuthorized": False}
    (output / "prepared.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["build-support", "prepare"])
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--floorp-head")
    parser.add_argument("--request", type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--provider-snapshot", type=Path)
    parser.add_argument("--deno", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "build-support":
        if not args.floorp_head:
            parser.error("--floorp-head is required")
        result = build_support(args.repository.resolve(), args.floorp_head, args.deno.resolve(), args.output.resolve())
    else:
        if not args.request or not args.archive or not args.provider_snapshot:
            parser.error("--request, --archive and --provider-snapshot are required")
        result = prepare(args.request.resolve(), args.archive.resolve(), args.provider_snapshot.resolve(), args.repository.resolve(), args.deno.resolve(), args.output.resolve())
    print(json.dumps(result))


if __name__ == "__main__":
    main()
