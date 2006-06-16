#!/usr/bin/python

import cgitb; cgitb.enable()

import sys
import cgi
import time
import re

from pysqlite2 import dbapi2 as sqlite

print "Content-type: text/plain\n\n"

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

if timeval is None:
    timeval = int(time.time())

if (value is None) or (tbox is None) or (testname is None):
    print "Bad args"
    sys.exit()

if re.match(r"[^A-Za-z0-9]", tbox):
    print "Bad tbox name"
    sys.exit()

db = sqlite.connect("db/" + tbox + ".sqlite")

try:
    db.execute("CREATE TABLE test_results (test_name STRING, test_time INTEGER, test_value FLOAT, test_data BLOB);")
    db.execute("CREATE TABLE annotations (anno_time INTEGER, anno_string STRING);")
    db.execute("CREATE INDEX test_name_idx ON test_results(test_name)")
    db.execute("CREATE INDEX test_time_idx ON test_results(test_time)")
    db.execute("CREATE INDEX anno_time_idx ON annotations(anno_time)")
except:
    pass

db.execute("INSERT INTO test_results VALUES (?,?,?,?)", (testname, timeval, value, data))

db.commit()

print "Inserted."

sys.exit()
