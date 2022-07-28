# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, absolute_import

import logging
import os
import pathlib
import re
import subprocess
import sys

from mach.decorators import (
    Command,
    CommandArgument,
)


def path_sep_to_native(path_str):
    """Make separators in the path OS native."""
    return pathlib.os.sep.join(path_str.split("/"))


def path_sep_from_native(path):
    """Make separators in the path OS native."""
    return "/".join(str(path).split(pathlib.os.sep))


excluded_from_convert_prefix = list(
    map(
        path_sep_to_native,
        [
            # Testcases for actors.
            "docshell/test/unit/AllowJavascriptChild.jsm",
            "docshell/test/unit/AllowJavascriptParent.jsm",
            "toolkit/actors/TestProcessActorChild.jsm",
            "toolkit/actors/TestProcessActorParent.jsm",
            "toolkit/actors/TestWindowChild.jsm",
            "toolkit/actors/TestWindowParent.jsm",
            # Testcases for loader.
            "docshell/test/browser/Bug1622420Child.jsm",
            "docshell/test/browser/Bug422543Child.jsm",
            "js/xpconnect/tests/unit/",
            # Testcase for build system.
            "python/mozbuild/mozbuild/test/",
        ],
    )
)


def is_excluded_from_convert(path):
    """Returns true if the JSM file shouldn't be converted to ESM."""
    path_str = str(path)
    for prefix in excluded_from_convert_prefix:
        if path_str.startswith(prefix):
            return True

    return False


excluded_from_imports_prefix = list(
    map(
        path_sep_to_native,
        [
            # Vendored or auto-generated files.
            "browser/components/pocket/content/panels/js/vendor.bundle.js",
            "devtools/client/debugger/dist/parser-worker.js",
            "devtools/client/debugger/packages/devtools-source-map/src/tests/fixtures/bundle.js",
            "devtools/client/debugger/test/mochitest/examples/react/build/main.js",
            "devtools/client/debugger/test/mochitest/examples/sourcemapped/polyfill-bundle.js",
            "devtools/client/inspector/markup/test/shadowdom_open_debugger.min.js",
            "layout/style/test/property_database.js",
            "services/fxaccounts/FxAccountsPairingChannel.js",
            "testing/talos/talos/tests/devtools/addon/content/pages/custom/debugger/static/js/main.js",  # noqa E501
            "testing/web-platform/",
            # Unrelated testcases that has edge case syntax.
            "browser/components/sessionstore/test/unit/data/",
            "devtools/client/debugger/src/workers/parser/tests/fixtures/",
            "devtools/client/debugger/test/mochitest/examples/sourcemapped/fixtures/",
            "devtools/client/webconsole/test/browser/test-syntaxerror-worklet.js",
            "devtools/shared/tests/xpcshell/test_eventemitter_basic.js",
            "devtools/shared/tests/xpcshell/test_eventemitter_static.js",
            "dom/base/test/file_bug687859-16.js",
            "dom/base/test/file_bug687859-16.js",
            "dom/base/test/file_js_cache_syntax_error.js",
            "dom/base/test/jsmodules/module_badSyntax.js",
            "dom/canvas/test/reftest/webgl-utils.js",
            "dom/encoding/test/file_utf16_be_bom.js",
            "dom/encoding/test/file_utf16_le_bom.js",
            "dom/media/webrtc/tests/mochitests/identity/idp-bad.js",
            "dom/serviceworkers/test/file_js_cache_syntax_error.js",
            "dom/serviceworkers/test/parse_error_worker.js",
            "dom/workers/test/importScripts_worker_imported3.js",
            "dom/workers/test/invalid.js",
            "dom/workers/test/threadErrors_worker1.js",
            "dom/xhr/tests/browser_blobFromFile.js",
            "image/test/browser/browser_image.js",
            "js/xpconnect/tests/chrome/test_bug732665_meta.js",
            "js/xpconnect/tests/unit/bug451678_subscript.js",
            "js/xpconnect/tests/unit/recursive_importA.jsm",
            "js/xpconnect/tests/unit/recursive_importB.jsm",
            "js/xpconnect/tests/unit/syntax_error.jsm",
            "js/xpconnect/tests/unit/test_defineModuleGetter.js",
            "js/xpconnect/tests/unit/test_import.js",
            "js/xpconnect/tests/unit/test_import_shim.js",
            "js/xpconnect/tests/unit/test_recursive_import.js",
            "js/xpconnect/tests/unit/test_unload.js",
            "modules/libpref/test/unit/data/testParser.js",
            "python/mozbuild/mozbuild/test/",
            "remote/shared/messagehandler/test/browser/resources/modules/root/invalid.jsm",
            "testing/talos/talos/startup_test/sessionrestore/profile-manywindows/sessionstore.js",
            "testing/talos/talos/startup_test/sessionrestore/profile/sessionstore.js",
            "toolkit/components/workerloader/tests/moduleF-syntax-error.js",
            "tools/lint/test/",
            "tools/update-packaging/test/",
            # SpiderMonkey internals.
            "js/examples/",
            "js/src/",
            # Files has macro.
            "browser/app/profile/firefox.js",
            "browser/branding/official/pref/firefox-branding.js",
            "browser/locales/en-US/firefox-l10n.js",
            "mobile/android/app/geckoview-prefs.js",
            "mobile/android/app/mobile.js",
            "mobile/android/locales/en-US/mobile-l10n.js",
            "modules/libpref/greprefs.js",
            "modules/libpref/init/all.js",
            "testing/condprofile/condprof/tests/profile/user.js",
            "testing/mozbase/mozprofile/tests/files/prefs_with_comments.js",
            "toolkit/mozapps/update/tests/data/xpcshellConstantsPP.js",
        ],
    )
)

