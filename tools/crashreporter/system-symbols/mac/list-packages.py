#!/usr/bin/env python

# Copyright 2015 Ted Mielczarek.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from __future__ import print_function, absolute_import

import os
import sys
from reposadolib import reposadocommon

reposadocommon.get_main_dir = lambda: "/usr/local/bin/"

products = reposadocommon.getProductInfo()
args = []
for product_id, p in products.iteritems():
    try:
        t = p["title"]
    except KeyError:
        print("Missing title in {}, skipping".format(p), file=sys.stderr)
        continue
    # p['CatalogEntry']['Packages']
    if t.startswith("OS X") or t.startswith("Mac OS X") or t.startswith("macOS"):
        args.append("--product-id=" + product_id)
    else:
        print("Skipping %r for repo_sync" % t, file=sys.stderr)
if "JUST_ONE_PACKAGE" in os.environ:
    args = args[:1]

print(" ".join(args))
