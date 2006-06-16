#!/usr/bin/python

import cgitb; cgitb.enable()

import os
import sys
import cgi
import time
import re
import gzip

import cStringIO

from pysqlite2 import dbapi2 as sqlite

#
# All objects are returned in the form:
# {
#  resultcode: n,
#  ...
# }
#
# The ... is dependant on the result type.
#
# Result codes:
#   0 success
#  -1 bad tinderbox
#  -2 bad test name
#
# incoming query string:
# tbox=name
#  tinderbox name
#
# If only tbox specified, returns array of test names for that tinderbox in data
# If invalid tbox specified, returns error -1
#
# test=testname
#  test name
#
# Returns results for that test in .results, in array of [time0, value0, time1, value1, ...]
# Also reteurns .annotations, in array of [time0, string0, time1, string1, ...]
#
# raw=1
# Same as full results, but includes raw data for test, in form [time0, value0, rawresult0, ...]
#
# starttime=tval
#  Start time to return results from, in seconds since GMT epoch
# endtime=tval
#  End time, in seconds since GMT epoch
#

def doError(errCode):
    errString = "unknown error"
    if errCode == -1:
        errString = "bad tinderbox"
    elif errCode == -2:
        errString = "bad test name"
    print "{ resultcode: " + str(errCode) + ", error: '" + errString + "' }"

db = None

def doListTinderboxes(fo):
    fo.write ("{ resultcode: 0, results: [")
    for f in os.listdir("db"):
        if f[-7:] == ".sqlite":
            fo.write ("'" + f[:-7] + "',")
    fo.write ("] }")

def doListTests(fo, tbox):
    fo.write ("{ resultcode: 0, results: [")
    
    cur = db.cursor()
    cur.execute("SELECT DISTINCT(test_name) FROM test_results")
    for row in cur:
        fo.write ("'" + row[0] + "',")
    cur.close()
    fo.write ("] }")

def doSendResults(fo, tbox, test, starttime, endtime, raw):
    raws = ""
    if raw is not None:
        raws = ", test_data"
    s1 = ""
    s2 = ""
    if starttime is not None:
        s1 = " AND test_time >= " + starttime
    if endtime is not None:
        s2 = " AND test_time <= " + endtime

    stmt = "SELECT test_time, test_value" + raws + " FROM test_results WHERE test_name = '" + test + "' " + s1 + s2 + " ORDER BY test_time";

    fo.write ("{ resultcode: 0, results: [")

    cur = db.cursor()
    cur.execute(stmt)
    for row in cur:
        if raw:
            fo.write ("%s,%s,'%s'," % (row[0], row[1], row[2]))
        else:
            fo.write ("%s,%s," % (row[0], row[1]))
    cur.close()
    fo.write ("],")

    cur = db.cursor()
    if starttime is not None and endtime is not None:
        cur.execute("SELECT anno_time, anno_string FROM annotations WHERE anno_time >= " + starttime + " AND anno_time <= " + endtime + " ORDER BY anno_time")
    elif starttime is not None:
        cur.execute("SELECT anno_time, anno_string FROM annotations WHERE anno_time >= " + starttime + " ORDER BY anno_time")
    elif endtime is not None:
        cur.execute("SELECT anno_time, anno_string FROM annotations WHERE anno_time <= " + endtime + " ORDER BY anno_time")
    else:
        cur.execute("SELECT anno_time, anno_string FROM annotations ORDER BY anno_time")
    fo.write ("annotations: [")
    
    for row in cur:
        fo.write("%s,'%s'," % (row[0], row[1]))
    cur.close()
    fo.write ("] }")

def main():
    doGzip = 0
    try:
        if string.find(os.environ["HTTP_ACCEPT_ENCODING"], "gzip") != -1:
            doGzip = 1
    except:
        pass

    form = cgi.FieldStorage()

    tbox = form.getfirst("tbox")
    test = form.getfirst("test")
    raw = form.getfirst("raw")
    starttime = form.getfirst("starttime")
    endtime = form.getfirst("endtime")

    zbuf = cStringIO.StringIO()
    zfile = zbuf
    if doGzip == 1:
        zfile = gzip.GzipFile(mode = 'wb', fileobj = zbuf, compresslevel = 5)

    if tbox is None:
        doListTinderboxes(zfile)
    else:
        if re.match(r"[^A-Za-z0-9]", tbox):
            doError(-1)
            return

        dbfile = "db/" + tbox + ".sqlite"
        if not os.path.isfile(dbfile):
            doError(-1)
            return

        global db
        db = sqlite.connect(dbfile)
        db.text_factory = sqlite.OptimizedUnicode
    
        if test is None:
            doListTests(zfile, tbox)
        else:
            doSendResults(zfile, tbox, test, starttime, endtime, raw)

    sys.stdout.write("Content-Type: text/plain\n")
    if doGzip == 1:
        zfile.close()
        sys.stdout.write("Content-Encoding: gzip\n")
    sys.stdout.write("\n")

    sys.stdout.write(zbuf.getvalue())

main()