EXCLUSION_FILES = [
    os.path.join("tools", "rewriting", "Generated.txt"),
    os.path.join("tools", "rewriting", "ThirdPartyPaths.txt"),
]


def load_exclusion_files():
    for path in EXCLUSION_FILES:
        with open(path, "r") as f:
            for line in f:
                p = path_sep_to_native(re.sub("\*$", "", line.strip()))
                excluded_from_imports_prefix.append(p)


def is_excluded_from_imports(path):
    """Returns true if the JS file content shouldn't be handled by
    jscodeshift.

    This filter is necessary because jscodeshift cannot handle some
    syntax edge cases and results in unexpected rewrite."""
    path_str = str(path)
    for prefix in excluded_from_imports_prefix:
        if path_str.startswith(prefix):
            return True

    return False


# Wrapper for hg/git operations
class VCSUtils:
    def run(self, cmd):
        # Do not pass check=True because the pattern can match no file.
        lines = subprocess.run(cmd, stdout=subprocess.PIPE).stdout.decode()
        return filter(lambda x: x != "", lines.split("\n"))


class HgUtils(VCSUtils):
    def is_available():
        return pathlib.Path(".hg").exists()

    def rename(self, before, after):
        cmd = ["hg", "rename", before, after]
        subprocess.run(cmd, check=True)

    def find_jsms(self, path):
        jsms = []

        cmd = ["hg", "files", f"set:glob:{path}/**/*.jsm"]
        for line in self.run(cmd):
            jsm = pathlib.Path(line)
            if is_excluded_from_convert(jsm):
                continue
            jsms.append(jsm)

        cmd = [
            "hg",
            "files",
            f"set:grep('EXPORTED_SYMBOLS = \[') and glob:{path}/**/*.js",
        ]
        for line in self.run(cmd):
            jsm = pathlib.Path(line)
            if is_excluded_from_convert(jsm):
                continue
            jsms.append(jsm)

        return jsms

    def find_all_jss(self, path):
        jss = []

        cmd = ["hg", "files", f"set:glob:{path}/**/*.jsm"]
        for line in self.run(cmd):
            js = pathlib.Path(line)
            if is_excluded_from_imports(js):
                continue
            jss.append(js)

        cmd = ["hg", "files", f"set:glob:{path}/**/*.js"]
        for line in self.run(cmd):
            js = pathlib.Path(line)
            if is_excluded_from_imports(js):
                continue
            jss.append(js)

        cmd = ["hg", "files", f"set:glob:{path}/**/*.sys.mjs"]
        for line in self.run(cmd):
            js = pathlib.Path(line)
            if is_excluded_from_imports(js):
                continue
            jss.append(js)

        return jss


