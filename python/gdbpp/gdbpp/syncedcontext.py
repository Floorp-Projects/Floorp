# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

import gdb

from gdbpp import GeckoPrettyPrinter


@GeckoPrettyPrinter(
    "syncedcontext::FieldValues", "^mozilla::dom::syncedcontext::FieldValues<.*>$"
)
class synced_context_field_printer(object):
    def __init__(self, value):
        self.type = gdb.types.get_basic_type(value.type).template_argument(0)
        self.value = value.cast(self.type)
        self.IDX_re = re.compile(r"<(.*?),")

    def to_string(self):
        return ""

    def children(self):
        for base_field in self.type.fields():
            base = self.value[base_field]
            for field in base.type.fields():
                field_value = base[field]
                if gdb.types.has_field(field_value.type, "mField"):
                    field_name = self.IDX_re.search(f"{field_value.type.name}")
                    field_type = field.type.template_argument(1)
                    yield (
                        f"Field<{field_name[1]}, {field_type}>",
                        field_value["mField"],
                    )

    def display_hint(self):
        return "array"
