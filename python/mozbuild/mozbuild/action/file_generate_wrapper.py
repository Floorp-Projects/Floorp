# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import subprocess
from pathlib import Path

import buildconfig


def action(fh, script, target_dir, *args):
    fh.close()
    os.unlink(fh.name)

    args = list(args)
    objdir = Path.cwd()
    topsrcdir = Path(buildconfig.topsrcdir)

    def make_absolute(base_path, p):
        return base_path / p[1:] if p.startswith("/") else base_path / p

    try:
        abs_target_dir = make_absolute(objdir, target_dir)
        abs_script = make_absolute(topsrcdir, script)
        subprocess.check_call([abs_script] + args, cwd=abs_target_dir)
    except Exception:
        relative = os.path.relpath(__file__, topsrcdir)
        print(
            "%s:action caught exception. params=%s\n"
            % (relative, json.dumps([script, target_dir] + args, indent=2))
        )
        raise
