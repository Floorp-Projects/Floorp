#!/usr/bin/env python

"""
script to view mozilla profiles
"""

import mozprofile
import optparse
import os
import sys

__all__ = ['view_profile']


def view_profile(args=sys.argv[1:]):

    usage = '%prog [options] profile_path <...>'
    parser = optparse.OptionParser(usage=usage, description=__doc__)
    options, args = parser.parse_args(args)
    if not args:
        parser.print_usage()
        parser.exit()

    # check existence
    missing = [i for i in args
               if not os.path.exists(i)]
    if missing:
        if len(missing) > 1:
            missing_string = "Profiles do not exist"
        else:
            missing_string = "Profile does not exist"
        parser.error("%s: %s" % (missing_string, ', '.join(missing)))

    # print summary for each profile
    while args:
        path = args.pop(0)
        profile = mozprofile.Profile(path)
        print profile.summary()
        if args:
            print '-' * 4


if __name__ == '__main__':
    view_profile()
