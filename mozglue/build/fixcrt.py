# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement

with open('crtdll.obj', 'rb') as infile:
  data = infile.read()
  with open('crtdll_fixed.obj', 'wb') as outfile:
    # for Win32
    data = data.replace('__imp__free', '__imp__frex')
    # for Win64
    data = data.replace('__imp_free', '__imp_frex')
    outfile.write(data)
