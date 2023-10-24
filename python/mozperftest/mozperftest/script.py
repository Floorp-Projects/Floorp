# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import io
import re
import textwrap
import traceback
from collections import defaultdict
from enum import Enum
from html.parser import HTMLParser
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


class MissingPerfMetadataError(Exception):
    def __init__(self, script):
        super().__init__("Missing `perfMetadata` variable")
        self.script = script


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
    mochitest = 3


class HTMLScriptParser(HTMLParser):
    def handle_data(self, data):
        if self.script_content is None:
            self.script_content = []
        if "perfMetadata" in data:
            self.script_content.append(data)
        if any(func_name in data for func_name in XPCSHELL_FUNCS):
            self.script_content.append(data)


class ScriptInfo(defaultdict):
    """Loads and parses a Browsertime test script."""

    def __init__(self, path):
        super(ScriptInfo, self).__init__()
        try:
            self.script = Path(path).resolve()
            if self.script.suffix == ".html":
                self._parse_html_file()
            else:
                self._parse_js_file()
        except Exception as e:
            raise ParseError(path, e)

        # If the fields found, don't match our known ones, then an error is raised
        for field, required in METADATA:
            if not required:
                continue
            if field not in self:
                raise MissingFieldError(path, field)

    def _set_script_content(self):
        self["filename"] = str(self.script)
        self.script_content = self.script.read_text()

    def _parse_js_file(self):
        self.script_type = ScriptType.browsertime
        self._set_script_content()
        self._parse_script_content()

    def _parse_script_content(self):
        self.parsed = esprima.parseScript(self.script_content)

        # looking for the exports statement
        found_perfmetadata = False
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
                    found_perfmetadata = True
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
            found_perfmetadata = True
            self.scan_properties(stmt.expression.right.properties)

        if not found_perfmetadata:
            raise MissingPerfMetadataError(self.script)

    def _parse_html_file(self):
        self._set_script_content()

        html_parser = HTMLScriptParser()
        html_parser.script_content = None
        html_parser.feed(self.script_content)

        if not html_parser.script_content:
            raise MissingPerfMetadataError(self.script)

        # Pass through all the scripts and gather up the data such as
        # the test itself, and the perfMetadata. These can be in separate
        # scripts, but later scripts override earlier ones if there
        # are redefinitions.
        found_perfmetadata = False
        for script_content in html_parser.script_content:
            self.script_content = script_content
            try:
                self._parse_script_content()
                found_perfmetadata = True
            except MissingPerfMetadataError:
                pass
        if not found_perfmetadata:
            raise MissingPerfMetadataError()

        # Mochitest gets detected as xpcshell during parsing
        # since they use similar methods to run tests
        self.script_type = ScriptType.mochitest

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
                        val = _render(val, level + 1)  # noqa
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
        if self.script_type == ScriptType.mochitest:
            result["flavor"] = "mochitest"

        return result
