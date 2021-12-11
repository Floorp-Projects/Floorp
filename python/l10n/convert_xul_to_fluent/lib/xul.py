# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import re
import string

tag_re = r"<([a-z]+[^>/]*)(/?>)([^<]*)(?:</[a-z]+>)?"
attr_re = r'\s+([a-z]+)="([\&\;\.a-zA-Z0-9]+)"'
prefix = ""

messages = {}


def is_entity(s):
    return s.startswith("&") and s.endswith(";")


def convert_camel_case(name):
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1-\2", name)
    return re.sub("([a-z0-9])([A-Z])", r"\1-\2", s1).lower()


def construct_l10n_id(val, attrs):
    global prefix
    id = None
    if val:
        id = val[1:-1].strip(string.digits)
    else:
        core = None
        for k in attrs:
            v = attrs[k][1:-1].strip(string.digits).split(".")
            if not core:
                if v[-1].lower() == k:
                    core = ".".join(v[:-1])
                else:
                    core = ".".join(v)
        if core:
            id = core
    id = id.replace(".", "-")
    id = convert_camel_case(id)
    if prefix:
        id = "{}-{}".format(prefix, id)
    return id


vector = 0
is_l10n = False


def get_indent(pre_tag):
    if "\n" not in pre_tag:
        return " "
    last_bl = pre_tag.rindex("\n")
    indent = 0
    for ch in pre_tag[last_bl:]:
        if ch == " ":
            indent += 1
    return "\n" + " " * indent


def tagrepl(m):
    global vector
    global is_l10n
    vector = 0

    is_l10n = False
    l10n_val = None
    l10n_attrs = {}
    if is_entity(m.group(3)):
        is_l10n = True
        l10n_val = m.group(3)

    def attrrepl(m2):
        global vector
        global is_l10n
        attr_l10n = False
        if is_entity(m2.group(2)):
            attr_l10n = True
            l10n_attrs[m2.group(1)] = m2.group(2)
        if attr_l10n:
            is_l10n = True
            vector = vector + len(m2.group(0))
            return ""
        return m2.group(0)

    tag = re.sub(attr_re, attrrepl, m.group(0))
    if is_l10n:
        l10n_id = construct_l10n_id(l10n_val, l10n_attrs)
        messages[l10n_id] = {"value": l10n_val, "attrs": l10n_attrs}
        indent = get_indent(tag[0 : len(m.group(1)) + 1 - vector])
        tag = (
            tag[0 : len(m.group(1)) + 1 - vector]
            + indent
            + 'data-l10n-id="'
            + l10n_id
            + '"'
            + m.group(2)
            + (m.group(3) if not l10n_val else "")
            + tag[len(m.group(1)) + 1 + len(m.group(2)) + len(m.group(3)) - vector :]
        )
    return tag


def collect_messages(xul_source, in_prefix):
    global messages
    global prefix
    messages = {}
    prefix = in_prefix

    new_source = re.sub(tag_re, tagrepl, xul_source)
    return (new_source, messages)


if __name__ == "__main__":
    pass
