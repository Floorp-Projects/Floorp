# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import defaultdict
import re
import textwrap
from pathlib import Path
from enum import Enum

import esprima


# list of metadata, each item is the name and if the field is mandatory
METADATA = [
    ("setUp", False),
    ("tearDown", False),
    ("test", True),
    ("owner", True),
    ("author", False),
    ("name", True),
    ("description", True),
    ("longDescription", False),
    ("usage", False),
    ("supportedBrowsers", False),
    ("supportedPlatforms", False),
    ("filename", True),
]


_INFO = """\
%(filename)s
%(filename_underline)s

%(description)s

Owner: %(owner)s
Test Name: %(name)s
Usage:
%(usage)s

Description:
%(longDescription)s
"""


XPCSHELL_FUNCS = "add_task", "run_test", "run_next_test"


class MissingFieldError(Exception):
    pass


class ScriptType(Enum):
    XPCSHELL = 1
    BROWSERTIME = 2


class ScriptInfo(defaultdict):
    """Loads and parses a Browsertime test script."""

    def __init__(self, path):
        super(ScriptInfo, self).__init__()
        self.script = Path(path)
        self["filename"] = str(self.script)
        self.script_type = ScriptType.BROWSERTIME

        with self.script.open() as f:
            self.parsed = esprima.parseScript(f.read())

        # looking for the exports statement
        for stmt in self.parsed.body:
            #  detecting if the script has add_task()
            if (
                stmt.type == "ExpressionStatement"
                and stmt.expression is not None
                and stmt.expression.callee is not None
                and stmt.expression.callee.type == "Identifier"
                and stmt.expression.callee.name in XPCSHELL_FUNCS
            ):
                self["test"] = "xpcshell"
                self.script_type = ScriptType.XPCSHELL
                continue

            # plain xpcshell tests functions markers
            if stmt.type == "FunctionDeclaration" and stmt.id.name in XPCSHELL_FUNCS:
                self["test"] = "xpcshell"
                self.script_type = ScriptType.XPCSHELL
                continue

            # is this the perfMetdatata plain var ?
            if stmt.type == "VariableDeclaration":
                for decl in stmt.declarations:
                    if (
                        decl.type != "VariableDeclarator"
                        or decl.id.type != "Identifier"
                        or decl.id.name != "perfMetadata"
                        or decl.init is None
                    ):
                        continue
                    self.scan_properties(decl.init.properties)
                    continue

            # or the module.exports map ?
            if (
                stmt.type != "ExpressionStatement"
                or stmt.expression.left is None
                or stmt.expression.left.property is None
                or stmt.expression.left.property.name != "exports"
                or stmt.expression.right is None
                or stmt.expression.right.properties is None
            ):
                continue

            # now scanning the properties
            self.scan_properties(stmt.expression.right.properties)

        # If the fields found, don't match our known ones, then an error is raised
        for field, required in METADATA:
            if not required:
                continue
            if field not in self:
                raise MissingFieldError(field)

    def scan_properties(self, properties):
        for prop in properties:
            if prop.value.type == "Identifier":
                value = prop.value.name
            elif prop.value.type == "Literal":
                value = prop.value.value
            elif prop.value.type == "TemplateLiteral":
                # ugly
                value = prop.value.quasis[0].value.cooked.replace("\n", " ")
                value = re.sub(r"\s+", " ", value).strip()
            elif prop.value.type == "ArrayExpression":
                value = [e.value for e in prop.value.elements]
            else:
                raise ValueError(prop.value.type)

            self[prop.key.name] = value

    def __str__(self):
        """Used to generate docs."""
        d = defaultdict(lambda: "N/A")
        for field, value in self.items():
            if field == "filename":
                d[field] = self.script.name
                continue

            # line wrapping
            if isinstance(value, str):
                value = "\n".join(textwrap.wrap(value, break_on_hyphens=False))
            elif isinstance(value, list):
                value = ", ".join(value)
            d[field] = value

        d["filename_underline"] = "=" * len(d["filename"])
        return _INFO % d

    def __missing__(self, key):
        return "N/A"
