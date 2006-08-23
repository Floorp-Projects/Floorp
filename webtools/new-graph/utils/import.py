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
replace = False
if len(sys.argv) == 4:
    replace = True

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
                (tbox, testname, "perf"))
    res = cur.fetchall()
    cur.close()

    if len(res) == 0:
        db.execute("INSERT INTO dataset_info (machine, test, test_type) VALUES (?,?,?)",
                   (tbox, testname, "perf"))
    else:
        setid = res[0][0]

if replace:
    db.execute("DELETE FROM dataset_values WHERE dataset_id = ?", (setid,))
    db.execute("DELETE FROM dataset_extra_data WHERE dataset_id = ?", (setid,))

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

    db.execute("INSERT INTO dataset_values (dataset_id,time,value) VALUES (?,?,?)", (setid, timeval, val))
    db.execute("INSERT INTO dataset_extra_data (dataset_id,time,data) VALUES (?,?,?)", (setid, timeval, data))
    count = count + 1
    line = sys.stdin.readline()

db.commit()

print "Added", count, "values"
