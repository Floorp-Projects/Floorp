#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import csv
import re
import os
import sys
import xtalos
import subprocess
import json
import mozfile
import shutil


EVENTNAME_INDEX = 0
PROCESS_INDEX = 2
THREAD_ID_INDEX = 3
DISKBYTES_COL = "Size"
FNAME_COL = "FileName"
IMAGEFUNC_COL = "Image!Function"
EVENTGUID_COL = "EventGuid"
ACTIVITY_ID_COL = "etw:ActivityId"
NUMBYTES_COL = "NumBytes"

CEVT_WINDOWS_RESTORED = "{917b96b1-ecad-4dab-a760-8d49027748ae}"
CEVT_XPCOM_SHUTDOWN = "{26d1e091-0ae7-4f49-a554-4214445c505c}"
stages = ["startup", "normal", "shutdown"]
net_events = {
    "TcpDataTransferReceive": "recv",
    "UdpEndpointReceiveMessages": "recv",
    "TcpDataTransferSend": "send",
    "UdpEndpointSendMessages": "send"
}
gThreads = {}
gConnectionIDs = {}
gHeaders = {}


def uploadFile(filename):
    mud = os.environ.get('MOZ_UPLOAD_DIR', None)
    if mud:
        print("uploading raw file %s via blobber" % filename)
        mud_filename = os.path.join(mud, filename)
        shutil.copyfile(filename, "%s.log" % mud_filename)


def filterOutHeader(data):
    # -1 means we have not yet found the header
    # 0 means we are in the header
    # 1+ means that we are past the header
    state = -1
    for row in data:

        if not len(row):
            continue

        if state < 0:
            # Keep looking for the header (denoted by "BeginHeader").
            if row[0] == "BeginHeader":
                state = 0
            continue

        if state == 0:
            # Eventually, we'll find the end (denoted by "EndHeader").
            if row[0] == "EndHeader":
                state = 1
                continue

            gHeaders[row[EVENTNAME_INDEX]] = row
            continue

        if state >= 1:
            state = state + 1

        # The line after "EndHeader" is also not useful, so we want to strip
        # that in addition to the header.
        if state > 2:
            yield row


def getIndex(eventType, colName):
    if colName not in gHeaders[eventType]:
        return None
    return gHeaders[eventType].index(colName)


def readFile(filename):
    print("etlparser: in readfile: %s" % filename)
    data = csv.reader(open(filename, 'rb'), delimiter=',', quotechar='"',
                      skipinitialspace=True)
    data = filterOutHeader(data)
    return data


def fileSummary(row, stage, retVal):
    event = row[EVENTNAME_INDEX]

    # TODO: do we care about the other events?
    if event not in ("FileIoRead", "FileIoWrite"):
        return
    fname_index = getIndex(event, FNAME_COL)

    # We only care about events that have a file name.
    if fname_index is None:
        return

    # Some data rows are missing the filename?
    if len(row) <= fname_index:
        return

    thread_extra = ""
    if gThreads[row[THREAD_ID_INDEX]] == "main":
        thread_extra = " (main)"
    key_tuple = (row[fname_index],
                 "%s%s" % (row[THREAD_ID_INDEX], thread_extra),
                 stages[stage])
    total_tuple = (row[fname_index],
                   "%s%s" % (row[THREAD_ID_INDEX], thread_extra),
                   "all")

    if key_tuple not in retVal:
        retVal[key_tuple] = {
            "DiskReadBytes": 0,
            "DiskReadCount": 0,
            "DiskWriteBytes": 0,
            "DiskWriteCount": 0
        }
        retVal[total_tuple] = retVal[key_tuple]

    if event == "FileIoRead":
        retVal[key_tuple]['DiskReadCount'] += 1
        retVal[total_tuple]['DiskReadCount'] += 1
        idx = getIndex(event, DISKBYTES_COL)
        retVal[key_tuple]['DiskReadBytes'] += int(row[idx], 16)
        retVal[total_tuple]['DiskReadBytes'] += int(row[idx], 16)
    elif event == "FileIoWrite":
        retVal[key_tuple]['DiskWriteCount'] += 1
        retVal[total_tuple]['DiskWriteCount'] += 1
        idx = getIndex(event, DISKBYTES_COL)
        retVal[key_tuple]['DiskWriteBytes'] += int(row[idx], 16)
        retVal[total_tuple]['DiskWriteBytes'] += int(row[idx], 16)


