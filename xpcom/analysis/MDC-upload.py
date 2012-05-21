#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
