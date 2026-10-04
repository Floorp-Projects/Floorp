#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Apply and verify Floorp Runtime patches inside artifact-build omnijars.

Artifact builds retain the Runtime's existing omni.ja files. Source-tree patch
checks do not change those archives, and `mach package` copies their entries
into the DMG. This tool patches exactly the archive members named by Floorp's
patch files and reverse-checks them in the installed package.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile


PATCH_PATH = re.compile(r"^diff --git a/(\S+) b/(\S+)$", re.MULTILINE)


def selected_patches(directory):
    return [patch for patch in sorted(directory.glob("*.patch"))
            if not patch.name.endswith((".windows.patch", ".linux.patch"))]


def patch_paths(patch):
    paths = []
    for before, after in PATCH_PATH.findall(patch.read_text()):
        if before != after or after.startswith("/") or ".." in Path(after).parts:
            raise ValueError(f"Unsupported patch path in {patch}: {before} -> {after}")
        paths.append(after)
    if not paths:
        raise ValueError(f"No file paths in {patch}")
    return paths


def archive_member(resources, path):
    if path.startswith("browser/"):
        return resources / "browser/omni.ja", path.removeprefix("browser/")
    return resources / "omni.ja", path


def extract(resources, paths, root):
    for path in paths:
        archive, member = archive_member(resources, path)
        with zipfile.ZipFile(archive) as source:
            data = source.read(member)
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)


def git_apply(root, patch, *options):
    return subprocess.run(["git", "apply", *options, str(patch.resolve())],
                          cwd=root, capture_output=True, text=True)


def check_patches(root, patches):
    for patch in patches:
        result = git_apply(root, patch, "--reverse", "--check")
        if result.returncode:
            raise RuntimeError(f"Patch is absent or incompatible: {patch.name}\n{result.stderr}")


def verify(resources, patches, paths):
    with tempfile.TemporaryDirectory(prefix="floorp-omnijar-check-") as directory:
        root = Path(directory)
        extract(resources, paths, root)
        check_patches(root, patches)
    print(f"Verified {len(paths)} patched Runtime resources in packaged omnijars")


def sync(resources, patches, paths):
    with tempfile.TemporaryDirectory(prefix="floorp-omnijar-patch-") as directory:
        root = Path(directory)
        extract(resources, paths, root)
        for patch in patches:
            if git_apply(root, patch, "--reverse", "--check").returncode == 0:
                continue
            checked = git_apply(root, patch, "--check")
            if checked.returncode:
                raise RuntimeError(f"Cannot apply {patch.name} to Runtime archive:\n{checked.stderr}")
            applied = git_apply(root, patch)
            if applied.returncode:
                raise RuntimeError(f"Could not apply {patch.name}:\n{applied.stderr}")
        check_patches(root, patches)
        replacements = {}
        for path in paths:
            archive, member = archive_member(resources, path)
            replacements.setdefault(archive, {})[member] = (root / path).read_bytes()

    for archive, members in replacements.items():
        with zipfile.ZipFile(archive) as source:
            names = set(source.namelist())
            missing = set(members) - names
            if missing:
                raise KeyError(f"Missing from {archive}: {sorted(missing)}")
            changed = {name for name, data in members.items()
                       if source.read(name) != data}
            if not changed:
                continue
            with tempfile.NamedTemporaryFile(prefix=archive.name + ".", suffix=".tmp",
                                             dir=archive.parent, delete=False) as temporary:
                temporary_path = Path(temporary.name)
            try:
                os.chmod(temporary_path, archive.stat().st_mode & 0o777)
                with zipfile.ZipFile(temporary_path, "w", allowZip64=True) as destination:
                    for info in source.infolist():
                        destination.writestr(
                            info, members[info.filename] if info.filename in changed
                            else source.read(info.filename))
                temporary_path.replace(archive)
            finally:
                temporary_path.unlink(missing_ok=True)
        print(f"Synchronized {len(changed)} entries in {archive}")
    verify(resources, patches, paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--sync", action="store_true")
    mode.add_argument("--verify", action="store_true")
    parser.add_argument("--resources", required=True, type=Path)
    parser.add_argument("--patch-dir", required=True, type=Path)
    args = parser.parse_args()
    patches = selected_patches(args.patch_dir)
    if not patches:
        parser.error("No macOS-compatible Runtime patches found")
    paths = sorted(set(path for patch in patches for path in patch_paths(patch)))
    if args.sync:
        sync(args.resources, patches, paths)
    else:
        verify(args.resources, patches, paths)


if __name__ == "__main__":
    main()
