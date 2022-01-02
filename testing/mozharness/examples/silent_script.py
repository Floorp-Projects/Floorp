#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" This script is an example of why I care so much about Mozharness' 2nd core
concept, logging.  http://escapewindow.dreamwidth.org/230853.html
"""

from __future__ import absolute_import
import os
import shutil

# print "downloading foo.tar.bz2..."
os.system("curl -s -o foo.tar.bz2 http://people.mozilla.org/~asasaki/foo.tar.bz2")
# os.system("curl -v -o foo.tar.bz2 http://people.mozilla.org/~asasaki/foo.tar.bz2")

# os.rename("foo.tar.bz2", "foo3.tar.bz2")
os.system("tar xjf foo.tar.bz2")

# os.chdir("x")
os.remove("x/ship2")
os.remove("foo.tar.bz2")
os.system("tar cjf foo.tar.bz2 x")
shutil.rmtree("x")
# os.system("scp -q foo.tar.bz2 people.mozilla.org:public_html/foo2.tar.bz2")
os.remove("foo.tar.bz2")
