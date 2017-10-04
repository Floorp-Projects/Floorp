#!/usr/bin/env python

"""
diff two profile summaries
"""

import difflib
import profile
import optparse
import os
import sys

__all__ = ['diff', 'diff_profiles']


def diff(profile1, profile2, diff_function=difflib.unified_diff):

    profiles = (profile1, profile2)
    parts = {}
    parts_dict = {}
    for index in (0, 1):
        prof = profiles[index]

        # first part, the path, isn't useful for diffing
        parts[index] = prof.summary(return_parts=True)[1:]

        parts_dict[index] = dict(parts[index])

    # keys the first profile is missing
    first_missing = [i for i in parts_dict[1]
                     if i not in parts_dict[0]]
    parts[0].extend([(i, '') for i in first_missing])

    # diffs
    retval = []
    for key, value in parts[0]:
        other = parts_dict[1].get(key, '')
        value = value.strip()
        other = other.strip()

        if key == 'Files':
            # first line of files is the path; we don't care to diff that
            value = '\n'.join(value.splitlines()[1:])
            if other:
                other = '\n'.join(other.splitlines()[1:])

        value = value.splitlines()
        other = other.splitlines()
        section_diff = list(diff_function(value, other, profile1.profile, profile2.profile))
        if section_diff:
            retval.append((key, '\n'.join(section_diff)))

    return retval


def diff_profiles(args=sys.argv[1:]):

    # parse command line
    usage = '%prog [options] profile1 profile2'
    parser = optparse.OptionParser(usage=usage, description=__doc__)
    options, args = parser.parse_args(args)
    if len(args) != 2:
        parser.error("Must give two profile paths")
    missing = [arg for arg in args if not os.path.exists(arg)]
    if missing:
        parser.error("Profile not found: %s" % (', '.join(missing)))

    # get the profile differences
    diffs = diff(*([profile.Profile(arg)
                    for arg in args]))

    # display them
    while diffs:
        key, value = diffs.pop(0)
        print '[%s]:\n' % key
        print value
        if diffs:
            print '-' * 4


if __name__ == '__main__':
    diff_profiles()
