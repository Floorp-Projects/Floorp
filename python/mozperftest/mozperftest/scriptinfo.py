# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import defaultdict
import re
import os
import textwrap

# This import will fail if esprima is not installed.
# The package is vendored in python/third_party and also available at PyPI
# The mach perftest command will make sure it's installed when --flavor doc
# is being used.
import esprima


METADATA = set(
    [
        "setUp",
        "tearDown",
        "test",
        "owner",
        "author",
        "name",
        "description",
        "longDescription",
        "usage",
        "supportedBrowsers",
        "supportedPlatforms",
        "filename",
    ]
)


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


class MetadataDict(defaultdict):
    def __missing__(self, key):
        return "N/A"


class ScriptInfo(MetadataDict):
    def __init__(self, script):
        super(ScriptInfo, self).__init__()
        filename = os.path.basename(script)
        self["filename"] = script, filename
        self.script = script
        with open(script) as f:
            self.parsed = esprima.parseScript(f.read())

        # looking for the exports statement
        for stmt in self.parsed.body:
            if (
                stmt.type != "ExpressionStatement"
                or stmt.expression.left is None
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
                #  line wrapping
                if isinstance(value, str):
                    repr = "\n".join(textwrap.wrap(value, break_on_hyphens=False))
                elif isinstance(value, list):
                    repr = ", ".join(value)

                self[prop.key.name] = value, repr

        # If the fields found, don't match our known ones,
        # then an error is raised
        assert set(list(self.keys())) - METADATA == set()

    def __str__(self):
        reprs = dict((k, v[1]) for k, v in self.items())
        d = MetadataDict()
        d.update(reprs)
        d.update({"filename_underline": "-" * len(self["filename"])})
        return _INFO % d
