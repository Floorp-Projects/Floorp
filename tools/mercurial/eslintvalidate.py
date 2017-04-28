# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version.

import os
import re
import json
from subprocess import check_output, CalledProcessError

lintable = re.compile(r'.+\.(?:js|jsm|jsx|xml|html)$')
ignored = 'File ignored because of a matching ignore pattern. Use "--no-ignore" to override.'


def is_lintable(filename):
    return lintable.match(filename)


def display(ui, output):
    results = json.loads(output)
    for file in results:
        path = os.path.relpath(file["filePath"])
        for message in file["messages"]:
            if message["message"] == ignored:
                continue

            if "line" in message:
                ui.warn("%s:%d:%d %s\n" % (path, message["line"], message["column"],
                        message["message"]))
            else:
                ui.warn("%s: %s\n" % (path, message["message"]))


def eslinthook(ui, repo, node=None, **opts):
    ctx = repo[node]
    if len(ctx.parents()) > 1:
        return 0

    deleted = repo.status(ctx.p1().node(), ctx.node()).deleted
    files = [f for f in ctx.files() if f not in deleted and is_lintable(f)]

    if len(files) == 0:
        return

    try:
        basepath = get_project_root()

        if not basepath:
            return

        dir = os.path.join(basepath, "node_modules", ".bin")

        eslint_path = os.path.join(dir, "eslint")
        if os.path.exists(os.path.join(dir, "eslint.cmd")):
            eslint_path = os.path.join(dir, "eslint.cmd")
        output = check_output([eslint_path,
                               "--format", "json", "--plugin", "html"] + files,
                              cwd=basepath)
        display(ui, output)
    except CalledProcessError as ex:
        display(ui, ex.output)
        ui.warn("ESLint found problems in your changes, please correct them.\n")


def reposetup(ui, repo):
    ui.setconfig('hooks', 'commit.eslint', eslinthook)


def get_project_root():
    file_found = False
    folder = os.getcwd()

    while (folder):
        if os.path.exists(os.path.join(folder, 'mach')):
            file_found = True
            break
        else:
            folder = os.path.dirname(folder)

    if file_found:
        return os.path.abspath(folder)

    return None
