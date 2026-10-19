#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Prepare an isolated, pinned native build tree from maintained source patches."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(arguments, cwd=None, capture=False):
    return subprocess.run(arguments, cwd=cwd, check=True,
                          stdout=subprocess.PIPE if capture else None,
                          text=True).stdout


def affected_paths(patches):
    paths = set()
    for patch in patches:
        for path in re.findall(r"^diff --git a/(\S+) b/\1$", patch.read_text(), re.M):
            relative = Path(path)
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"Unsafe patch path: {path}")
            paths.add(path)
    return paths


def select_patches(original, common, production, platform=None):
    selected = []
    upstream = {}
    if production:
        upstream = {patch.name: patch for patch in sorted((original / ".github/patches/upstream").glob("*.patch"))}
        selected.extend(("runtime-upstream/" + name, patch) for name, patch in upstream.items())
    for patch in sorted(common.glob("*.patch")):
        inherited = upstream.get(patch.name)
        # A maintained Floorp patch can migrate into the locked Runtime. Only
        # identical postimages may be inherited; divergent patches still compose
        # normally and expose any overlap through git apply --check.
        if inherited and inherited.read_bytes() == patch.read_bytes():
            continue
        selected.append((patch.name, patch))
    if production:
        selected.extend(("runtime-packaging/" + patch.name, patch)
                        for patch in sorted((original / ".github/patches/packaging").glob("*.patch")))
    if production and platform == "windows":
        selected.extend(("windows/" + patch.name, patch)
                        for patch in sorted((common.parent / "windows").glob("*.patch")))
    return selected


def file_identity(path):
    if path.is_symlink():
        raise RuntimeError(f"Patched source must not be a symbolic link: {path}")
    if not path.exists():
        return {"sha256": None, "executable": False}
    if not path.is_file():
        raise RuntimeError(f"Patched source is not a file: {path}")
    return {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "executable": bool(path.stat().st_mode & 0o111)}


def populate_fixture(fixture, original, revision, paths):
    run(["git", "init", "--quiet"], fixture)
    for path in sorted(paths):
        entry = run(["git", "ls-tree", revision, "--", path], original, True).strip()
        if not entry:
            continue
        mode = entry.split(None, 1)[0]
        if mode not in ("100644", "100755"):
            raise RuntimeError(f"Unsupported patched source mode {mode}: {path}")
        target = fixture / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=original))
        target.chmod(0o755 if mode == "100755" else 0o644)


