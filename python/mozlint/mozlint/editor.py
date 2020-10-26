# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import tempfile

from mozlint import formatters


def get_editor():
    return os.environ.get("EDITOR")


def edit_issues(result):
    if not result.issues:
        return

    editor = get_editor()
    if not editor:
        print("warning: could not find a default editor")
        return

    name = os.path.basename(editor)
    if name in ("vim", "nvim", "gvim"):
        cmd = [
            editor,
            # need errorformat to match both Error and Warning, with or without a column
            "--cmd",
            "set errorformat+=%f:\\ line\\ %l\\\\,\\ col\\ %c\\\\,\\ %trror\\ -\\ %m",
            "--cmd",
            "set errorformat+=%f:\\ line\\ %l\\\\,\\ col\\ %c\\\\,\\ %tarning\\ -\\ %m",
            "--cmd",
            "set errorformat+=%f:\\ line\\ %l\\\\,\\ %trror\\ -\\ %m",
            "--cmd",
            "set errorformat+=%f:\\ line\\ %l\\\\,\\ %tarning\\ -\\ %m",
            # start with quickfix window opened
            "-c",
            "copen",
            # running with -q seems to open an empty buffer in addition to the
            # first file, this removes that empty buffer
            "-c",
            "1bd",
        ]

        with tempfile.NamedTemporaryFile(mode="w") as fh:
            s = formatters.get("compact", summary=False)(result)
            fh.write(s)
            fh.flush()

            cmd.extend(["-q", fh.name])
            subprocess.call(cmd)

    else:
        for path, errors in result.issues.items():
            subprocess.call([editor, path])
