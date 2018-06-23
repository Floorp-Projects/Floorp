#! /usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''This script analyzes a JSON file emitted by DMD.'''

from __future__ import print_function, division

import argparse
import collections
import gzip
import json
import os
import platform
import re
import shutil
import sys
import tempfile
from bisect import bisect_right

# The DMD output version this script handles.
outputVersion = 5

# If --ignore-alloc-fns is specified, stack frames containing functions that
# match these strings will be removed from the *start* of stack traces. (Once
# we hit a non-matching frame, any subsequent frames won't be removed even if
# they do match.)
allocatorFns = [
    # Matches malloc, replace_malloc, moz_xmalloc, vpx_malloc, js_malloc,
    # pod_malloc, malloc_zone_*, g_malloc.
    'malloc',
    # Matches calloc, replace_calloc, moz_xcalloc, vpx_calloc, js_calloc,
    # pod_calloc, malloc_zone_calloc, pod_callocCanGC.
    'calloc',
    # Matches realloc, replace_realloc, moz_xrealloc, vpx_realloc, js_realloc,
    # pod_realloc, pod_reallocCanGC.
    'realloc',
    # Matches memalign, posix_memalign, replace_memalign, replace_posix_memalign,
    # moz_xmemalign, vpx_memalign, malloc_zone_memalign.
    'memalign',
    'operator new(',
    'operator new[](',
    'g_slice_alloc',
    # This one necessary to fully filter some sequences of allocation functions
    # that happen in practice. Note that ??? entries that follow non-allocation
    # functions won't be stripped, as explained above.
    '???',
]


class Record(object):
    '''A record is an aggregation of heap blocks that have identical stack
    traces. It can also be used to represent the difference between two
    records.'''

    def __init__(self):
        self.numBlocks = 0
        self.reqSize = 0
        self.slopSize = 0
        self.usableSize = 0
        self.allocatedAtDesc = None
        self.reportedAtDescs = []
        self.usableSizes = collections.defaultdict(int)

    def isZero(self, args):
        return self.numBlocks == 0 and \
            self.reqSize == 0 and \
            self.slopSize == 0 and \
            self.usableSize == 0 and \
            len(self.usableSizes) == 0

    def negate(self):
        self.numBlocks = -self.numBlocks
        self.reqSize = -self.reqSize
        self.slopSize = -self.slopSize
        self.usableSize = -self.usableSize

        negatedUsableSizes = collections.defaultdict(int)
        for usableSize, count in self.usableSizes.items():
            negatedUsableSizes[-usableSize] = count
        self.usableSizes = negatedUsableSizes

    def subtract(self, r):
        # We should only be calling this on records with matching stack traces.
        # Check this.
        assert self.allocatedAtDesc == r.allocatedAtDesc
        assert self.reportedAtDescs == r.reportedAtDescs

        self.numBlocks -= r.numBlocks
        self.reqSize -= r.reqSize
        self.slopSize -= r.slopSize
        self.usableSize -= r.usableSize

        usableSizes1 = self.usableSizes
        usableSizes2 = r.usableSizes
        usableSizes3 = collections.defaultdict(int)
        for usableSize in usableSizes1:
            counts1 = usableSizes1[usableSize]
            if usableSize in usableSizes2:
                counts2 = usableSizes2[usableSize]
                del usableSizes2[usableSize]
                counts3 = counts1 - counts2
                if counts3 != 0:
                    if counts3 < 0:
                        usableSize = -usableSize
                        counts3 = -counts3
                    usableSizes3[usableSize] = counts3
            else:
                usableSizes3[usableSize] = counts1

        for usableSize in usableSizes2:
            usableSizes3[-usableSize] = usableSizes2[usableSize]

        self.usableSizes = usableSizes3

    @staticmethod
    def cmpByUsableSize(r1, r2):
        # Sort by usable size, then by req size.
        return cmp(abs(r1.usableSize), abs(r2.usableSize)) or \
            Record.cmpByReqSize(r1, r2)

    @staticmethod
    def cmpByReqSize(r1, r2):
        # Sort by req size.
        return cmp(abs(r1.reqSize), abs(r2.reqSize))

    @staticmethod
    def cmpBySlopSize(r1, r2):
        # Sort by slop size.
        return cmp(abs(r1.slopSize), abs(r2.slopSize))

    @staticmethod
    def cmpByNumBlocks(r1, r2):
        # Sort by block counts, then by usable size.
        return cmp(abs(r1.numBlocks), abs(r2.numBlocks)) or \
            Record.cmpByUsableSize(r1, r2)


