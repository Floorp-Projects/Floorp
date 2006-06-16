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
# user=name
#  (REQUIRED) user that made this annotation
# tbox=foopy
#  (REQUIRED) name of the tinderbox to annotate
# data=string
#  annotation to record
# time=seconds
#  time since the epoch in GMT of this test result; if ommitted, current time at time of script run is used

tbox = form.getfirst("tbox")
user = form.getfirst("user")
data = form.getfirst("data")
timeval = form.getfirst("time")
if timeval is None:
    timeval = int(time.time())

if (user is None) or (tbox is None) or (data is None):
    print "Bad args"
    sys.exit()

if re.match(r"[^A-Za-z0-9]", tbox):
    print "Bad tbox name"
    sys.exit()

db = sqlite.connect("db/" + tbox + ".sqlite")

try:
    db.execute("CREATE TABLE test_results (test_name STRING, test_time INTEGER, test_value FLOAT, test_data BLOB);")
    db.execute("CREATE TABLE annotations (anno_user STRING, anno_time INTEGER, anno_string STRING);")
    db.execute("CREATE INDEX test_name_idx ON test_results.test_name")
    db.execute("CREATE INDEX test_time_idx ON test_results.test_time")
    db.execute("CREATE INDEX anno_time_idx ON annotations.anno_time")
except:
    pass

db.execute("INSERT INTO annotations VALUES (?,?,?)", (user, timeval, data))

db.commit()

print "Inserted."

sys.exit()
