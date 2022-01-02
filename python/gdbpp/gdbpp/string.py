# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from gdbpp import GeckoPrettyPrinter


@GeckoPrettyPrinter("nsString", "^nsTString<.*>$")
class string_printer(object):
    def __init__(self, value):
        self.value = value

    def to_string(self):
        return self.value["mData"]

    def display_hint(self):
        return "string"