sortByChoices = {
    'usable':     Record.cmpByUsableSize,   # the default
    'req':        Record.cmpByReqSize,
    'slop':       Record.cmpBySlopSize,
    'num-blocks': Record.cmpByNumBlocks,
}


def parseCommandLine():
    # 24 is the maximum number of frames that DMD will produce.
    def range_1_24(string):
        value = int(string)
        if value < 1 or value > 24:
            msg = '{:s} is not in the range 1..24'.format(string)
            raise argparse.ArgumentTypeError(msg)
        return value

    description = '''
Analyze heap data produced by DMD.
If one file is specified, analyze it; if two files are specified, analyze the
difference.
Input files can be gzipped.
Write to stdout unless -o/--output is specified.
Stack traces are fixed to show function names, filenames and line numbers
unless --no-fix-stacks is specified; stack fixing modifies the original file
and may take some time. If specified, the BREAKPAD_SYMBOLS_PATH environment
variable is used to find breakpad symbols for stack fixing.
'''
    p = argparse.ArgumentParser(description=description)

    p.add_argument('-o', '--output', type=argparse.FileType('w'),
                   help='output file; stdout if unspecified')

    p.add_argument('-f', '--max-frames', type=range_1_24, default=8,
                   help='maximum number of frames to consider in each trace')

    p.add_argument('-s', '--sort-by', choices=sortByChoices.keys(),
                   default='usable',
                   help='sort the records by a particular metric')

    p.add_argument('-a', '--ignore-alloc-fns', action='store_true',
                   help='ignore allocation functions at the start of traces')

    p.add_argument('--no-fix-stacks', action='store_true',
                   help='do not fix stacks')

    p.add_argument('--clamp-contents', action='store_true',
                   help='for a scan mode log, clamp addresses to the start of live blocks, '
                   'or zero if not in one')

    p.add_argument('--print-clamp-stats', action='store_true',
                   help='print information about the results of pointer clamping; mostly '
                   'useful for debugging clamping')

    p.add_argument('--filter-stacks-for-testing', action='store_true',
                   help='filter stack traces; only useful for testing purposes')

    p.add_argument('--allocation-filter',
                   help='Only print entries that have a stack that matches the filter')

    p.add_argument('input_file',
                   help='a file produced by DMD')

    p.add_argument('input_file2', nargs='?',
                   help='a file produced by DMD; if present, it is diff\'d with input_file')

    return p.parse_args(sys.argv[1:])


