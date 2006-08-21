#!/usr/bin/python

import cgitb; cgitb.enable()

import sys
import cgi
import time
import re

from pysqlite2 import dbapi2 as sqlite

print "Content-type: text/plain\n\n"

DBPATH = "db/data.sqlite"

form = cgi.FieldStorage()

# incoming query string has the following parameters:
# value=n
#  (REQUIRED) value to be recorded as the actual test value
# tbox=foopy
#  (REQUIRED) name of the tinderbox reporting the value (or rather, the name that is to be given this set of data)
# testname=test
#  (REQUIRED) the name of this test
# data=rawdata
#  raw data for this test
# time=seconds
#  time since the epoch in GMT of this test result; if ommitted, current time at time of script run is used

value = form.getfirst("value")
data = form.getfirst("data")
tbox = form.getfirst("tbox")
testname = form.getfirst("testname")
timeval = form.getfirst("time")
branch = form.getfirst("branch")

if timeval is None:
    timeval = int(time.time())

if (value is None) or (tbox is None) or (testname is None):
    print "Bad args"
    sys.exit()

if re.match(r"[^A-Za-z0-9_-]", tbox):
    print "Bad tbox name"
    sys.exit()

if re.match(r"[^A-Za-z0-9_-]", branch):
    print "Bad branch name"
    sys.exit()

db = sqlite.connect(DBPATH)

# Create the DB schema if it doesn't already exist
# XXX can pull out dataset_info.machine and dataset_info.{test,test_type} into two separate tables,
# if we need to.
try:
    db.execute("CREATE TABLE dataset_info (id INTEGER PRIMARY KEY AUTOINCREMENT, machine STRING, test STRING, test_type STRING, extra_data STRING);")
    db.execute("CREATE TABLE datasets (dataset_id INTEGER, time INTEGER, value FLOAT, extra BLOB);")
    db.execute("CREATE TABLE annotations (dataset_id INTEGER, time INTEGER, value STRING);")
    db.execute("CREATE INDEX datasets_id_idx ON datasets(dataset_id);")
    db.execute("CREATE INDEX datasets_time_idx ON datasets(time);")
except:
    pass

# figure out our dataset id
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

db.execute("INSERT INTO datasets (dataset_id, time, value, extra) VALUES (?,?,?,?)", (setid, timeval, value, data))

db.commit()

print "Inserted."

sys.exit()
