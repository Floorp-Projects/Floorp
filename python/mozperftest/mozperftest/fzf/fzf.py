# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys
import subprocess
from pathlib import Path
import json

from mozterm import Terminal
from mozboot.util import get_state_dir
from mozbuild.util import ensure_subprocess_env
from distutils.spawn import find_executable


HERE = Path(__file__).parent.resolve()
SRC_ROOT = (HERE / ".." / ".." / ".." / "..").resolve()
PREVIEW_SCRIPT = HERE / "preview.py"
FZF_HEADER = """
Please select a performance test to execute.
{shortcuts}
""".strip()

fzf_shortcuts = {
    "ctrl-t": "toggle-all",
    "alt-bspace": "beginning-of-line+kill-line",
    "?": "toggle-preview",
}

fzf_header_shortcuts = [
    ("select", "tab"),
    ("accept", "enter"),
    ("cancel", "ctrl-c"),
    ("cursor-up", "up"),
    ("cursor-down", "down"),
]


def run_fzf(cmd, tasks):
    env = dict(os.environ)
    env.update(
        {"PYTHONPATH": os.pathsep.join([p for p in sys.path if "requests" in p])}
    )
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stdin=subprocess.PIPE,
        env=ensure_subprocess_env(env),
        universal_newlines=True,
    )
    out = proc.communicate("\n".join(tasks))[0].splitlines()
    selected = []
    query = None
    if out:
        query = out[0]
        selected = out[1:]
    return query, selected


def format_header():
    terminal = Terminal()
    shortcuts = []
    for action, key in fzf_header_shortcuts:
        shortcuts.append(
            "{t.white}{action}{t.normal}: {t.yellow}<{key}>{t.normal}".format(
                t=terminal, action=action, key=key
            )
        )
    return FZF_HEADER.format(shortcuts=", ".join(shortcuts), t=terminal)


def select(test_objects):
    mozbuild_dir = Path(Path.home(), ".mozbuild")
    os.makedirs(str(mozbuild_dir), exist_ok=True)
    cache_file = Path(mozbuild_dir, ".perftestfuzzy")

    with cache_file.open("w") as f:
        f.write(json.dumps(test_objects))

    def _display(task):
        from mozperftest.script import ScriptInfo

        path = Path(task["path"])
        script_info = ScriptInfo(str(path))
        flavor = script_info.script_type.name
        if flavor == "browsertime":
            flavor = "bt"
        tags = script_info.get("tags", [])

        location = str(path.parent).replace(str(SRC_ROOT), "").strip("/")
        if len(tags) > 0:
            return f"[{flavor}][{','.join(tags)}] {path.name} in {location}"
        return f"[{flavor}] {path.name} in {location}"

    candidate_tasks = [_display(t) for t in test_objects]
    fzf_bin = find_executable("fzf", str(Path(get_state_dir(), "fzf", "bin")))
    key_shortcuts = [k + ":" + v for k, v in fzf_shortcuts.items()]

    base_cmd = [
        fzf_bin,
        "-m",
        "--bind",
        ",".join(key_shortcuts),
        "--header",
        format_header(),
        "--preview-window=right:50%",
        "--print-query",
        "--preview",
        sys.executable + ' {} -t "{{+f}}"'.format(str(PREVIEW_SCRIPT)),
    ]
    query_str, tasks = run_fzf(base_cmd, sorted(candidate_tasks))
    return tasks