# Fix stacks if necessary: first write the output to a tempfile, then replace
# the original file with it.
def fixStackTraces(inputFilename, isZipped, opener):
    # This append() call is needed to make the import statements work when this
    # script is installed as a symlink.
    sys.path.append(os.path.dirname(__file__))

    bpsyms = os.environ.get('BREAKPAD_SYMBOLS_PATH', None)
    sysname = platform.system()
    if bpsyms and os.path.exists(bpsyms):
        import fix_stack_using_bpsyms as fixModule

        def fix(line): return fixModule.fixSymbols(line, bpsyms)
    elif sysname == 'Linux':
        import fix_linux_stack as fixModule

        def fix(line): return fixModule.fixSymbols(line)
    elif sysname == 'Darwin':
        import fix_macosx_stack as fixModule

        def fix(line): return fixModule.fixSymbols(line)
    else:
        fix = None  # there is no fix script for Windows

    if fix:
        # Fix stacks, writing output to a temporary file, and then
        # overwrite the original file.
        tmpFile = tempfile.NamedTemporaryFile(delete=False)

        # If the input is gzipped, then the output (written initially to
        # |tmpFile|) should be gzipped as well.
        #
        # And we want to set its pre-gzipped filename to '' rather than the
        # name of the temporary file, so that programs like the Unix 'file'
        # utility don't say that it was called 'tmp6ozTxE' (or something like
        # that) before it was zipped. So that explains the |filename=''|
        # parameter.
        #
        # But setting the filename like that clobbers |tmpFile.name|, so we
        # must get that now in order to move |tmpFile| at the end.
        tmpFilename = tmpFile.name
        if isZipped:
            tmpFile = gzip.GzipFile(filename='', fileobj=tmpFile)

        with opener(inputFilename, 'rb') as inputFile:
            for line in inputFile:
                tmpFile.write(fix(line))

        tmpFile.close()

        shutil.move(tmpFilename, inputFilename)


