# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def add_define(out_file, in_path, expr, num = ''):
    out_file.write('#define %s %s\n' % (expr, num))
    with open(in_path, 'r', encoding='utf-8') as fh:
        out_file.write(fh.read())
