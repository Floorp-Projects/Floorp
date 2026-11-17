#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Two-test native qualification only. Never satisfies the full/pilot gate.
Inputs must come from the trusted cold controller, not PR-authored records.
"""
import hashlib
import json
from pathlib import Path
import tomllib
import gate

SCOPE = "floorp-tabs-session-qualification-v1"
CASES = {
 "tabs": ("browser/components/tabbrowser/test/browser/tabs/browser.toml", "browser_open_newtab_start_observer_notification.js"),
 "session": ("browser/components/sessionstore/test/browser_oldformat.toml", "browser_906076_lazy_tabs.js"),
}

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def resolve(support, dumps, floorp_sha, runtime_sha, *, mochitest_root=None):
    gate.require(gate.sha(floorp_sha,40) and gate.sha(runtime_sha,40), "exact source required")
    gate.require(all(CASES.values()), "BLOCKED: approved session selection unavailable")
    root=Path(support).resolve(strict=True)/"mochitest/browser"
    groups=[]
    for ident,(relative,name) in CASES.items():
        manifest=root/relative; test=manifest.parent/name
        for path in (manifest,test):
            gate.require(path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(root), "missing/unsafe selected source")
        gate.require(name in tomllib.loads(manifest.read_text()), "selected test not registered in original manifest")
        dump=Path(dumps)/(ident+".dump.json")
        data=json.loads(dump.read_text())
        tests=data.get("active_tests")
        gate.require(isinstance(tests,list) and len(tests)==1, "qualification must resolve exactly one test per manifest")
        row=tests[0]
        candidate=Path(row.get("path", "")).resolve()
        if mochitest_root is not None:
            mutable=Path(mochitest_root).resolve(strict=True)
            gate.require(not mutable.is_relative_to(Path(support).resolve()), "mutable harness overlaps frozen support")
            expected=mutable/"browser"/test.relative_to(root)
            gate.require(candidate==expected and candidate.is_file() and sha(candidate)==sha(test), "changed/foreign mutable selected test")
        else:
            gate.require(candidate==test.resolve(), "dump selected a foreign test")
        gate.require("disabled" not in row, "selected native test is disabled on this cohort")
        groups.append({"id":ident,"test":name,"path":test.relative_to(root).as_posix(),
                       "manifest":relative,"manifest_sha256":sha(manifest),"test_sha256":sha(test),"dump_sha256":sha(dump)})
    return {"schema":SCOPE,"floorp_sha":floorp_sha,"runtime_sha":runtime_sha,"groups":groups,"required_check":False}

def finalize(plan, execution, evidence):
    gate.require(all(CASES.values()), "BLOCKED: approved session selection unavailable")
    gate.require(plan.get("schema")==SCOPE and plan.get("required_check") is False, "wrong qualification scope")
    groups=plan.get("groups",[])
    gate.require(len(groups)==2 and {g["id"] for g in groups}==set(CASES), "missing qualification cases")
    for g in groups:
        manifest,name=CASES[g["id"]]
        gate.require(g["manifest"]==manifest and g["test"]==name and g["path"]==str(Path(manifest).parent/name), "changed approved selection")
        gate.require(all(gate.sha(g[k]) for k in ("manifest_sha256","test_sha256","dump_sha256")), "missing source/dump digest")
    expected=hashlib.sha256(json.dumps(plan,sort_keys=True,separators=(",", ":")).encode()).hexdigest()
    gate.require(execution.get("plan_sha256")==expected, "execution plan mismatch")
    gate.require(execution.get("schema")=="floorp-focused-native-execution-v1", "not native qualification execution")
    for key in ("finished","owned_cleanup_complete","product_unchanged","support_unchanged"):
        gate.require(execution.get(key) is True, "incomplete/changed native execution")
    rows=execution.get("results",[])
    gate.require(len(rows)==2 and {r["id"] for r in rows}==set(CASES), "missing/duplicate native result")
    for row in rows:
        gate.require(type(row.get("exit_code")) is int and row["exit_code"]==0, "native process failed")
        log=gate.material(Path(evidence),row["raw_log"],row["raw_sha256"])
        # No declared skips, expected differences, empty, missing or unknown results.
        gate.require(gate.raw_summary(log,{CASES[row["id"]][1]},"unique-file-basename")==1, "native coverage mismatch")
    return {"schema":SCOPE,"verdict":"PASS","native_tests_executed":2,"required_check":False,
            "full_pilot":"NOT_RUN","production_equivalence":False}


def command(case, python, support, product, output, prefs, *, mochitest_root, dump=None):
    """Construct native original-manifest argv; never execute or infer PASS."""
    import cold_schedule
    gate.require(case in CASES and CASES[case] is not None, "BLOCKED: reviewed selection unavailable")
    relative,name=CASES[case]
    manifest=Path(support).resolve()/"mochitest/browser"/relative
    gate.require(name in tomllib.loads(manifest.read_text()), "selected test not in original manifest")
    group={"kind":"browser","manifest":str(manifest),"manifest_sha256":sha(manifest)}
    argv=cold_schedule.command(group,python,support,product,output,prefs,dump=dump,mochitest_root=mochitest_root)
    argv[2]=str(Path(__file__).with_name("focused_browser_entry.py"))
    argv.append(str(Path(relative).parent/name))
    return argv, {"FLOORP_QUALIFICATION_CASE":case,"COLD_MOCHITEST_ROOT":str(Path(mochitest_root).resolve())}