def getDigestFromFile(args, inputFile):
    # Handle gzipped input if necessary.
    isZipped = inputFile.endswith('.gz')
    opener = gzip.open if isZipped else open

    # Fix stack traces unless otherwise instructed.
    if not args.no_fix_stacks:
        fixStackTraces(inputFile, isZipped, opener)

    if args.clamp_contents:
        clampBlockList(args, inputFile, isZipped, opener)

    with opener(inputFile, 'rb') as f:
        j = json.load(f)

    if j['version'] != outputVersion:
        raise Exception("'version' property isn't '{:d}'".format(outputVersion))

    # Extract the main parts of the JSON object.
    invocation = j['invocation']
    dmdEnvVar = invocation['dmdEnvVar']
    mode = invocation['mode']
    blockList = j['blockList']
    traceTable = j['traceTable']
    frameTable = j['frameTable']

    # Insert the necessary entries for unrecorded stack traces. Note that 'ut'
    # and 'uf' will not overlap with any keys produced by DMD's
    # ToIdStringConverter::Base32() function.
    unrecordedTraceID = 'ut'
    unrecordedFrameID = 'uf'
    traceTable[unrecordedTraceID] = [unrecordedFrameID]
    frameTable[unrecordedFrameID] = \
        '#00: (no stack trace recorded due to --stacks=partial)'

    # For the purposes of this script, 'scan' behaves like 'live'.
    if mode == 'scan':
        mode = 'live'

    if mode not in ['live', 'dark-matter', 'cumulative']:
        raise Exception("bad 'mode' property: '{:s}'".format(mode))

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

    def buildTraceDescription(traceTable, frameTable, traceKey):
        frameKeys = traceTable[traceKey]
        fmt = '    #{:02d}{:}'

        if args.filter_stacks_for_testing:
            # When running SmokeDMD.cpp, every stack trace should contain at
            # least one frame that contains 'DMD.cpp', from either |DMD.cpp| or
            # |SmokeDMD.cpp|. (Or 'dmd.cpp' on Windows.) If we see such a
            # frame, we replace the entire stack trace with a single,
            # predictable frame. There is too much variation in the stack
            # traces across different machines and platforms to do more precise
            # matching, but this level of matching will result in failure if
            # stack fixing fails completely.
            for frameKey in frameKeys:
                frameDesc = frameTable[frameKey]
                if 'DMD.cpp' in frameDesc or 'dmd.cpp' in frameDesc:
                    return [fmt.format(1, ': ... DMD.cpp ...')]

        # The frame number is always '#00' (see DMD.h for why), so we have to
        # replace that with the correct frame number.
        desc = []
        for n, frameKey in enumerate(traceTable[traceKey], start=1):
            desc.append(fmt.format(n, frameTable[frameKey][3:]))
        return desc

    # Aggregate blocks into records. All sufficiently similar blocks go into a
    # single record.

    if mode in ['live', 'cumulative']:
        liveOrCumulativeRecords = collections.defaultdict(Record)
    elif mode == 'dark-matter':
        unreportedRecords = collections.defaultdict(Record)
        onceReportedRecords = collections.defaultdict(Record)
        twiceReportedRecords = collections.defaultdict(Record)

    heapUsableSize = 0
    heapBlocks = 0

    recordKeyPartCache = {}

    for block in blockList:
        # For each block we compute a |recordKey|, and all blocks with the same
        # |recordKey| are aggregated into a single record. The |recordKey| is
        # derived from the block's 'alloc' and 'reps' (if present) stack
        # traces.
        #
        # We use frame descriptions (e.g. "#00: foo (X.cpp:99)") when comparing
        # traces for equality. We can't use trace keys or frame keys because
        # they're not comparable across different DMD runs (which is relevant
        # when doing diffs).
        #
        # Using frame descriptions also fits in with the stack trimming done
        # for --max-frames, which requires that stack traces with common
        # beginnings but different endings to be considered equivalent. E.g. if
        # we have distinct traces T1:[A:D1,B:D2,C:D3] and T2:[X:D1,Y:D2,Z:D4]
        # and we trim the final frame of each they should be considered
        # equivalent because the untrimmed frame descriptions (D1 and D2)
        # match.
        #
        # Having said all that, during a single invocation of dmd.py on a
        # single DMD file, for a single frameKey value the record key will
        # always be the same, and we might encounter it 1000s of times. So we
        # cache prior results for speed.
        def makeRecordKeyPart(traceKey):
            if traceKey in recordKeyPartCache:
                return recordKeyPartCache[traceKey]

            recordKeyPart = str(map(lambda frameKey: frameTable[frameKey],
                                    traceTable[traceKey]))
            recordKeyPartCache[traceKey] = recordKeyPart
            return recordKeyPart

        allocatedAtTraceKey = block.get('alloc', unrecordedTraceID)
        if mode in ['live', 'cumulative']:
            recordKey = makeRecordKeyPart(allocatedAtTraceKey)
            records = liveOrCumulativeRecords
        elif mode == 'dark-matter':
            recordKey = makeRecordKeyPart(allocatedAtTraceKey)
            if 'reps' in block:
                reportedAtTraceKeys = block['reps']
                for reportedAtTraceKey in reportedAtTraceKeys:
                    recordKey += makeRecordKeyPart(reportedAtTraceKey)
                if len(reportedAtTraceKeys) == 1:
                    records = onceReportedRecords
                else:
                    records = twiceReportedRecords
            else:
                records = unreportedRecords

        record = records[recordKey]

        if 'req' not in block:
            raise Exception("'req' property missing in block'")

        reqSize = block['req']
        slopSize = block.get('slop', 0)

        if 'num' in block:
            num = block['num']
        else:
            num = 1

        usableSize = reqSize + slopSize
        heapUsableSize += num * usableSize
        heapBlocks += num

        record.numBlocks += num
        record.reqSize += num * reqSize
        record.slopSize += num * slopSize
        record.usableSize += num * usableSize
        if record.allocatedAtDesc is None:
            record.allocatedAtDesc = \
                buildTraceDescription(traceTable, frameTable,
                                      allocatedAtTraceKey)

        if mode in ['live', 'cumulative']:
            pass
        elif mode == 'dark-matter':
            if 'reps' in block and record.reportedAtDescs == []:
                def f(k): return buildTraceDescription(traceTable, frameTable, k)
                record.reportedAtDescs = map(f, reportedAtTraceKeys)
        record.usableSizes[usableSize] += num

    # All the processed data for a single DMD file is called a "digest".
    digest = {}
    digest['dmdEnvVar'] = dmdEnvVar
    digest['mode'] = mode
    digest['heapUsableSize'] = heapUsableSize
    digest['heapBlocks'] = heapBlocks
    if mode in ['live', 'cumulative']:
        digest['liveOrCumulativeRecords'] = liveOrCumulativeRecords
    elif mode == 'dark-matter':
        digest['unreportedRecords'] = unreportedRecords
        digest['onceReportedRecords'] = onceReportedRecords
        digest['twiceReportedRecords'] = twiceReportedRecords
    return digest


