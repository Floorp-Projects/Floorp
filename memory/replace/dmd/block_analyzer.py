#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# From a scan mode DMD log, extract some information about a
# particular block, such as its allocation stack or which other blocks
# contain pointers to it. This can be useful when investigating leaks
# caused by unknown references to refcounted objects.

import json
import gzip
import sys
import argparse
import re


# The DMD output version this script handles.
outputVersion = 5

# If --ignore-alloc-fns is specified, stack frames containing functions that
# match these strings will be removed from the *start* of stack traces. (Once
# we hit a non-matching frame, any subsequent frames won't be removed even if
# they do match.)
allocatorFns = [
    'malloc (',
    'replace_malloc',
    'replace_calloc',
    'replace_realloc',
    'replace_memalign',
    'replace_posix_memalign',
    'malloc_zone_malloc',
    'moz_xmalloc',
    'moz_xcalloc',
    'moz_xrealloc',
    'operator new(',
    'operator new[](',
    'g_malloc',
    'g_slice_alloc',
    'callocCanGC',
    'reallocCanGC',
    'vpx_malloc',
    'vpx_calloc',
    'vpx_realloc',
    'vpx_memalign',
    'js_malloc',
    'js_calloc',
    'js_realloc',
    'pod_malloc',
    'pod_calloc',
    'pod_realloc',
    'nsTArrayInfallibleAllocator::Malloc',
    # This one necessary to fully filter some sequences of allocation functions
    # that happen in practice. Note that ??? entries that follow non-allocation
    # functions won't be stripped, as explained above.
    '???',
]

####

# Command line arguments

def range_1_24(string):
    value = int(string)
    if value < 1 or value > 24:
        msg = '{:s} is not in the range 1..24'.format(string)
        raise argparse.ArgumentTypeError(msg)
    return value

parser = argparse.ArgumentParser(description='Analyze the heap graph to find out things about an object. \
By default this prints out information about blocks that point to the given block.')

parser.add_argument('dmd_log_file_name',
                    help='clamped DMD log file name')

parser.add_argument('block',
                    help='address of the block of interest')

parser.add_argument('--info', dest='info', action='store_true',
                    default=False,
                    help='Print out information about the block.')

parser.add_argument('-sfl', '--max-stack-frame-length', type=int,
                    default=150,
                    help='Maximum number of characters to print from each stack frame')

parser.add_argument('-a', '--ignore-alloc-fns', action='store_true',
                    help='ignore allocation functions at the start of traces')

parser.add_argument('-f', '--max-frames', type=range_1_24,
                    help='maximum number of frames to consider in each trace')

parser.add_argument('-c', '--chain-reports', action='store_true',
                    help='if only one block is found to hold onto the object, report the next one, too')


####


class BlockData:
    def __init__(self, json_block):
        self.addr = json_block['addr']

        if 'contents' in json_block:
            contents = json_block['contents']
        else:
            contents = []
        self.contents = []
        for c in contents:
            self.contents.append(int(c, 16))

        self.req_size = json_block['req']

        self.alloc_stack = json_block['alloc']


def print_trace_segment(args, stacks, block):
    (traceTable, frameTable) = stacks

    for l in traceTable[block.alloc_stack]:
        # The 5: is to remove the bogus leading "#00: " from the stack frame.
        print ' ', frameTable[l][5:args.max_stack_frame_length]


def show_referrers(args, blocks, stacks, block):
    visited = set([])

    anyFound = False

    while True:
        referrers = {}

        for b, data in blocks.iteritems():
            which_edge = 0
            for e in data.contents:
                if e == block:
                    # 8 is the number of bytes per word on a 64-bit system.
                    # XXX This means that this output will be wrong for logs from 32-bit systems!
                    referrers.setdefault(b, []).append(8 * which_edge)
                    anyFound = True
                which_edge += 1

        for r in referrers:
            sys.stdout.write('0x{} size = {} bytes'.format(blocks[r].addr, blocks[r].req_size))
            plural = 's' if len(referrers[r]) > 1 else ''
            sys.stdout.write(' at byte offset' + plural + ' ' + (', '.join(str(x) for x in referrers[r])))
            print
            print_trace_segment(args, stacks, blocks[r])
            print

        if args.chain_reports:
            if len(referrers) == 0:
                sys.stdout.write('Found no more referrers.\n')
                break
            if len(referrers) > 1:
                sys.stdout.write('Found too many referrers.\n')
                break

            sys.stdout.write('Chaining to next referrer.\n\n')
            for r in referrers:
                block = r
            if block in visited:
                sys.stdout.write('Found a loop.\n')
                break
            visited.add(block)
        else:
            break

    if not anyFound:
        print 'No referrers found.'


def show_block_info(args, blocks, stacks, block):
    b = blocks[block]
    sys.stdout.write('block: 0x{}\n'.format(b.addr))
    sys.stdout.write('requested size: {} bytes\n'.format(b.req_size))
    sys.stdout.write('\n')
    sys.stdout.write('block contents: ')
    for c in b.contents:
        v = '0' if c == 0 else blocks[c].addr
        sys.stdout.write('0x{} '.format(v))
    sys.stdout.write('\n\n')
    sys.stdout.write('allocation stack:\n')
    print_trace_segment(args, stacks, b)
    return


def cleanupTraceTable(args, frameTable, traceTable):
    # Remove allocation functions at the start of traces.
    if args.ignore_alloc_fns:
        # Build a regexp that matches every function in allocatorFns.
        escapedAllocatorFns = map(re.escape, allocatorFns)
        fn_re = re.compile('|'.join(escapedAllocatorFns))

        # Remove allocator fns from each stack trace.
        for traceKey, frameKeys in traceTable.items():
            numSkippedFrames = 0
            for frameKey in frameKeys:
                frameDesc = frameTable[frameKey]
                if re.search(fn_re, frameDesc):
                    numSkippedFrames += 1
                else:
                    break
            if numSkippedFrames > 0:
                traceTable[traceKey] = frameKeys[numSkippedFrames:]

    # Trim the number of frames.
    for traceKey, frameKeys in traceTable.items():
        if len(frameKeys) > args.max_frames:
            traceTable[traceKey] = frameKeys[:args.max_frames]


def loadGraph(options):
    # Handle gzipped input if necessary.
    isZipped = options.dmd_log_file_name.endswith('.gz')
    opener = gzip.open if isZipped else open

    with opener(options.dmd_log_file_name, 'rb') as f:
        j = json.load(f)

    if j['version'] != outputVersion:
        raise Exception("'version' property isn't '{:d}'".format(outputVersion))

    invocation = j['invocation']

    block_list = j['blockList']
    blocks = {}

    for json_block in block_list:
        blocks[int(json_block['addr'], 16)] = BlockData(json_block)

    traceTable = j['traceTable']
    frameTable = j['frameTable']

    cleanupTraceTable(options, frameTable, traceTable)

    return (blocks, (traceTable, frameTable))


def analyzeLogs():
    options = parser.parse_args()

    (blocks, stacks) = loadGraph(options)

    block = int(options.block, 16)

    if not block in blocks:
        print 'Object', block, 'not found in traces.'
        print 'It could still be the target of some nodes.'
        return

    if options.info:
        show_block_info(options, blocks, stacks, block)
        return

    show_referrers(options, blocks, stacks, block)


if __name__ == "__main__":
    analyzeLogs()