def etl2csv(xperf_path, etl_filename, debug=False):
    """
    Convert etl_filename to etl_filename.csv (temp file) which is the .csv
    representation of the .etl file Etlparser will read this .csv and parse
    the information we care about into the final output. This is done to keep
    things simple and to preserve resources on talos machines
    (large files == high memory + cpu)
    """

    xperf_cmd = [xperf_path,
                 '-merge',
                 '%s.user' % etl_filename,
                 '%s.kernel' % etl_filename,
                 etl_filename]
    if debug:
        print("executing '%s'" % subprocess.list2cmdline(xperf_cmd))
    subprocess.call(xperf_cmd)

    csv_filename = '%s.csv' % etl_filename
    xperf_cmd = [xperf_path,
                 '-i', etl_filename,
                 '-o', csv_filename]
    if debug:
        print("executing '%s'" % subprocess.list2cmdline(xperf_cmd))
    subprocess.call(xperf_cmd)
    return csv_filename


def trackThread(row, firefoxPID):
    event, proc, tid = \
        row[EVENTNAME_INDEX], row[PROCESS_INDEX], row[THREAD_ID_INDEX]
    if event in ["T-DCStart", "T-Start"]:
        procName, procID = re.search("^(.*) \(\s*(\d+)\)$", proc).group(1, 2)
        if procID == str(firefoxPID):
            imgIdx = getIndex(event, IMAGEFUNC_COL)
            img = re.match("([^!]+)!", row[imgIdx]).group(1)
            if img == procName:
                gThreads[tid] = "main"
            else:
                gThreads[tid] = "nonmain"
    elif event in ["T-DCEnd", "T-End"] and tid in gThreads:
        del gThreads[tid]


def trackThreadFileIO(row, io, stage):
    event, tid = row[EVENTNAME_INDEX], row[THREAD_ID_INDEX]
    opType = {"FileIoWrite": "write", "FileIoRead": "read"}[event]
    th, stg = gThreads[tid], stages[stage]
    sizeIdx = getIndex(event, DISKBYTES_COL)
    bytes = int(row[sizeIdx], 16)
    io[(th, stg, "file_%s_ops" % opType)] = \
        io.get((th, stg, "file_%s_ops" % opType), 0) + 1
    io[(th, stg, "file_%s_bytes" % opType)] = \
        io.get((th, stg, "file_%s_bytes" % opType), 0) + bytes
    io[(th, stg, "file_io_bytes")] = \
        io.get((th, stg, "file_io_bytes"), 0) + bytes


def trackThreadNetIO(row, io, stage):
    event, tid = row[EVENTNAME_INDEX], row[THREAD_ID_INDEX]
    connIdIdx = getIndex(event, ACTIVITY_ID_COL)
    connID = row[connIdIdx]
    if connID not in gConnectionIDs:
        gConnectionIDs[connID] = tid
    origThread = gConnectionIDs[connID]
    if origThread in gThreads:
        netEvt = re.match("[\w-]+\/([\w-]+)", event).group(1)
        if netEvt in net_events:
            opType = net_events[netEvt]
            th, stg = gThreads[origThread], stages[stage]
            lenIdx = getIndex(event, NUMBYTES_COL)
            bytes = int(row[lenIdx])
            io[(th, stg, "net_%s_bytes" % opType)] = \
                io.get((th, stg, "net_%s_bytes" % opType), 0) + bytes
            io[(th, stg, "net_io_bytes")] = \
                io.get((th, stg, "net_io_bytes"), 0) + bytes


def updateStage(row, stage):
    guidIdx = getIndex(row[EVENTNAME_INDEX], EVENTGUID_COL)
    if row[guidIdx] == CEVT_WINDOWS_RESTORED and stage == 0:
        stage = 1
    elif row[guidIdx] == CEVT_XPCOM_SHUTDOWN and stage == 1:
        stage = 2
    return stage


def loadWhitelist(filename):
    if not filename:
        return
    if not os.path.exists(filename):
        print("Warning: xperf whitelist %s was not found" % filename)
        return
    lines = open(filename).readlines()
    # Expand paths
    lines = [os.path.expandvars(elem.strip()) for elem in lines]
    files = set()
    dirs = set()
    recur = set()
    for line in lines:
        if line.startswith("#"):
            continue
        elif line.endswith("\\*\\*"):
            recur.add(line[:-4])
        elif line.endswith("\\*"):
            dirs.add(line[:-2])
        else:
            files.add(line)
    return (files, dirs, recur)


def checkWhitelist(filename, whitelist):
    if not whitelist:
        return False
    if filename in whitelist[0]:
        return True
    if os.path.dirname(filename) in whitelist[1]:
        return True
    head = filename
    while len(head) > 3:  # Length 3 implies root directory, e.g. C:\
        head, tail = os.path.split(head)
        if head in whitelist[2]:
            return True
    return False