def diffRecords(args, records1, records2):
    records3 = {}

    # Process records1.
    for k in records1:
        r1 = records1[k]
        if k in records2:
            # This record is present in both records1 and records2.
            r2 = records2[k]
            del records2[k]
            r2.subtract(r1)
            if not r2.isZero(args):
                records3[k] = r2
        else:
            # This record is present only in records1.
            r1.negate()
            records3[k] = r1

    for k in records2:
        # This record is present only in records2.
        records3[k] = records2[k]

    return records3


def diffDigests(args, d1, d2):
    if (d1['mode'] != d2['mode']):
        raise Exception("the input files have different 'mode' properties")

    d3 = {}
    d3['dmdEnvVar'] = (d1['dmdEnvVar'], d2['dmdEnvVar'])
    d3['mode'] = d1['mode']
    d3['heapUsableSize'] = d2['heapUsableSize'] - d1['heapUsableSize']
    d3['heapBlocks'] = d2['heapBlocks'] - d1['heapBlocks']
    if d1['mode'] in ['live', 'cumulative']:
        d3['liveOrCumulativeRecords'] = \
            diffRecords(args, d1['liveOrCumulativeRecords'],
                        d2['liveOrCumulativeRecords'])
    elif d1['mode'] == 'dark-matter':
        d3['unreportedRecords'] = diffRecords(args, d1['unreportedRecords'],
                                              d2['unreportedRecords'])
        d3['onceReportedRecords'] = diffRecords(args, d1['onceReportedRecords'],
                                                d2['onceReportedRecords'])
        d3['twiceReportedRecords'] = diffRecords(args, d1['twiceReportedRecords'],
                                                 d2['twiceReportedRecords'])
    return d3


