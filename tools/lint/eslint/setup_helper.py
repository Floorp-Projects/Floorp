# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import platform
import re
import subprocess
import sys
from filecmp import dircmp

from mozbuild.nodeutil import (
    NODE_MIN_VERSION,
    NPM_MIN_VERSION,
    find_node_executable,
    find_npm_executable,
)
from mozfile.mozfile import remove as mozfileremove
from packaging.version import Version

NODE_MACHING_VERSION_NOT_FOUND_MESSAGE = """
Could not find Node.js executable later than %s.

Executing `mach bootstrap --no-system-changes` should
install a compatible version in ~/.mozbuild on most platforms.
""".strip()

NPM_MACHING_VERSION_NOT_FOUND_MESSAGE = """
Could not find npm executable later than %s.

Executing `mach bootstrap --no-system-changes` should
install a compatible version in ~/.mozbuild on most platforms.
""".strip()

NODE_NOT_FOUND_MESSAGE = """
nodejs is either not installed or is installed to a non-standard path.

Executing `mach bootstrap --no-system-changes` should
install a compatible version in ~/.mozbuild on most platforms.
""".strip()

NPM_NOT_FOUND_MESSAGE = """
Node Package Manager (npm) is either not installed or installed to a
non-standard path.

Executing `mach bootstrap --no-system-changes` should
install a compatible version in ~/.mozbuild on most platforms.
""".strip()


VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
CARET_VERSION_RANGE_RE = re.compile(r"^\^((\d+)\.\d+\.\d+)$")

project_root = None


def eslint_maybe_setup():
    """Setup ESLint only if it is needed."""
    has_issues, needs_clobber = eslint_module_needs_setup()

    if has_issues:
        eslint_setup(needs_clobber)


def eslint_setup(should_clobber=False):
    """Ensure eslint is optimally configured.

    This command will inspect your eslint configuration and
    guide you through an interactive wizard helping you configure
    eslint for optimal use on Mozilla projects.
    """
    package_setup(get_project_root(), "eslint", should_clobber=should_clobber)


def remove_directory(path):
    print("Clobbering %s..." % path)
    if sys.platform.startswith("win") and have_winrm():
        process = subprocess.Popen(["winrm", "-rf", path])
        process.wait()
    else:
        mozfileremove(path)


def package_setup(
    package_root,
    package_name,
    should_update=False,
    should_clobber=False,
    no_optional=False,
):
    """Ensure `package_name` at `package_root` is installed.

    When `should_update` is true, clobber, install, and produce a new
    "package-lock.json" file.

    This populates `package_root/node_modules`.

    """
    orig_project_root = get_project_root()
    orig_cwd = os.getcwd()

    if should_update:
        should_clobber = True

    try:
        set_project_root(package_root)
        sys.path.append(os.path.dirname(__file__))

        # npm sometimes fails to respect cwd when it is run using check_call so
        # we manually switch folders here instead.
        project_root = get_project_root()
        os.chdir(project_root)

        if should_clobber:
            remove_directory(os.path.join(project_root, "node_modules"))

        # Always remove the eslint-plugin-mozilla sub-directory as that can
        # sometimes conflict with the top level node_modules, see bug 1809036.
        remove_directory(
            os.path.join(
                get_eslint_module_path(), "eslint-plugin-mozilla", "node_modules"
            )
        )

        npm_path, _ = find_npm_executable()
        if not npm_path:
            return 1

        node_path, _ = find_node_executable()
        if not node_path:
            return 1

        extra_parameters = ["--loglevel=error"]

        if no_optional:
            extra_parameters.append("--no-optional")

        package_lock_json_path = os.path.join(get_project_root(), "package-lock.json")

        if should_update:
            cmd = [npm_path, "install"]
            mozfileremove(package_lock_json_path)
        else:
            cmd = [npm_path, "ci"]

        # On non-Windows, ensure npm is called via node, as node may not be in the
        # path.
        if platform.system() != "Windows":
            cmd.insert(0, node_path)

        cmd.extend(extra_parameters)

        # Ensure that bare `node` and `npm` in scripts, including post-install scripts, finds the
        # binary we're invoking with.  Without this, it's easy for compiled extensions to get
        # mismatched versions of the Node.js extension API.
        path = os.environ.get("PATH", "").split(os.pathsep)
        node_dir = os.path.dirname(node_path)
        if node_dir not in path:
            path = [node_dir] + path

        print('Installing %s for mach using "%s"...' % (package_name, " ".join(cmd)))
        result = call_process(
            package_name, cmd, append_env={"PATH": os.pathsep.join(path)}
        )

        if not result:
            return 1

        bin_path = os.path.join(
            get_project_root(), "node_modules", ".bin", package_name
        )

        print("\n%s installed successfully!" % package_name)
        print("\nNOTE: Your local %s binary is at %s\n" % (package_name, bin_path))

    finally:
        set_project_root(orig_project_root)
        os.chdir(orig_cwd)


