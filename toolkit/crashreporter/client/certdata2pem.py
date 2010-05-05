#!/usr/bin/python
# vim:set et sw=4:
#
# Originally from:
# http://cvs.fedoraproject.org/viewvc/F-13/ca-certificates/certdata2pem.py?revision=1.1&content-type=text%2Fplain&view=co
#
# certdata2pem.py - converts certdata.txt into PEM format. 
#
# Copyright (C) 2009 Philipp Kern <pkern@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.

import base64
import os.path
import re
import sys
import textwrap

objects = []

# Dirty file parser.
in_data, in_multiline, in_obj = False, False, False
field, type, value, obj = None, None, None, dict()
for line in sys.stdin: 
    # Ignore the file header.
    if not in_data:
        if line.startswith('BEGINDATA'):
            in_data = True
        continue
    # Ignore comment lines.
    if line.startswith('#'):
        continue
    # Empty lines are significant if we are inside an object.
    if in_obj and len(line.strip()) == 0:
        objects.append(obj)
        obj = dict()
        in_obj = False
        continue
    if len(line.strip()) == 0:
        continue
    if in_multiline:
        if not line.startswith('END'):
            if type == 'MULTILINE_OCTAL':
                line = line.strip()
                for i in re.finditer(r'\\([0-3][0-7][0-7])', line):
                    value += chr(int(i.group(1), 8))
            else:
                value += line
            continue
        obj[field] = value
        in_multiline = False
        continue
    if line.startswith('CKA_CLASS'):
        in_obj = True
    line_parts = line.strip().split(' ', 2)
    if len(line_parts) > 2:
        field, type = line_parts[0:2]
        value = ' '.join(line_parts[2:])
    elif len(line_parts) == 2:
        field, type = line_parts
        value = None
    else:
        raise NotImplementedError, 'line_parts < 2 not supported.'
    if type == 'MULTILINE_OCTAL':
        in_multiline = True
        value = ""
        continue
    obj[field] = value
if len(obj.items()) > 0:
    objects.append(obj)

# Build up trust database.
trust = dict()
for obj in objects:
    if obj['CKA_CLASS'] != 'CKO_NETSCAPE_TRUST':
        continue
    # For some reason, OpenSSL on Maemo has a bug where if we include
    # this certificate, and it winds up as the last certificate in the file,
    # then OpenSSL is unable to verify the server certificate. For now,
    # we'll just omit this particular CA cert, since it's not one we need
    # for crash reporting.
    # This is likely to be fragile if the NSS certdata.txt changes.
    # The bug is filed upstream:
    # https://bugs.maemo.org/show_bug.cgi?id=10069
    if obj['CKA_LABEL'] == '"ACEDICOM Root"':
        continue
    # We only want certs that are trusted for SSL server auth
    if obj['CKA_TRUST_SERVER_AUTH'] == 'CKT_NETSCAPE_TRUSTED_DELEGATOR':
        trust[obj['CKA_LABEL']] = True

for obj in objects:
    if obj['CKA_CLASS'] == 'CKO_CERTIFICATE':
        if not obj['CKA_LABEL'] in trust or not trust[obj['CKA_LABEL']]:
            continue
        sys.stdout.write("-----BEGIN CERTIFICATE-----\n")
        sys.stdout.write("\n".join(textwrap.wrap(base64.b64encode(obj['CKA_VALUE']), 64)))
        sys.stdout.write("\n-----END CERTIFICATE-----\n\n")

