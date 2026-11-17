#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Prepare official artifact-mode layout without executing candidate source.
No package install, bootstrap, network clone, build, test, or PASS issuance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import artifact_assembly_recipe as recipe


def require(ok,message):
    if not ok:raise ValueError(message)

def digest(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda:stream.read(1024*1024),b""):h.update(block)
    return h.hexdigest()

def checked(argv, *, cwd=None):
    # No shell interpolation, inherited token, Git hooks/config or prompt channel.
    env={"PATH":"/usr/bin:/bin","HOME":"/nonexistent","GIT_CONFIG_NOSYSTEM":"1","GIT_CONFIG_GLOBAL":"/dev/null","GIT_TERMINAL_PROMPT":"0"}
    return subprocess.run(argv,cwd=cwd,env=env,check=True,capture_output=True,timeout=180)

def clone(source,destination,sha):
    source=Path(source).resolve(strict=True)
    require(re.fullmatch("[0-9a-f]{40}",sha or ""),"immutable source required")
    checked(["git","-C",str(source),"cat-file","-e",sha+"^{commit}"])
    # Read only the existing local object database; never fetch remote state.
    checked(["git","-c","core.hooksPath=/dev/null","clone","--local","--no-hardlinks","--no-checkout",str(source),str(destination)])
    checked(["git","-C",str(destination),"remote","remove","origin"])
    checked(["git","-C",str(destination),"-c","core.hooksPath=/dev/null","checkout","--detach",sha])
    require(not (destination/".gitmodules").exists(),"submodule inputs need separate reviewed pins")
    actual=checked(["git","-C",str(destination),"rev-parse","HEAD"]).stdout.decode().strip()
    require(actual==sha,"staged source mismatch")

def protected(path,worker_uid):
    path=Path(path)
    for current in (path,*path.parents):
        mode=current.lstat()
        require(not stat.S_ISLNK(mode.st_mode) and mode.st_uid!=worker_uid and not mode.st_mode & 0o022,"worker can modify protected input/ancestor")
    require(path.is_file(),"protected input must be a regular file")

def prepare(runtime_source,floorp_source,runtime_sha,floorp_sha,workflow,workflow_sha,primary,primary_sha,output):
    output=Path(output).absolute();workflow=Path(workflow);primary=Path(primary)
    require(not output.exists() and not output.is_symlink(),"fresh layout required")
    require(not any(part.is_symlink() for part in output.parents),"layout ancestor is symlink")
    require(not any(output.is_relative_to(Path(src).resolve()) for src in (runtime_source,floorp_source)),"layout overlaps source checkout")
    require(workflow.is_file() and not workflow.is_symlink() and digest(workflow)==workflow_sha,"trusted package recipe digest mismatch")
    require(primary.is_file() and not primary.is_symlink() and digest(primary)==primary_sha,"authenticated primary mismatch")
    output.mkdir(parents=False)
    runtime=output/"runtime";floorp=runtime/"noraneko"
    clone(runtime_source,runtime,runtime_sha)
    require(not floorp.exists(),"Runtime already has candidate overlay")
    clone(floorp_source,floorp,floorp_sha)
    home=output/"home";artifacts=home/"artifacts";artifacts.mkdir(parents=True)
    target=artifacts/"floorp-linux-x86_64-moz-artifact.tar.xz"
    shutil.copyfile(primary,target);require(digest(target)==primary_sha,"primary copy changed")
    steps=recipe.write(workflow,output/"recipe")
    for row in steps:(output/"recipe"/row["script"]).chmod(0o444)
    target.chmod(0o444)
    record={"schema":"floorp-artifact-layout-v1","status":"STAGED_NOT_EXECUTED","runtime_sha":runtime_sha,"floorp_sha":floorp_sha,"workflow_sha256":workflow_sha,"primary_sha256":primary_sha,"steps":steps,
      "paths":{"runtime":str(runtime),"floorp":str(floorp),"home":str(home),"primary":str(target)},
      "blockers":["reviewed browser_artifact_mode bootstrap/toolchain readiness","different-owner protected controller recipe and primary","owned build worker and native test worker integration"],"native_execution":"NOT_RUN"}
    (output/"layout.json").write_text(json.dumps(record,indent=2)+"\n")
    return record

def main():
    p=argparse.ArgumentParser()
    for name in ("runtime-source","floorp-source","runtime-sha","floorp-sha","workflow","workflow-sha","primary","primary-sha","output"):p.add_argument("--"+name,required=True)
    a=p.parse_args();prepare(**vars(a))
    print("BLOCKED: staged layout is not an assembled or qualified product")
    return 2

if __name__=="__main__":raise SystemExit(main())
