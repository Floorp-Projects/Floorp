# SPDX-License-Identifier: MPL-2.0
"""Replay an original Runtime xpcshell export; never run native code or qualify a product."""
import argparse
import hashlib
import importlib.util
import json
import re
import sys
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
MIB = 1024**2


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


archive_tools = module("qa3_debug_archive", ROOT / "tools/linux-l2/collect.py")
supervisor = module("qa3_debug_supervisor", ROOT / "tools/qa3/source.py")
require = archive_tools.require
read_once = archive_tools.read_once
positive = archive_tools.positive


def decode(data):
    require(0 < len(data) <= 16 * MIB, "JSON byte budget rejected")
    return json.loads(data, object_pairs_hook=archive_tools.unique_object,
                      parse_float=archive_tools.finite_float,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def same(actual, expected):
    return json.dumps(actual, sort_keys=True, separators=(",", ":"), allow_nan=False) == json.dumps(
        expected, sort_keys=True, separators=(",", ":"), allow_nan=False)


def write(path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write("\n")


def pin_recipe(repository, output, policy):
    """Read only public pinned Git blobs; never fetch or import a working tree."""
    recipe = output / "runtime-recipe"
    recipe.mkdir()
    for path, identity in policy["files"].items():
        destination = recipe / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        code = supervisor.execute([
            "/usr/bin/env", "-i", "PATH=/usr/bin:/bin", "LC_ALL=C.UTF-8",
            "GIT_CONFIG_NOSYSTEM=1", "GIT_CONFIG_GLOBAL=/dev/null",
            "GIT_NO_LAZY_FETCH=1", "GIT_NO_REPLACE_OBJECTS=1", "GIT_TERMINAL_PROMPT=0",
            "/usr/bin/git", "-C", str(repository), "-c", "core.fsmonitor=false",
            "-c", "core.hooksPath=/dev/null", "cat-file", "blob",
            policy["runtimeHead"] + ":" + path,
        ], destination, ROOT, timeout=15)
        require(code == 0, "pinned Runtime Git object unavailable")
        data = read_once(destination, MIB)
        require(len(data) == identity["bytes"] and sha(data) == identity["sha256"],
                "pinned Runtime helper/plan bytes differ")
        destination.chmod(0o400)
    return recipe


def one(values, predicate, message):
    return archive_tools.one(values, predicate, message)


def option(argv, flag):
    require(argv.count(flag) == 1 and argv.index(flag) + 1 < len(argv),
            "missing or repeated original harness option")
    return argv[argv.index(flag) + 1]


def original_xpcshell_argv(command, case, phase):
    argv = command.get("argv")
    require(isinstance(argv, list) and all(isinstance(v, str) for v in argv)
            and len(argv) > 5 and argv[:2] == ["/usr/bin/xvfb-run", "-a"]
            and argv[3] == "-s", "original xpcshell entry missing")
    entry = PurePosixPath(argv[4])
    require(entry.is_absolute() and ".." not in entry.parts and
            str(entry).endswith("/.github/workflows/scripts/qa3_xpcshell_entry.py"),
            "wrong original xpcshell entry")
    support = PurePosixPath(option(argv, "--testing-modules-dir")).parent
    app = PurePosixPath(option(argv, "--app-path"))
    raw = PurePosixPath(option(argv, "--log-raw"))
    for path in (support, app, raw, PurePosixPath(argv[2])):
        require(path.is_absolute() and ".." not in path.parts and "\\" not in str(path),
                "noncanonical original native path")
    test = support / case["archivePath"]
    expected = ["/usr/bin/xvfb-run", "-a", argv[2], "-s", str(entry),
                "--xpcshell", str(support / "bin/xpcshell"), "--app-path", str(app),
                "--sequential", "--verbose", "--no-logfiles", "--manifest",
                str(test.parent / "xpcshell.toml"), "--xre-path", str(app),
                "--utility-path", str(support / "bin"), "--testing-modules-dir",
                str(support / "modules"), "--log-raw", str(raw), str(test)]
    require(argv == expected and raw.name == phase + ".mozlog",
            "original argv differs from the pinned one-pass xpcshell entry")
    return str(support)


def replay(parameters):
    """Private host-only subprocess. The imported parser is hash-pinned upstream code."""
    value = decode(read_once(parameters))
    sys.path.insert(0, str(Path(value["recipe"]) / ".github/workflows/scripts"))
    from qa3_mozlog import verify_mozlog

    results = []
    for item in value["cases"]:
        verdict = verify_mozlog(Path(item["raw"]), item["case"],
                                Path(item["originalSupport"]), item["process"],
                                canary=item["canary"])
        require(same(verdict, item["recordedVerdict"]), "recorded raw verdict differs")
        results.append({"phase": item["phase"], "verdict": verdict})
    write(Path(value["output"]), results)


def collect(args, output, result):
    provider_bytes = read_once(args.provider)
    (output / "original-provider.json").write_bytes(provider_bytes)
    snapshot = decode(provider_bytes)
    run, job, artifact = snapshot["run"], snapshot["job"], snapshot["artifact"]
    result["originalProvider"] = {"run": run.get("id"), "job": job.get("id"),
                                  "attempt": run.get("run_attempt"), "artifact": artifact.get("id")}
    if run.get("conclusion") in {"failure", "cancelled", "timed_out"} or job.get("conclusion") in {"failure", "cancelled", "timed_out"}:
        result["status"] = "FAIL"
    steps = job.get("steps")
    if isinstance(steps, list):
        result["originalFailureSteps"] = [s.get("name") for s in steps
                                          if s.get("conclusion") in {"failure", "cancelled", "timed_out"}]
        if result["originalFailureSteps"]:
            result["status"] = "FAIL"
    policy = decode(read_once(HERE / "runtime-recipe.json"))
    expected_head = policy["runtimeHead"]
    require(run.get("repository", {}).get("full_name") == "Floorp-Projects/Floorp-Runtime"
            and positive(run.get("id")) and run.get("head_sha") == expected_head
            and type(run.get("run_attempt")) is int and run["run_attempt"] == 1
            and run.get("path") in {".github/workflows/daily-build.yml", ".github/workflows/qa3-hosted-linux-debug.yml"}
            and run.get("status") == "completed" and run.get("conclusion") == "success",
            "original successful pinned Runtime run required")
    require(positive(job.get("id")) and job.get("run_id") == run["id"]
            and job.get("head_sha") == expected_head
            and type(job.get("run_attempt")) is int and job["run_attempt"] == 1
            and job.get("name", "").endswith("Cold Linux x64 Debug harness")
            and job.get("status") == "completed" and job.get("conclusion") == "success",
            "original successful cold Debug job required")
    require(isinstance(steps, list) and not result.get("originalFailureSteps"),
            "original native step metadata missing or failed")
    for name in ("Acquire original same-run producer artifact", "Execute dedicated cold consumer", "Collect original bounded evidence"):
        step = one(steps, lambda s: s.get("name") == name, "required original cold-consumer step missing")
        require(step.get("status") == "completed" and step.get("conclusion") == "success",
                "required original cold-consumer step skipped or incomplete")
    require(positive(artifact.get("id")) and artifact.get("expired") is False
            and artifact.get("name") == f"qa3-consumer-evidence-{run['id']}-1"
            and artifact.get("workflow_run", {}).get("id") == run["id"]
            and artifact["workflow_run"].get("head_sha") == expected_head
            and positive(artifact.get("size_in_bytes")) and artifact["size_in_bytes"] <= 64 * MIB,
            "original unexpired consumer artifact required")
    _, inventory = archive_tools.read_archive(args.archive, output, artifact)
    frozen = output / "original-evidence.zip"
    require(frozen.stat().st_size == artifact["size_in_bytes"], "original archive size differs")
    result["archive"] = {"id": artifact["id"], "digest": artifact["digest"], "members": inventory}
    with zipfile.ZipFile(frozen) as archive:
        def member(name):
            require(name in inventory, "required original evidence member missing")
            with archive.open(name) as stream:
                data = stream.read(inventory[name]["bytes"] + 1)
            require({"bytes": len(data), "sha256": sha(data)} == inventory[name],
                    "frozen evidence member changed")
            return data

        def document(name):
            return decode(member(name))

        collection = document("collection.json")
        final = document("evidence/final.json")
        export = document("export/export.json")
        consumer = document("export/consumer.json")
        if final.get("status") != "PASS" or "export/worker-failure.json" in inventory:
            result["status"] = "FAIL"
        require("export/worker-failure.json" not in inventory, "original worker failure retained")
        context = consumer["context"]
        require(context.get("schema") == "qa3-hosted-context-v1"
                and context.get("profile") == "github-hosted-debug-to-blacksmith-consumer"
                and context.get("executionRepository") == "Floorp-Projects/Floorp-Runtime"
                and context.get("runtimeRecipe") == context.get("controllerHead") == expected_head
                and type(context.get("run")) is int and context.get("run") == run["id"] and type(context.get("attempt")) is int
                and context["attempt"] == 1 and context.get("publicationAuthorized") is False
                and re.fullmatch(r"[0-9]{14}", context.get("buildID", "")), "Debug context differs")
        require(collection.get("schema") == "qa3-hosted-collection-v1"
                and same(collection.get("context"), context)
                and collection.get("acquireOutcome") == collection.get("launchOutcome") == "success"
                and final.get("schema") == "qa3-hosted-root-supervision-v1"
                and same(final.get("context"), context) and final.get("status") == "PASS"
                and final.get("reason") is None and final.get("scopeEmpty") is True
                and type(final.get("serviceExit")) is int and final["serviceExit"] == 0
                and final.get("publicationAuthorized") is False,
                "original acquisition/root supervision failed or incomplete")
        rows = export.get("files")
        require(export.get("schema") == "qa3-hosted-export-v1" and isinstance(rows, list)
                and 0 < len(rows) <= 80 and same(rows, final.get("export")), "root/export inventory differs")
        require(len({r["path"] for r in rows}) == len(rows) and
                {"export/" + r["path"] for r in rows} ==
                {n for n in inventory if n.startswith("export/") and n != "export/export.json"},
                "root/export file membership differs")
        for row in rows:
            require(set(row) == {"path", "bytes", "sha256"} and type(row["bytes"]) is int
                    and {"bytes": row["bytes"], "sha256": row["sha256"]} == inventory["export/" + row["path"]],
                    "root/export member digest differs")
        producer_bytes = read_once(args.producer, 16 * MIB)
        (output / "original-producer.json").write_bytes(producer_bytes)
        require(sha(producer_bytes) == snapshot.get("producerReceiptSha256"), "original producer receipt differs")
        producer = decode(producer_bytes)
        require(producer.get("schema") == "qa3-hosted-debug-producer-v1"
                and producer.get("status") == "PACKAGED_NOT_CONSUMED" and same(producer.get("context"), context)
                and producer.get("producer") == "existing-common-build-github-hosted"
                and producer.get("consumerVerdict") == "UNVERIFIED" and producer.get("publicationAuthorized") is False
                and consumer.get("schema") == "qa3-hosted-cold-consumer-v1" and consumer.get("status") == "PASS"
                and consumer.get("nativeProductQualification") == "UNVERIFIED"
                and consumer.get("publicationAuthorized") is False
                and consumer.get("producerBootSha256") == producer.get("bootSha256")
                and re.fullmatch(r"[0-9a-f]{64}", consumer.get("producerBootSha256", ""))
                and re.fullmatch(r"[0-9a-f]{64}", consumer.get("consumerBootSha256", ""))
                and consumer["producerBootSha256"] != consumer["consumerBootSha256"],
                "producer/consumer Debug subject differs")
        require(all(isinstance(producer.get(key), str) and re.fullmatch(r"[0-9a-f]{64}", producer[key])
                    for key in ("primarySha256", "supportSha256", "planSha256")),
                "Debug primary/support/plan identities missing")
        for key in ("effectiveConfig", "effectiveConfigAfterTests"):
            config = producer[key]["substs"]
            require(all(type(config.get(k)) in {bool, int, str} and config[k] in (True, 1, "1")
                        for k in ("ENABLE_TESTS", "MOZ_DEBUG", "COMPILE_ENVIRONMENT"))
                    and all(config.get(k) in (None, False, 0, "", "0")
                            for k in ("MOZ_ARTIFACT_BUILDS", "MOZ_PROFILE_GENERATE", "MOZ_PROFILE_USE"))
                    and config.get("TARGET_CPU") == "x86_64" and config.get("OS_TARGET") == "Linux",
                    "declared canonical native Debug configuration missing")
        require(isinstance(consumer.get("commands"), list) and 0 < len(consumer["commands"]) <= 32,
                "original command history missing")
        failures = [command.get("name") for command in consumer["commands"]
                    if (type(command.get("exit")) is not int
                        or command["exit"] != (1 if str(command.get("name", "")).endswith("assertion-canary") else 0)
                        or command.get("stop") is not None or command.get("error")
                        or command.get("cancelled") or command.get("cleanupErrors")
                        or command.get("ownedCleanupComplete") is not True)]
        if failures:
            result["status"] = "FAIL"
            result["originalFailedCommands"] = failures
        require(not failures, "original consumer command failed")
        recipe = pin_recipe(args.runtime_git, output, policy)
        plan = decode(read_once(recipe / ".github/qa/hosted-linux-debug-plan.json"))
        require(producer.get("planSha256") == policy["files"][".github/qa/hosted-linux-debug-plan.json"]["sha256"],
                "producer used another Debug plan")
        case = one(plan["selection"], lambda c: c["id"] == policy["caseId"] and c["suite"] == "xpcshell",
                   "pinned xpcshell selection missing")
        parameters = {"recipe": str(recipe), "cases": [], "output": str(output / "replayed.json")}
        for phase, canary in zip(policy["phases"], (False, True)):
            selected = dict(case)
            if canary:
                selected["id"] = "xpcom/tests/unit/test_qa3_assertion.js"
                selected["archivePath"] = "xpcshell/tests/" + selected["id"]
            item = one(consumer["cases"], lambda c: c.get("name") == phase, "missing/duplicate original xpcshell case")
            command = one(consumer["commands"], lambda c: c.get("name") == phase, "missing/duplicate original command")
            require(item.get("suite") == "xpcshell" and item.get("canary") is canary
                    and same(item.get("process"), command)
                    and type(command.get("exit")) is int and command["exit"] == (1 if canary else 0)
                    and command.get("stop") is None and command.get("ownedCleanupComplete") is True
                    and not command.get("cancelled") and not command.get("error") and not command.get("cleanupErrors")
                    and command.get("stdoutIsData") is False, "original xpcshell phase failed")
            require(same(document(f"export/logs/{phase}.process.json"), command),
                    "original process receipt differs")
            for stream in ("stdout", "stderr"):
                require(inventory[f"export/logs/{phase}.{stream}"]["sha256"] == command[stream + "Sha256"],
                        "original process log digest differs")
            raw_name = phase + ".mozlog"
            identity = inventory["export/logs/" + raw_name]
            require(item.get("rawLog") == {"path": "logs/" + raw_name, **identity}
                    and command.get("watchedLogs") == {raw_name: identity}, "raw mozlog binding differs")
            raw = output / raw_name
            with raw.open("xb") as stream:
                stream.write(member("export/logs/" + raw_name))
            parameters["cases"].append({"phase": phase, "canary": canary, "case": selected,
                "process": command, "raw": str(raw), "originalSupport": original_xpcshell_argv(command, selected, phase),
                "recordedVerdict": item["mozlog"]})
        write(output / "replay-input.json", parameters)
        code = supervisor.execute([sys.executable, "-I", "-B", str(Path(__file__).resolve()),
                                   "_replay", str(output / "replay-input.json")],
                                  output / "replay.log", ROOT, timeout=30)
        if code != 0:
            result["status"] = "FAIL"
        require(code == 0, "pinned Mozilla raw-log replay rejected original evidence")
        result["cases"] = decode(read_once(output / "replayed.json"))
        result["debugSubject"] = {"runtimeHead": expected_head, "run": run["id"], "attempt": 1,
                                  "target": "linux-x86_64", "buildID": context["buildID"],
                                  "producerReceiptSha256": sha(producer_bytes),
                                  "primarySha256": producer["primarySha256"],
                                  "supportSha256": producer["supportSha256"],
                                  "planSha256": producer["planSha256"]}
        result["originalArgv"] = [c["process"]["argv"] for c in parameters["cases"]]
    if args.product_record:
        data = read_once(args.product_record)
        (output / "original-product-record.json").write_bytes(data)
        product = decode(data)
        result["productSubject"] = {"nativeRecordSha256": sha(data), "floorp": product["floorp"],
                                    "runtime": product["runtime"], "nativeBuildID": product["app_build_id"]}
        result["runtimeSourceMatchesProduct"] = product["runtime"]["head_sha"] == expected_head
        # Different compile subjects can have different BuildIDs. No applicability is issued here.
    require(result["status"] != "FAIL", "original failure cannot be recovered")
    result["status"] = "PASS_DEBUG_EVIDENCE_ONLY"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("provider", "producer", "archive", "runtime-git", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--product-record", type=Path)
    args = parser.parse_args()
    args.output.mkdir(mode=0o700)
    result = {"schema": "qa3-mozilla-debug-observation-v1", "status": "BLOCK", "cases": [],
              "scope": "linux-x64-xpcshell-mutable-array-and-assertion-canary",
              "externalAuthentication": "REQUIRED", "productApplicability": "UNVERIFIED",
              "sourceAndBuildQualification": "UNVERIFIED",
              "nativeProductQualification": "UNVERIFIED", "publicationAuthorized": False,
              "nativeLaunchedByCollector": False}
    try:
        collect(args, args.output, result)
    except Exception as error:
        # Malformed provider/archive shapes still produce a negative envelope.
        # Interruptions remain outside this handler; absent evidence never qualifies.
        result["errorType"] = type(error).__name__
        result["reason"] = "original Debug evidence missing, failed, changed or unsupported"
    write(args.output / "observation.json", result)
    print(json.dumps({k: result[k] for k in ("status", "publicationAuthorized", "productApplicability")}))
    return 0 if result["status"] == "PASS_DEBUG_EVIDENCE_ONLY" else 1


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "_replay":
        replay(Path(sys.argv[2]))
    else:
        sys.exit(main())
