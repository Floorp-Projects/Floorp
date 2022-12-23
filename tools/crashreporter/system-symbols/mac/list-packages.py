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

import os
import sys

from reposadolib import reposadocommon

reposadocommon.get_main_dir = lambda: "/usr/local/bin/"

products = reposadocommon.get_product_info()
args = []
for product_id, product in products.items():
    try:
        title = product["title"]
    except KeyError:
        print("Missing title in {}, skipping".format(product), file=sys.stderr)
        continue

    try:
        major_version = int(product["version"].split(".")[0])
    except Exception:
        print(
            "Cannot extract the major version number in {}, skipping".format(product),
            file=sys.stderr,
        )
        continue

    if (
        title.startswith("OS X")
        or title.startswith("Mac OS X")
        or title.startswith("macOS")
    ) and major_version <= 10:
        args.append(product_id)
    else:
        print("Skipping %r for repo_sync" % title, file=sys.stderr)
if "JUST_ONE_PACKAGE" in os.environ:
    args = args[:1]

print("\n".join(args))
