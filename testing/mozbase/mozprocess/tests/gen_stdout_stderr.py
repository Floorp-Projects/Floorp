import sys

# try to write out enough output to overflow the OS buffer
# for both stdout and stderr.
for i in xrange(50):
    print "O" * 100
    print >>sys.stderr, "X" * 100
