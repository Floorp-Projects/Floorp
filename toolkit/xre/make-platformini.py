#!/usr/bin/python

from optparse import OptionParser
from datetime import datetime
import sys

(milestoneFile,) = sys.argv[1:]

for line in open(milestoneFile, 'r'):
    if line[0] == '#':
        continue

    line = line.strip()
    if line == '':
        continue

    milestone = line

print """[Build]
BuildID=%s
Milestone=%s""" % (datetime.now().strftime('%Y%m%d%H'), milestone)
