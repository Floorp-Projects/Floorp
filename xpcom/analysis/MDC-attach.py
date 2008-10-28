#!/usr/bin/env python

"""
Upload a file attachment to MDC

Usage: python MDC-attach.py <file> <parent page name> <MIME type> <description>
Please set MDC_USER and MDC_PASSWORD in the environment
"""

import os, sys, deki

wikiuser = os.environ['MDC_USER']
wikipw = os.environ['MDC_PASSWORD']

file, pageid, mimetype, description = sys.argv[1:]

wiki = deki.Deki("http://developer.mozilla.org/@api/deki/", wikiuser, wikipw)
wiki.create_file(pageid, os.path.basename(file), open(file).read(), mimetype,
                 description)