def etlparser(xperf_path, etl_filename, processID, approot=None,
              configFile=None, outputFile=None, whitelist_file=None,
              error_filename=None, all_stages=False, all_threads=False,
              debug=False):

    # setup output file
    if outputFile:
        outFile = open(outputFile, 'w')
    else:
        outFile = sys.stdout

    files = {}
    io = {}
    stage = 0

    print("reading etl filename: %s" % etl_filename)
    csvname = etl2csv(xperf_path, etl_filename, debug=debug)
    for row in readFile(csvname):
        event = row[EVENTNAME_INDEX]
        if event in ["T-DCStart", "T-Start", "T-DCEnd", "T-End"]:
            trackThread(row, processID)
        elif event in ["FileIoRead", "FileIoWrite"] and \
                row[THREAD_ID_INDEX] in gThreads:
            fileSummary(row, stage, files)
            trackThreadFileIO(row, io, stage)
        elif event.endswith("Event/Classic") and \
                row[THREAD_ID_INDEX] in gThreads:
            stage = updateStage(row, stage)
        elif event.startswith("Microsoft-Windows-TCPIP"):
            trackThreadNetIO(row, io, stage)

    if debug:
        uploadFile(csvname)
    else:
        mozfile.remove(csvname)

    output = "thread, stage, counter, value\n"
    for cntr in sorted(io.iterkeys()):
        output += "%s, %s\n" % (", ".join(cntr), str(io[cntr]))
    if outputFile:
        fname = "%s_thread_stats%s" % os.path.splitext(outputFile)
        with open(fname, "w") as f:
            f.write(output)

        if debug:
            uploadFile(fname)
    else:
        print(output)

    whitelist = loadWhitelist(whitelist_file)

    header = ("filename, tid, stage, readcount, readbytes, writecount,"
              " writebytes")
    outFile.write(header + "\n")

    # Filter out stages, threads, and whitelisted files that we're not
    # interested in
    filekeys = filter(lambda x: (all_stages or x[2] == stages[0]) and
                                (all_threads or x[1].endswith("(main)")) and
                                (all_stages and x[2] != stages[0] or
                                 not checkWhitelist(x[0], whitelist)),
                      files.iterkeys())
    if debug:
        # in debug, we want stages = [startup+normal] and all threads, not just (main)
        # we will use this data to upload fileIO info to blobber only for debug mode
        outputData = filter(lambda x: (all_stages or x[2] in [stages[0], stages[1]]) and
                                      (all_stages and x[2] not in [stages[0], stages[1]] or
                                       not checkWhitelist(x[0], whitelist)),
                            files.iterkeys())
    else:
        outputData = filekeys

    # output data
    for row in outputData:
        output = "%s, %s, %s, %s, %s, %s, %s\n" % (
            row[0],
            row[1],
            row[2],
            files[row]['DiskReadCount'],
            files[row]['DiskReadBytes'],
            files[row]['DiskWriteCount'],
            files[row]['DiskWriteBytes']
        )
        outFile.write(output)

    if outputFile:
        # close the file handle
        outFile.close()
        if debug:
            uploadFile(outputFile)

    # We still like to have the outputfile to record the raw data, now
    # filter out acceptable files/ranges
    filename = None
    wl_temp = {}
    dirname = os.path.dirname(__file__)
    if os.path.exists(os.path.join(dirname, 'xperf_whitelist.json')):
        filename = os.path.join(dirname, 'xperf_whitelist.json')
    elif os.path.exists(os.path.join(dirname, 'xtalos')) and \
            os.path.exists(os.path.join(dirname, 'xtalos',
                                        'xperf_whitelist.json')):
        filename = os.path.join(dirname, 'xtalos', 'xperf_whitelist.json')

    wl_temp = {}
    if filename:
        with open(filename, 'r') as fHandle:
            wl_temp = json.load(fHandle)

    # Approot is the full path where the application is located at
    # We depend on it for dependentlibs.list to ignore files required for
    # normal startup.
    if approot:
        if os.path.exists('%s\\dependentlibs.list' % approot):
            with open('%s\\dependentlibs.list' % approot, 'r') as fhandle:
                libs = fhandle.readlines()

            for lib in libs:
                wl_temp['{firefox}\\%s' % lib.strip()] = {'ignore': True}

    # Windows isn't case sensitive, this protects us against mismatched
    # systems.
    wl = {}
    for item in wl_temp:
        wl[item.lower()] = wl_temp[item]

    errors = []
    for row in filekeys:
        filename = row[0]
        filename = filename.lower()
        # take care of 'program files (x86)' matching 'program files'
        filename = filename.replace(" (x86)", '')

        paths = ['profile', 'firefox', 'desktop', 'talos']
        for path in paths:
            pathname = '%s\\' % path
            parts = filename.split(pathname)
            if len(parts) >= 2:
                filename = "{%s}\\%s" % (path, pathname.join(parts[1:]))

        parts = filename.split('\\installtime')
        if len(parts) >= 2:
            filename = "%s\\{time}" % parts[0]

        # NOTE: this is Prefetch or prefetch, not case sensitive operating
        # system
        parts = filename.split('refetch')
        if len(parts) >= 2:
            filename = "%srefetch\\{prefetch}.pf" % parts[0]

        if filename in wl:
            if 'ignore' in wl[filename] and wl[filename]['ignore']:
                continue

