#!/usr/bin/python

import string
import sys
import time
import re

from pysqlite2 import dbapi2 as sqlite

if len(sys.argv) != 3:
    print "Usage: import.py test_name tinderbox_name < data.txt"
    sys.exit()

(testname, tbox) = sys.argv[1:3]

db = sqlite.connect("db/" + tbox + ".sqlite")

try:
    db.execute("CREATE TABLE test_results (test_name STRING, test_time INTEGER, test_value FLOAT, test_data BLOB);")
    db.execute("CREATE TABLE annotations (anno_time INTEGER, anno_string STRING);")
    db.execute("CREATE INDEX test_name_idx ON test_results(test_name)")
    db.execute("CREATE INDEX test_time_idx ON test_results(test_time)")
    db.execute("CREATE INDEX anno_time_idx ON annotations(anno_time)")
except:
    pass

count = 0
line = sys.stdin.readline()
while line is not None:
    chunks = string.split(line, "\t")
    if len(chunks) != 6 and len(chunks) != 7:
        print "chunks not 6 or 7:", len(chunks)
        break
    if len(chunks) == 6:
        (datestr, val, data, ip, tinderbox, ua) = chunks[0:6]
    elif len(chunks) == 7:
        (datestr, val, codate, data, ip, tinderbox, ua) = chunks[0:7]
    else:
        raise "Unknown chunk length"

    timeval = time.mktime(map(int, string.split(datestr, ":")) + [0, 0, 0])

    db.execute("INSERT INTO test_results VALUES (?, ?, ?, ?)", (testname, timeval, val, data))
    count = count + 1
    line = sys.stdin.readline()

db.commit()

print "Added", count, "values"
