#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Build the exact Runtime with maintained patches; never consume a prebuilt core."""
import argparse
from collections import deque
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
_selection_spec = importlib.util.spec_from_file_location(
    "candidate_package_selection", ROOT / "tools/chrome-extension-runtime/candidate-package-selection.py")
_selection = importlib.util.module_from_spec(_selection_spec)
_selection_spec.loader.exec_module(_selection)


def run(args, cwd, env, log=None):
    command = [str(value) for value in args]
    if log is None:
        subprocess.run(command, cwd=cwd, env=env, check=True)
        return
    log.parent.mkdir(parents=True, exist_ok=True)
    tail = deque(maxlen=300)
    with log.open("w", encoding="utf-8") as destination:
        with subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                              errors="replace", bufsize=1) as process:
            for line in process.stdout:
                destination.write(line)
                tail.append(line)
                sys.stdout.write(line)
                sys.stdout.flush()
            code = process.wait()
    if code:
        log.with_name(log.stem + "-failure-tail.log").write_text("".join(tail), encoding="utf-8")
        raise subprocess.CalledProcessError(code, command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["linux", "windows", "mac"], required=True)
    parser.add_argument("--runtime-source", type=Path, required=True)
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()
    if len(args.build_id) != 14 or not args.build_id.isdecimal():
        parser.error("--build-id must be a 14-digit UTC build ID")
    # mach's desktop completion notifier can block on hosted macOS runners.
    env = {**os.environ, "MOZ_BUILD_DATE": args.build_id, "CARGO_INCREMENTAL": "0",
           "RUSTUP_TOOLCHAIN": "1.95.0", "NODE_ENV": "production", "MOZ_NOSPAM": "1"}
    compiler_cache = shutil.which("sccache")
    if compiler_cache:
        env["RUSTC_WRAPPER"] = compiler_cache
    # Avoid bootstrap or vendored toolchain overrides selecting another Rust.
    for name in ("rustc", "cargo"):
        env[name.upper()] = subprocess.check_output(
            ["rustup", "which", "--toolchain", "1.95.0", name], text=True).strip()
    run([sys.executable, ROOT / "tools/chrome-extension-runtime/prepare-native-source.py",
         "--runtime-source", args.runtime_source.resolve(), "--production",
         "--platform", args.platform], ROOT, env)
    state = ROOT / "_dist/native-runtime"
    source = state / "source"
    obj = state / "obj"
    overlay = source / "noraneko"
    if overlay.is_symlink():
        if overlay.resolve() != ROOT:
            raise RuntimeError("Existing production overlay points at a different checkout")
    elif overlay.exists():
        raise RuntimeError("Refusing to replace an existing Runtime noraneko directory")
    else:
        overlay.symlink_to(ROOT, target_is_directory=True)
    deno = shutil.which("deno")
    if not deno:
        raise RuntimeError("Deno is required to build the actual Floorp production overlay")
    env["DENO_EXECUTABLE_PATH"] = deno
    run([deno, "task", "feles-build", "misc", "writeVersion"], ROOT, env)
    run([deno, "task", "feles-build", "build", "--phase", "before-mach"], ROOT, env)
    branding = source / "browser/branding"
    shutil.copytree(source / ".github/assets/branding", branding, dirs_exist_ok=True)
    target = {"linux": "x86_64-pc-linux-gnu", "windows": "x86_64-pc-windows-msvc",
              "mac": "aarch64-apple-darwin"}[args.platform]
    jobs = max(1, min(4, (os.cpu_count() or 2) - 1))
    physical_memory = None
    if hasattr(os, "sysconf"):
        try:
            physical_memory = os.sysconf("SC_PHYS_PAGES") * os.sysconf("SC_PAGE_SIZE")
        except (ValueError, OSError):
            pass
    logs = state / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    (logs / "host-resources.json").write_text(json.dumps({
        "host": sys.platform, "cpuCount": os.cpu_count(), "parallelJobs": jobs,
        "physicalMemoryBytes": physical_memory,
        "freeDiskBytesBeforeBootstrap": shutil.disk_usage(ROOT).free,
    }, indent=2) + "\n")
    config = [f"mk_add_options MOZ_OBJDIR={obj}", f'mk_add_options MOZ_MAKE_FLAGS="-j{jobs}"',
              "ac_add_options --enable-application=browser", "ac_add_options --enable-bootstrap",
              "ac_add_options --enable-extensions-webidl-bindings", "ac_add_options --disable-debug",
              "ac_add_options --disable-debug-symbols", "ac_add_options --enable-optimize=-O1",
              "ac_add_options --disable-crashreporter", "ac_add_options --with-app-name=floorp",
              "ac_add_options --with-app-basename=Floorp",
              "ac_add_options --with-branding=browser/branding/floorp-daylight",
              "ac_add_options --enable-update-channel=chrome-extension-candidate",
              f"ac_add_options --target={target}"]
    if compiler_cache:
        config.append(f"ac_add_options --with-ccache={compiler_cache}")
    if args.platform == "windows":
        config.extend(["ac_add_options --disable-maintenance-service", "ac_add_options --disable-desktop-launcher"])
    mozconfig = state / "mozconfig"
    mozconfig.write_text("\n".join(config) + "\n")
    env["MOZCONFIG"] = str(mozconfig)
    run([sys.executable, "./mach", "--no-interactive", "bootstrap",
         "--application-choice=browser", "--no-system-changes"], source, env, logs / "bootstrap.log")
    run([sys.executable, "./mach", "--no-interactive", "build"], source, env, logs / "build.log")
    if args.platform == "mac":
        apps = [path for path in (obj / "dist").glob("*.app") if path.is_dir()]
        if len(apps) != 1:
            raise RuntimeError("Expected exactly one compiled macOS application bundle")
        resources = apps[0] / "Contents/Resources"
    else:
        resources = obj / "dist/bin"
    # Use the same fail-closed production XHTML transformer as package.yml,
    # giving it this freshly compiled output instead of an artifact-build path.
    run([deno, "run", "--allow-read", "--allow-write", ROOT / "tools/scripts/xhtml.ts",
         resources], ROOT, env)
    preferences = resources / "browser/defaults/preferences/firefox.js"
    run(["bash", ROOT / "static/gecko/pref/override.sh", preferences], ROOT, env)
    shutil.copyfile(ROOT / "_dist/buildid2", resources / "browser/buildid2")
    run([sys.executable, "./mach", "--no-interactive", "package"], source, env, logs / "package.log")
    packages = _selection.distributable_packages(obj / "dist")
    if not packages:
        raise RuntimeError("Native build succeeded without a distributable package")
    _selection.select_native_package([{"name": path.name} for path in packages], args.platform)
    manifest = json.loads((state / "applied-source.json").read_text())
    manifest.update({"schemaVersion": 1, "platform": args.platform, "target": target,
                     "buildId": args.build_id, "productionOverlay": True,
                     "nativeWebExtensionBindings": True,
                     "rustVersion": subprocess.check_output([env["RUSTC"], "--version"], text=True).strip(),
                     "denoVersion": subprocess.check_output([deno, "--version"], text=True).splitlines()[0],
                     "floorpVersion": json.loads((ROOT / "package.json").read_text())["version"],
                     "buildId2": (ROOT / "_dist/buildid2").read_text().strip(),
                     "floorpCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                     "packages": [{"name": path.name, "bytes": path.stat().st_size,
                                   "sha256": hashlib.file_digest(path.open("rb"), "sha256").hexdigest()}
                                  for path in packages]})
    (state / "candidate-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
