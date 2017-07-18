# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from filecmp import dircmp
import json
import os
import platform
import re
import subprocess
import sys
from distutils.version import LooseVersion
sys.path.append(os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "third_party", "python", "which"))
import which

NODE_MIN_VERSION = "6.9.1"
NPM_MIN_VERSION = "3.10.8"

NODE_MACHING_VERSION_NOT_FOUND_MESSAGE = """
nodejs is out of date. You currently have node v%s but v%s is required.
Please update nodejs from https://nodejs.org and try again.
""".strip()

NPM_MACHING_VERSION_NOT_FOUND_MESSAGE = """
npm is out of date. You currently have npm v%s but v%s is required.
You can usually update npm with:

npm i -g npm
""".strip()

NODE_NOT_FOUND_MESSAGE = """
nodejs is either not installed or is installed to a non-standard path.
Please install nodejs from https://nodejs.org and try again.

Valid installation paths:
""".strip()

NPM_NOT_FOUND_MESSAGE = """
Node Package Manager (npm) is either not installed or installed to a
non-standard path. Please install npm from https://nodejs.org (it comes as an
option in the node installation) and try again.

Valid installation paths:
""".strip()


VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
CARET_VERSION_RANGE_RE = re.compile(r"^\^((\d+)\.\d+\.\d+)$")

project_root = None


def eslint_setup():
    """Ensure eslint is optimally configured.

    This command will inspect your eslint configuration and
    guide you through an interactive wizard helping you configure
    eslint for optimal use on Mozilla projects.
    """
    orig_cwd = os.getcwd()
    sys.path.append(os.path.dirname(__file__))

    # npm sometimes fails to respect cwd when it is run using check_call so
    # we manually switch folders here instead.
    os.chdir(get_project_root())

    npm_path = get_node_or_npm_path("npm")
    if not npm_path:
        return 1

    extra_parameters = ["--loglevel=error"]

    # Install ESLint and external plugins
    cmd = [npm_path, "install"]
    cmd.extend(extra_parameters)
    print("Installing eslint for mach using \"%s\"..." % (" ".join(cmd)))
    if not call_process("eslint", cmd):
        return 1

    eslint_path = os.path.join(get_project_root(), "node_modules", ".bin", "eslint")

    print("\nESLint and approved plugins installed successfully!")
    print("\nNOTE: Your local eslint binary is at %s\n" % eslint_path)

    os.chdir(orig_cwd)


def call_process(name, cmd, cwd=None):
    try:
        with open(os.devnull, "w") as fnull:
            subprocess.check_call(cmd, cwd=cwd, stdout=fnull)
    except subprocess.CalledProcessError:
        if cwd:
            print("\nError installing %s in the %s folder, aborting." % (name, cwd))
        else:
            print("\nError installing %s, aborting." % name)

        return False

    return True


def expected_eslint_modules():
    # Read the expected version of ESLint and external modules
    expected_modules_path = os.path.join(get_project_root(), "package.json")
    with open(expected_modules_path, "r") as f:
        expected_modules = json.load(f)["dependencies"]

    # Also read the in-tree ESLint plugin mozilla information, to ensure the
    # dependencies are up to date.
    mozilla_json_path = os.path.join(get_eslint_module_path(),
                                     "eslint-plugin-mozilla", "package.json")
    with open(mozilla_json_path, "r") as f:
        expected_modules.update(json.load(f)["dependencies"])

    # Also read the in-tree ESLint plugin spidermonkey information, to ensure the
    # dependencies are up to date.
    mozilla_json_path = os.path.join(get_eslint_module_path(),
                                     "eslint-plugin-spidermonkey-js", "package.json")
    with open(mozilla_json_path, "r") as f:
        expected_modules.update(json.load(f)["dependencies"])

    return expected_modules


def check_eslint_files(node_modules_path, name):
    def check_file_diffs(dcmp):
        # Diff files only looks at files that are different. Not for files
        # that are only present on one side. This should be generally OK as
        # new files will need to be added in the index.js for the package.
        if dcmp.diff_files and dcmp.diff_files != ['package.json']:
            return True

        result = False

        # Again, we only look at common sub directories for the same reason
        # as above.
        for sub_dcmp in dcmp.subdirs.values():
            result = result or check_file_diffs(sub_dcmp)

        return result

    dcmp = dircmp(os.path.join(node_modules_path, name),
                  os.path.join(get_eslint_module_path(), name))

    return check_file_diffs(dcmp)


def eslint_module_needs_setup():
    has_issues = False
    node_modules_path = os.path.join(get_project_root(), "node_modules")

    for name, expected_data in expected_eslint_modules().iteritems():
        # expected_eslint_modules returns a string for the version number of
        # dependencies for installation of eslint generally, and an object
        # for our in-tree plugins (which contains the entire module info).
        if "version" in expected_data:
            version_range = expected_data["version"]
        else:
            version_range = expected_data

        path = os.path.join(node_modules_path, name, "package.json")

        if not os.path.exists(path):
            print("%s v%s needs to be installed locally." % (name, version_range))
            has_issues = True
            continue

        data = json.load(open(path))

        if version_range.startswith("file:"):
            # We don't need to check local file installations for versions, as
            # these are symlinked, so we'll always pick up the latest.
            continue

        if not version_in_range(data["version"], version_range):
            print("%s v%s should be v%s." % (name, data["version"], version_range))
            has_issues = True
            continue

    return has_issues


