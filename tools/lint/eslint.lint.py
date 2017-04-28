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
import signal
import subprocess
import sys
from distutils.version import LooseVersion

import which
from mozprocess import ProcessHandler

from mozlint import result

ESLINT_ERROR_MESSAGE = """
An error occurred running eslint. Please check the following error messages:

{}
""".strip()

ESLINT_NOT_FOUND_MESSAGE = """
Could not find eslint!  We looked at the --binary option, at the ESLINT
environment variable, and then at your local node_modules path. Please Install
eslint and needed plugins with:

mach eslint --setup

and try again.
""".strip()

NODE_NOT_FOUND_MESSAGE = """
nodejs v6.9.1 is either not installed or is installed to a non-standard path.
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

EXTENSIONS = ['.js', '.jsm', '.jsx', '.xml', '.html', '.xhtml']

project_root = None


def eslint_setup():
    """Ensure eslint is optimally configured.

    This command will inspect your eslint configuration and
    guide you through an interactive wizard helping you configure
    eslint for optimal use on Mozilla projects.
    """
    orig_cwd = os.getcwd()
    sys.path.append(os.path.dirname(__file__))

    module_path = get_eslint_module_path()

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

    # Install in-tree ESLint plugin mozilla.
    cmd = [npm_path, "install",
           os.path.join(module_path, "eslint-plugin-mozilla")]
    cmd.extend(extra_parameters)
    print("Installing eslint-plugin-mozilla using \"%s\"..." % (" ".join(cmd)))
    if not call_process("eslint-plugin-mozilla", cmd):
        return 1

    # Install in-tree ESLint plugin spidermonkey.
    cmd = [npm_path, "install",
           os.path.join(module_path, "eslint-plugin-spidermonkey-js")]
    cmd.extend(extra_parameters)
    print("Installing eslint-plugin-spidermonkey-js using \"%s\"..." % (" ".join(cmd)))
    if not call_process("eslint-plugin-spidermonkey-js", cmd):
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

    # Also read the in-tree ESLint plugin mozilla information.
    mozilla_json_path = os.path.join(get_eslint_module_path(),
                                     "eslint-plugin-mozilla", "package.json")
    with open(mozilla_json_path, "r") as f:
        expected_modules["eslint-plugin-mozilla"] = json.load(f)

    # Also read the in-tree ESLint plugin spidermonkey information.
    mozilla_json_path = os.path.join(get_eslint_module_path(),
                                     "eslint-plugin-spidermonkey-js", "package.json")
    with open(mozilla_json_path, "r") as f:
        expected_modules["eslint-plugin-spidermonkey-js"] = json.load(f)

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


def eslint_module_has_issues():
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

        if not version_in_range(data["version"], version_range):
            print("%s v%s should be v%s." % (name, data["version"], version_range))
            has_issues = True
            continue

        if name == "eslint-plugin-mozilla" or name == "eslint-plugin-spidermonkey-js":
            # For our in-tree modules, check that package.json has the same dependencies.
            if (cmp(data["dependencies"], expected_data["dependencies"]) != 0 or
                cmp(data["devDependencies"], expected_data["devDependencies"]) != 0):
                print("Dependencies of %s differ." % (name))
                has_issues = True
                continue

            # We also need to check the files themselves in case one changed without
            # the version number being updated.
            if check_eslint_files(node_modules_path, name):
                print("%s has out of-date files." % (name))
                has_issues = True

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


def get_node_or_npm_path(filename, minversion=None):
    """
    Return the nodejs or npm path.
    """
    if platform.system() == "Windows":
        for ext in [".cmd", ".exe", ""]:
            try:
                node_or_npm_path = which.which(filename + ext,
                                               path=get_possible_node_paths_win())
                if is_valid(node_or_npm_path, minversion):
                    return node_or_npm_path
            except which.WhichError:
                pass
    else:
        try:
            node_or_npm_path = which.which(filename)
            if is_valid(node_or_npm_path, minversion):
                return node_or_npm_path
        except which.WhichError:
            if filename == "node":
                # Retry it with "nodejs" as Linux tends to prefer nodejs rather than node.
                return get_node_or_npm_path("nodejs", minversion)

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


def is_valid(path, minversion=None):
    try:
        version_str = subprocess.check_output([path, "--version"],
                                              stderr=subprocess.STDOUT)
        if minversion:
            # nodejs prefixes its version strings with "v"
            version = LooseVersion(version_str.lstrip('v'))
            return version >= minversion
        return True
    except (subprocess.CalledProcessError, OSError):
        return False


def get_project_root():
    global project_root
    return project_root


def get_eslint_module_path():
    return os.path.join(get_project_root(), "tools", "lint", "eslint")


def lint(paths, binary=None, fix=None, setup=None, **lintargs):
    """Run eslint."""
    global project_root
    project_root = lintargs['root']

    module_path = get_project_root()

    # eslint requires at least node 6.9.1
    node_path = get_node_or_npm_path("node", LooseVersion("6.9.1"))
    if not node_path:
        return 1

    if setup:
        return eslint_setup()

    npm_path = get_node_or_npm_path("npm")
    if not npm_path:
        return 1

    if eslint_module_has_issues():
        eslint_setup()

    # Valid binaries are:
    #  - Any provided by the binary argument.
    #  - Any pointed at by the ESLINT environmental variable.
    #  - Those provided by mach eslint --setup.
    #
    #  eslint --setup installs some mozilla specific plugins and installs
    #  all node modules locally. This is the preferred method of
    #  installation.

    if not binary:
        binary = os.environ.get('ESLINT', None)

        if not binary:
            binary = os.path.join(module_path, "node_modules", ".bin", "eslint")
            if not os.path.isfile(binary):
                binary = None

    if not binary:
        print(ESLINT_NOT_FOUND_MESSAGE)
        return 1

    extra_args = lintargs.get('extra_args') or []
    cmd_args = [binary,
                # Enable the HTML plugin.
                # We can't currently enable this in the global config file
                # because it has bad interactions with the SublimeText
                # ESLint plugin (bug 1229874).
                '--plugin', 'html',
                # This keeps ext as a single argument.
                '--ext', '[{}]'.format(','.join(EXTENSIONS)),
                '--format', 'json',
                ] + extra_args + paths

    # eslint requires that --fix be set before the --ext argument.
    if fix:
        cmd_args.insert(1, '--fix')

    shell = False
    if os.environ.get('MSYSTEM') in ('MINGW32', 'MINGW64'):
        # The eslint binary needs to be run from a shell with msys
        shell = True

    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = ProcessHandler(cmd_args, env=os.environ, stream=None, shell=shell)
    proc.run()
    signal.signal(signal.SIGINT, orig)

    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()
        return []

    if not proc.output:
        return []  # no output means success

    try:
        jsonresult = json.loads(proc.output[0])
    except ValueError:
        print(ESLINT_ERROR_MESSAGE.format("\n".join(proc.output)))
        return 1

    results = []
    for obj in jsonresult:
        errors = obj['messages']

        for err in errors:
            err.update({
                'hint': err.get('fix'),
                'level': 'error' if err['severity'] == 2 else 'warning',
                'lineno': err.get('line'),
                'path': obj['filePath'],
                'rule': err.get('ruleId'),
            })
            results.append(result.from_linter(LINTER, **err))

    return results


LINTER = {
    'name': "eslint",
    'description': "JavaScript linter",
    # ESLint infra handles its own path filtering, so just include cwd
    'include': ['.'],
    'exclude': [],
    'extensions': EXTENSIONS,
    'type': 'external',
    'payload': lint,
}
