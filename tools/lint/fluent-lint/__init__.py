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

    def __init__(self, path, config, exclusions, contents, offsets_and_lines):
        super().__init__()
        self.path = path
        self.config = config
        self.exclusions = exclusions
        self.contents = contents
        self.offsets_and_lines = offsets_and_lines

        self.results = []
        self.identifier_re = re.compile(r"[a-z0-9-]+")
        self.apostrophe_re = re.compile(r"\w'")
        self.incorrect_apostrophe_re = re.compile(r"\w\u2018\w")
        self.single_quote_re = re.compile(r"'(.+)'")
        self.double_quote_re = re.compile(r"\".+\"")
        self.ellipsis_re = re.compile(r"\.\.\.")

        self.minimum_id_length = 9

        self.state = {
            # The resource comment should be at the top of the page after the license.
            "node_can_be_resource_comment": True,
            # Group comments must be followed by a message. Two group comments are not
            # allowed in a row.
            "can_have_group_comment": True,
        }

        # Set this to true to debug print the root node's json. This is useful for
        # writing new lint rules, or debugging existing ones.
        self.debug_print_json = False

    def generic_visit(self, node):
        node_name = type(node).__name__
        self.state["node_can_be_resource_comment"] = self.state[
            "node_can_be_resource_comment"
        ] and (
            # This is the root node.
            node_name == "Resource"
            # Empty space is allowed.
            or node_name == "Span"
            # Comments are allowed
            or node_name == "Comment"
        )

        if self.debug_print_json:
            import json

            print(json.dumps(node.to_json(), indent=2))
            # Only debug print the root node.
            self.debug_print_json = False

        super(Linter, self).generic_visit(node)

    def visit_Attribute(self, node):
        # Only visit values for Attribute nodes, the identifier comes from dom.
        super().generic_visit(node.value)

    def visit_FunctionReference(self, node):
        # We don't recurse into function references, the identifiers there are
        # allowed to be free form.
        pass

    def visit_Message(self, node):
        # There must be at least one message or term between group comments.
        self.state["can_have_group_comment"] = True
        super().generic_visit(node)

    def visit_Term(self, node):
        # There must be at least one message or term between group comments.
        self.state["can_have_group_comment"] = True
        super().generic_visit(node)

    def visit_MessageReference(self, node):
        # We don't recurse into message references, the identifiers are either
        # checked elsewhere or are attributes and come from DOM.
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
        if (
            len(node.name) < self.minimum_id_length
            and self.path not in self.exclusions["ID02"]["files"]
            and node.name not in self.exclusions["ID02"]["messages"]
        ):
            self.add_error(
                node,
                "ID02",
                f"Identifiers must be at least {self.minimum_id_length} characters long",
            )

    def visit_TextElement(self, node):
        parser = TextElementHTMLParser()
        parser.feed(node.value)
        for text in parser.extracted_text:
            # To check for apostrophes, first remove pairs of straight quotes
            # used as delimiters.
            cleaned_str = re.sub(self.single_quote_re, "\1", node.value)
            if self.apostrophe_re.search(cleaned_str):
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

    def visit_ResourceComment(self, node):
        # This node is a comment with: "###"
        if not self.state["node_can_be_resource_comment"]:
            self.add_error(
                node,
                "RC01",
                "Resource comments (###) should be placed at the top of the file, just "
                "after the license header. There should only be one resource comment "
                "per file.",
            )
            return

        lines_after = get_newlines_count_after(node.span, self.contents)
        lines_before = get_newlines_count_before(node.span, self.contents)

        if node.span.end == len(self.contents) - 1:
            # This file only contains a resource comment.
            return

        if lines_after != 2:
            self.add_error(
                node,
                "RC02",
                "Resource comments (###) should be followed by one empty line.",
            )
            return

        if lines_before != 2:
            self.add_error(
                node,
                "RC03",
                "Resource comments (###) should have one empty line above them.",
            )
            return

    def visit_SelectExpression(self, node):
        # We only want to visit the variant values, the identifiers in selectors
        # and keys are allowed to be free form.
        for variant in node.variants:
            super().generic_visit(variant.value)

    def visit_GroupComment(self, node):
        # This node is a comment with: "##"

        if not self.state["can_have_group_comment"]:
            self.add_error(
                node,
                "GC04",
                "Group comments (##) must be followed by at least one message "
                "or term. Make sure that a single group comment with multiple "
                "pararaphs is not separated by whitespace, as it will be "
                "interpreted as two different comments.",
            )
            return

        self.state["can_have_group_comment"] = False

        lines_after = get_newlines_count_after(node.span, self.contents)
        lines_before = get_newlines_count_before(node.span, self.contents)

        if node.span.end == len(self.contents) - 1:
            # The group comment is the last thing in the file.

            if node.content == "":
                # Empty comments are allowed at the end of the file.
                return

            self.add_error(
                node,
                "GC01",
                "Group comments (##) should not be at the end of the file, they should "
                "always be above a message. Only an empty group comment is allowed at "
                "the end of a file.",
            )
            return

        if lines_after != 2:
            self.add_error(
                node,
                "GC02",
                "Group comments (##) should be followed by one empty line.",
            )
            return

        if lines_before != 2:
            self.add_error(
                node,
                "GC03",
                "Group comments (##) should have an empty line before them.",
            )
            return

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


def get_newlines_count_after(span, contents):
    # Determine the number of newlines.
    count = 0
    for i in range(span.end, len(contents)):
        assert contents[i] != "\r", "This linter does not handle \\r characters."
        if contents[i] != "\n":
            break
        count += 1

    return count


def get_newlines_count_before(span, contents):
    # Determine the range of newline characters.
    count = 0
    for i in range(span.start - 1, 0, -1):
        assert contents[i] != "\r", "This linter does not handle \\r characters."
        if contents[i] != "\n":
            break
        count += 1

    return count


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
        linter = Linter(
            path, config, exclusions, contents, get_offsets_and_lines(contents)
        )
        linter.visit(parse(contents))
        results.extend(linter.results)
    return results
