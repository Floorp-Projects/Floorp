#! /usr/bin/python

import sys, time, os.path, StringIO

mode = 0
if len(sys.argv) > 1:
    mode = int(sys.argv[1])

if mode == 0:
    log2 = open("log2.out", "wt")
    log3 = open("log3.out", "wt")
elif mode == 1:
    # delete the logfiles first, and wait a moment to exercise a failure path
    if os.path.exists("log2.out"):
        os.unlink("log2.out")
    if os.path.exists("log3.out"):
        os.unlink("log3.out")
    time.sleep(2)
    log2 = open("log2.out", "wt")
    log3 = open("log3.out", "wt")
elif mode == 2:
    # don't create the logfiles at all
    log2 = StringIO.StringIO()
    log3 = StringIO.StringIO()

def write(i):
    log2.write("this is log2 %d\n" % i)
    log2.flush()
    log3.write("this is log3 %d\n" % i)
    log3.flush()
    sys.stdout.write("this is stdout %d\n" % i)
    sys.stdout.flush()

write(0)
time.sleep(1)
write(1)
sys.stdin.read(1)
write(2)

log2.close()
log3.close()

sys.exit(0)

