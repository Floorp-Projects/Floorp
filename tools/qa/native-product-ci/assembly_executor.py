#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Owned official assembly execution interface; never issues native qualification.
Call only in a disposable secretless worker with controller-protected recipe.
System provisioning is separately explicit and must not run candidate code as root.
"""
import hashlib
import os
from pathlib import Path
import shutil
import yaml
import assembly_layout as layout
import artifact_assembly_recipe
from build_worker import environment_after_phase

SETUP="sudo apt update\nsudo apt install msitools jq -y\n./mach --no-interactive bootstrap --application-choice browser_artifact_mode\n"


def setup_contract(workflow, expected_sha):
    layout.require(layout.digest(workflow)==expected_sha,"trusted workflow changed")
    doc=yaml.safe_load(Path(workflow).read_text())
    rows=[s for j in doc["jobs"].values() for s in j.get("steps",[]) if s.get("name")=="Setup" and "run" in s]
    layout.require(len(rows)==1 and rows[0]["run"]==SETUP,"official Setup boundary changed")
    return {"system_argv":[["apt","update"],["apt","install","msitools","jq","-y"]],
            "worker_argv":["./mach","--no-interactive","bootstrap","--application-choice","browser_artifact_mode"]}


SYSTEM_PATH="/usr/local/bin:/usr/bin:/bin"

def deno_binding(deno, path=None):
    deno=Path(deno)
    layout.require(deno.is_absolute() and deno.name=="deno" and not any(c in str(deno) for c in ":\n\r"),"malformed Deno path")
    expected=":".join(dict.fromkeys([str(deno.parent),*SYSTEM_PATH.split(":")]))
    if path is None:path=expected
    layout.require(path==expected and all(Path(part).is_absolute() for part in path.split(":")),"malformed/uncontrolled PATH")
    layout.require(deno.is_file() and not deno.is_symlink() and os.access(deno,os.X_OK),"Deno is not an executable regular file")
    layout.require(shutil.which("deno",path=path)==str(deno),"Deno PATH resolution mismatch")
    return path


def environment(record, state, deno):
    state=Path(state);state.mkdir(exist_ok=False)
    for name in ("tmp","cache","config","data","mozbuild"):(state/name).mkdir()
    return {"PATH":deno_binding(deno),"HOME":record["paths"]["home"],"LANG":"C.UTF-8",
      "TMPDIR":str(state/"tmp"),"XDG_CACHE_HOME":str(state/"cache"),"XDG_CONFIG_HOME":str(state/"config"),"XDG_DATA_HOME":str(state/"data"),
      "MOZBUILD_STATE_PATH":str(state/"mozbuild"),"GIT_CONFIG_NOSYSTEM":"1","GIT_CONFIG_GLOBAL":"/dev/null","GIT_TERMINAL_PROMPT":"0",
      "GITHUB_WORKSPACE":record["paths"]["runtime"],"GITHUB_ENV":str(state/"phase.env"),
      "INPUT_PLATFORM":"Linux-x86_64","CANON_PLATFORM":"Linux-x86_64","IS_BETA":"false","CI":"true",
      "DENO_EXECUTABLE_PATH":str(deno),"LIBGL_ALWAYS_SOFTWARE":"1","PYTHONDONTWRITEBYTECODE":"1","PYTHONNOUSERSITE":"1",
      "NATIVE_DEBUG_PRIMARY_SHA256":record["primary_sha256"]}


def execute(record, recipe_root, workflow, primary_root, state, io, *, build_id, deno):
    """io is exact reviewed qa3_native_io loaded by helper_modules, not PR code.
    Caller has already authenticated record, producer primary and worker UID.
    No shell callbacks, arbitrary phase commands or privileged operations.
    """
    import re
    layout.require(os.geteuid()>0,"assembly worker must not be root")
    layout.require(re.fullmatch("[0-9]{14}",build_id or ""),"producer BuildID required")
    layout.require(not any(k.startswith(("GITHUB","ACTIONS")) or any(s in k.upper() for s in ("TOKEN","SECRET","PASSWORD")) for k in os.environ),"worker inherited credentials")
    contract=setup_contract(workflow,record["workflow_sha256"])
    recipe_root=Path(recipe_root);runtime=Path(record["paths"]["runtime"])
    expected=[]
    for index,(name,script) in enumerate(artifact_assembly_recipe.extract(workflow)):
        data=('#!/usr/bin/env bash\nset -euo pipefail\n'+script).encode()
        expected.append({"name":name,"script":f"{index:02}.sh","sha256":hashlib.sha256(data).hexdigest()})
    layout.require(record["steps"]==expected,"layout is not the exact compiled official sequence")
    for source,sha in ((runtime,record["runtime_sha"]),(Path(record["paths"]["floorp"]),record["floorp_sha"])):
        actual=layout.checked(["git","-C",str(source),"rev-parse","HEAD"]).stdout.decode().strip()
        layout.require(actual==sha,"worker source revision changed")
        layout.checked(["git","-C",str(source),"diff","--exit-code","HEAD","--"])
    # No apt/sudo here. Privileged provisioning is an earlier trusted CI image step.
    for tool in ("jq","msiinfo","node","patchelf"):
        layout.require(shutil.which(tool,path="/usr/local/bin:/usr/bin:/bin"),"BLOCKED: preprovisioned tool missing: "+tool)
    layout.protected(workflow,os.getuid());layout.protected(deno,os.getuid())
    for name in ("floorp","libxul.so"):layout.protected(Path(primary_root)/name,os.getuid())
    for step in record["steps"]:
        path=recipe_root/step["script"];layout.protected(path,os.getuid())
        layout.require(layout.digest(path)==step["sha256"],"compiled phase changed")
    layout.require(layout.digest(record["paths"]["primary"])==record["primary_sha256"],"staged primary changed")
    env=environment(record,state,deno);env["MOZ_BUILD_DATE"]=build_id
    deno_sha=layout.digest(deno)
    state=Path(state);logs=io.fresh(state/"logs");results=[];injected=None
    def run(name,argv,timeout):
        deno_binding(deno,env["PATH"])
        layout.require(layout.digest(deno)==deno_sha,"protected Deno bytes changed")
        result=io.run_owned(argv,runtime,env,logs,name,timeout,memory=14*io.GIB,log_limit=16*1024**2,minimum_free=10*io.GIB)
        results.append(result)
        io.write_json(state/(name+".receipt.json"),result)
    for index,step in enumerate(record["steps"]):
        name=step["name"]
        # Match official Setup position: after Setup Noraneko, before versions.
        if name=="Write Versions":run("artifact-bootstrap",contract["worker_argv"],1200)
        path=recipe_root/step["script"]
        layout.require(layout.digest(path)==step["sha256"],"phase changed before execution")
        run("phase-"+str(index),["/bin/bash",str(path)],1800)
        env=environment_after_phase(name,env,runtime)
        phase_env=state/"phase.env"
        if phase_env.exists():
            for line in phase_env.read_text().splitlines():
                key,sep,value=line.partition("=")
                layout.require(sep and key=="UPDATE_CHANNEL" and value,"unreviewed phase environment output")
                env[key]=value
        if name=="Inject Noraneko":
            injected=(runtime/"obj-artifact-build-output/dist/bin/browser/chrome/browser/content/browser/browser.xhtml").read_bytes()
    layout.require(injected is not None,"injection phase did not produce browser XHTML")
    product=runtime/"obj-artifact-build-output/dist/floorp"
    integrity=artifact_assembly_recipe.verify_product(primary_root,product,injected)
    return {"status":"ASSEMBLED_PENDING_REGISTRATION_AND_NATIVE_TESTS","integrity":integrity,"product":str(product),"commands":results,"native_execution":"NOT_RUN"}
