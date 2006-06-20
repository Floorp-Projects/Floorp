#!/usr/bin/python
#
# bonsaibouncer
#
# Bounce a request to bonsai.mozilla.org, getting around silly
# cross-domain XMLHTTPRequest deficiencies.
#

import cgitb; cgitb.enable()

import os
import sys
import cgi
import gzip
import urllib
from urllib import quote

import cStringIO

bonsai = "http://bonsai.mozilla.org/cvsquery.cgi";

def main():
    #doGzip = 0
    #try:
    #    if string.find(os.environ["HTTP_ACCEPT_ENCODING"], "gzip") != -1:
    #        doGzip = 1
    #except:
    #    pass

    form = cgi.FieldStorage()
    
    treeid = form.getfirst("treeid")
    module = form.getfirst("module")
    branch = form.getfirst("branch")
    mindate = form.getfirst("mindate")
    maxdate = form.getfirst("maxdate")
    xml_nofiles = form.getfirst("xml_nofiles")

    if not treeid or not module or not branch or not mindate or not maxdate:
        print "Content-type: text/plain\n\n"
        print "ERROR"
        return
    
    url = bonsai + "?" + "branchtype=match&sortby=Date&date=explicit&cvsroot=%2Fcvsroot&xml=1"
    url += "&treeid=%s&module=%s&branch=%s&mindate=%s&maxdate=%s" % (quote(treeid), quote(module), quote(branch), quote(mindate), quote(maxdate))
    
    if (xml_nofiles):
        url += "&xml_nofiles=1"

    urlstream = urllib.urlopen(url)
    
    sys.stdout.write("Content-type: text/xml\n\n")

    for s in urlstream:
        sys.stdout.write(s)

    urlstream.close()

main()
