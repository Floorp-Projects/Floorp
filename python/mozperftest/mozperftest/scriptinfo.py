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


_INFO = """\
%(filename)s
%(filename_underline)s

%(description)s

Owner: %(owner)s
Usage:
%(usage)s

Description:
%(long_description)s
"""


class MetadataDict(defaultdict):
    def __missing__(self, key):
        return "N/A"


class ScriptInfo(MetadataDict):
    def __init__(self, script):
        super(ScriptInfo, self).__init__()
        filename = os.path.basename(script)
        self["filename"] = script, filename
        self["filename_underline"] = None, "-" * len(filename)
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
                    value = re.sub("\s+", " ", value).strip()
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

    def __str__(self):
        reprs = dict((k, v[1]) for k, v in self.items())
        d = MetadataDict()
        d.update(reprs)
        return _INFO % d
