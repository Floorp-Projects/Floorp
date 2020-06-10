# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import defaultdict
import re
import textwrap
from pathlib import Path

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


class MissingFieldError(Exception):
    pass


class ScriptInfo(defaultdict):
    """Loads and parses a Browsertime test script."""

    def __init__(self, path):
        super(ScriptInfo, self).__init__()
        self.script = Path(path)
        self["filename"] = str(self.script)

        with self.script.open() as f:
            self.parsed = esprima.parseScript(f.read())

        # looking for the exports statement
        for stmt in self.parsed.body:
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
            for prop in stmt.expression.right.properties:
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

        # If the fields found, don't match our known ones, then an error is raised
        for field, required in METADATA:
            if not required:
                continue
            if field not in self:
                raise MissingFieldError(field)

    def __str__(self):
        """Used to generate docs."""
        d = {}
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

        d["filename_underline"] = "-" * len(d["filename"])
        return _INFO % d

    def __missing__(self, key):
        return "N/A"