def printDigest(args, digest):
    dmdEnvVar = digest['dmdEnvVar']
    mode = digest['mode']
    heapUsableSize = digest['heapUsableSize']
    heapBlocks = digest['heapBlocks']
    if mode in ['live', 'cumulative']:
        liveOrCumulativeRecords = digest['liveOrCumulativeRecords']
    elif mode == 'dark-matter':
        unreportedRecords = digest['unreportedRecords']
        onceReportedRecords = digest['onceReportedRecords']
        twiceReportedRecords = digest['twiceReportedRecords']

    separator = '#' + '-' * 65 + '\n'

    def number(n):
        '''Format a number with comma as a separator.'''
        return '{:,d}'.format(n)

    def perc(m, n):
        return 0 if n == 0 else (100 * m / n)

    def plural(n):
        return '' if n == 1 else 's'

    # Prints to stdout, or to file if -o/--output was specified.
    def out(*arguments, **kwargs):
        print(*arguments, file=args.output, **kwargs)

    def printStack(traceDesc):
        for frameDesc in traceDesc:
            out(frameDesc)

    def printRecords(recordKind, records, heapUsableSize):
        RecordKind = recordKind.capitalize()
        out(separator)
        numRecords = len(records)
        cmpRecords = sortByChoices[args.sort_by]
        sortedRecords = sorted(records.values(), cmp=cmpRecords, reverse=True)
        kindBlocks = 0
        kindUsableSize = 0
        maxRecord = 1000

        if args.allocation_filter:
            sortedRecords = list(filter(
                lambda x: any(map(lambda y: args.allocation_filter in y, x.allocatedAtDesc)),
                sortedRecords))

        # First iteration: get totals, etc.
        for record in sortedRecords:
            kindBlocks += record.numBlocks
            kindUsableSize += record.usableSize

        # Second iteration: print.
        if numRecords == 0:
            out('# no {:} heap blocks\n'.format(recordKind))

        kindCumulativeUsableSize = 0
        for i, record in enumerate(sortedRecords, start=1):
            # Stop printing at the |maxRecord|th record.
            if i == maxRecord:
                out('# {:}: stopping after {:,d} heap block records\n'.
                    format(RecordKind, i))
                break

            kindCumulativeUsableSize += record.usableSize

            out(RecordKind + ' {')
            out('  {:} block{:} in heap block record {:,d} of {:,d}'.
                format(number(record.numBlocks),
                       plural(record.numBlocks), i, numRecords))
            out('  {:} bytes ({:} requested / {:} slop)'.
                format(number(record.usableSize),
                       number(record.reqSize),
                       number(record.slopSize)))

            def abscmp((usableSize1, _1), (usableSize2, _2)): return \
                cmp(abs(usableSize1), abs(usableSize2))
            usableSizes = sorted(record.usableSizes.items(), cmp=abscmp,
                                 reverse=True)

            hasSingleBlock = len(usableSizes) == 1 and usableSizes[0][1] == 1

            if not hasSingleBlock:
                out('  Individual block sizes: ', end='')
                if len(usableSizes) == 0:
                    out('(no change)', end='')
                else:
                    isFirst = True
                    for usableSize, count in usableSizes:
                        if not isFirst:
                            out('; ', end='')
                        out('{:}'.format(number(usableSize)), end='')
                        if count > 1:
                            out(' x {:,d}'.format(count), end='')
                        isFirst = False
                out()

            out('  {:4.2f}% of the heap ({:4.2f}% cumulative)'.
                format(perc(record.usableSize, heapUsableSize),
                       perc(kindCumulativeUsableSize, heapUsableSize)))
            if mode in ['live', 'cumulative']:
                pass
            elif mode == 'dark-matter':
                out('  {:4.2f}% of {:} ({:4.2f}% cumulative)'.
                    format(perc(record.usableSize, kindUsableSize),
                           recordKind,
                           perc(kindCumulativeUsableSize, kindUsableSize)))
            out('  Allocated at {')
            printStack(record.allocatedAtDesc)
            out('  }')
            if mode in ['live', 'cumulative']:
                pass
            elif mode == 'dark-matter':
                for n, reportedAtDesc in enumerate(record.reportedAtDescs):
                    again = 'again ' if n > 0 else ''
                    out('  Reported {:}at {{'.format(again))
                    printStack(reportedAtDesc)
                    out('  }')
            out('}\n')

        return (kindUsableSize, kindBlocks)

    def printInvocation(n, dmdEnvVar, mode):
        out('Invocation{:} {{'.format(n))
        if dmdEnvVar is None:
            out('  $DMD is undefined')
        else:
            out('  $DMD = \'' + dmdEnvVar + '\'')
        out('  Mode = \'' + mode + '\'')
        out('}\n')

    # Print command line. Strip dirs so the output is deterministic, which is
    # needed for testing.
    out(separator, end='')
    out('# ' + ' '.join(map(os.path.basename, sys.argv)) + '\n')

    # Print invocation(s).
    if type(dmdEnvVar) is not tuple:
        printInvocation('', dmdEnvVar, mode)
    else:
        printInvocation(' 1', dmdEnvVar[0], mode)
        printInvocation(' 2', dmdEnvVar[1], mode)

    # Print records.
    if mode in ['live', 'cumulative']:
        liveOrCumulativeUsableSize, liveOrCumulativeBlocks = \
            printRecords(mode, liveOrCumulativeRecords, heapUsableSize)
    elif mode == 'dark-matter':
        twiceReportedUsableSize, twiceReportedBlocks = \
            printRecords('twice-reported', twiceReportedRecords, heapUsableSize)

        unreportedUsableSize, unreportedBlocks = \
            printRecords('unreported', unreportedRecords, heapUsableSize)

        onceReportedUsableSize, onceReportedBlocks = \
            printRecords('once-reported', onceReportedRecords, heapUsableSize)

    # Print summary.
    out(separator)
    out('Summary {')
    if mode in ['live', 'cumulative']:
        out('  Total: {:} bytes in {:} blocks'.
            format(number(liveOrCumulativeUsableSize),
                   number(liveOrCumulativeBlocks)))
    elif mode == 'dark-matter':
        fmt = '  {:15} {:>12} bytes ({:6.2f}%) in {:>7} blocks ({:6.2f}%)'
        out(fmt.
            format('Total:',
                   number(heapUsableSize),
                   100,
                   number(heapBlocks),
                   100))
        out(fmt.
            format('Unreported:',
                   number(unreportedUsableSize),
                   perc(unreportedUsableSize, heapUsableSize),
                   number(unreportedBlocks),
                   perc(unreportedBlocks, heapBlocks)))
        out(fmt.
            format('Once-reported:',
                   number(onceReportedUsableSize),
                   perc(onceReportedUsableSize, heapUsableSize),
                   number(onceReportedBlocks),
                   perc(onceReportedBlocks, heapBlocks)))
        out(fmt.
            format('Twice-reported:',
                   number(twiceReportedUsableSize),
                   perc(twiceReportedUsableSize, heapUsableSize),
                   number(twiceReportedBlocks),
                   perc(twiceReportedBlocks, heapBlocks)))
    out('}\n')