class GitUtils(VCSUtils):
    def is_available():
        return pathlib.Path(".git").exists()

    def rename(self, before, after):
        cmd = ["git", "mv", before, after]
        subprocess.run(cmd, check=True)

    def find_jsms(self, path):
        jsms = []

        cmd = ["git", "ls-files", f"{path}/*.jsm"]
        for line in self.run(cmd):
            jsm = pathlib.Path(line)
            if is_excluded_from_convert(jsm):
                continue
            jsms.append(jsm)

        handled = {}
        cmd = ["git", "grep", "EXPORTED_SYMBOLS = \[", f"{path}/*.js"]
        for line in self.run(cmd):
            m = re.search("^([^:]+):", line)
            if not m:
                continue
            filename = m.group(1)
            if filename in handled:
                continue
            handled[filename] = True
            jsm = pathlib.Path(filename)
            if is_excluded_from_convert(jsm):
                continue
            jsms.append(jsm)

        return jsms

    def find_all_jss(self, path):
        jss = []

        cmd = ["git", "ls-files", f"{path}/*.jsm"]
        for line in self.run(cmd):
            js = pathlib.Path(line)
            if is_excluded_from_imports(js):
                continue
            jss.append(js)

        cmd = ["git", "ls-files", f"{path}/*.js"]
        for line in self.run(cmd):
            js = pathlib.Path(line)
            if is_excluded_from_imports(js):
                continue
            jss.append(js)

        cmd = ["git", "ls-files", f"{path}/*.mjs"]
        for line in self.run(cmd):
            js = pathlib.Path(line)
            if is_excluded_from_imports(js):
                continue
            jss.append(js)

        return jss


@Command(
    "esmify",
    category="misc",
    description="ESMify JSM files.",
)
@CommandArgument(
    "path",
    nargs=1,
    help="Path to the JSM file to ESMify, or the directory that contains "
    "JSM files and/or JS files that imports ESM-ified JSM.",
)
@CommandArgument(
    "--convert",
    action="store_true",
    help="Only perform the step 1 = convert part",
)
@CommandArgument(
    "--imports",
    action="store_true",
    help="Only perform the step 2 = import calls part",
)
@CommandArgument(
    "--prefix",
    default="",
    help="Restrict the target of import in the step 2 to ESM-ified JSM, by the "
    "prefix match for the JSM file's path.  e.g. 'browser/'.",
)
def esmify(command_context, path=None, convert=False, imports=False, prefix=""):
    """
    This command does the following 2 steps:
      1. Convert the JSM file specified by `path` to ESM file, or the JSM files
         inside the directory specified by `path` to ESM files, and also
         fix references in build files and test definitions
      2. Convert import calls inside file(s) specified by `path` for ESM-ified
         files to use new APIs

    Example 1:
      # Convert all JSM files inside `browser/components/pagedata` directory,
      # and replace all references for ESM-ified files in the entire tree to use
      # new APIs

      $ ./mach esmify --convert browser/components/pagedata
      $ ./mach esmify --imports . --prefix=browser/components/pagedata

    Example 2:
      # Convert all JSM files inside `browser` directory, and replace all
      # references for the JSM files inside `browser` directory to use
      # new APIs

      $ ./mach esmify browser
    """

    def error(text):
        command_context.log(logging.ERROR, "esmify", {}, f"[ERROR] {text}")

    def info(text):
        command_context.log(logging.INFO, "esmify", {}, f"[INFO] {text}")

    # If no options is specified, perform both.
    if not convert and not imports:
        convert = True
        imports = True

    path = pathlib.Path(path[0])

    if not verify_path(command_context, path):
        return 1

    if HgUtils.is_available():
        vcs_utils = HgUtils()
    elif GitUtils.is_available():
        vcs_utils = GitUtils()
    else:
        error(
            "This script needs to be run inside mozilla-central "
            "checkout of either mercurial or git."
        )
        return 1

    load_exclusion_files()

    info("Setting up jscodeshift...")
    setup_jscodeshift()

    is_single_file = path.is_file()

    modified_files = []

    if convert:
        info("Searching files to convert to ESM...")
        if is_single_file:
            jsms = [path]
        else:
            jsms = vcs_utils.find_jsms(path)

        info(f"Found {len(jsms)} file(s) to convert to ESM.")

        info("Converting to ESM...")
        jsms = convert_module(command_context, jsms)
        if jsms is None:
            error("Failed to rewrite exports.")
            return 1

        info("Renaming...")
        esms = rename_jsms(command_context, vcs_utils, jsms)

        modified_files += esms

    if imports:
        info("Searching files to rewrite imports...")

        if is_single_file:
            if convert:
                # Already converted above
                jss = esms
            else:
                jss = [path]
        else:
            jss = vcs_utils.find_all_jss(path)

        info(f"Found {len(jss)} file(s). Rewriting imports...")

        result = rewrite_imports(jss, prefix)
        if result is None:
            return 1

        info(f"Rewritten {len(result)} file(s).")

        # Only modified files needs eslint fix
        modified_files += result

    modified_files = list(set(modified_files))

    info(f"Applying eslint --fix for {len(modified_files)} file(s)...")
    eslint_fix(command_context, modified_files)

    return 0