def incremental_prepare(original, source, revision, snapshot, patches, previous):
    if previous.get("sourceCommit") != revision or run(["git", "rev-parse", "HEAD"], source, True).strip() != revision:
        raise RuntimeError("Incremental preparation requires the same locked source commit")
    if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=source).returncode:
        raise RuntimeError("Refusing to overwrite staged changes in the native source checkout")
    desired_paths = affected_paths(patches)
    prior_files = previous.get("files")
    with tempfile.TemporaryDirectory(prefix="desired-source-", dir=snapshot) as temporary:
        fixture = Path(temporary)
        if prior_files is None:
            prior_snapshot = Path(previous["snapshot"]).resolve(strict=True)
            if not prior_snapshot.is_relative_to(snapshot.parent.resolve()):
                raise RuntimeError("Prior patch snapshot is outside the native build state")
            prior_patches = []
            for label, digest in previous["patches"].items():
                patch = (prior_snapshot / label).resolve(strict=True)
                if not patch.is_relative_to(prior_snapshot) or hashlib.sha256(patch.read_bytes()).hexdigest() != digest:
                    raise RuntimeError(f"Prior patch snapshot changed: {label}")
                prior_patches.append(patch)
            prior_paths = affected_paths(prior_patches)
            with tempfile.TemporaryDirectory(prefix="prior-source-", dir=snapshot) as prior_temporary:
                prior_fixture = Path(prior_temporary)
                populate_fixture(prior_fixture, original, revision, prior_paths)
                for patch in prior_patches:
                    run(["git", "apply", str(patch)], prior_fixture)
                prior_files = {path: file_identity(prior_fixture / path) for path in prior_paths}
        if not isinstance(prior_files, dict):
            raise RuntimeError("Invalid prior native source file manifest")
        prior_paths = set(prior_files)
        changed = set(filter(None, run(["git", "diff", "HEAD", "--name-only", "-z"], source, True).split("\0")))
        if changed - prior_paths:
            raise RuntimeError("Unexpected tracked native source changes: " + ", ".join(sorted(changed - prior_paths)))
        for path, expected in prior_files.items():
            if Path(path).is_absolute() or ".." in Path(path).parts or file_identity(source / path) != expected:
                raise RuntimeError(f"Native source differs from its maintained postimage: {path}")
        all_paths = desired_paths | prior_paths
        populate_fixture(fixture, original, revision, all_paths)
        # Newly affected paths must still agree with their locked preimage.
        for path in desired_paths - prior_paths:
            if file_identity(source / path) != file_identity(fixture / path):
                raise RuntimeError(f"Unexpected content at newly patched native source: {path}")
        for patch in patches:
            run(["git", "apply", "--check", str(patch)], fixture)
            run(["git", "apply", str(patch)], fixture)
        identities = {path: file_identity(fixture / path) for path in all_paths}
        updates = [path for path in sorted(all_paths) if file_identity(source / path) != identities[path]]
        # Validate every path before the first mutation and retain exact prior
        # postimages in the new immutable snapshot for a reviewable rollback.
        for path in updates:
            current = source / path
            if current.exists():
                saved = snapshot / "previous-postimages" / path
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(current, saved)
        (snapshot / "incremental-changes.json").write_text(json.dumps({
            "changed": updates, "prior": {path: file_identity(source / path) for path in updates},
        }, indent=2) + "\n")
        for path in updates:
            current = source / path
            desired = fixture / path
            if desired.exists():
                current.parent.mkdir(parents=True, exist_ok=True)
                with tempfile.NamedTemporaryFile(prefix=".floorp-source-", dir=current.parent, delete=False) as stream:
                    stream.write(desired.read_bytes())
                    temporary_path = Path(stream.name)
                try:
                    temporary_path.chmod(0o755 if identities[path]["executable"] else 0o644)
                    os.replace(temporary_path, current)
                finally:
                    temporary_path.unlink(missing_ok=True)
            else:
                current.unlink()
        print(f"Incremental preparation changed {len(updates)} paths; preserved {len(all_paths) - len(updates)} mtimes", flush=True)
        return {path: identities[path] for path in desired_paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-source", type=Path,
                        default=ROOT.parent / "Floorp-Runtime")
    parser.add_argument("--production", action="store_true",
                        help="Include locked Runtime upstream and Floorp packaging integration")
    parser.add_argument("--platform", choices=["linux", "windows", "mac"],
                        help="Select platform-specific source patches for production integration")
    parser.add_argument("--check-only", action="store_true",
                        help="Validate composition in a small temporary source fixture")
    parser.add_argument("--incremental", action="store_true",
                        help="Preserve unchanged postimage mtimes; refuse unexpected source changes")
    args = parser.parse_args()
    original = args.runtime_source.resolve(strict=True)
    state = ROOT / "_dist/native-runtime"
    source = state / "source"
    revision = json.loads((ROOT / "floorp-runtime.lock.json").read_text())["source"]["commit"]
    selected = select_patches(original, ROOT / ".github/patches/floorp-runtime/common",
                              args.production, args.platform)
    if args.check_only:
        with tempfile.TemporaryDirectory(prefix="floorp-native-source-check-") as temporary:
            fixture = Path(temporary)
            run(["git", "init", "--quiet"], fixture)
            populate_fixture(fixture, original, revision, affected_paths([patch for _, patch in selected]))
            for label, patch in selected:
                run(["git", "apply", "--check", str(patch)], fixture)
                run(["git", "apply", str(patch)], fixture)
                print(f"PASS {label}", flush=True)
        return
    state.mkdir(parents=True, exist_ok=True)
    if not source.exists():
        run(["git", "clone", "--shared", "--no-hardlinks", "--no-checkout",
             str(original), str(source)])
    if source.resolve() == original or not (source / ".git").is_dir():
        raise RuntimeError("Refusing to reset a non-isolated Runtime checkout")
    origin = run(["git", "remote", "get-url", "origin"], source, True).strip()
    if Path(origin).resolve() != original:
        raise RuntimeError("The build checkout was not cloned from the selected Runtime")

    snapshot = Path(tempfile.mkdtemp(prefix="source-patches-", dir=state))
    patches = []
    created = set()
    manifest = {"sourceCommit": revision, "patches": {}}
    for label, patch in selected:
        target = snapshot / label
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(patch, target)
        patches.append(target)
        manifest["patches"][label] = hashlib.sha256(target.read_bytes()).hexdigest()
        for path in re.findall(r"^diff --git a/(\S+) b/\1$", target.read_text(), re.M):
            relative = Path(path)
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"Unsafe patch path: {path}")
            exists = subprocess.run(["git", "cat-file", "-e", f"{revision}:{path}"],
                                    cwd=original, capture_output=True).returncode == 0
            if not exists:
                created.add(relative)

    changes = run(["git", "diff", "--binary"], source, True)
    (snapshot / "previous-generated-source.patch").write_text(changes)
    previous_path = state / "applied-source.json"
    if args.incremental and previous_path.is_file():
        previous = json.loads(previous_path.read_text())
        manifest["files"] = incremental_prepare(original, source, revision, snapshot, patches, previous)
        (snapshot / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        previous_path.write_text(json.dumps({"snapshot": str(snapshot), **manifest}, indent=2) + "\n")
        return
    for path in created:
        current = source / path
        if current.is_file():
            saved = snapshot / "previous-created-files" / path
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(current, saved)
    run(["git", "reset", "--hard", revision], source)
    for path in created:
        current = source / path
        if current.is_file():
            current.unlink()
    for patch in patches:
        run(["git", "apply", "--check", str(patch)], source)
        run(["git", "apply", str(patch)], source)
        print(f"Applied {patch.name}", flush=True)
    manifest["files"] = {path: file_identity(source / path) for path in affected_paths(patches)}
    (snapshot / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (state / "applied-source.json").write_text(json.dumps({
        "snapshot": str(snapshot), **manifest,
    }, indent=2) + "\n")
    print(f"Prepared {source} at {revision}")


if __name__ == "__main__":
    main()
