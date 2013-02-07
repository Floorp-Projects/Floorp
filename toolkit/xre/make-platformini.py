#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from optparse import OptionParser
from datetime import datetime
import time
import sys
import os

o = OptionParser()
o.add_option("--buildid", dest="buildid")
o.add_option("--print-buildid", action="store_true", dest="print_buildid")
o.add_option("--print-timestamp", action="store_true", dest="print_timestamp")
o.add_option("--sourcestamp", dest="sourcestamp")
o.add_option("--sourcerepo", dest="sourcerepo")

(options, args) = o.parse_args()

if options.print_timestamp:
    print int(time.time())
    sys.exit(0)

if options.print_buildid:
    print datetime.now().strftime('%Y%m%d%H%M%S')
    sys.exit(0)

if not options.buildid:
    print >>sys.stderr, "--buildid is required"
    sys.exit(1)

(milestoneFile,) = args
for line in open(milestoneFile, 'r'):
    if line[0] == '#':
        continue

    line = line.strip()
    if line == '':
        continue

    milestone = line

print """[Build]
BuildID=%s
Milestone=%s""" % (options.buildid, milestone)
if options.sourcestamp:
    print "SourceStamp=%s" % options.sourcestamp
if options.sourcerepo:
    print "SourceRepository=%s" % options.sourcerepo
