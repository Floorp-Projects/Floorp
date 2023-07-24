# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

begin_script_only_re = re.compile("^// #BEGIN_SCRIPT_ONLY")
end_script_only_re = re.compile("^// #END_SCRIPT_ONLY")
export_re = re.compile("^// #EXPORT (.+)")


def process_file(template_js, kind):
    lines = []
    is_script_only = False
    exports = []

    with open(template_js, "r") as f:
        for line in f:
            if kind == "module":
                if is_script_only:
                    m = end_script_only_re.match(line)
                    if m:
                        is_script_only = False

                    # NOTE: Put an empty line to keep the line number same.
                    lines.append("\n")
                    continue
                else:
                    m = begin_script_only_re.match(line)
                    if m:
                        is_script_only = True
                        lines.append("\n")
                        continue
            else:
                m = end_script_only_re.match(line)
                if m:
                    lines.append("\n")
                    continue

                m = begin_script_only_re.match(line)
                if m:
                    lines.append("\n")
                    continue

            m = export_re.match(line)
            if m:
                name = m.group(1)

                if kind == "script":
                    lines.append(f"exports.{name} = {name};\n")
                else:
                    exports.append(name)
                    lines.append("\n")
                continue

            lines.append(line)

    if kind == "module":
        lines.append("export const PromiseWorker = { " + ", ".join(exports) + " };\n")

    return "".join(lines)


def generate_script(output, template_js):
    output.write(process_file(template_js, "script"))


def generate_module(output, template_js):
    output.write(process_file(template_js, "module"))
