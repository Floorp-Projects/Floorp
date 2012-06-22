# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# NOTE: This is a hack which disobeys a number of conventions and best
# practices. It's here just to be easily shared. If it's to remain in the
# repository it should be refactored.

#!/usr/bin/env python

import stringmanipulation
import filemanagement
import sys

trace_remove_key_word = 'kTraceModuleCall'

if((len(sys.argv) != 2) and (len(sys.argv) != 3)):
    print 'parameters are: parent directory [--commit]'
    quit()

if((len(sys.argv) == 3) and (sys.argv[2] != '--commit')):
    print 'parameters are: parent directory [--commit]'
    quit()

commit = (len(sys.argv) == 3)

directory = sys.argv[1];
occurances = []

trace_identifier = 'WEBRTC_TRACE('
extensions = ['.h','.cc','.c','.cpp']
files_to_fix = []
for extension in extensions:
    files_to_fix.extend(filemanagement.listallfilesinfolder(directory,\
                                                       extension))

# This function identifies the begining of a trace statement
def istracebegining(line):
    return stringmanipulation.issubstring(line, trace_identifier) != -1

def endofstatement(line):
    return stringmanipulation.issubstring(line, ';') != -1

def removekeywordfound(line):
    return stringmanipulation.issubstring(line, trace_remove_key_word) != -1

# Used to store temporary result before flushing to real file when finished
def temporaryfilename():
    return 'deleteme.txt'


def find_occurances(path, file_name):
    full_filename = path + file_name
    file_handle = open(full_filename,'r')
    line_is_trace = False
    last_trace_line = -1
    for line_nr, line in enumerate(file_handle):
        if(istracebegining(line)):
            line_is_trace = True;
            last_trace_line = line_nr

        if(line_is_trace):
            if(removekeywordfound(line)):
                occurances.append(last_trace_line)

        if(endofstatement(line)):
            line_is_trace = False;

def remove_occurances(path, file_name):
    full_file_name = path + file_name
    if (not filemanagement.fileexist(full_file_name)):
        print 'File ' + full_file_name + ' is not found.'
        print 'Should not happen! Ever!'
        quit()

    full_temporary_file_name = path + temporaryfilename()
    temporary_file = open(full_temporary_file_name,'w')
    original_file = open(full_file_name,'r')
    next_occurance_id = 0;
    removing_statement = False
    if(len(occurances) == next_occurance_id):
        return
    next_occurance = occurances[next_occurance_id]
    next_occurance_id += 1
    for line_nr, line in enumerate(original_file):
        if(line_nr == next_occurance):
            removing_statement = True
            if(len(occurances) == next_occurance_id):
                next_occurance_id = -1
            else:
                next_occurance = occurances[next_occurance_id]
                next_occurance_id += 1

        if (not removing_statement):
            temporary_file.writelines(line)

        if(endofstatement(line)):
            removing_statement = False;

    temporary_file.close()
    original_file.close()
    filemanagement.copyfile(full_file_name,full_temporary_file_name)
    filemanagement.deletefile(full_temporary_file_name)

def nextoccurance():
    if (len(occurances) == 0):
        return -1
    return_value = occurances[0]
    occurances = occurances[1:len(occurances)]
    return return_value

def would_be_removed_occurances(path, file_name):
    full_file_name = path + file_name
    if (not filemanagement.fileexist(full_file_name)):
        print 'File ' + full_file_name + ' is not found.'
        print 'Should not happen! Ever!'
        quit()

    original_file = open(full_file_name,'r')
    removing_statement = False
    next_occurance_id = 0;
    if(len(occurances) == next_occurance_id):
        return
    next_occurance = occurances[next_occurance_id]
    next_occurance_id += 1
    for line_nr, line in enumerate(original_file):
        if(line_nr == next_occurance):
            removing_statement = True
            if(len(occurances) == next_occurance_id):
                return
            next_occurance = occurances[next_occurance_id]
            next_occurance_id += 1

        if (removing_statement):
            print line_nr

        if(endofstatement(line)):
            removing_statement = False;
            if(next_occurance == -1):
                break
    original_file.close()

for index in range(len(files_to_fix)):
    if(commit):
        print (100*index)/len(files_to_fix)

    path_dir = files_to_fix[index][0]
    filename = files_to_fix[index][1]

    #print path_dir + filename
    occurances = []
    find_occurances(path_dir, filename)

    if(not commit):
        would_be_removed_occurances(path_dir, filename)
        continue
    remove_occurances(path_dir, filename)
