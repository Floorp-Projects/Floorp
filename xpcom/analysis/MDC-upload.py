#!/usr/bin/env python

"""
Upload a page to MDC

Usage: python MDC-upload.py <file> <MDC-path>
Please set MDC_USER and MDC_PASSWORD in the environment
"""

import os, sys, deki

wikiuser = os.environ['MDC_USER']
wikipw = os.environ['MDC_PASSWORD']

(file, wikipath) = sys.argv[1:]

wiki = deki.Deki("http://developer.mozilla.org/@api/deki/", wikiuser, wikipw)
wiki.create_page(wikipath, open(file).read(), overwrite=True)
