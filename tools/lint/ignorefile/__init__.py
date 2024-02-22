# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import pathlib
import re

from mozlint import result
from mozlint.pathutils import expand_exclusions

validReasons = [
    "git-only",
    "hg-only",
    "syntax-difference",
]


class IgnorePattern:
    """A class that represents single pattern in .gitignore or .hgignore, and
    provides rough comparison, and also hashing for SequenceMatcher."""

    def __init__(self, pattern, lineno):
        self.pattern = pattern
        self.lineno = lineno

        self.simplePattern = self.removePunctuation(pattern)
        self.hash = hash(self.pattern)

    @staticmethod
    def removePunctuation(s):
        # Remove special characters.
        # '.' is also removed because .hgignore uses regular expression.
        s = re.sub(r"[^A-Za-z0-9_~/]", "", s)

        # '/' is kept, except for the following cases:
        #   * leading '/' in .gitignore to specify top-level file,
        #     which is represented as '^' in .hgignore
        #   * leading '(^|/)' in .hgignore to specify filename
        #     (characters except for '/' are removed above)
        #   * leading '[^/]*' in .hgignore
        #     (characters except for '/' are removed above)
        s = re.sub(r"^/", "", s)

        return s

    def __eq__(self, other):
        return self.simplePattern == other.simplePattern

    def __hash__(self):
        return self.hash


def parseFile(results, path, config):
    patterns = []
    ignoreNextLine = False

    lineno = 0
    with open(path, "r") as f:
        for line in f:
            line = line.rstrip("\n")
            lineno += 1

            if ignoreNextLine:
                ignoreNextLine = False
                continue

            m = re.match(r"^# lint-ignore-next-line: (.+)", line)
            if m:
                reason = m.group(1)
                if reason not in validReasons:
                    res = {
                        "path": str(path),
                        "lineno": lineno,
                        "message": f'Unknown lint rule: "{reason}"',
                        "level": "error",
                    }
                    results.append(result.from_config(config, **res))
                    continue

                ignoreNextLine = True
                continue

            m = line.startswith("#")
            if m:
                continue

            if line == "":
                continue

            patterns.append(IgnorePattern(line, lineno))

    return patterns


def getLineno(patterns, index):
    if index >= len(patterns):
        return patterns[-1].lineno

    return patterns[index].lineno


def doLint(results, path1, config):
    if path1.name == ".gitignore":
        path2 = path1.parent / ".hgignore"
    elif path1.name == ".hgignore":
        path2 = path1.parent / ".gitignore"
    else:
        res = {
            "path": str(path1),
            "lineno": 0,
            "message": "Unsupported file",
            "level": "error",
        }
        results.append(result.from_config(config, **res))
        return

    patterns1 = parseFile(results, path1, config)
    patterns2 = parseFile(results, path2, config)

    # Comparison for each line is done via IgnorePattern.__eq__, which
    # ignores punctuations.
    if patterns1 == patterns2:
        return

    # Report minimal differences.
    from difflib import SequenceMatcher

    s = SequenceMatcher(None, patterns1, patterns2)
    for tag, index1, _, index2, _ in s.get_opcodes():
        if tag == "replace":
            res = {
                "path": str(path1),
                "lineno": getLineno(patterns1, index1),
                "message": f'Pattern mismatch: "{patterns1[index1].pattern}" in {path1.name} vs "{patterns2[index2].pattern}" in {path2.name}',
                "level": "error",
            }
            results.append(result.from_config(config, **res))
        elif tag == "delete":
            res = {
                "path": str(path1),
                "lineno": getLineno(patterns1, index1),
                "message": f'Pattern "{patterns1[index1].pattern}" not found in {path2.name}',
                "level": "error",
            }
            results.append(result.from_config(config, **res))
        elif tag == "insert":
            res = {
                "path": str(path1),
                "lineno": getLineno(patterns1, index1),
                "message": f'Pattern "{patterns2[index2].pattern}" not found in {path1.name}',
                "level": "error",
            }
            results.append(result.from_config(config, **res))


def lint(paths, config, fix=None, **lintargs):
    results = []

    for path in expand_exclusions(paths, config, lintargs["root"]):
        doLint(results, pathlib.Path(path), config)

    return {"results": results, "fixed": 0}
