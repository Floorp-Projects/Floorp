#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Bind an installed browser to this checkout's freshly built native package.

The manifest/package must come from the authenticated CI artifact. This checks
their source identity and every application payload file; it is not a vendor
signature or an independent attestation that a manifest was honestly produced.
"""
import configparser
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import stat
import subprocess
import sys
import tarfile
import tempfile
import zipfile

_HOST = {"linux": "linux", "win32": "windows", "darwin": "mac"}
_OS = {"linux": "Linux", "windows": "WINNT", "mac": "Darwin"}
_selection_spec = importlib.util.spec_from_file_location(
    "candidate_package_selection", Path(__file__).with_name("candidate-package-selection.py"))
_selection = importlib.util.module_from_spec(_selection_spec)
_selection_spec.loader.exec_module(_selection)
_MAX_FILES = 100000
_MAX_BYTES = 12 * 1024 ** 3


def _digest(stream):
    return hashlib.file_digest(stream, "sha256").hexdigest()


def _sha(path):
    with path.open("rb") as stream:
        return _digest(stream)


def _name(value):
    if "\\" in value or ":" in value:
        raise ValueError("Unsafe native archive path")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise ValueError("Unsafe native archive path")
    return path.as_posix()


def _tree_inventory(directory):
    result = {}
    total = 0
    for current, dirs, files in os.walk(directory, followlinks=False):
        base = Path(current)
        for name in [*dirs, *files]:
            path = base / name
            key = path.relative_to(directory).as_posix()
            if path.is_symlink():
                # All bundled links must stay inside the authenticated app.
                if not path.resolve(strict=True).is_relative_to(directory.resolve()):
                    raise ValueError("Native application symlink escapes its bundle")
                result[key] = ("link", os.readlink(path).replace("\\", "/"))
            elif path.is_file():
                total += path.stat().st_size
                if total > _MAX_BYTES:
                    raise ValueError("Native application payload is too large")
                result[key] = ("file", _sha(path))
            elif not path.is_dir():
                raise ValueError("Unsupported native application filesystem object")
            if len(result) > _MAX_FILES:
                raise ValueError("Native application inventory is too large")
    return result


def _compare(expected, actual_root):
    actual = _tree_inventory(actual_root)
    if actual != expected:
        missing = sorted(expected.keys() - actual.keys())[:5]
        extra = sorted(actual.keys() - expected.keys())[:5]
        changed = sorted(key for key in expected.keys() & actual.keys()
                         if expected[key] != actual[key])[:5]
        raise RuntimeError(f"Installed native package differs: missing={missing}, extra={extra}, changed={changed}")
    inventory = json.dumps(sorted(expected.items()), separators=(",", ":")).encode()
    return len(actual), hashlib.sha256(inventory).hexdigest()


def _archive_inventory(package, binary):
    """Stream tar/ZIP bytes without extracting or executing package contents."""
    archive = zipfile.ZipFile(package) if package.name.endswith(".zip") else tarfile.open(package)
    with archive:
        zip_format = isinstance(archive, zipfile.ZipFile)
        members = archive.infolist() if zip_format else archive.getmembers()
        if len(members) > _MAX_FILES:
            raise ValueError("Native archive has too many entries")
        entries = {}
        seen = set()
        total = 0
        for member in members:
            name = _name(member.filename if zip_format else member.name)
            directory = member.is_dir() if zip_format else member.isdir()
            if name in seen:
                raise ValueError("Duplicate native archive entry")
            seen.add(name)
            if directory:
                continue
            size = member.file_size if zip_format else member.size
            total += size
            if total > _MAX_BYTES:
                raise ValueError("Native archive payload is too large")
            if zip_format:
                link = stat.S_ISLNK(member.external_attr >> 16)
                if link:
                    if size > 4096:
                        raise ValueError("Oversized native archive symlink")
                    value = ("link", archive.read(member).decode("utf-8"))
                else:
                    with archive.open(member) as stream:
                        value = ("file", _digest(stream))
            elif member.issym():
                value = ("link", member.linkname)
            elif member.isfile() or member.islnk():
                if member.islnk():
                    _name(member.linkname)
                with archive.extractfile(member) as stream:
                    value = ("file", _digest(stream))
            else:
                raise ValueError("Unsupported native archive object")
            entries[name] = value
        # Require the supplied executable and application.ini in one package
        # root; never bind a manifest merely to a matching filename elsewhere.
        candidates = [name for name, value in entries.items()
                      if PurePosixPath(name).name == binary.name and value[0] == "file" and
                      (PurePosixPath(name).parent / "application.ini").as_posix() in entries]
        if len(candidates) != 1:
            raise RuntimeError("Native archive must contain exactly one matching executable/application.ini pair")
        prefix = PurePosixPath(candidates[0]).parent
        expected = {PurePosixPath(name).relative_to(prefix).as_posix(): value
                    for name, value in entries.items() if PurePosixPath(name).is_relative_to(prefix)}
        return _compare(expected, binary.parent), binary.parent / "application.ini"


def _dmg_inventory(package, binary):
    apps = [path for path in binary.parents if path.suffix == ".app"]
    if len(apps) != 1 or binary.parent != apps[0] / "Contents/MacOS":
        raise RuntimeError("Native macOS executable must be inside one application bundle")
    actual = apps[0]
    with (actual / "Contents/Info.plist").open("rb") as stream:
        if plistlib.load(stream).get("CFBundleExecutable") != binary.name:
            raise RuntimeError("macOS bundle executable does not match Info.plist")
    with tempfile.TemporaryDirectory(prefix="floorp-candidate-dmg-") as temporary:
        mount = Path(temporary) / "mounted"
        mount.mkdir()
        attached = False
        try:
            subprocess.run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", str(mount), str(package)],
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
            attached = True
            bundled = list(mount.glob("*.app"))
            if len(bundled) != 1 or bundled[0].is_symlink() or not bundled[0].is_dir():
                raise RuntimeError("Native DMG must contain exactly one application")
            result = _compare(_tree_inventory(bundled[0]), actual)
        finally:
            # Even an interrupted attach may already have mounted the volume.
            detached = subprocess.run(["hdiutil", "detach", str(mount)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
            if attached and detached.returncode:
                raise RuntimeError("Could not detach the authenticated read-only candidate DMG")
    return result, actual / "Contents/Resources/application.ini"


def validate_maintained_patches(manifest, root):
    """Require all maintained common and candidate-OS source patch hashes.

    Native API jobs have the Floorp checkout and authenticated manifest, but no
    Runtime source clone. Match the same common and windows/<name> labels used
    by native production preparation directly against this checkout's bytes.
    """
    platform = manifest.get("platform")
    if platform not in _OS:
        raise RuntimeError("Candidate patch provenance requires a supported platform")
    recorded = manifest.get("patches")
    if not isinstance(recorded, dict):
        raise RuntimeError("Candidate patch inventory must be an object")
    directory = Path(root) / ".github/patches/floorp-runtime"
    groups = [("", directory / "common")]
    if platform == "windows":
        groups.append(("windows/", directory / "windows"))
    for prefix, folder in groups:
        patches = sorted(folder.glob("*.patch"))
        if not patches:
            raise RuntimeError("Maintained source patch inventory is empty: " + folder.name)
        for patch in patches:
            label = prefix + patch.name
            if recorded.get(label) != _sha(patch):
                raise RuntimeError("Candidate omitted or changed maintained source: " + label)


def authenticate_candidate(manifest_path, binary, root):
    """Authenticate exact source/package identity and all installed app files."""
    manifest_path, binary, root = Path(manifest_path), Path(binary), Path(root).resolve(strict=True)
    if manifest_path.is_symlink():
        raise ValueError("Candidate manifest must be a regular artifact file")
    manifest_path = manifest_path.resolve(strict=True)
    binary = binary.resolve(strict=True)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    lock = json.loads((root / "floorp-runtime.lock.json").read_text(encoding="utf-8"))
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    platform = manifest.get("platform")
    if (manifest.get("schemaVersion") != 1 or platform != _HOST.get(sys.platform) or
            manifest.get("sourceCommit") != lock["source"]["commit"] or
            manifest.get("floorpCommit") != commit or manifest.get("productionOverlay") is not True or
            manifest.get("nativeWebExtensionBindings") is not True):
        raise RuntimeError("Candidate identity does not match the current native production source")
    validate_maintained_patches(manifest, root)
    build_id = manifest.get("buildId")
    if not isinstance(build_id, str) or len(build_id) != 14 or not build_id.isdecimal():
        raise RuntimeError("Candidate build ID must be a 14-digit UTC value")
    lock_platform = "macos" if platform == "mac" else platform
    versions = {item["version"] for item in lock["artifacts"] if item["platform"] == lock_platform}
    if len(versions) != 1:
        raise RuntimeError("Runtime lock must specify one unambiguous native version")
    version = versions.pop()
    expected = _selection.select_native_package(manifest.get("packages", []), platform)
    name = expected["name"]
    if Path(name).name != name or "/" in name or "\\" in name or ":" in name:
        raise ValueError("Candidate package name must be a single filename")
    matches = list(manifest_path.parent.rglob(name))
    if len(matches) != 1 or matches[0].is_symlink() or not matches[0].is_file():
        raise RuntimeError("Candidate package must be one regular artifact file")
    package = matches[0]
    package_sha = _sha(package)
    if package.stat().st_size != expected.get("bytes") or package_sha != expected.get("sha256"):
        raise RuntimeError("Candidate package checksum or size differs from its manifest")
    if not binary.is_file():
        raise RuntimeError("Candidate browser executable is not a regular file")
    (count, inventory_sha), ini_path = (_dmg_inventory(package, binary) if platform == "mac"
                                      else _archive_inventory(package, binary))
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(ini_path, encoding="utf-8")
    if ini.get("App", "BuildID", fallback=None) != build_id or ini.get("App", "Version", fallback=None) != version:
        raise RuntimeError("Authenticated application.ini build ID/version differs from source identity")
    return {"manifest": manifest, "manifestPath": str(manifest_path), "packagePath": str(package),
            "packageSha256": package_sha, "binaryPath": str(binary), "binarySha256": _sha(binary),
            "verifiedFileCount": count, "applicationInventorySha256": inventory_sha,
            "expectedVersion": version, "applicationIniPath": str(ini_path)}


def verify_runtime_identity(candidate, runtime):
    """Require the running process's actual Gecko identity before any suite."""
    manifest = candidate["manifest"]
    platform = manifest["platform"]
    arch = "aarch64-" if platform == "mac" else "x86_64-"
    if (platform != _HOST.get(sys.platform) or runtime.get("os") != _OS[platform] or
            runtime.get("version") != candidate["expectedVersion"] or
            runtime.get("buildId") != manifest["buildId"] or
            not runtime.get("abi", "").startswith(arch) or
            runtime.get("workerWebIDLCompiled") is not True):
        raise RuntimeError("Running browser does not match the authenticated native candidate identity")
