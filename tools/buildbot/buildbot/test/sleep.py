#! /usr/bin/python

import sys, time
delay = int(sys.argv[1])

sys.stdout.write("sleeping for %d seconds\n" % delay)
time.sleep(delay)
sys.stdout.write("woke up\n")
sys.exit(0)