# too noisy
#            if wl[filename]['minbytes'] > (files[row]['DiskReadBytes'] +\
#                   files[row]['DiskWriteBytes']):
#                print "%s: read %s bytes, less than expected minimum: %s"
#                       % (filename, (files[row]['DiskReadBytes'] +
#                                     files[row]['DiskWriteBytes']),
#                          wl[filename]['minbytes'])

# don't report in first round
#            elif wl[filename]['maxbytes'] < (files[row]['DiskReadBytes'] +\
#                   files[row]['DiskWriteBytes']):
#                errors.append("%s: read %s bytes, more than expected maximum:"
#                              " %s"
#                              % (filename, (files[row]['DiskReadBytes'] +
#                                 files[row]['DiskWriteBytes']),
#                              wl[filename]['maxbytes']))

# too noisy
#            elif wl[filename]['mincount'] > (files[row]['DiskReadCount'] +\
#                   files[row]['DiskWriteCount']):
#                print "%s: %s accesses, less than expected minimum: %s"
#                       % (filename, (files[row]['DiskReadCount'] +
#                                     files[row]['DiskWriteCount']),
#                          wl[filename]['mincount'])

# don't report in first round
#            elif wl[filename]['maxcount'] < (files[row]['DiskReadCount'] +\
#                   files[row]['DiskWriteCount']):
#                errors.append("%s: %s accesses, more than expected maximum:"
#                              " %s"
#                              % (filename, (files[row]['DiskReadCount'] +
#                                            files[row]['DiskWriteCount']),
#                                 wl[filename]['maxcount']))
        else:
            errors.append("File '%s' was accessed and we were not expecting"
                          " it.  DiskReadCount: %s, DiskWriteCount: %s,"
                          " DiskReadBytes: %s, DiskWriteBytes: %s"
                          % (filename,
                             files[row]['DiskReadCount'],
                             files[row]['DiskWriteCount'],
                             files[row]['DiskReadBytes'],
                             files[row]['DiskWriteBytes']))

    if errors:
        # output specific errors to be picked up by tbpl parser
        for error in errors:
            # NOTE: the ' :' is intentional, without the space before the :,
            # some parser will translate this
            print("TEST-UNEXPECTED-FAIL : xperf: %s" % error)

        # We detect if browser_failures.txt exists to exit and turn the job
        # orange
        if error_filename:
            with open(error_filename, 'w') as errorFile:
                errorFile.write('\n'.join(errors))

        if debug:
            uploadFile(etl_filename)


def etlparser_from_config(config_file, **kwargs):
    """start from a YAML config file"""

    # option defaults
    args = {'xperf_path': 'xperf.exe',
            'etl_filename': 'test.etl',
            'outputFile': 'etl_output.csv',
            'processID': None,
            'approot': None,
            'whitelist_file': None,
            'error_filename': None,
            'all_stages': False,
            'all_threads': False
            }
    args.update(kwargs)

    # override from YAML config file
    args = xtalos.options_from_config(args, config_file)

    # ensure process ID is given
    if not args.get('processID'):
        raise xtalos.XTalosError("No process ID option given")

    # ensure path to xperf given
    if not os.path.exists(args['xperf_path']):
        raise xtalos.XTalosError("ERROR: xperf_path '%s' does not exist"
                                 % args['xperf_path'])

    # update args with config file
    args['configFile'] = config_file

    # call etlparser
    etlparser(**args)


def main(args=sys.argv[1:]):

    # parse command line arguments
    parser = xtalos.XtalosOptions()
    args = parser.parse_args(args)
    args = parser.verifyOptions(args)
    if args is None:
        parser.error("Unable to verify arguments")
    if not args.processID:
        parser.error("No process ID argument given")

    # call API
    etlparser(args.xperf_path, args.etl_filename, args.processID, args.approot,
              args.configFile, args.outputFile, args.whitelist_file,
              args.error_filename, args.all_stages, args.all_threads,
              debug=args.debug_level >= xtalos.DEBUG_INFO)

if __name__ == "__main__":
    main()
