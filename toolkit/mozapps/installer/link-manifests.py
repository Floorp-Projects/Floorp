import sys, os

outmanifest = sys.argv[1]
manifestdirs = sys.argv[2:]

outfd = open(outmanifest, 'w')

for manifestdir in manifestdirs:
    if not os.path.isdir(manifestdir):
        print >>sys.stderr, "Warning: trying to link manifests in missing directory '%s'" % manifestdir
        continue

    for name in os.listdir(manifestdir):
        infd = open(os.path.join(manifestdir, name))
        print >>outfd, "# %s" % name
        outfd.write(infd.read())
        print >>outfd
        infd.close()

outfd.close()
