"""Plan serial original Mozilla browser-chrome tests; never launches or grants a gate."""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tomllib

PLAN = Path(__file__).with_name("firefox-product-cohort.json")
MANIFESTS = {
    "tabs": "browser/components/tabbrowser/test/browser/tabs/browser.toml",
    "sessionstore": "browser/components/sessionstore/test/browser.toml",
    "sessionstore-oldformat": "browser/components/sessionstore/test/browser_oldformat.toml",
}
TEST_NAME = re.compile(r"browser_[A-Za-z0-9_.-]+[.]js\Z")


def under(root: Path, relative: str) -> Path:
    candidate = (root / relative).resolve(strict=True)
    if not candidate.is_relative_to(root):
        raise ValueError(f"test material leaves source root: {relative}")
    if not candidate.is_file():
        raise ValueError(f"not a file: {relative}")
    return candidate


def materialize(plan: dict, source: Path) -> list[dict]:
    """Validate every path against the original pinned TOML, without editing it."""
    if plan.get("schema") != 1 or not re.fullmatch(
        r"[0-9a-f]{40}", plan.get("source_runtime", "")
    ):
        raise ValueError("unsupported upstream plan")
    source = source.resolve(strict=True)
    cohorts = plan["cohorts"]
    if [c["id"] for c in cohorts] != list(MANIFESTS):
        raise ValueError("missing or reordered original cohort")
    seen: set[str] = set()
    for c in cohorts:
        name = MANIFESTS[c["id"]]
        if c["manifest"] != name:
            raise ValueError(f"wrong original manifest for {c['id']}")
        raw = under(source, name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != c["manifest_sha256"]:
            raise ValueError(f"original manifest changed: {name}")
        actual = tomllib.loads(raw.decode("utf-8"))
        tests = c["tests"]
        if not tests or tests != sorted(set(tests)):
            raise ValueError(f"empty, duplicated or unordered cohort: {c['id']}")
        parent = PurePosixPath(name).parent
        for path in tests:
            key = PurePosixPath(path).name
            if (
                not TEST_NAME.fullmatch(key)
                or path != str(parent / key)
                or key not in actual
                or path in seen
            ):
                raise ValueError(f"test is not uniquely registered in {name}: {path}")
            under(source, path)
            seen.add(path)
    return cohorts


def commands(cohorts, support, python, product, output, batch_size):
    """Return argv arrays in serial order. Qualification is the caller's job.

    ``support`` is the extracted common + mochitest test packages that
    ``mach package-tests`` produced from the compiled Runtime OBJDIR.
    """
    if not 3 <= batch_size <= 32:
        raise ValueError("batch size must be 3..32")
    support = support.resolve(strict=True)
    harness = under(support, "mochitest/runtests.py")
    binary = product.resolve(strict=True)
    if binary.name not in {"floorp", "floorp-bin", "floorp.exe"} or not binary.is_file():
        raise ValueError("explicit Floorp executable required, not an implicit default")
    for name in ("bin", "certs", "modules"):
        if not (support / name).is_dir():
            raise ValueError(f"packaged test support lacks {name}/")
    output = output.resolve()
    result = []
    for c in cohorts:
        batches = [c["tests"][i : i + batch_size] for i in range(0, len(c["tests"]), batch_size)]
        if len(batches[-1]) == 1:
            if len(batches) == 1:
                raise ValueError("one-case discovery can ignore the original skip condition")
            batches[-1].insert(0, batches[-2].pop())
        for index, batch in enumerate(batches, 1):
            label = f"{c['id']}-{index:03d}"
            argv = [
                str(python.absolute()), str(harness),
                "--flavor", "browser", "--appname", str(binary),
                "--utility-path", str(support / "bin"),
                "--certificate-path", str(support / "certs"),
                "--extra-profile-file", str(support / "bin/plugins"),
                "--testing-modules-dir", str(support / "modules"),
                "--log-raw", str(output / f"{label}.jsonl"),
                "--log-tbpl", str(output / f"{label}.tbpl"),
            ]
            result.append(
                {
                    "id": label,
                    "original_manifest": c["manifest"],
                    "cases": batch,
                    "argv": [*argv, *batch],
                    "discovery_argv": [
                        *argv, "--dump-tests", str(output / f"{label}.discovery.json"), *batch,
                    ],
                }
            )
    return result


def validate_discovery(job: dict, document: dict) -> None:
    """Reject any mismatch in stock --dump-tests data; matching is not a test pass."""
    expected = job["cases"]
    if len(expected) < 2 or len(set(expected)) != len(expected):
        raise ValueError("discovery requires multiple distinct expected originals")
    active = document.get("active_tests")
    if not isinstance(active, list) or not all(isinstance(row, dict) for row in active):
        raise ValueError("invalid native discovery")
    original = PurePosixPath(job["original_manifest"])
    if any(PurePosixPath(path).parent != original.parent for path in expected):
        raise ValueError("cohort paths do not share their original manifest directory")
    observed = Counter()
    for row in active:
        path = row.get("path")
        if not isinstance(path, str):
            raise ValueError("invalid discovered path")
        if row.get("manifest") not in (str(original), original.name):
            raise ValueError(f"unexpected discovered manifest: {path}")
        if "disabled" in row or row.get("expected", "pass") != "pass":
            raise ValueError(f"original test is disabled or not expected to pass: {path}")
        observed[path] += 1
    planned = Counter(expected)
    if observed != planned:
        missing = sorted((planned - observed).elements())
        extra = sorted((observed - planned).elements())
        raise ValueError(f"native discovery mismatch: missing={missing}, extra={extra}")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for flag in ("source", "support", "python", "floorp", "output"):
        p.add_argument(f"--{flag}", type=Path, required=True)
    p.add_argument("--batch-size", type=int, default=16)
    args = p.parse_args()
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    cohorts = materialize(plan, args.source)
    jobs = commands(
        cohorts, args.support, args.python, args.floorp, args.output, args.batch_size,
    )
    print(json.dumps({
        "status": "PLAN_ONLY_NOT_EXECUTED",
        "source_runtime": plan["source_runtime"],
        "case_count": sum(len(j["cases"]) for j in jobs),
        "jobs_serial": jobs,
    }, indent=2))


if __name__ == "__main__":
    main()
