#!/usr/bin/env python
# Copyright 2015 Ted Mielczarek. See the LICENSE
# file at the top-level directory of this distribution.
from __future__ import print_function

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