def verify_path(command_context, path):
    """Check if the path passed to the command is valid relative path."""

    def error(text):
        command_context.log(logging.ERROR, "esmify", {}, f"[ERROR] {text}")

    if not path.exists():
        error(f"{path} does not exist.")
        return False

    if path.is_absolute():
        error("Path must be a relative path from mozilla-central checkout.")
        return False

    return True


def find_file(path, target):
    """Find `target` file in ancestor of path."""
    target_path = path.parent / target
    if not target_path.exists():
        if path.parent == path:
            return None

        return find_file(path.parent, target)

    return target_path


def try_rename_in(command_context, path, target, jsm_name, esm_name, jsm_path):
    """Replace the occurrences of `jsm_name` with `esm_name` in `target`
    file."""

    def info(text):
        command_context.log(logging.INFO, "esmify", {}, f"[INFO] {text}")

    target_path = find_file(path, target)
    if not target_path:
        return False

    # Single moz.build or jar.mn can contain multiple files with same name.
    # Check the relative path.

    jsm_relative_path = jsm_path.relative_to(target_path.parent)
    jsm_relative_str = path_sep_from_native(str(jsm_relative_path))

    jsm_name_re = re.compile(r"\b" + jsm_name.replace(".", r"\.") + r"\b")
    jsm_relative_re = re.compile(r"\b" + jsm_relative_str.replace(".", r"\.") + r"\b")

    modified = False
    content = ""
    with open(target_path, "r") as f:
        for line in f:
            if jsm_relative_re.search(line):
                modified = True
                line = jsm_name_re.sub(esm_name, line)

            content += line

    if modified:
        info(f"  {str(target_path)}")
        info(f"    {jsm_name} => {esm_name}")
        with open(target_path, "w") as f:
            f.write(content)

    return True


def try_rename_components_conf(command_context, path, jsm_name, esm_name):
    """Replace the occurrences of `jsm_name` with `esm_name` in components.conf
    file."""

    def info(text):
        command_context.log(logging.INFO, "esmify", {}, f"[INFO] {text}")

    target_path = find_file(path, "components.conf")
    if not target_path:
        return False

    # Unlike try_rename_in, components.conf contains the URL instead of
    # relative path, and also there are no known files with same name.
    # Simply replace the filename.

    with open(target_path, "r") as f:
        content = f.read()

    prop_re = re.compile(
        "[\"']jsm[\"']:(.*)" + r"\b" + jsm_name.replace(".", r"\.") + r"\b"
    )

    if not prop_re.search(content):
        return False

    info(f"  {str(target_path)}")
    info(f"    {jsm_name} => {esm_name}")

    content = prop_re.sub(r"'esModule':\1" + esm_name, content)
    with open(target_path, "w") as f:
        f.write(content)

    return True


def esmify_name(name):
    return re.sub(r"\.(jsm|js|jsm\.js)$", ".sys.mjs", name)


def esmify_path(jsm_path):
    jsm_name = jsm_path.name
    esm_name = re.sub(r"\.(jsm|js|jsm\.js)$", ".sys.mjs", jsm_name)
    esm_path = jsm_path.parent / esm_name
    return esm_path


def rename_single_file(command_context, vcs_utils, jsm_path):
    """Rename `jsm_path` to .sys.mjs, and fix refereces to the file in build and
    test definitions."""

    def warn(text):
        command_context.log(logging.WARN, "esmify", {}, f"[WARN] {text}")

    def info(text):
        command_context.log(logging.INFO, "esmify", {}, f"[INFO] {text}")

    esm_path = esmify_path(jsm_path)

    jsm_name = jsm_path.name
    esm_name = esm_path.name

    target_files = [
        "moz.build",
        "jar.mn",
        "browser.ini",
        "browser-common.ini",
        "chrome.ini",
        "mochitest.ini",
        "xpcshell.ini",
        "xpcshell-child-process.ini",
        "xpcshell-common.ini",
        "xpcshell-parent-process.ini",
    ]

    info(f"{jsm_path} => {esm_path}")

    renamed = False
    for target in target_files:
        if try_rename_in(
            command_context, jsm_path, target, jsm_name, esm_name, jsm_path
        ):
            renamed = True

    if try_rename_components_conf(command_context, jsm_path, jsm_name, esm_name):
        renamed = True

    if not renamed:
        warn(f"  {jsm_path} is not found in any build file")

    vcs_utils.rename(jsm_path, esm_path)

    return esm_path


