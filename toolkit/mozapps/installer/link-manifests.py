import sys, os

manifestsdir, distdir = sys.argv[1:]

if not os.path.exists(manifestsdir):
    print >>sys.stderr, "Warning: %s does not exist." % manifestsdir
    sys.exit(0)

for name in os.listdir(manifestsdir):
    manifestdir = os.path.join(manifestsdir, name)
    if not os.path.isdir(manifestdir):
        continue

    manifestfile = os.path.join(distdir, 'components', name + '.manifest')
    outfd = open(manifestfile, 'a')

    for name in os.listdir(manifestdir):
        infd = open(os.path.join(manifestdir, name))
        print >>outfd, "# %s" % name
        outfd.write(infd.read())
        print >>outfd
        infd.close()

    outfd.close()
