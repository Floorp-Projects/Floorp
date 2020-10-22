#!/usr/bin/env python
""" This script is an example of why I care so much about Mozharness' 2nd core
concept, logging.  http://escapewindow.dreamwidth.org/230853.html
"""

import os
import shutil

#print "downloading foo.tar.bz2..."
os.system("curl -s -o foo.tar.bz2 http://people.mozilla.org/~asasaki/foo.tar.bz2")
#os.system("curl -v -o foo.tar.bz2 http://people.mozilla.org/~asasaki/foo.tar.bz2")

#os.rename("foo.tar.bz2", "foo3.tar.bz2")
os.system("tar xjf foo.tar.bz2")

#os.chdir("x")
os.remove("x/ship2")
os.remove("foo.tar.bz2")
os.system("tar cjf foo.tar.bz2 x")
shutil.rmtree("x")
#os.system("scp -q foo.tar.bz2 people.mozilla.org:public_html/foo2.tar.bz2")
os.remove("foo.tar.bz2")