def rename_jsms(command_context, vcs_utils, jsms):
    esms = []
    for jsm in jsms:
        esm = rename_single_file(command_context, vcs_utils, jsm)
        esms.append(esm)

    return esms


npm_prefix = pathlib.Path("tools") / "esmify"
path_from_npm_prefix = pathlib.Path("..") / ".."


def setup_jscodeshift():
    """Install jscodeshift."""
    cmd = [
        sys.executable,
        "./mach",
        "npm",
        "install",
        "jscodeshift",
        "--save-dev",
        "--prefix",
        str(npm_prefix),
    ]
    subprocess.run(cmd, check=True)


def convert_module(command_context, jsms):
    """Replace EXPORTED_SYMBOLS with export declarations, and replace
    ChromeUtils.importESModule with static import as much as possible,
    and return the list of successfully rewritten files."""

    def warn(text):
        command_context.log(logging.WARN, "esmify", {}, f"[WARN] {text}")

    if len(jsms) == 0:
        return []

    cmd = [
        sys.executable,
        "./mach",
        "npm",
        "run",
        "convert_module",
        "--prefix",
        str(npm_prefix),
    ]
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    p.stdin.write("\n".join(map(str, paths_from_npm_prefix(jsms))).encode())
    p.stdin.close()

    ok_files = []
    error_files = []
    while True:
        line = p.stdout.readline()
        if not line:
            break
        line = line.rstrip().decode()

        if line.startswith(" NOC "):
            continue

        print(line)

        m = re.search(r"^ (OKK|ERR) \.\.(/|\\)\.\.(/|\\)([^ ]+)", line)
        if not m:
            continue

        result = m.group(1)
        path = pathlib.Path(m.group(4))

        if result == "OKK":
            ok_files.append(path)
        else:
            error_files.append(path)

    if p.wait() != 0:
        return None

    if len(error_files):
        warn("Following files are not rewritten due to error:")
        for path in error_files:
            warn(f"  {path}")

    return ok_files


def rewrite_imports(jss, prefix):
    """Replace import calls for JSM with import calls for ESM or static import
    for ESM."""

    if len(jss) == 0:
        return []

    env = os.environ.copy()
    env["ESMIFY_TARGET_PREFIX"] = prefix
    cmd = [
        sys.executable,
        "./mach",
        "npm",
        "run",
        "rewrite_imports",
        "--prefix",
        str(npm_prefix),
    ]
    p = subprocess.Popen(cmd, env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    p.stdin.write("\n".join(map(str, paths_from_npm_prefix(jss))).encode())
    p.stdin.close()

    modified = []
    while True:
        line = p.stdout.readline()
        if not line:
            break
        line = line.rstrip().decode()

        if line.startswith(" NOC "):
            continue

        print(line)

        if line.startswith(" OKK "):
            # Modified file
            path = pathlib.Path(line[5:])
            modified.append(path.relative_to(path_from_npm_prefix))

    if p.wait() != 0:
        return None

    return modified


def paths_from_npm_prefix(paths):
    """Convert relative path from mozilla-central to relative path from
    tools/esmify."""
    return list(map(lambda path: path_from_npm_prefix / path, paths))


def eslint_fix(command_context, files):
    """Auto format files."""

    def info(text):
        command_context.log(logging.INFO, "esmify", {}, f"[INFO] {text}")

    if len(files) == 0:
        return

    remaining = files[0:]

    # There can be too many files for single command line, perform by chunk.
    max_files = 16
    while len(remaining) > max_files:
        info(f"{len(remaining)} files remaining")

        chunk = remaining[0:max_files]
        remaining = remaining[max_files:]

        cmd = [sys.executable, "./mach", "eslint", "--fix"] + chunk
        subprocess.run(cmd, check=True)

    info(f"{len(remaining)} files remaining")
    chunk = remaining
    cmd = [sys.executable, "./mach", "eslint", "--fix"] + chunk
    subprocess.run(cmd, check=True)
