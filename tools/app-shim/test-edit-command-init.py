#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Compile scoped binding regressions from the patch and fixed Gecko contract.

Only host C++ adapters execute. Cocoa, native dispatch, IME, and the browser do
not run. Git reads use the lock commit and prohibit lazy network materialization.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path, PurePosixPath


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    index = opening + 1
    quoted = None
    while depth:
        char = source[index]
        if quoted:
            if char == "\\":
                index += 1
            elif char == quoted:
                quoted = None
        elif source.startswith("//", index):
            index = source.index("\n", index)
        elif source.startswith("/*", index):
            index = source.index("*/", index) + 1
        elif char in ('"', "'"):
            quoted = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        index += 1
    return source[start:index], source[opening + 1 : index - 1]


def capture(source, pattern):
    found = re.search(pattern, source, re.DOTALL)
    if not found:
        raise ValueError(f"Missing source contract: {pattern}")
    return found.group(0)


def render(source, template):
    input_mm = source["widget/cocoa/appshim/MacWebAppInput.mm"]
    input_h = source["widget/cocoa/appshim/MacWebAppInput.h"]
    text_events = source["widget/TextEvents.h"]
    dispatcher = source["widget/TextEventDispatcher.cpp"]
    implementation = source["widget/WidgetEventImpl.cpp"]
    _, handle = block(input_mm, "void MacWebAppInput::Handle(")
    _, command = block(input_mm, 'if ([kind isEqualToString:@"textCommand"])')
    command = command[command.index("AutoRestore<Maybe<Command>>") :]
    command = command[: command.rindex("return;")]
    _, insert = block(input_mm, "} else if (mLastKeyEvent)")
    _, disconnect = block(input_mm, "void MacWebAppInput::OnShimDisconnected(")
    reset = disconnect[disconnect.index("mButtons =") :]
    reset = reset[: reset.index("[mLastKeyEvent release]")]
    suppression = re.search(r"bool mSuppressNativeKeyBindings = false;", input_h)
    snippets = {
        "PREVENT_BINDINGS": block(text_events, "void PreventNativeKeyBindings()")[0],
        "ALL_INITIALIZED": block(text_events, "bool AreAllEditCommandsInitialized()")[
            0
        ],
        "ASSIGN_INITIALIZED_FLAGS": capture(
            text_events,
            r"mEditCommandsForSingleLineEditorInitialized =\s*"
            r"aEvent\.mEditCommandsForSingleLineEditorInitialized;.*?"
            r"aEvent\.mEditCommandsForRichTextEditorInitialized;",
        ),
        "DISPATCH_COPY": capture(
            dispatcher,
            r"WidgetKeyboardEvent keyEvent\(true, aMessage, mWidget\);\s*"
            r"InitEvent\(keyEvent\);\s*"
            r"keyEvent\.AssignKeyEventData\(aKeyboardEvent, false\);",
        ),
        "DISPATCH_INITIALIZE": block(
            dispatcher, "if (XRE_IsParentProcess() && mHasFocus &&"
        )[0],
        "COMMAND_FIELD": capture(input_h, r"Maybe<Command> mCommand;"),
        "SUPPRESSION_FIELD": (
            suppression.group(0)
            if suppression
            else "bool mSuppressNativeKeyBindings = false;"
        ),
        "HANDLE_SCOPE": handle[
            handle.index("RefPtr<MacWebAppInput>") : handle.index("NSString* kind")
        ],
        "TEXT_COMMAND": command,
        "INSERT_TEXT": insert,
        "DISCONNECT_RESET": reset,
        "DESTROY_RESET": capture(
            block(input_mm, "void MacWebAppInput::OnDestroy(")[1],
            r"mWidget = nullptr;",
        ),
        "WIDGET_FORWARD": block(
            source["widget/cocoa/appshim/MacWebAppWidget.mm"],
            "bool MacWebAppWidget::GetEditCommands(",
        )[0],
        "IS_ALIVE": block(input_mm, "bool MacWebAppInput::IsAlive()")[0],
        "GET_EDIT_COMMANDS": block(input_mm, "bool MacWebAppInput::GetEditCommands(")[
            0
        ],
        "INIT_ALL_COMMANDS": block(
            implementation, "void WidgetKeyboardEvent::InitAllEditCommands("
        )[0],
        "INIT_COMMANDS_FOR": block(
            implementation, "bool WidgetKeyboardEvent::InitEditCommandsFor("
        )[0],
        "MAYBE_KEYPRESS": block(
            dispatcher, "bool TextEventDispatcher::MaybeDispatchKeypressEvents("
        )[0],
    }
    for name, snippet in snippets.items():
        template = template.replace(f"// @@{name}@@", snippet)
    if "@@" in template:
        raise ValueError("Unfilled source marker")
    return (
        template,
        {
            name: {"bytes": len(code.encode()), "sha256": sha256(code.encode())}
            for name, code in snippets.items()
        },
        suppression is None,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-repo", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--patch", type=Path)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    floorp = Path(__file__).resolve().parents[2]
    commit = json.loads((floorp / "floorp-runtime.lock.json").read_text())["source"][
        "commit"
    ]
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Lock source must be an exact commit")
    patch = args.patch or (
        floorp / ".github/patches/floorp-runtime/common/"
        "macos-app-shim-pwa-input-and-compositor.patch"
    )
    patch = patch.resolve()
    pairs = re.findall(r"^diff --git a/(\S+) b/(\S+)$", patch.read_text(), re.MULTILINE)
    paths = []
    for before, after in pairs:
        if (
            before != after
            or PurePosixPath(after).is_absolute()
            or ".." in PurePosixPath(after).parts
            or "\\" in after
            or after in paths
        ):
            raise ValueError(f"Unsafe or duplicate patch path: {after}")
        paths.append(after)
    if not paths:
        raise ValueError("Empty patch")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixture = output / "source"
    fixture.mkdir()
    commands = []

    def run(command, cwd, *, check=True):
        result = subprocess.run(
            command,
            cwd=cwd,
            env=os.environ | {"GIT_NO_LAZY_FETCH": "1"},
            capture_output=True,
            timeout=60,
            check=False,
        )
        commands.append(
            {
                "command": [str(arg) for arg in command],
                "cwd": str(cwd),
                "exitCode": result.returncode,
                "stdoutBytes": len(result.stdout),
                "stdoutSha256": sha256(result.stdout),
                "stderr": result.stderr.decode(errors="replace"),
            }
        )
        (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if check and result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        return result

    contracts = [
        "widget/TextEvents.h",
        "widget/TextEventDispatcher.cpp",
        "widget/WidgetEventImpl.cpp",
        "xpcom/base/AutoRestore.h",
    ]
    materials = []
    for path in sorted(set(paths + contracts)):
        tree = run(["git", "ls-tree", commit, "--", path], args.runtime_repo)
        if not tree.stdout:
            materials.append({"path": path, "beforeAbsent": True})
            continue
        data = run(["git", "show", f"{commit}:{path}"], args.runtime_repo).stdout
        target = fixture / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        materials.append(
            {
                "path": path,
                "blob": tree.stdout.decode().split()[2],
                "bytes": len(data),
                "sha256": sha256(data),
            }
        )
    run(["git", "init", "-q"], fixture)
    run(["git", "add", "."], fixture)
    run(["git", "apply", "--check", "--index", str(patch)], fixture)
    run(["git", "apply", "--index", str(patch)], fixture)
    run(["git", "apply", "--reverse", "--check", "--index", str(patch)], fixture)
    source = {path: (fixture / path).read_text() for path in paths + contracts}
    generated, snippets, baseline_flag = render(
        source, Path(__file__).with_suffix(".cpp").read_text()
    )
    cpp = output / "host-contract.cpp"
    cpp.write_text(generated)
    (output / "AutoRestore.h").write_text(source["xpcom/base/AutoRestore.h"])
    (output / "mozilla").mkdir()
    (output / "mozilla/Attributes.h").write_text("#define MOZ_RAII\n")
    executable = output / "host-contract"
    version = run([args.compiler, "--version"], output).stdout.decode().splitlines()[0]
    run(
        [
            args.compiler,
            "-std=c++17",
            "-DXP_MACOSX",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-I",
            str(output),
            str(cpp),
            "-o",
            str(executable),
        ],
        output,
    )
    result = run([str(executable)], output, check=False)
    (output / "host-tests.log").write_bytes(result.stdout + result.stderr)
    report = {
        "sourceCommit": commit,
        "patchSha256": sha256(patch.read_bytes()),
        "materials": materials,
        "postimages": {path: sha256((fixture / path).read_bytes()) for path in paths},
        "actualSnippets": snippets,
        "generatedCppSha256": sha256(cpp.read_bytes()),
        "compiler": version,
        "hostTestExitCode": result.returncode,
        "baselineNeutralShadowFlag": baseline_flag,
        "limitations": [
            "Cocoa/native browser/IME bodies do not compile or execute",
            "Focused parent dispatcher contract fragments execute in host adapters",
            "XP_MACOSX selects the macOS contract with stub NativeKeyBindings",
            "Native platform and payload mapping are stubs; RefPtr retention is not modeled",
            "Disconnect reentry uses a synthetic lifecycle callback, not the Cocoa IME body",
            "Assertion uses a host exception so all regression failures can be recorded",
        ],
        "nativeQualification": False,
        "bootstrapOrNetworkOrBrowserLaunch": False,
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(result.stdout.decode(), end="")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
