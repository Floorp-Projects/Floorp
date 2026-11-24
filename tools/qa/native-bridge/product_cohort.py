"""Plan serial original Mozilla browser-chrome tests; never launches or grants a gate."""
from __future__ import annotations

import argparse
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


def commands(cohorts, source, python, product, modules, output, batch_size):
    """Return argv arrays in serial order. Qualification is the caller's job."""
    if not 1 <= batch_size <= 32:
        raise ValueError("batch size must be 1..32")
    source = source.resolve(strict=True)
    mach = under(source, "mach")
    binary = product.resolve(strict=True)
    if binary.name not in {"floorp", "floorp-bin", "floorp.exe"} or not binary.is_file():
        raise ValueError("explicit Floorp executable required, not an implicit default")
    modules = modules.resolve(strict=True)
    if not modules.is_dir():
        raise ValueError("explicit same-build testing-modules directory required")
    output = output.resolve()
    result = []
    for c in cohorts:
        for offset in range(0, len(c["tests"]), batch_size):
            batch = c["tests"][offset : offset + batch_size]
            label = f"{c['id']}-{offset // batch_size + 1:03d}"
            result.append(
                {
                    "id": label,
                    "original_manifest": c["manifest"],
                    "cases": batch,
                    "argv": [
                        str(python.resolve(strict=True)), str(mach), "mochitest",
                        "--flavor", "browser", "--appname", str(binary),
                        "--testing-modules-dir", str(modules),
                        "--log-raw", str(output / f"{label}.jsonl"),
                        "--log-tbpl", str(output / f"{label}.tbpl"), *batch,
                    ],
                }
            )
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for flag in ("source", "python", "floorp", "modules", "output"):
        p.add_argument(f"--{flag}", type=Path, required=True)
    p.add_argument("--batch-size", type=int, default=16)
    args = p.parse_args()
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    cohorts = materialize(plan, args.source)
    jobs = commands(
        cohorts, args.source, args.python, args.floorp, args.modules,
        args.output, args.batch_size,
    )
    print(json.dumps({
        "status": "PLAN_ONLY_NOT_EXECUTED",
        "source_runtime": plan["source_runtime"],
        "case_count": sum(len(j["cases"]) for j in jobs),
        "jobs_serial": jobs,
    }, indent=2))


if __name__ == "__main__":
    main()
