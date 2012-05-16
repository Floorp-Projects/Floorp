import os, sys

filename = sys.argv[1]
with open(filename, 'r') as f:
    for l in f.readlines():
        l = l.strip()
        if not l.startswith("bin/"):
            continue
        print l[4:]
