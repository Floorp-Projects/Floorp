#!/usr/bin/python

from optparse import OptionParser
from datetime import datetime
import sys
import os

o = OptionParser()
o.add_option("--print-buildid", action="store_true", dest="print_buildid")

(options, args) = o.parse_args()
buildid = os.environ.get('MOZ_BUILD_DATE', datetime.now().strftime('%Y%m%d%H'))

if options.print_buildid:
    print buildid
    sys.exit(0)

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
Milestone=%s""" % (buildid, milestone)
