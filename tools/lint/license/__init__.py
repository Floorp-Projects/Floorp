# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import os
from glob import glob
from html.parser import HTMLParser

from mozlint import result
from mozlint.pathutils import expand_exclusions

here = os.path.abspath(os.path.dirname(__file__))
topsrcdir = os.path.join(here, "..", "..", "..")

results = []

# Official source: https://www.mozilla.org/en-US/MPL/headers/
TEMPLATES = {
    "mpl2_license": """
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at https://mozilla.org/MPL/2.0/.
    """.strip().splitlines(),
    "public_domain_license": """
    Any copyright is dedicated to the public domain.
    https://creativecommons.org/publicdomain/zero/1.0/
    """.strip().splitlines(),
}
license_list = os.path.join(here, "valid-licenses.txt")


def load_valid_license():
    """
    Load the list of license patterns
    """
    with open(license_list) as f:
        l = f.readlines()
        # Remove the empty lines
        return list(filter(bool, [x.replace("\n", "") for x in l]))


def is_valid_license(licenses, filename):
    """
    From a given file, check if we can find the license patterns
    in the X first lines of the file
    """
    with open(filename, "r", errors="replace") as myfile:
        contents = myfile.read()
        # Empty files don't need a license.
        if not contents:
            return True

        for l in licenses:
            if l.lower().strip() in contents.lower():
                return True
    return False


def add_header(log, filename, header):
    """
    Add the header to the top of the file
    """
    header.append("\n")
    with open(filename, "r+") as f:
        # lines in list format
        try:
            lines = f.readlines()
        except UnicodeDecodeError as e:
            log.debug("Could not read file '{}'".format(f))
            log.debug("Error: {}".format(e))
            return

        i = 0
        if lines:
            # if the file isn't empty (__init__.py can be empty files)
            if lines[0].startswith("#!") or lines[0].startswith("<?xml "):
                i = 1

            if lines[0].startswith("/* -*- Mode"):
                i = 2
        # Insert in the top of the data structure
        lines[i:i] = header
        f.seek(0, 0)
        f.write("".join(lines))


def is_test(f):
    """
    is the file a test or not?
    """
    if "lint/test/" in f:
        # For the unit tests
        return False
    return (
        "/tests/" in f
        or "/test/" in f
        or "/test_" in f
        or "/gtest" in f
        or "/crashtest" in f
        or "/mochitest" in f
        or "/reftest" in f
        or "/imptest" in f
        or "/androidTest" in f
        or "/jit-test/" in f
        or "jsapi-tests/" in f
    )


def fix_me(log, filename):
    """
    Add the copyright notice to the top of the file
    """
    _, ext = os.path.splitext(filename)
    license = []

    license_template = TEMPLATES["mpl2_license"]
    test = False

    if is_test(filename):
        license_template = TEMPLATES["public_domain_license"]
        test = True

    if ext in [
        ".cpp",
        ".c",
        ".cc",
        ".h",
        ".m",
        ".mm",
        ".rs",
        ".java",
        ".js",
        ".jsm",
        ".jsx",
        ".css",
        ".idl",
        ".webidl",
    ]:
        for i, l in enumerate(license_template):
            start = " "
            end = ""
            if i == 0:
                # first line, we have the /*
                start = "/"
            if i == len(license_template) - 1:
                # Last line, we end by */
                end = " */"
            license.append(start + "* " + l.strip() + end + "\n")

        add_header(log, filename, license)
        return

    if ext in [".py", ".ftl", ".properties"] or filename.endswith(".inc.xul"):
        for l in license_template:
            license.append("# " + l.strip() + "\n")
        add_header(log, filename, license)
        return

    if ext in [".xml", ".xul", ".html", ".xhtml", ".dtd", ".svg"]:
        for i, l in enumerate(license_template):
            start = "   - "
            end = ""
            if i == 0:
                # first line, we have the <!--
                start = "<!-- "
            if i == 2 or (i == 1 and test):
                # Last line, we end by -->
                end = " -->"
            license.append(start + l.strip() + end)
            if ext != ".svg" or not end:
                # When dealing with an svg, we should not have a space between
                # the license and the content
                license.append("\n")
        add_header(log, filename, license)
        return


class HTMLParseError(Exception):
    def __init__(self, msg, pos):
        super().__init__(msg, *pos)


class LicenseHTMLParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.in_code = False
        self.invalid_paths = []

    def handle_starttag(self, tag, attrs):
        if tag == "code":
            if self.in_code:
                raise HTMLParseError("nested code tag", self.getpos())
            self.in_code = True

    def handle_endtag(self, tag):
        if tag == "code":
            if not self.in_code:
                raise HTMLParseError("not started code tag", self.getpos())
            self.in_code = False

    def handle_data(self, data):
        if self.in_code:
            path = data.strip()
            abspath = os.path.join(topsrcdir, path)
            if not glob(abspath):
                self.invalid_paths.append((path, self.getpos()))


def lint_license_html(path):
    parser = LicenseHTMLParser()
    with open(path) as fd:
        content = fd.read()
        parser.feed(content)
    return parser.invalid_paths


def is_html_licence_summary(path):
    license_html = os.path.join(topsrcdir, "toolkit", "content", "license.html")
    return os.path.samefile(path, license_html)


def lint(paths, config, fix=None, **lintargs):
    log = lintargs["log"]
    files = list(expand_exclusions(paths, config, lintargs["root"]))
    fixed = 0

    licenses = load_valid_license()
    for f in files:
        if is_test(f):
            # For now, do not do anything with test (too many)
            continue

        if not is_valid_license(licenses, f):
            res = {
                "path": f,
                "message": "No matching license strings found in tools/lint/license/valid-licenses.txt",  # noqa
                "level": "error",
            }
            results.append(result.from_config(config, **res))
            if fix:
                fix_me(log, f)
                fixed += 1

        if is_html_licence_summary(f):
            try:
                for invalid_path, (lineno, column) in lint_license_html(f):
                    res = {
                        "path": f,
                        "message": "references unknown path {}".format(invalid_path),
                        "level": "error",
                        "lineno": lineno,
                        "column": column,
                    }
                    results.append(result.from_config(config, **res))
            except HTMLParseError as err:
                res = {
                    "path": f,
                    "message": err.args[0],
                    "level": "error",
                    "lineno": err.args[1],
                    "column": err.args[2],
                }
                results.append(result.from_config(config, **res))

    return {"results": results, "fixed": fixed}
