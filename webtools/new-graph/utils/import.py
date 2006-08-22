#!/usr/bin/python

import string
import sys
import time
import re

from pysqlite2 import dbapi2 as sqlite

if len(sys.argv) < 3 or len(sys.argv) > 4:
    print "Usage: import.py test_name tinderbox_name [replace] < data.txt"
    sys.exit()

(testname, tbox) = sys.argv[1:3]
replace = false
if len(sys.argv) == 4:
    replace = true

DBPATH = "db/data.sqlite"
db = sqlite.connect(DBPATH)

try:
    db.execute("CREATE TABLE dataset_info (id INTEGER PRIMARY KEY AUTOINCREMENT, machine STRING, test STRING, test_type STRING, extra_data STRING);")
    db.execute("CREATE TABLE datasets (dataset_id INTEGER, time INTEGER, value FLOAT, extra BLOB);")
    db.execute("CREATE TABLE annotations (dataset_id INTEGER, time INTEGER, value STRING);")
    db.execute("CREATE INDEX datasets_id_idx ON datasets(dataset_id);")
    db.execute("CREATE INDEX datasets_time_idx ON datasets(time);")
except:
    pass

setid = -1
while setid == -1:
    cur = db.cursor()
    cur.execute("SELECT id FROM dataset_info WHERE machine=? AND test=? AND test_type=? AND extra=?",
                (tbox, testname, "perf", "branch="+branch))
    res = cur.fetchall()
    cur.close()

    if len(res) == 0:
        db.execute("INSERT INTO dataset_info (machine, test, test_type, extra) VALUES (?,?,?,?)",
                   (tbox, testname, "perf", "branch="+branch))
    else:
        setid = res[0]

if replace:
    db.execute("DELETE FROM datasets WHERE dataset_id = ?", (setid))

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

    db.execute("INSERT INTO datasets (dataset_id,time,value,extra) VALUES (?,?,?,?)", (setid, timeval, val, data))
    count = count + 1
    line = sys.stdin.readline()

db.commit()

print "Added", count, "values"
