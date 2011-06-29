#!/usr/bin/env python
# Copyright 2011 Mozilla Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE MOZILLA FOUNDATION ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE MOZILLA FOUNDATION OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation
# are those of the authors and should not be interpreted as representing
# official policies, either expressed or implied, of the Mozilla
# Foundation.

import sys, os

def sumDirectorySize(path):
    """
    Calculate the total size of all the files under |path|.
    """
    total = 0
    def reraise(e):
        raise e
    for root, dirs, files in os.walk(path, onerror=reraise):
        for f in files:
            total += os.path.getsize(os.path.join(root, f))
    return total

def codesighs(stagepath, installerpath):
    """
    Calculate the total size of the files under |stagepath| on disk
    and print it as "__codesize", and print the size of the file at
    |installerpath| as "__installersize".
    """
    try:
        print "__codesize:%d" % sumDirectorySize(stagepath)
        print "__installersize:%d" % os.path.getsize(installerpath)
    except OSError, e:
        print >>sys.stderr, """Couldn't read file %s.
Perhaps you need to run |make package| or |make installer|?""" % e.filename

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print >>sys.stderr, "Usage: codesighs.py <package stage path> <installer path>"
        sys.exit(1)
    codesighs(sys.argv[1], sys.argv[2])
