# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version.

# This file provides helper functions for the repository hooks for git/hg.

import os
import re
import json
from subprocess import check_output, CalledProcessError

lintable = re.compile(r'.+\.(?:js|jsm|jsx|xml|html)$')
ignored = 'File ignored because of a matching ignore pattern. Use "--no-ignore" to override.'


def is_lintable(filename):
    """Determine if a file is lintable based on the file extension.

    Keyword arguments:
    filename -- the file to check.
    """
    return lintable.match(filename)


def display(print_func, output_json):
    """Formats an ESLint result into a human readable message.

    Keyword arguments:
    print_func -- A function to call to print the output.
    output_json -- the json ESLint results to format.
    """
    results = json.loads(output_json)
    for file in results:
        path = os.path.relpath(file["filePath"])
        for message in file["messages"]:
            if message["message"] == ignored:
                continue

            if "line" in message:
                print_func("%s:%d:%d %s\n" % (path, message["line"], message["column"],
                           message["message"]))
            else:
                print_func("%s: %s\n" % (path, message["message"]))


def runESLint(print_func, files):
    """Runs ESLint on the files that are passed.

    Keyword arguments:
    print_func -- A function to call to print the output.
    files -- A list of files to be checked.
    """
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
        display(print_func, output)
        return True
    except CalledProcessError as ex:
        display(print_func, ex.output)
        return False


def get_project_root():
    """Returns the absolute path to the root of the project, by looking for the
    mach executable.
    """
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
