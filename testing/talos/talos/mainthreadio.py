# -*- Mode: python; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# vim: set ts=8 sts=4 et sw=4 tw=80:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import utils
import whitelist

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))

STAGE_STARTUP = 0
STAGE_STRINGS = ('startup', 'normal', 'shutdown')

LENGTH_IO_ENTRY = 5
LENGTH_NEXT_STAGE_ENTRY = 2
TOKEN_NEXT_STAGE = 'NEXT-STAGE'

INDEX_OPERATION = 1
INDEX_DURATION = 2
INDEX_EVENT_SOURCE = 3
INDEX_FILENAME = 4

KEY_COUNT = 'Count'
KEY_DURATION = 'Duration'
KEY_NO_FILENAME = '(not available)'
KEY_RUN_COUNT = 'RunCount'

LEAKED_SYMLINK_PREFIX = "::\\{"

PATH_SUBSTITUTIONS = {'profile': '{profile}', 'firefox': '{xre}',
                      'desktop': '{desktop}',
                      'fonts': '{fonts}', 'appdata': ' {appdata}'}
NAME_SUBSTITUTIONS = {'installtime': '{time}', 'prefetch': '{prefetch}',
                      'thumbnails': '{thumbnails}',
                      'windows media player': '{media_player}'}

TUPLE_FILENAME_INDEX = 2
WHITELIST_FILENAME = os.path.join(SCRIPT_DIR, 'mtio-whitelist.json')


def parse(logfilename, data):
    try:
        with open(logfilename, 'r') as logfile:
            if not logfile:
                return False
            stage = STAGE_STARTUP
            for line in logfile:
                prev_filename = str()
                entries = line.strip().split(',')
                if len(entries) == LENGTH_IO_ENTRY:
                    if stage == STAGE_STARTUP:
                        continue
                    if entries[INDEX_FILENAME] == KEY_NO_FILENAME:
                        continue
                    # Format 1: I/O entry

                    # Temporary hack: logs are leaking Windows NT symlinks.
                    # We need to ignore those.
                    if entries[INDEX_FILENAME]\
                            .startswith(LEAKED_SYMLINK_PREFIX):
                        continue

                    # We'll key each entry on (stage, event source,
                    # filename, operation)
                    key_tuple = (STAGE_STRINGS[stage],
                                 entries[INDEX_EVENT_SOURCE],
                                 entries[INDEX_FILENAME],
                                 entries[INDEX_OPERATION])
                    if key_tuple not in data:
                        data[key_tuple] = {
                            KEY_COUNT: 1,
                            KEY_RUN_COUNT: 1,
                            KEY_DURATION: float(entries[INDEX_DURATION])
                        }
                    else:
                        if prev_filename != entries[INDEX_FILENAME]:
                            data[key_tuple][KEY_RUN_COUNT] += 1
                        data[key_tuple][KEY_COUNT] += 1
                        data[key_tuple][KEY_DURATION] += \
                            float(entries[INDEX_DURATION])
                    prev_filename = entries[INDEX_FILENAME]
                elif len(entries) == LENGTH_NEXT_STAGE_ENTRY and \
                        entries[1] == TOKEN_NEXT_STAGE:
                    # Format 2: next stage
                    stage = stage + 1
            return True
    except IOError as e:
        print "%s: %s" % (e.filename, e.strerror)
        return False


def write_output(outfilename, data):
    # Write the data out so that we can track it
    try:
        with open(outfilename, 'w') as outfile:
            outfile.write("[\n")
            for idx, (key, value) in utils.indexed_items(data.iteritems()):
                output = "    [\"%s\", \"%s\", \"%s\", \"%s\", %d, %d, %f]" % (
                    key[0], key[1], key[2], key[3], value[KEY_COUNT],
                    value[KEY_RUN_COUNT], value[KEY_DURATION])
                outfile.write(output)
                if idx >= 0:
                    outfile.write(",\n")
                else:
                    outfile.write("\n")
            outfile.write("]\n")
            return True
    except IOError as e:
        print "%s: %s" % (e.filename, e.strerror)
        return False


def main(argv):
    if len(argv) < 4:
        print ("Usage: %s <main_thread_io_log_file> <output_file> <xre_path>"
               % argv[0])
        return 1
    if not os.path.exists(argv[3]):
        print "XRE Path \"%s\" does not exist" % argv[3]
        return 1
    data = {}
    if not parse(argv[1], data):
        print "Log parsing failed"
        return 1

    wl = whitelist.Whitelist(test_name='mainthreadio',
                             paths={"{xre}": argv[3]},
                             path_substitutions=PATH_SUBSTITUTIONS,
                             name_substitutions=NAME_SUBSTITUTIONS)
    if not wl.load(WHITELIST_FILENAME):
        print "Failed to load whitelist"
        return 1

    wl.filter(data, TUPLE_FILENAME_INDEX)

    if not write_output(argv[2], data):
        return 1

    # Disabled until we enable TBPL oranges
    # search for unknown filenames
    errors = wl.check(data, TUPLE_FILENAME_INDEX)
    if errors:
        strs = wl.get_error_strings(errors)
        wl.print_errors(strs)

    # search for duration > 1.0
    errors = wl.checkDuration(data, TUPLE_FILENAME_INDEX, 'Duration')
    if errors:
        strs = wl.get_error_strings(errors)
        wl.print_errors(strs)

    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main(sys.argv))
