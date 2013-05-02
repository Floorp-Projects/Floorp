# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from collections import OrderedDict
from itertools import groupby
from operator import itemgetter

WHITESPACE_CHARACTERS = ' \t'

def indentation(line):
    """Number of whitespace (tab and space) characters at start of |line|."""
    i = 0
    while i < len(line):
        if line[i] not in WHITESPACE_CHARACTERS:
            break
        i += 1
    return i

def dependency_map(lines):
    """Return a dictionary with keys that are targets and values that
    are ordered lists of targets that should also be built.

    This implementation is O(n^2), but lovely and simple!  We walk the
    targets in the list, and for each target we walk backwards
    collecting its dependencies.  To make the walking easier, we
    reverse the list so that we are always walking forwards.

    """
    pairs = [(indentation(line), line.strip()) for line in lines]
    pairs.reverse()

    deps = {}

    for i, (indent, target) in enumerate(pairs):
        if not deps.has_key(target):
            deps[target] = []

        for j in range(i+1, len(pairs)):
            ind, tar = pairs[j]
            if ind < indent:
                indent = ind
                if tar not in deps[target]:
                    deps[target].append(tar)

    return deps

def all_dependencies(*targets, **kwargs):
    """Return a list containing |targets| and all the dependencies of
    those targets.

    The relative order of targets is maintained if possible.

    """
    dm = kwargs.pop('dependency_map', None)
    if dm is None:
        dm = dependency_map(targets)

    all_targets = OrderedDict() # Used as an ordered set.

    for target in targets:
        all_targets[target] = True
        if target in dm:
            for dependency in dm[target]:
                # Move element back in the ordered set.
                if dependency in all_targets:
                    del all_targets[dependency]
                all_targets[dependency] = True

    return all_targets.keys()

def add_extra_dependencies(target_pairs, dependency_map):
    """Take a list [(make_dir, make_target)] and expand (make_dir, None)
    entries with extra make dependencies from |dependency_map|.

    Returns an iterator of pairs (make_dir, make_target).

    """
    for make_target, group in groupby(target_pairs, itemgetter(1)):
        # Return non-simple directory targets untouched.
        if make_target is not None:
            for pair in group:
                yield pair
            continue

        # Add extra dumbmake dependencies to simple directory targets.
        make_dirs = [make_dir for make_dir, _ in group]
        new_make_dirs = all_dependencies(*make_dirs, dependency_map=dependency_map)

        for make_dir in new_make_dirs:
            yield make_dir, None
