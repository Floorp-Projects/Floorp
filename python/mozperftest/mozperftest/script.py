# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import io
import re
import textwrap
import traceback
from collections import defaultdict
from enum import Enum
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
    ("options", False),
    ("supportedBrowsers", False),
    ("supportedPlatforms", False),
    ("filename", True),
    ("tags", False),
]


_INFO = """\
%(filename)s
%(filename_underline)s

:owner: %(owner)s
:name: %(name)s
"""


XPCSHELL_FUNCS = "add_task", "run_test", "run_next_test"


class BadOptionTypeError(Exception):
    """Raised when an option defined in a test has an incorrect type."""

    pass


class MissingFieldError(Exception):
    def __init__(self, script, field):
        super().__init__(f"Missing metadata {field}")
        self.script = script
        self.field = field


class ParseError(Exception):
    def __init__(self, script, exception):
        super().__init__(f"Cannot parse {script}")
        self.script = script
        self.exception = exception

    def __str__(self):
        output = io.StringIO()
        traceback.print_exception(
            type(self.exception),
            self.exception,
            self.exception.__traceback__,
            file=output,
        )
        return f"{self.args[0]}\n{output.getvalue()}"


class ScriptType(Enum):
    xpcshell = 1
    browsertime = 2


class ScriptInfo(defaultdict):
    """Loads and parses a Browsertime test script."""

    def __init__(self, path):
        super(ScriptInfo, self).__init__()
        try:
            self._parse_file(path)
        except Exception as e:
            raise ParseError(path, e)

        # If the fields found, don't match our known ones, then an error is raised
        for field, required in METADATA:
            if not required:
                continue
            if field not in self:
                raise MissingFieldError(path, field)

    def _parse_file(self, path):
        self.script = Path(path).resolve()
        self["filename"] = str(self.script)
        self.script_type = ScriptType.browsertime
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
                self.script_type = ScriptType.xpcshell
                continue

            # plain xpcshell tests functions markers
            if stmt.type == "FunctionDeclaration" and stmt.id.name in XPCSHELL_FUNCS:
                self["test"] = "xpcshell"
                self.script_type = ScriptType.xpcshell
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

    def parse_value(self, value):
        if value.type == "Identifier":
            return value.name

        if value.type == "Literal":
            return value.value

        if value.type == "TemplateLiteral":
            # ugly
            value = value.quasis[0].value.cooked.replace("\n", " ")
            return re.sub(r"\s+", " ", value).strip()

        if value.type == "ArrayExpression":
            return [self.parse_value(e) for e in value.elements]

        if value.type == "ObjectExpression":
            elements = {}
            for prop in value.properties:
                sub_name, sub_value = self.parse_property(prop)
                elements[sub_name] = sub_value
            return elements

        raise ValueError(value.type)

    def parse_property(self, property):
        return property.key.name, self.parse_value(property.value)

    def scan_properties(self, properties):
        for prop in properties:
            name, value = self.parse_property(prop)
            self[name] = value

    def __str__(self):
        """Used to generate docs."""

        def _render(value, level=0):
            if not isinstance(value, (list, tuple, dict)):
                if not isinstance(value, str):
                    value = str(value)
                # line wrapping
                return "\n".join(textwrap.wrap(value, break_on_hyphens=False))

            # options
            if isinstance(value, dict):
                if level > 0:
                    return ",".join([f"{k}:{v}" for k, v in value.items()])

                res = []
                for key, val in value.items():
                    if isinstance(val, bool):
                        res.append(f" --{key.replace('_', '-')}")
                    else:
                        val = _render(val, level + 1)
                        res.append(f" --{key.replace('_', '-')} {val}")

                return "\n".join(res)

            # simple flat list
            return ", ".join([_render(v, level + 1) for v in value])

        options = ""
        d = defaultdict(lambda: "N/A")
        for field, value in self.items():
            if field == "longDescription":
                continue
            if field == "filename":
                d[field] = self.script.name
                continue
            if field == "options":
                for plat in "default", "linux", "mac", "win":
                    if plat not in value:
                        continue
                    options += f":{plat.capitalize()} options:\n\n::\n\n{_render(value[plat])}\n"
            else:
                d[field] = _render(value)

        d["filename_underline"] = "=" * len(d["filename"])
        info = _INFO % d
        if "tags" in self:
            info += f":tags: {','.join(self['tags'])}\n"
        info += options
        info += f"\n**{self['description']}**\n"
        if "longDescription" in self:
            info += f"\n{self['longDescription']}\n"

        return info

    def __missing__(self, key):
        return "N/A"

    @classmethod
    def detect_type(cls, path):
        return cls(path).script_type

    def update_args(self, **args):
        """Updates arguments with options from the script."""
        from mozperftest.utils import simple_platform

        # Order of precedence:
        #   cli options > platform options > default options
        options = self.get("options", {})
        result = options.get("default", {})
        result.update(options.get(simple_platform(), {}))
        result.update(args)

        # XXX this is going away, see https://bugzilla.mozilla.org/show_bug.cgi?id=1675102
        for opt, val in result.items():
            if opt.startswith("visualmetrics") or "metrics" not in opt:
                continue
            if not isinstance(val, list):
                raise BadOptionTypeError("Metrics should be defined within a list")
            for metric in val:
                if not isinstance(metric, dict):
                    raise BadOptionTypeError(
                        "Each individual metrics must be defined within a JSON-like object"
                    )

        if self.script_type == ScriptType.xpcshell:
            result["flavor"] = "xpcshell"
        return result