#############################
# Pretty printer for DMD JSON
#############################

def prettyPrintDmdJson(out, j):
    out.write('{\n')

    out.write(' "version": {0},\n'.format(j['version']))
    out.write(' "invocation": ')
    json.dump(j['invocation'], out, sort_keys=True)
    out.write(',\n')

    out.write(' "blockList": [')
    first = True
    for b in j['blockList']:
        out.write('' if first else ',')
        out.write('\n  ')
        json.dump(b, out, sort_keys=True)
        first = False
    out.write('\n ],\n')

    out.write(' "traceTable": {')
    first = True
    for k, l in j['traceTable'].iteritems():
        out.write('' if first else ',')
        out.write('\n  "{0}": {1}'.format(k, json.dumps(l)))
        first = False
    out.write('\n },\n')

    out.write(' "frameTable": {')
    first = True
    for k, v in j['frameTable'].iteritems():
        out.write('' if first else ',')
        out.write('\n  "{0}": {1}'.format(k, json.dumps(v)))
        first = False
    out.write('\n }\n')

    out.write('}\n')


##################################################################
# Code for clamping addresses using conservative pointer analysis.
##################################################################

# Start is the address of the first byte of the block, while end is
# the address of the first byte after the final byte in the block.
class AddrRange:
    def __init__(self, block, length):
        self.block = block
        self.start = int(block, 16)
        self.length = length
        self.end = self.start + self.length

        assert self.start > 0
        assert length >= 0


class ClampStats:
    def __init__(self):
        # Number of pointers already pointing to the start of a block.
        self.startBlockPtr = 0

        # Number of pointers pointing to the middle of a block. These
        # are clamped to the start of the block they point into.
        self.midBlockPtr = 0

        # Number of null pointers.
        self.nullPtr = 0

        # Number of non-null pointers that didn't point into the middle
        # of any blocks. These are clamped to null.
        self.nonNullNonBlockPtr = 0

    def clampedBlockAddr(self, sameAddress):
        if sameAddress:
            self.startBlockPtr += 1
        else:
            self.midBlockPtr += 1

    def nullAddr(self):
        self.nullPtr += 1

    def clampedNonBlockAddr(self):
        self.nonNullNonBlockPtr += 1

    def log(self):
        sys.stderr.write('Results:\n')
        sys.stderr.write(
            '  Number of pointers already pointing to start of blocks: ' +
            str(self.startBlockPtr) + '\n')
        sys.stderr.write('  Number of pointers clamped to start of blocks: ' +
                         str(self.midBlockPtr) + '\n')
        sys.stderr.write('  Number of non-null pointers not pointing into blocks '
                         'clamped to null: ' +
                         str(self.nonNullNonBlockPtr) + '\n')
        sys.stderr.write('  Number of null pointers: ' + str(self.nullPtr) + '\n')


