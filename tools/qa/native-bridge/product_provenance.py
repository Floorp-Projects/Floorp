"""Bind the packaged Floorp product to the compiled Runtime OBJDIR it was assembled from."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import zipfile

sys.path.insert(0, str(Path(__file__).parent))
import product_cohort as cohort  # noqa: E402


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def ini(path: Path, section: str) -> dict:
    parser = configparser.RawConfigParser()
    parser.optionxform = str
    if not parser.read(path):
        raise SystemExit(f"missing {path}")
    return dict(parser[section])


def git(repo: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for flag in ("runtime", "objdir", "runtime-package", "floorp-source", "product-archive", "product-root", "output"):
        p.add_argument(f"--{flag}", type=Path, required=True)
    args = p.parse_args()
    lock = json.loads((args.floorp_source / "floorp-runtime.lock.json").read_text(encoding="utf-8"))
    plan = json.loads(cohort.PLAN.read_text(encoding="utf-8"))
    failures = []
    head, tree = git(args.runtime, "rev-parse", "HEAD"), git(args.runtime, "rev-parse", "HEAD^{tree}")
    if (head, tree) != (lock["source"]["commit"], lock["source"]["tree"]):
        failures.append("Runtime checkout is not the locked commit/tree")
    if plan["source_runtime"] != head:
        failures.append("cohort plan is not pinned to the compiled Runtime")
    runtime_platform = ini(args.objdir / "dist/bin/platform.ini", "Build")
    product_platform = ini(args.product_root / "platform.ini", "Build")
    product_app = ini(args.product_root / "application.ini", "App")
    if runtime_platform.get("BuildID") != product_platform.get("BuildID"):
        failures.append("product Gecko BuildID differs from the compiled Runtime")
    if runtime_platform.get("SourceStamp") != product_platform.get("SourceStamp"):
        failures.append("product Gecko SourceStamp differs from the compiled Runtime")
    binary = args.product_root / "floorp"
    libxul = args.product_root / "libxul.so"
    with tarfile.open(args.runtime_package) as runtime_tar:
        member = runtime_tar.extractfile("floorp/libxul.so")
        runtime_libxul = hashlib.sha256(member.read()).hexdigest() if member else None
    if sha256(libxul) != runtime_libxul:
        failures.append("product libxul.so is not the compiled Runtime package libxul.so")
    with zipfile.ZipFile(args.product_root / "browser/omni.ja") as omni:
        noraneko = sorted(n for n in omni.namelist() if n.startswith("chrome/noraneko/"))[:3]
    if not noraneko:
        failures.append("noraneko chrome is absent from the packaged product")
    cohorts = cohort.materialize(plan, args.runtime)
    for c in cohorts:
        staged = args.objdir / "_tests/testing/mochitest/browser" / c["manifest"]
        if not staged.is_file() or sha256(staged) != c["manifest_sha256"]:
            failures.append(f"staged OBJDIR manifest differs: {c['manifest']}")
        for test in c["tests"]:
            staged_test = args.objdir / "_tests/testing/mochitest/browser" / test
            if not staged_test.is_file() or sha256(staged_test) != sha256(args.runtime / test):
                failures.append(f"staged OBJDIR test differs: {test}")
    tests_packages = sorted((args.objdir / "dist").glob("*.tests.tar.gz")) + sorted(
        (args.objdir / "dist").glob("*.mochitest.tests.tar.gz"))
    record = {
        "status": "BOUND" if not failures else "REJECTED",
        "failures": failures,
        "runtime": {"commit": head, "tree": tree, "ref": lock["source"]["ref"],
                    "upstream_patches": {q.name: sha256(q) for q in sorted(
                        (args.runtime / ".github/patches/upstream").glob("*.patch"))},
                    "platform": runtime_platform,
                    "package": {"name": args.runtime_package.name, "sha256": sha256(args.runtime_package)}},
        "floorp": {"commit": git(args.floorp_source, "rev-parse", "HEAD"),
                   "tree": git(args.floorp_source, "rev-parse", "HEAD^{tree}")},
        "product": {"archive": args.product_archive.name, "sha256": sha256(args.product_archive),
                    "application": product_app, "platform": product_platform,
                    "floorp_sha256": sha256(binary), "libxul_sha256": sha256(libxul),
                    "noraneko_chrome_sample": noraneko},
        "test_support": {q.name: sha256(q) for q in dict.fromkeys(tests_packages)},
        "cohort": {"planned": sum(len(c["tests"]) for c in cohorts),
                   "manifests": {c["manifest"]: c["manifest_sha256"] for c in cohorts}},
    }
    args.output.write_text(json.dumps(record, indent=2), encoding="utf-8")
    print(json.dumps({"status": record["status"], "failures": failures}))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
