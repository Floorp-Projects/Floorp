#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check native Worker APIs with a configured Runtime's genuine compiler flags.

The source and object directories are read-only. An isolated source overlay and
Gecko-generated binding declarations can be supplied before a candidate rebuild.
Candidate-generated binding C++ is checked for native signature mismatches too.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def make_values(path):
    result = {}
    for line in path.read_text().splitlines():
        match = re.match(r"^([A-Z_]+)\s*(\+?=)\s*(.*)$", line)
        if match:
            key, operator, value = match.groups()
            result[key] = result.get(key, "") + " " + value if operator == "+=" else value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-build", type=Path, required=True,
                        help="Directory containing configured source/ and obj/")
    parser.add_argument("--source-overlay", type=Path,
                        help="Isolated patched source; defaults to candidate source")
    parser.add_argument("--binding-declarations", type=Path,
                        help="Genuine Gecko-generated Binding.h declarations overriding candidate headers")
    parser.add_argument("--binding-sources", type=Path,
                        help="Genuine Gecko-generated Binding.cpp sources overriding candidate bindings")
    parser.add_argument("--extra-include", type=Path, action="append", default=[],
                        help="Isolated generated headers, such as an updated XPIDL header directory")
    parser.add_argument("--include-header", type=Path, action="append", default=[],
                        help="Force an isolated regenerated header before candidate headers")
    parser.add_argument("--match", help="Only translation units whose filenames match this regular expression")
    parser.add_argument("--kind", choices=("native", "binding"),
                        help="Check native API bodies or generated bindings only")
    parser.add_argument("--additional-native-source", type=Path, action="append", default=[],
                        help="Additional C++ source under the selected source overlay")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--jobs", type=int, default=1)
    arguments = parser.parse_args()
    if not 1 <= arguments.jobs <= 8:
        parser.error("--jobs must be between 1 and 8")

    base = arguments.native_build.resolve()
    obj, source = base / "obj", base / "source"
    own = (arguments.source_overlay or source).resolve()
    output = arguments.output.resolve() if arguments.output else Path(
        tempfile.mkdtemp(prefix="floorp-worker-syntax-")
    )
    if output.is_relative_to(source) or output.is_relative_to(obj):
        parser.error("--output must be outside the candidate source and object directories")
    output.mkdir(parents=True, exist_ok=True)
    overlay = output / "include"
    for namespace in ("extensions", "dom"):
        (overlay / "mozilla" / namespace).mkdir(parents=True, exist_ok=True)
    for header in (own / "toolkit/components/extensions/webidl-api").glob("*.h"):
        target = overlay / "mozilla/extensions" / header.name
        if not target.is_symlink() and not target.exists():
            target.symlink_to(header)
    if arguments.binding_declarations:
        for header in arguments.binding_declarations.resolve().glob("*Binding.h"):
            target = overlay / "mozilla/dom" / header.name
            if not target.is_symlink() and not target.exists():
                target.symlink_to(header)

    common = make_values(obj / "config/autoconf.mk")

    def check(path, directory, kind):
        local = make_values(obj / directory / "backend.mk")

        def expand(value):
            for key, replacement in (
                ("topobjdir", obj), ("topsrcdir", source),
                ("srcdir", source / directory), ("DEPTH", obj),
            ):
                value = value.replace("$(" + key + ")", str(replacement))
            return value

        flags = []
        for name in ("DEFINES", "LOCAL_INCLUDES", "COMPUTED_CXXFLAGS"):
            flags.extend(shlex.split(expand(local.get(name, ""))))
        command = shlex.split(common["CXX"]) + [
            "-fsyntax-only", "-I" + str(overlay), "-I" + str(path.parent),
        ] + ["-I" + str(path.resolve()) for path in arguments.extra_include] + [
            "-DMOZILLA_INTERNAL_API", "-include", str(obj / "mozilla-config.h"),
            "-I" + str(source / directory), "-I" + str(obj / directory),
            "-I" + str(obj / "dist/include"), "-I" + str(obj / "dist/include/nspr"),
        ] + [part for header in arguments.include_header
             for part in ("-include", str(header.resolve()))] + flags + [str(path)]
        log = output / (kind + "-" + path.stem + ".log")
        with log.open("w") as stream:
            result = subprocess.run(command, cwd=source / directory,
                                    stdout=stream, stderr=stream)
        record = {"source": str(path), "kind": kind,
                  "returnCode": result.returncode, "log": str(log)}
        print("PASS" if not result.returncode else "FAIL", kind, path.name, flush=True)
        return record

    jobs = [(path, "toolkit/components/extensions/webidl-api", "native")
            for path in sorted((own / "toolkit/components/extensions/webidl-api").glob("*.cpp"))]
    for path in arguments.additional_native_source:
        path = path.resolve()
        try:
            directory = str(path.parent.relative_to(own))
        except ValueError:
            parser.error("Additional C++ sources must be under the selected source overlay")
        jobs.append((path, directory, "native"))
    bindings = {path.name: path for path in (obj / "dom/bindings").glob("Extension*Binding.cpp")}
    if arguments.binding_sources:
        bindings.update({path.name: path for path in arguments.binding_sources.resolve().glob("*Binding.cpp")})
    jobs.extend((path, "dom/bindings", "binding") for path in sorted(bindings.values()))
    if arguments.kind:
        jobs = [job for job in jobs if job[2] == arguments.kind]
    if arguments.match:
        selected = re.compile(arguments.match)
        jobs = [job for job in jobs if selected.search(job[0].name)]
    if not jobs:
        parser.error("No matching native or generated binding translation units")
    records = []
    with ThreadPoolExecutor(max_workers=arguments.jobs) as pool:
        pending = [pool.submit(check, *job) for job in jobs]
        for future in as_completed(pending):
            records.append(future.result())
            (output / "report.json").write_text(json.dumps({
                "passed": all(record["returnCode"] == 0 for record in records),
                "checks": sorted(records, key=lambda record: (record["kind"], record["source"])),
            }, indent=2))
    failures = sum(record["returnCode"] != 0 for record in records)
    print("CHECKS", len(records), "FAILURES", failures)
    print("REPORT", output / "report.json")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
