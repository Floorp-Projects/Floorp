#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" Helper script for download_file()

We lose some mozharness functionality by splitting this out, but we gain output_timeout.
"""

import os
import socket
import sys
import urllib2
import urlparse


def download_file(url, file_name):
    try:
        f_length = None
        f = urllib2.urlopen(url, timeout=30)
        if f.info().get('content-length') is not None:
            f_length = int(f.info()['content-length'])
            got_length = 0
        local_file = open(file_name, 'wb')
        while True:
            block = f.read(1024 ** 2)
            if not block:
                if f_length is not None and got_length != f_length:
                    raise urllib2.URLError("Download incomplete; content-length was %d, but only received %d" % (f_length, got_length))
                break
            local_file.write(block)
            if f_length is not None:
                got_length += len(block)
        local_file.close()
        print "%s downloaded to %s" % (url, file_name)
    except urllib2.HTTPError, e:
        print "Warning: Server returned status %s %s for %s" % (str(e.code), str(e), url)
        raise
    except urllib2.URLError, e:
        print "URL Error: %s" % url
        remote_host = urlparse.urlsplit(url)[1]
        if remote_host:
            os.system("nslookup %s" % remote_host)
        raise
    except socket.timeout, e:
        print "Timed out accessing %s: %s" % (url, str(e))
        raise
    except socket.error, e:
        print "Socket error when accessing %s: %s" % (url, str(e))
        raise

if __name__ == '__main__':
    if len(sys.argv) != 3:
        if len(sys.argv) != 2:
            print "Usage: download_file.py URL [FILENAME]"
            sys.exit(-1)
        parts = urlparse.urlparse(sys.argv[1])
        file_name = parts[2].split('/')[-1]
    else:
        file_name = sys.argv[2]
    if os.path.exists(file_name):
        print "%s exists; removing" % file_name
        os.remove(file_name)
    if os.path.exists(file_name):
        print "%s still exists; exiting"
        sys.exit(-1)
    download_file(sys.argv[1], file_name)
