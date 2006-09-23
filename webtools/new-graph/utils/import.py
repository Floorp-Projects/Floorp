#!/usr/bin/env python

import string
import sys
import time
import re
from optparse import OptionParser
from optparse import OptionValueError

from pysqlite2 import dbapi2 as sqlite

parser = OptionParser(usage="Usage: %prog [options] test_name tinderbox_name < data.txt")
parser.add_option("-b", "--baseline", dest="baseline", metavar="NAME",
                  action="store", default=None,
                  help="use as baseline data for a test of the same name")
parser.add_option("-r", "--replace", dest="replace",
                  action="store_true", default=False,
                  help="remove current data for this test and re-add all data")
(options, args) = parser.parse_args()

if options.baseline == None:
    test_type = "perf"
else:
    test_type = "baseline"

if len(args) != 2:
    parser.print_help()
    sys.exit()

(testname, tbox) = args[0:2]

DBPATH = "db/data.sqlite"
db = sqlite.connect(DBPATH)

try:
    db.execute("CREATE TABLE dataset_info (id INTEGER PRIMARY KEY AUTOINCREMENT, machine STRING, test STRING, test_type STRING, extra_data STRING);")
    db.execute("CREATE TABLE dataset_values (dataset_id INTEGER, time INTEGER, value FLOAT);")
    db.execute("CREATE TABLE dataset_extra_data (dataset_id INTEGER, time INTEGER, data BLOB);");
    db.execute("CREATE TABLE annotations (dataset_id INTEGER, time INTEGER, value STRING);")
    db.execute("CREATE INDEX datasets_id_idx ON dataset_values(dataset_id);")
    db.execute("CREATE INDEX datasets_time_idx ON dataset_values(time);")
except:
    pass

setid = -1
while setid == -1:
    cur = db.cursor()
    cur.execute("SELECT id FROM dataset_info WHERE machine=? AND test=? AND test_type=?",
                (tbox, testname, test_type))
    res = cur.fetchall()
    cur.close()

    if len(res) == 0:
        db.execute("INSERT INTO dataset_info (machine, test, test_type, extra_data) VALUES (?,?,?,?)",
                   (tbox, testname, test_type, options.baseline))
    else:
        setid = res[0][0]

if options.replace:
    db.execute("DELETE FROM dataset_values WHERE dataset_id = ?", (setid,))
    db.execute("DELETE FROM dataset_extra_data WHERE dataset_id = ?", (setid,))

cur = db.cursor()
cur.execute("SELECT time FROM dataset_values WHERE dataset_id = ? ORDER BY time DESC LIMIT 1", (setid,))
latest = cur.fetchone()
if latest == None:
    latest = 0
else:
    latest = latest[0]

db.commit() # release any locks

count = 0
linenum = 0
line = sys.stdin.readline()
while line is not None:
    linenum = linenum + 1

    chunks = string.split(line, "\t")
    if len(chunks) == 1 and chunks[0] == '':
        break # don't warn about empty lines
    if len(chunks) != 6 and len(chunks) != 7:
        print "chunks not 6 or 7:", len(chunks), "line#", linenum, "value:", chunks
        break

    if len(chunks) == 6:
        (datestr, val, data, ip, tinderbox, ua) = chunks[0:6]
    else:
        (datestr, val, codate, data, ip, tinderbox, ua) = chunks[0:7]
        foo = string.split(codate, '=')
        if foo[0] == "MOZ_CO_DATE":
            datestr = foo[1]

    timeval = time.mktime(map(int, string.split(datestr, ":")) + [0, 0, 0])

    if timeval > latest:
        db.execute("INSERT INTO dataset_values (dataset_id,time,value) VALUES (?,?,?)", (setid, timeval, val))
        db.execute("INSERT INTO dataset_extra_data (dataset_id,time,data) VALUES (?,?,?)", (setid, timeval, data))
        count = count + 1

    line = sys.stdin.readline()

db.commit()

print "Added", count, "values"