def version_in_range(version, version_range):
    """
    Check if a module version is inside a version range.  Only supports explicit versions and
    caret ranges for the moment, since that's all we've used so far.
    """
    if version == version_range:
        return True

    version_match = VERSION_RE.match(version)
    if not version_match:
        raise RuntimeError("mach eslint doesn't understand module version %s" % version)
    version = LooseVersion(version)

    # Caret ranges as specified by npm allow changes that do not modify the left-most non-zero
    # digit in the [major, minor, patch] tuple.  The code below assumes the major digit is
    # non-zero.
    range_match = CARET_VERSION_RANGE_RE.match(version_range)
    if range_match:
        range_version = range_match.group(1)
        range_major = int(range_match.group(2))

        range_min = LooseVersion(range_version)
        range_max = LooseVersion("%d.0.0" % (range_major + 1))

        return range_min <= version < range_max

    return False


def get_possible_node_paths_win():
    """
    Return possible nodejs paths on Windows.
    """
    if platform.system() != "Windows":
        return []

    return list({
        "%s\\nodejs" % os.environ.get("SystemDrive"),
        os.path.join(os.environ.get("ProgramFiles"), "nodejs"),
        os.path.join(os.environ.get("PROGRAMW6432"), "nodejs"),
        os.path.join(os.environ.get("PROGRAMFILES"), "nodejs")
    })


def simple_which(filename, path=None):
    exts = [".cmd", ".exe", ""] if platform.system() == "Windows" else [""]

    for ext in exts:
        try:
            return which.which(filename + ext, path)
        except which.WhichError:
            pass

    # If we got this far, we didn't find it with any of the extensions, so
    # just return.
    return None


def which_path(filename):
    """
    Return the nodejs or npm path.
    """
    # Look in the system path first.
    path = simple_which(filename)
    if path is not None:
        return path

    if platform.system() == "Windows":
        # If we didn't find it fallback to the non-system paths.
        path = simple_which(filename, get_possible_node_paths_win())
    elif filename == "node":
        path = simple_which("nodejs")

    return path


def get_node_or_npm_path(filename, minversion=None):
    node_or_npm_path = which_path(filename)

    if not node_or_npm_path:
        if filename in ('node', 'nodejs'):
            print(NODE_NOT_FOUND_MESSAGE)
        elif filename == "npm":
            print(NPM_NOT_FOUND_MESSAGE)

        if platform.system() == "Windows":
            app_paths = get_possible_node_paths_win()

            for p in app_paths:
                print("  - %s" % p)
        elif platform.system() == "Darwin":
            print("  - /usr/local/bin/{}".format(filename))
        elif platform.system() == "Linux":
            print("  - /usr/bin/{}".format(filename))

        return None

    if not minversion:
        return node_or_npm_path

    version_str = get_version(node_or_npm_path).lstrip('v')

    version = LooseVersion(version_str)

    if version > minversion:
        return node_or_npm_path

    if filename == "npm":
        print(NPM_MACHING_VERSION_NOT_FOUND_MESSAGE % (version_str.strip(), minversion))
    else:
        print(NODE_MACHING_VERSION_NOT_FOUND_MESSAGE % (version_str.strip(), minversion))

    return None


def get_version(path):
    try:
        version_str = subprocess.check_output([path, "--version"],
                                              stderr=subprocess.STDOUT)
        return version_str
    except (subprocess.CalledProcessError, OSError):
        return None


def set_project_root(root=None):
    """Sets the project root to the supplied path, or works out what the root
    is based on looking for 'mach'.

    Keyword arguments:
    root - (optional) The path to set the root to.
    """
    global project_root

    if root:
        project_root = root
        return

    file_found = False
    folder = os.getcwd()

    while (folder):
        if os.path.exists(os.path.join(folder, 'mach')):
            file_found = True
            break
        else:
            folder = os.path.dirname(folder)

    if file_found:
        project_root = os.path.abspath(folder)


def get_project_root():
    """Returns the absolute path to the root of the project, see set_project_root()
    for how this is determined.
    """
    global project_root

    if not project_root:
        set_project_root()

    return project_root


def get_eslint_module_path():
    return os.path.join(get_project_root(), "tools", "lint", "eslint")


def check_node_executables_valid():
    # eslint requires at least node 6.9.1
    node_path = get_node_or_npm_path("node", LooseVersion(NODE_MIN_VERSION))
    if not node_path:
        return False

    npm_path = get_node_or_npm_path("npm", LooseVersion(NPM_MIN_VERSION))
    if not npm_path:
        return False

    return True
