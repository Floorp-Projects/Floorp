# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import bisect
from fluent.syntax import parse, visitor
from mozlint import result
from mozlint.pathutils import expand_exclusions
import mozpack.path as mozpath
import re
import yaml

from html.parser import HTMLParser


class TextElementHTMLParser(HTMLParser):
    """HTML Parser for TextElement.

    TextElements may contain embedded html tags, which can include
    quotes in attributes. We only want to check the actual text.
    """

    def __init__(self):
        super().__init__()
        self.extracted_text = []

    def handle_data(self, data):
        self.extracted_text.append(data)


class Linter(visitor.Visitor):
    """Fluent linter implementation.

    This subclasses the Fluent AST visitor. Methods are called corresponding
    to each type of node in the Fluent AST. It is possible to control
    whether a node is recursed into by calling the generic_visit method on
    the superclass.

    See the documentation here:
    https://www.projectfluent.org/python-fluent/fluent.syntax/stable/usage.html
    """

    def __init__(self, path, config, exclusions, offsets_and_lines):
        super().__init__()
        self.path = path
        self.config = config
        self.exclusions = exclusions
        self.offsets_and_lines = offsets_and_lines

        self.results = []
        self.identifier_re = re.compile(r"[a-z0-9-]+")
        self.apostrophe_re = re.compile(r"\w\'\w")
        self.incorrect_apostrophe_re = re.compile(r"\w\u2018\w")
        self.single_quote_re = re.compile(r"'.+'")
        self.double_quote_re = re.compile(r"\"")
        self.ellipsis_re = re.compile(r"\.\.\.")

    def visit_Attribute(self, node):
        # Only visit values for Attribute nodes, the identifier comes from dom.
        super().generic_visit(node.value)

    def visit_FunctionReference(self, node):
        # We don't recurse into function references, the identifiers there are
        # allowed to be free form.
        pass

    def visit_Identifier(self, node):
        if (
            self.path not in self.exclusions["ID01"]["files"]
            and node.name not in self.exclusions["ID01"]["messages"]
            and not self.identifier_re.fullmatch(node.name)
        ):
            self.add_error(
                node, "ID01", "Identifiers may only contain lowercase characters and -"
            )

    def visit_TextElement(self, node):
        parser = TextElementHTMLParser()
        parser.feed(node.value)
        for text in parser.extracted_text:
            if self.apostrophe_re.search(text):
                self.add_error(
                    node,
                    "TE01",
                    "Strings with apostrophes should use foo\u2019s instead of foo's.",
                )
            if self.incorrect_apostrophe_re.search(text):
                self.add_error(
                    node,
                    "TE02",
                    "Strings with apostrophes should use foo\u2019s instead of foo\u2018s.",
                )
            if self.single_quote_re.search(text):
                self.add_error(
                    node,
                    "TE03",
                    "Single-quoted strings should use Unicode \u2018foo\u2019 instead of 'foo'.",
                )
            if self.double_quote_re.search(text):
                self.add_error(
                    node,
                    "TE04",
                    'Double-quoted strings should use Unicode \u201cfoo\u201d instead of "foo".',
                )
            if self.ellipsis_re.search(text):
                self.add_error(
                    node,
                    "TE05",
                    "Strings with an ellipsis should use the Unicode \u2026 character"
                    " instead of three periods",
                )

    def visit_VariableReference(self, node):
        # We don't recurse into variable references, the identifiers there are
        # allowed to be free form.
        pass

    def add_error(self, node, rule, msg):
        (col, line) = self.span_to_line_and_col(node.span)
        res = {
            "path": self.path,
            "lineno": line,
            "column": col,
            "rule": rule,
            "message": msg,
        }
        self.results.append(result.from_config(self.config, **res))

    def span_to_line_and_col(self, span):
        i = bisect.bisect_left(self.offsets_and_lines, (span.start, 0))
        if i > 0:
            col = span.start - self.offsets_and_lines[i - 1][0]
        else:
            col = 1 + span.start
        return (col, self.offsets_and_lines[i][1])


def get_offsets_and_lines(contents):
    """Return a list consisting of tuples of (offset, line).

    The Fluent AST contains spans of start and end offsets in the file.
    This function returns a list of offsets and line numbers so that errors
    can be reported using line and column.
    """
    line = 1
    result = []
    for m in re.finditer(r"\n", contents):
        result.append((m.start(), line))
        line += 1
    return result


def get_exclusions(root):
    with open(
        mozpath.join(root, "tools", "lint", "fluent-lint", "exclusions.yml")
    ) as f:
        exclusions = list(yaml.safe_load_all(f))[0]
        for error_type in exclusions:
            exclusions[error_type]["files"] = set(
                [mozpath.join(root, x) for x in exclusions[error_type]["files"]]
            )
        return exclusions


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs["root"]))
    exclusions = get_exclusions(lintargs["root"])
    results = []
    for path in files:
        contents = open(path, "r", encoding="utf-8").read()
        linter = Linter(path, config, exclusions, get_offsets_and_lines(contents))
        linter.visit(parse(contents))
        results.extend(linter.results)
    return results
