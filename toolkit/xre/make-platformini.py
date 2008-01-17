#!/usr/bin/python

from optparse import OptionParser
from datetime import datetime
import sys
import os

o = OptionParser()
o.add_option("--buildid", dest="buildid")
o.add_option("--print-buildid", action="store_true", dest="print_buildid")

(options, args) = o.parse_args()

if options.print_buildid:
    print datetime.now().strftime('%Y%m%d%H')
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