def call_process(name, cmd, cwd=None, append_env={}):
    env = dict(os.environ)
    env.update(append_env)

    try:
        with open(os.devnull, "w") as fnull:
            subprocess.check_call(cmd, cwd=cwd, stdout=fnull, env=env)
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
    with open(expected_modules_path, encoding="utf-8") as f:
        sections = json.load(f)
        expected_modules = sections.get("dependencies", {})
        expected_modules.update(sections.get("devDependencies", {}))

    # Also read the in-tree ESLint plugin mozilla information, to ensure the
    # dependencies are up to date.
    mozilla_json_path = os.path.join(
        get_eslint_module_path(), "eslint-plugin-mozilla", "package.json"
    )
    with open(mozilla_json_path, encoding="utf-8") as f:
        dependencies = json.load(f).get("dependencies", {})
        # Bug 1860508: We skip checking eslint-visitor-keys because the Babel
        # dependencies currently require an older version to that which
        # eslint-plugin-mozilla requires.
        dependencies.pop("eslint-visitor-keys")
        expected_modules.update(dependencies)

    # Also read the in-tree ESLint plugin spidermonkey information, to ensure the
    # dependencies are up to date.
    mozilla_json_path = os.path.join(
        get_eslint_module_path(), "eslint-plugin-spidermonkey-js", "package.json"
    )
    with open(mozilla_json_path, encoding="utf-8") as f:
        expected_modules.update(json.load(f).get("dependencies", {}))

    return expected_modules


def check_eslint_files(node_modules_path, name):
    def check_file_diffs(dcmp):
        # Diff files only looks at files that are different. Not for files
        # that are only present on one side. This should be generally OK as
        # new files will need to be added in the index.js for the package.
        if dcmp.diff_files and dcmp.diff_files != ["package.json"]:
            return True

        result = False

        # Again, we only look at common sub directories for the same reason
        # as above.
        for sub_dcmp in dcmp.subdirs.values():
            result = result or check_file_diffs(sub_dcmp)

        return result

    dcmp = dircmp(
        os.path.join(node_modules_path, name),
        os.path.join(get_eslint_module_path(), name),
    )

    return check_file_diffs(dcmp)


def eslint_module_needs_setup():
    has_issues = False
    needs_clobber = False
    node_modules_path = os.path.join(get_project_root(), "node_modules")

    for name, expected_data in expected_eslint_modules().items():
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
        data = json.load(open(path, encoding="utf-8"))

        if version_range.startswith("file:"):
            # We don't need to check local file installations for versions, as
            # these are symlinked, so we'll always pick up the latest.
            continue

        if name == "eslint" and Version("4.0.0") > Version(data["version"]):
            print("ESLint is an old version, clobbering node_modules directory")
            needs_clobber = True
            has_issues = True
            continue

        if not version_in_range(data["version"], version_range):
            print("%s v%s should be v%s." % (name, data["version"], version_range))
            has_issues = True
            continue

    return has_issues, needs_clobber


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
    version = Version(version)

    # Caret ranges as specified by npm allow changes that do not modify the left-most non-zero
    # digit in the [major, minor, patch] tuple.  The code below assumes the major digit is
    # non-zero.
    range_match = CARET_VERSION_RANGE_RE.match(version_range)
    if range_match:
        range_version = range_match.group(1)
        range_major = int(range_match.group(2))

        range_min = Version(range_version)
        range_max = Version("%d.0.0" % (range_major + 1))

        return range_min <= version < range_max

    return False


def get_possible_node_paths_win():
    """
    Return possible nodejs paths on Windows.
    """
    if platform.system() != "Windows":
        return []

    return list(
        {
            "%s\\nodejs" % os.environ.get("SystemDrive"),
            os.path.join(os.environ.get("ProgramFiles"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMW6432"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMFILES"), "nodejs"),
        }
    )


def get_version(path):
    try:
        version_str = subprocess.check_output(
            [path, "--version"], stderr=subprocess.STDOUT, universal_newlines=True
        )
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

    while folder:
        if os.path.exists(os.path.join(folder, "mach")):
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
    node_path, version = find_node_executable()
    if not node_path:
        print(NODE_NOT_FOUND_MESSAGE)
        return False
    if not version:
        print(NODE_MACHING_VERSION_NOT_FOUND_MESSAGE % NODE_MIN_VERSION)
        return False

    npm_path, version = find_npm_executable()
    if not npm_path:
        print(NPM_NOT_FOUND_MESSAGE)
        return False
    if not version:
        print(NPM_MACHING_VERSION_NOT_FOUND_MESSAGE % NPM_MIN_VERSION)
        return False

    return True


def have_winrm():
    # `winrm -h` should print 'winrm version ...' and exit 1
    try:
        p = subprocess.Popen(
            ["winrm.exe", "-h"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        return p.wait() == 1 and p.stdout.read().startswith("winrm")
    except Exception:
        return False
