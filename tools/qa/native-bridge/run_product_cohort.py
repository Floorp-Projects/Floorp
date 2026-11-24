"""Run the planned product cohort serially and aggregate stock structured logs.

The verdict is PASS only when every planned original case started and ended
with an expected pass, every batch exited zero, discovery matched exactly, no
unexpected subtest or leak record was logged and no product process outlived
its batch. Anything else, including missing evidence, is FAIL.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).parent))
import product_cohort as cohort  # noqa: E402

PASSING = {"PASS", "OK"}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def product_processes(root: Path) -> list[int]:
    found = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            exe = Path(os.readlink(entry / "exe"))
        except OSError:
            continue
        if root in exe.parents:
            found.append(int(entry.name))
    return found


def leak_failed(row: dict) -> bool:
    """Mirror mozlog's TBPL leakcheck verdict for a mozleak_total record."""
    if row.get("bytes") is None:
        return not (row.get("induced_crash") or row.get("ignore_missing"))
    return row["bytes"] > row.get("threshold", 0)


def read_raw(path: Path) -> dict:
    tests, unexpected, leaks, leak_warnings, crashes, suite_end = {}, [], [], [], [], False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        try:
            row = json.loads(line)
        except ValueError:
            continue
        action = row.get("action")
        test = row.get("test")
        if action == "test_start":
            tests.setdefault(test, {"started": 0, "ended": []})["started"] += 1
        elif action == "test_end":
            tests.setdefault(test, {"started": 0, "ended": []})["ended"].append(
                {"status": row.get("status"), "expected": row.get("expected")}
            )
        elif action == "test_status" and "expected" in row:
            unexpected.append({"test": test, "subtest": row.get("subtest"),
                               "status": row.get("status"), "expected": row.get("expected")})
        elif action == "mozleak_total":
            if leak_failed(row):
                leaks.append(row)
            elif row.get("bytes"):
                leak_warnings.append(row)
        elif action == "crash":
            crashes.append({"test": row.get("test"), "signature": row.get("signature")})
        elif action == "suite_end":
            suite_end = True
    return {"tests": tests, "unexpected": unexpected, "leaks": leaks,
            "leak_warnings": leak_warnings, "crashes": crashes, "suite_end": suite_end}


def classify(job: dict, raw: dict | None, exit_code: int | None) -> dict:
    outcomes = {}
    for case in job["cases"]:
        record = raw["tests"].get(case) if raw else None
        if not record or record["started"] == 0:
            outcomes[case] = "unexecuted"
        elif record["started"] != 1 or len(record["ended"]) != 1:
            outcomes[case] = "incomplete"
        elif any(u["test"] == case for u in raw["unexpected"]):
            outcomes[case] = "fail"
        elif any(c["test"] == case for c in raw["crashes"]):
            outcomes[case] = "crash"
        else:
            end = record["ended"][0]
            outcomes[case] = "pass" if end["expected"] is None and end["status"] in PASSING else "fail"
    return outcomes


def run_job(job: dict, product_root: Path, timeout: int, env: dict) -> dict:
    out = {"id": job["id"], "cases": job["cases"], "original_manifest": job["original_manifest"]}
    discovery = Path(job["discovery_argv"][job["discovery_argv"].index("--dump-tests") + 1])
    proc = subprocess.run(job["discovery_argv"], env=env, timeout=600,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    Path(str(discovery) + ".log").write_bytes(proc.stdout)
    out["discovery_exit"] = proc.returncode
    try:
        if proc.returncode != 0 or not discovery.is_file():
            raise ValueError("native discovery did not complete")
        cohort.validate_discovery(job, json.loads(discovery.read_text(encoding="utf-8")))
        out["discovery"] = "matched"
    except ValueError as error:
        out["discovery"] = f"rejected: {error}"
        out["outcomes"] = {case: "preflight_rejected" for case in job["cases"]}
        return out
    raw_path = Path(job["argv"][job["argv"].index("--log-raw") + 1])
    terminal = raw_path.with_suffix(".terminal.log")
    started = time.time()
    with terminal.open("wb") as sink:
        try:
            out["exit"] = subprocess.run(job["argv"], env=env, timeout=timeout,
                                         stdout=sink, stderr=subprocess.STDOUT).returncode
        except subprocess.TimeoutExpired:
            out["exit"] = "timeout"
    out["seconds"] = round(time.time() - started, 1)
    survivors = product_processes(product_root)
    for pid in survivors:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
    out["teardown_survivors"] = survivors
    raw = read_raw(raw_path) if raw_path.is_file() else None
    out["raw_log"] = {"path": raw_path.name, "sha256": digest(raw_path)} if raw else None
    out["suite_end"] = bool(raw and raw["suite_end"])
    out["unexpected"] = raw["unexpected"] if raw else []
    out["leaks"] = raw["leaks"] if raw else []
    out["leak_warnings"] = raw["leak_warnings"] if raw else []
    out["crashes"] = raw["crashes"] if raw else []
    extra = sorted(set(raw["tests"]) - set(job["cases"])) if raw else []
    out["unplanned_tests"] = extra
    out["outcomes"] = classify(job, raw, out["exit"])
    return out


def verdict(results: list[dict], planned: int) -> dict:
    outcomes = {}
    for r in results:
        outcomes.update(r["outcomes"])
    counts = {}
    for value in outcomes.values():
        counts[value] = counts.get(value, 0) + 1
    clean = (
        len(outcomes) == planned
        and counts.get("pass", 0) == planned
        and all(r.get("exit") == 0 and r.get("suite_end") and not r.get("teardown_survivors")
                and not r.get("leaks") and not r.get("unplanned_tests") for r in results)
    )
    return {"verdict": "PASS" if clean else "FAIL", "planned": planned,
            "recorded": len(outcomes), "counts": counts}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for flag in ("source", "python", "floorp", "modules", "output"):
        p.add_argument(f"--{flag}", type=Path, required=True)
    p.add_argument("--batch-size", type=int, default=16)
    p.add_argument("--batch-timeout", type=int, default=5400)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    plan = json.loads(cohort.PLAN.read_text(encoding="utf-8"))
    cohorts = cohort.materialize(plan, args.source)
    jobs = cohort.commands(cohorts, args.source, args.python, args.floorp, args.modules,
                           args.output, args.batch_size)
    planned = sum(len(j["cases"]) for j in jobs)
    product_root = args.floorp.resolve().parent
    if product_processes(product_root):
        raise SystemExit("product processes already running before the first batch")
    results = []
    summary_path = args.output / "summary.json"
    for job in jobs:
        results.append(run_job(job, product_root, args.batch_timeout, dict(os.environ)))
        summary = {"source_runtime": plan["source_runtime"],
                   "floorp_sha256": digest(args.floorp.resolve()),
                   "jobs_planned": len(jobs), "jobs_run": len(results),
                   **verdict(results, planned), "batches": results}
        summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps({k: summary[k] for k in ("verdict", "planned", "recorded", "counts")}))
    return 0 if summary["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
