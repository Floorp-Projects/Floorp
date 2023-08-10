#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import pathlib
import re
import subprocess
import sys

TBPL_FAILURE = 2

excluded_files = [
    # Testcase for loader.
    "js/xpconnect/tests/chrome/file_expandosharing.jsm",
    "js/xpconnect/tests/unit/environment_script.js",
    "js/xpconnect/tests/unit/bogus_element_type.jsm",
    "js/xpconnect/tests/unit/bogus_exports_type.jsm",
    "js/xpconnect/tests/unit/envChain.jsm",
    "js/xpconnect/tests/unit/envChain_subscript.jsm",
    "js/xpconnect/tests/unit/environment_checkscript.jsm",
    "js/xpconnect/tests/unit/environment_loadscript.jsm",
    "js/xpconnect/tests/unit/import_stack.jsm",
    "js/xpconnect/tests/unit/importer.jsm",
    "js/xpconnect/tests/unit/jsm_loaded-1.jsm",
    "js/xpconnect/tests/unit/jsm_loaded-2.jsm",
    "js/xpconnect/tests/unit/jsm_loaded-3.jsm",
    "js/xpconnect/tests/unit/not-esmified-not-exported.jsm",
    "js/xpconnect/tests/unit/recursive_importA.jsm",
    "js/xpconnect/tests/unit/recursive_importB.jsm",
    "js/xpconnect/tests/unit/ReturnCodeChild.jsm",
    "js/xpconnect/tests/unit/syntax_error.jsm",
    "js/xpconnect/tests/unit/TestBlob.jsm",
    "js/xpconnect/tests/unit/TestFile.jsm",
    "js/xpconnect/tests/unit/uninitialized_lexical.jsm",
    "dom/url/tests/file_url.jsm",
    "dom/url/tests/file_worker_url.jsm",
    "dom/url/tests/test_bug883784.jsm",
    "dom/workers/test/WorkerTest.jsm",
    "dom/encoding/test/file_stringencoding.jsm",
    "remote/shared/messagehandler/test/browser/resources/modules/root/invalid.jsm",
    "toolkit/actors/TestProcessActorChild.jsm",
    "toolkit/actors/TestProcessActorParent.jsm",
    "toolkit/actors/TestWindowChild.jsm",
    "toolkit/actors/TestWindowParent.jsm",
    # Testcase for build system.
    "python/mozbuild/mozbuild/test/backend/data/build/bar.jsm",
    "python/mozbuild/mozbuild/test/backend/data/build/baz.jsm",
    "python/mozbuild/mozbuild/test/backend/data/build/foo.jsm",
    "python/mozbuild/mozbuild/test/backend/data/build/qux.jsm",
    # EXPORTED_SYMBOLS inside testcase.
    "tools/lint/eslint/eslint-plugin-mozilla/tests/mark-exported-symbols-as-used.js",
]

if pathlib.Path(".hg").exists():
    mode = "hg"
elif pathlib.Path(".git").exists():
    mode = "git"
else:
    print(
        "Error: This script needs to be run inside mozilla-central checkout "
        "of either mercurial or git.",
        file=sys.stderr,
    )
    sys.exit(TBPL_FAILURE)


def new_files_struct():
    return {
        "jsm": [],
        "esm": [],
        "subdir": {},
    }


def put_file(files, kind, path):
    """Put a path into files tree structure."""

    if str(path) in excluded_files:
        return

    name = path.name

    current_files = files
    for part in path.parent.parts:
        if part not in current_files["subdir"]:
            current_files["subdir"][part] = new_files_struct()
        current_files = current_files["subdir"][part]

    current_files[kind].append(name)


def run(cmd):
    """Run command and return output as lines, excluding empty line."""
    lines = subprocess.run(cmd, stdout=subprocess.PIPE).stdout.decode()
    return filter(lambda x: x != "", lines.split("\n"))


def collect_jsm(files):
    """Collect JSM files."""
    kind = "jsm"

    # jsm files
    if mode == "hg":
        cmd = ["hg", "files", "set:glob:**/*.jsm"]
    else:
        cmd = ["git", "ls-files", "*.jsm"]
    for line in run(cmd):
        put_file(files, kind, pathlib.Path(line))

    # js files with EXPORTED_SYMBOLS
    if mode == "hg":
        cmd = ["hg", "files", "set:grep('EXPORTED_SYMBOLS = \[') and glob:**/*.js"]
        for line in run(cmd):
            put_file(files, kind, pathlib.Path(line))
    else:
        handled = {}
        cmd = ["git", "grep", "EXPORTED_SYMBOLS = \[", "*.js"]
        for line in run(cmd):
            m = re.search("^([^:]+):", line)
            if not m:
                continue
            path = m.group(1)
            if path in handled:
                continue
            handled[path] = True
            put_file(files, kind, pathlib.Path(path))


def collect_esm(files):
    """Collect system ESM files."""
    kind = "esm"

    # sys.mjs files
    if mode == "hg":
        cmd = ["hg", "files", "set:glob:**/*.sys.mjs"]
    else:
        cmd = ["git", "ls-files", "*.sys.mjs"]
    for line in run(cmd):
        put_file(files, kind, pathlib.Path(line))


def to_stat(files):
    """Convert files tree into status tree."""
    jsm = len(files["jsm"])
    esm = len(files["esm"])
    subdir = {}

    for key, sub_files in files["subdir"].items():
        sub_stat = to_stat(sub_files)

        subdir[key] = sub_stat
        jsm += sub_stat["jsm"]
        esm += sub_stat["esm"]

    stat = {
        "jsm": jsm,
        "esm": esm,
    }
    if len(subdir):
        stat["subdir"] = subdir

    return stat


if mode == "hg":
    cmd = ["hg", "parent", "--template", "{node}"]
    commit_hash = list(run(cmd))[0]

    cmd = ["hg", "parent", "--template", "{date|shortdate}"]
    date = list(run(cmd))[0]
else:
    cmd = ["git", "log", "-1", "--pretty=%H"]
    git_hash = list(run(cmd))[0]
    cmd = ["git", "cinnabar", "git2hg", git_hash]
    commit_hash = list(run(cmd))[0]

    cmd = ["git", "log", "-1", "--pretty=%cs"]
    date = list(run(cmd))[0]

files = new_files_struct()
collect_jsm(files)
collect_esm(files)

stat = to_stat(files)
stat["hash"] = commit_hash
stat["date"] = date

print(json.dumps(stat, indent=2))
