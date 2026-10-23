#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Validate the native workspace protocol against the locked Runtime source.

Uses an isolated source fixture by default. --apply is only for an intentional
source-build checkout, never for an extracted/prebuilt Runtime artifact.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / ".github/patches/floorp-runtime/common/workspace-trackpad-input.patch"


def run(command: list[str], *, cwd: Path, **options: object) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, check=True, **options)


def patch_paths(patch: str) -> tuple[list[str], set[str]]:
    paths: list[str] = []
    new_files: set[str] = set()
    current = ""
    for line in patch.splitlines():
        if line.startswith("diff --git a/"):
            left, right = line.removeprefix("diff --git a/").split(" b/", 1)
            path = PurePosixPath(right)
            if left != right or path.is_absolute() or ".." in path.parts:
                raise ValueError(f"Unexpected native patch path: {line}")
            current = right
            paths.append(current)
        elif line.startswith("new file mode "):
            new_files.add(current)
    if not paths:
        raise ValueError("Native patch is empty")
    return paths, new_files


def validate_webidl(runtime: Path, fixture: Path, output: Path) -> None:
    sys.path[:0] = [
        str(runtime / "dom/bindings/parser"),
        str(runtime / "third_party/python/ply"),
    ]
    import WebIDL

    parser = WebIDL.Parser(outputdir=str(output))
    # Resolve the unchanged external base types while checking the complete
    # actual SimpleGestureEvent declaration with the pinned Mozilla parser.
    parser.parse(
        "[Global=Window,Exposed=Window] interface Window {}; "
        "[Exposed=Window] interface EventTarget {}; "
        "[Exposed=Window] interface MouseEvent {};"
    )
    parser.parse((fixture / "dom/webidl/SimpleGestureEvent.webidl").read_text())
    parser.finish()


def validate_state_machine(fixture: Path, output: Path, compiler: str) -> None:
    test = output / "workspace-swipe-test.cpp"
    test.write_text(
        r'''
#include <cassert>
#include <limits>
#include "widget/WorkspaceSwipeGesture.h"
using mozilla::widget::WorkspaceSwipeGestureState;
using mozilla::widget::WorkspaceSwipePhase;
int main() {
  using Phase = WorkspaceSwipePhase;
  WorkspaceSwipeGestureState state;
  assert(!state.Update(0, Phase::Start, 0, 0));
  assert(!state.Update(2, Phase::Start, 0, 0));
  assert(!state.Update(4, Phase::Start, 0, 0));
  assert(!state.Update(3, Phase::End, 100, 0));
  assert(state.Update(3, Phase::Start, 0, 0));
  auto id = state.OperationId();
  assert(id != 0);
  assert(state.Update(3, Phase::Update, -40, 3));
  assert(state.Update(3, Phase::Update, -60, -1));
  assert(state.DeltaX() == -100 && state.DeltaY() == 2);
  assert(state.Update(3, Phase::End, 0, 0));
  assert(state.OperationId() == id && state.Phase() == Phase::End);
  assert(!state.Update(3, Phase::End, 0, 0));
  assert(!state.Update(3, Phase::Update, -50, 0));
  assert(state.Update(3, Phase::Start, 0, 0));
  assert(state.OperationId() != id && state.DeltaX() == 0);
  assert(state.Update(4, Phase::Update, 100, 0));
  assert(state.Phase() == Phase::Cancel);
  assert(!state.Update(3, Phase::End, 0, 0));
  assert(state.Update(3, Phase::Start, 0, 0));
  assert(state.Update(3, Phase::Update,
                      std::numeric_limits<double>::infinity(), 0));
  assert(state.Phase() == Phase::Cancel && std::isfinite(state.DeltaX()));
  assert(state.Update(3, Phase::Start, 0, 0));
  assert(state.Update(3, static_cast<Phase>(100), 1, 0));
  assert(state.Phase() == Phase::Cancel);
  assert(state.Update(3, Phase::Start, 0, 0));
  state.Reset();
  assert(!state.Update(3, Phase::End, 100, 0));
}
'''
    )
    executable = output / "workspace-swipe-test"
    run(
        [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-I",
            str(fixture),
            str(test),
            "-o",
            str(executable),
        ],
        cwd=output,
    )
    run([str(executable)], cwd=output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    runtime = arguments.runtime_dir.resolve()
    lock = json.loads((ROOT / "floorp-runtime.lock.json").read_text())
    revision = lock["source"]["commit"]
    actual = run(
        ["git", "rev-parse", "HEAD"], cwd=runtime, capture_output=True, text=True
    ).stdout.strip()
    if actual != revision:
        raise ValueError(f"Runtime HEAD {actual} is not the locked source {revision}")
    compiler = shutil.which(arguments.compiler)
    if not compiler:
        raise ValueError(f"C++ compiler is unavailable: {arguments.compiler}")
    paths, new_files = patch_paths(PATCH.read_text())
    with tempfile.TemporaryDirectory(prefix="floorp-native-workspace-") as temporary:
        output = Path(temporary)
        fixture = output / "source"
        fixture.mkdir()
        for path in paths:
            if path in new_files:
                continue
            content = run(
                ["git", "show", f"{revision}:{path}"], cwd=runtime,
                capture_output=True,
            ).stdout
            target = fixture / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        run(["git", "apply", "--check", str(PATCH)], cwd=fixture)
        run(["git", "apply", str(PATCH)], cwd=fixture)
        validate_webidl(runtime, fixture, output)
        validate_state_machine(fixture, output, compiler)
    if arguments.apply:
        run(["git", "apply", "--check", str(PATCH)], cwd=runtime)
        run(["git", "apply", str(PATCH)], cwd=runtime)
    report = {
        "schemaVersion": 1,
        "runtimeSourceCommit": revision,
        "patchSha256": hashlib.sha256(PATCH.read_bytes()).hexdigest(),
        "protocolVersion": 1,
        "nativeBuildRequired": True,
        "nativeBuildCompleted": False,
        "physicalDeviceVerified": False,
        "validated": ["pinned-source-patch", "webidl", "native-state-machine"],
    }
    if arguments.report:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
