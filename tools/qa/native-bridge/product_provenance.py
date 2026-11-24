"""Bind the packaged Floorp product to the compiled Runtime OBJDIR it was assembled from.

``runtime`` runs in the build job and records the OBJDIR identity next to the
Runtime and test packages it produced. ``bind`` runs where the cohort executes
and rejects any product, package or staged test that is not that identity.
"""
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

PACKAGES = ("runtime", "common", "mochitest", "condprof")


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


def staged_hashes(root: Path, cohorts: list[dict]) -> dict:
    out = {}
    for c in cohorts:
        for path in (c["manifest"], *c["tests"]):
            staged = root / path
            out[path] = sha256(staged) if staged.is_file() else None
    return out


def runtime_identity(args) -> int:
    lock = json.loads(args.lock.read_text(encoding="utf-8"))
    plan = json.loads(cohort.PLAN.read_text(encoding="utf-8"))
    head, tree = git(args.runtime, "rev-parse", "HEAD"), git(args.runtime, "rev-parse", "HEAD^{tree}")
    failures = []
    if (head, tree) != (lock["source"]["commit"], lock["source"]["tree"]):
        failures.append("Runtime checkout is not the locked commit/tree")
    if plan["source_runtime"] != head:
        failures.append("cohort plan is not pinned to the compiled Runtime")
    cohorts = cohort.materialize(plan, args.runtime)
    staged = staged_hashes(args.objdir / "_tests/testing/mochitest/browser", cohorts)
    for path, digest in staged.items():
        if digest != sha256(args.runtime / path):
            failures.append(f"staged OBJDIR material differs from source: {path}")
    dist = args.objdir / "dist"
    found = {
        "runtime": sorted(dist.glob("floorp-*.linux-x86_64.tar.xz")),
        "common": sorted(dist.glob("floorp-*.common.tests.tar.*")),
        "mochitest": sorted(dist.glob("floorp-*.mochitest.tests.tar.*")),
        "condprof": sorted(dist.glob("floorp-*.condprof.tests.tar.*")),
    }
    args.output.mkdir(parents=True, exist_ok=False)
    packages = {}
    for kind in PACKAGES:
        if len(found[kind]) != 1:
            failures.append(f"expected exactly one {kind} package, found {len(found[kind])}")
            continue
        target = args.output / found[kind][0].name
        target.write_bytes(found[kind][0].read_bytes())
        packages[kind] = {"name": target.name, "sha256": sha256(target)}
    record = {
        "status": "COMPILED" if not failures else "REJECTED",
        "failures": failures,
        "runtime": {"repository": lock["source"]["repository"], "commit": head, "tree": tree,
                    "upstream_patches": {q.name: sha256(q) for q in sorted(
                        (args.runtime / ".github/patches/upstream").glob("*.patch"))},
                    "platform": ini(dist / "bin/platform.ini", "Build"),
                    "mozinfo_sha256": sha256(args.objdir / "mozinfo.json")},
        "packages": packages,
        "staged": staged,
    }
    (args.output / "runtime-identity.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
    print(json.dumps({"status": record["status"], "failures": failures}))
    return 0 if not failures else 1


def bind(args) -> int:
    lock = json.loads((args.floorp_source / "floorp-runtime.lock.json").read_text(encoding="utf-8"))
    plan = json.loads(cohort.PLAN.read_text(encoding="utf-8"))
    identity = json.loads((args.runtime_support / "runtime-identity.json").read_text(encoding="utf-8"))
    failures = [] if identity.get("status") == "COMPILED" else ["Runtime identity was not COMPILED"]
    head, tree = git(args.runtime, "rev-parse", "HEAD"), git(args.runtime, "rev-parse", "HEAD^{tree}")
    expected = (lock["source"]["commit"], lock["source"]["tree"])
    if (head, tree) != expected or (identity["runtime"]["commit"], identity["runtime"]["tree"]) != expected:
        failures.append("Runtime source or compiled identity is not the locked commit/tree")
    if plan["source_runtime"] != head:
        failures.append("cohort plan is not pinned to the compiled Runtime")
    packages = identity.get("packages", {})
    for kind in PACKAGES:
        item = packages.get(kind)
        path = args.runtime_support / item["name"] if item else None
        if not path or not path.is_file() or sha256(path) != item["sha256"]:
            failures.append(f"{kind} package is missing or differs from the compiled identity")
    product_platform = ini(args.product_root / "platform.ini", "Build")
    product_app = ini(args.product_root / "application.ini", "App")
    for key in ("BuildID", "SourceStamp"):
        if identity["runtime"]["platform"].get(key) != product_platform.get(key):
            failures.append(f"product Gecko {key} differs from the compiled Runtime")
    libxul = args.product_root / "libxul.so"
    runtime_libxul = None
    if "runtime" in packages:
        with tarfile.open(args.runtime_support / packages["runtime"]["name"]) as runtime_tar:
            member = runtime_tar.extractfile("floorp/libxul.so")
            runtime_libxul = hashlib.sha256(member.read()).hexdigest() if member else None
    if not libxul.is_file() or sha256(libxul) != runtime_libxul:
        failures.append("product libxul.so is not the compiled Runtime package libxul.so")
    with zipfile.ZipFile(args.product_root / "browser/omni.ja") as omni:
        noraneko = sorted(n for n in omni.namelist() if n.startswith("chrome/noraneko/"))[:3]
    if not noraneko:
        failures.append("noraneko chrome is absent from the packaged product")
    cohorts = cohort.materialize(plan, args.runtime)
    extracted = staged_hashes(args.support / "mochitest/browser", cohorts)
    for path, digest in extracted.items():
        if digest is None or digest != identity["staged"].get(path) or digest != sha256(args.runtime / path):
            failures.append(f"packaged test material differs from OBJDIR/source: {path}")
    if sha256(args.support / "mochitest/mozinfo.json") != identity["runtime"]["mozinfo_sha256"]:
        failures.append("packaged mozinfo.json differs from the compiled OBJDIR")
    record = {
        "status": "BOUND" if not failures else "REJECTED",
        "failures": failures,
        "runtime": identity["runtime"],
        "packages": packages,
        "floorp": {"commit": git(args.floorp_source, "rev-parse", "HEAD"),
                   "tree": git(args.floorp_source, "rev-parse", "HEAD^{tree}")},
        "product": {"archive": args.product_archive.name, "sha256": sha256(args.product_archive),
                    "application": product_app, "platform": product_platform,
                    "floorp_sha256": sha256(args.product_root / "floorp"),
                    "libxul_sha256": sha256(libxul) if libxul.is_file() else None,
                    "noraneko_chrome_sample": noraneko},
        "cohort": {"planned": sum(len(c["tests"]) for c in cohorts),
                   "manifests": {c["manifest"]: c["manifest_sha256"] for c in cohorts}},
    }
    args.output.write_text(json.dumps(record, indent=2), encoding="utf-8")
    print(json.dumps({"status": record["status"], "failures": failures}))
    return 0 if not failures else 1


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    r = sub.add_parser("runtime")
    for flag in ("runtime", "objdir", "lock", "output"):
        r.add_argument(f"--{flag}", type=Path, required=True)
    b = sub.add_parser("bind")
    for flag in ("runtime", "runtime-support", "support", "floorp-source", "product-archive",
                 "product-root", "output"):
        b.add_argument(f"--{flag}", type=Path, required=True)
    args = p.parse_args()
    return runtime_identity(args) if args.command == "runtime" else bind(args)


if __name__ == "__main__":
    sys.exit(main())
