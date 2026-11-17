#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Trusted secretless controller material stage, never a native PASS issuer.
The caller supplies its controlled checked-in pin, never a candidate PR pin.
Uses the exact pinned Runtime helpers rather than a vendored extraction fork.
"""
import argparse
import importlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import artifact_preflight as preflight


def helper_modules(recipe, pin):
    recipe=Path(recipe).resolve(strict=True)
    expected=pin["runtime_sha"]
    preflight.require(re.fullmatch("[0-9a-f]{40}", expected or ""), "invalid helper revision")
    def git(*args):
        return subprocess.check_output(["git","-C",str(recipe),*args])
    preflight.require(git("rev-parse","HEAD").decode().strip()==expected,"helper revision mismatch")
    preflight.require(not git("status","--porcelain","--untracked-files=all","--",".github"),"modified/untracked helper source")
    scripts=recipe/".github/workflows/scripts"
    # A clean index alone is insufficient with assume-unchanged/skip-worktree.
    tracked=git("ls-tree","-rz",expected,"--",".github").split(b"\0")
    for entry in filter(None,tracked):
        header,name=entry.split(b"\t",1);mode,kind,blob=header.split()
        path=recipe/os.fsdecode(name)
        preflight.require(mode in (b"100644",b"100755") and kind==b"blob" and path.is_file() and not any(part.is_symlink() for part in (path,*path.parents) if part!=recipe.parent),"unsafe helper tree entry")
        preflight.require(path.read_bytes()==git("cat-file","blob",blob.decode()),"helper bytes differ from reviewed revision")
    preflight.require(not any(name in sys.modules for name in ("qa3_hosted","qa3_native_io","qa3_hosted_support")),"helper module already loaded")
    sys.path.insert(0,str(scripts))
    return tuple(importlib.import_module(name) for name in ("qa3_hosted","qa3_native_io","qa3_hosted_support"))


def stage(inputs, output, recipe, pin):
    preflight.require(pin.get("enabled") is True,"BLOCKED: normal Debug qualification pin unavailable")
    preflight.require(os.geteuid()>0 and not any(k.startswith(("GITHUB","ACTIONS")) or any(s in k.upper() for s in ("TOKEN","SECRET","PASSWORD")) for k in os.environ),"material worker must be secretless/non-root")
    inputs=Path(inputs).resolve(strict=True);output=Path(output)
    value=json.loads((inputs/"producer.json").read_text())
    context=value["context"]
    preflight.require(context.get("profile")=="runtime-native-debug-artifact-v1" and context.get("runtimeRecipe")==pin["runtime_sha"],"not the approved normal Debug cohort")
    preflight.require(context.get("run")==pin["run_id"] and context.get("attempt")==pin["run_attempt"] and context.get("buildID")==pin["build_id"] and value.get("productProfile")==pin["product_profile"],"approved producer attempt/profile mismatch")
    # This stage follows trusted API/outer-wrapper preflight, and repeats inner hashes.
    hosted,io,support=helper_modules(recipe,pin)
    hosted.validate_producer(value,context)
    preflight.require(io.file_digest(inputs/"runtime.tar.xz")==value["primarySha256"] and io.file_digest(inputs/"test-support.tar.gz")==value["supportSha256"],"input archive changed")
    io.fresh(output)
    budget=io.IOBudget(output,maximum=50*io.GIB,minimum_free=10*io.GIB)
    expansion=io.ExpansionBudget()
    io.extract_archive(inputs/"runtime.tar.xz",output/"primary",budget=budget,expansion=expansion)
    io.extract_archive(inputs/"test-support.tar.gz",output/"support",budget=budget,expansion=expansion)
    preflight.require(support.verify_primary(output/"primary",context)==value["primaryIdentity"],"primary ELF/cohort mismatch")
    hosted.verify_recipe_metadata(Path(recipe),value)
    hosted.verify_support(output/"support",value)
    result={"status":"MATERIALS_VERIFIED_NOT_QUALIFIED","native_execution":"NOT_RUN","runtime_sha":pin["runtime_sha"],"primary_sha256":value["primarySha256"],"primaryInventory":io.capture_material_tree(output/"primary"),"supportInventory":io.capture_material_tree(output/"support")}
    io.write_json(output/"materials.json",result)
    return result


def main():
    parser=argparse.ArgumentParser()
    for key in ("inputs","output","recipe","pin"):parser.add_argument("--"+key,type=Path,required=True)
    args=parser.parse_args()
    stage(args.inputs,args.output,args.recipe,json.loads(args.pin.read_text()))
    # Material verification cannot be used as a successful qualification check.
    print("BLOCKED: official UI assembly and owned native execution not yet connected",file=sys.stderr)
    return 2

if __name__=="__main__":sys.exit(main())
