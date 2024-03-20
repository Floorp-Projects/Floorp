# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import json
import os.path
import re
import sys

BASE = os.path.dirname(__file__.replace("\\", "/"))
sys.path.insert(0, BASE)  # For importing `data.py`

from mako import exceptions
from mako.lookup import TemplateLookup
from mako.template import Template

import data

RE_PYTHON_ADDR = re.compile(r"<.+? object at 0x[0-9a-fA-F]+>")

OUT_DIR = os.environ.get("OUT_DIR", "")


def main():
    usage = (
        "Usage: %s [ servo | gecko ] [ style-crate | geckolib <template> ]"
        % sys.argv[0]
    )
    if len(sys.argv) < 3:
        abort(usage)
    engine = sys.argv[1]
    output = sys.argv[2]

    if engine not in ["servo", "gecko"] or output not in [
        "style-crate",
        "geckolib",
    ]:
        abort(usage)

    properties = data.PropertiesData(engine=engine)
    properties_template = os.path.join(BASE, "properties.mako.rs")
    properties_file = render(
        properties_template,
        engine=engine,
        data=properties,
        __file__=properties_template,
        OUT_DIR=OUT_DIR,
    )
    if output == "style-crate":
        write(OUT_DIR, "properties.rs", properties_file)

        if engine == "servo":
            properties_dict = {
                kind: {
                    p.name: {"pref": getattr(p, "servo_pref")}
                    for prop in properties_list
                    if prop.enabled_in_content()
                    for p in [prop] + prop.aliases
                }
                for kind, properties_list in [
                    ("longhands", properties.longhands),
                    ("shorthands", properties.shorthands),
                ]
            }
            as_html = render(
                os.path.join(BASE, "properties.html.mako"), properties=properties_dict
            )
            as_json = json.dumps(properties_dict, indent=4, sort_keys=True)
            doc_servo = os.path.join(BASE, "..", "..", "..", "target", "doc", "servo")
            write(doc_servo, "css-properties.html", as_html)
            write(doc_servo, "css-properties.json", as_json)
            write(OUT_DIR, "css-properties.json", as_json)
    elif output == "geckolib":
        if len(sys.argv) < 4:
            abort(usage)
        template = sys.argv[3]
        header = render(template, data=properties)
        sys.stdout.write(header)


def abort(message):
    print(message, file=sys.stderr)
    sys.exit(1)


def render(filename, **context):
    try:
        lookup = TemplateLookup(
            directories=[BASE], input_encoding="utf8", strict_undefined=True
        )
        template = Template(
            open(filename, "rb").read(),
            filename=filename,
            input_encoding="utf8",
            lookup=lookup,
            strict_undefined=True,
        )
        # Uncomment to debug generated Python code:
        # write("/tmp", "mako_%s.py" % os.path.basename(filename), template.code)
        return template.render(**context)
    except Exception:
        # Uncomment to see a traceback in generated Python code:
        # raise
        abort(exceptions.text_error_template().render())


def write(directory, filename, content):
    if not os.path.exists(directory):
        os.makedirs(directory)
    full_path = os.path.join(directory, filename)
    open(full_path, "w", encoding="utf-8").write(content)

    python_addr = RE_PYTHON_ADDR.search(content)
    if python_addr:
        abort('Found "{}" in {} ({})'.format(python_addr.group(0), filename, full_path))


if __name__ == "__main__":
    main()
