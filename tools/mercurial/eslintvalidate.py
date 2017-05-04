# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "lint", "eslint"))
from hook_helper import is_lintable, runESLint


def eslinthook(ui, repo, node=None, **opts):
    ctx = repo[node]
    if len(ctx.parents()) > 1:
        return 0

    deleted = repo.status(ctx.p1().node(), ctx.node()).deleted
    files = [f for f in ctx.files() if f not in deleted and is_lintable(f)]

    if len(files) == 0:
        return

    if not runESLint(ui.warn, files):
        ui.warn("Note: ESLint failed, but the commit will still happen. "
                "Please fix before pushing.\n")


def reposetup(ui, repo):
    ui.setconfig('hooks', 'commit.eslint', eslinthook)