# Search the block ranges array for a block that address points into.
# The search is carried out in an array of starting addresses for each blocks
# because it is faster.
def clampAddress(blockRanges, blockStarts, clampStats, address):
    i = bisect_right(blockStarts, address)

    # Any addresses completely out of the range should have been eliminated already.
    assert i > 0
    r = blockRanges[i - 1]
    assert r.start <= address

    if address >= r.end:
        assert address < blockRanges[i].start
        clampStats.clampedNonBlockAddr()
        return '0'

    clampStats.clampedBlockAddr(r.start == address)
    return r.block


def clampBlockList(args, inputFileName, isZipped, opener):
    # XXX This isn't very efficient because we end up reading and writing
    # the file multiple times.
    with opener(inputFileName, 'rb') as f:
        j = json.load(f)

    if j['version'] != outputVersion:
        raise Exception("'version' property isn't '{:d}'".format(outputVersion))

    # Check that the invocation is reasonable for contents clamping.
    invocation = j['invocation']
    if invocation['mode'] != 'scan':
        raise Exception("Log was taken in mode " + invocation['mode'] + " not scan")

    sys.stderr.write('Creating block range list.\n')
    blockList = j['blockList']
    blockRanges = []
    for block in blockList:
        blockRanges.append(AddrRange(block['addr'], block['req']))
    blockRanges.sort(key=lambda r: r.start)

    # Make sure there are no overlapping blocks.
    prevRange = blockRanges[0]
    for currRange in blockRanges[1:]:
        assert prevRange.end <= currRange.start
        prevRange = currRange

    sys.stderr.write('Clamping block contents.\n')
    clampStats = ClampStats()
    firstAddr = blockRanges[0].start
    lastAddr = blockRanges[-1].end

    blockStarts = []
    for r in blockRanges:
        blockStarts.append(r.start)

    for block in blockList:
        # Small blocks don't have any contents.
        if 'contents' not in block:
            continue

        cont = block['contents']
        for i in range(len(cont)):
            address = int(cont[i], 16)

            if address == 0:
                clampStats.nullAddr()
                continue

            # If the address is before the first block or after the last
            # block then it can't be within a block.
            if address < firstAddr or address >= lastAddr:
                clampStats.clampedNonBlockAddr()
                cont[i] = '0'
                continue

            cont[i] = clampAddress(blockRanges, blockStarts, clampStats, address)

        # Remove any trailing nulls.
        while len(cont) and cont[-1] == '0':
            cont.pop()

    if args.print_clamp_stats:
        clampStats.log()

    sys.stderr.write('Saving file.\n')
    tmpFile = tempfile.NamedTemporaryFile(delete=False)
    tmpFilename = tmpFile.name
    if isZipped:
        tmpFile = gzip.GzipFile(filename='', fileobj=tmpFile)
    prettyPrintDmdJson(tmpFile, j)
    tmpFile.close()
    shutil.move(tmpFilename, inputFileName)


def main():
    args = parseCommandLine()
    digest = getDigestFromFile(args, args.input_file)
    if args.input_file2:
        digest2 = getDigestFromFile(args, args.input_file2)
        digest = diffDigests(args, digest, digest2)
    printDigest(args, digest)


if __name__ == '__main__':
    main()
