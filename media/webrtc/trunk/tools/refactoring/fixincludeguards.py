#!/usr/bin/env python

import stringmanipulation
import filemanagement
import sys

extensions = ['.h']

ignore_these = ['my_ignore_header.h']

if((len(sys.argv) != 2) and (len(sys.argv) != 3)):
    print 'parameters are: directory [--commit]'
    quit()

directory = sys.argv[1];
if(not filemanagement.pathexist(directory)):
    print 'path ' + directory + ' does not exist'
    quit()

if((len(sys.argv) == 3) and (sys.argv[2] != '--commit')):
    print 'parameters are: parent directory extension new extension [--commit]'
    quit()

commit = False
if(len(sys.argv) == 3):
    commit = True

for extension in extensions:
    files_to_fix = filemanagement.listallfilesinfolder(directory,\
                                                       extension)

def buildincludeguardname(path,filename):
    full_file_name = 'WEBRTC_' + path + filename
    full_file_name = full_file_name.upper()
    full_file_name = stringmanipulation.replaceoccurances(full_file_name, '/', '_')
    full_file_name = stringmanipulation.replaceoccurances(full_file_name, '\\', '_')
    full_file_name = stringmanipulation.replaceoccurances(full_file_name, '.', '_')
    full_file_name += '_'
    return full_file_name

def buildnewincludeguardset(path,filename):
    include_guard_name = buildincludeguardname(path,filename)
    if(include_guard_name == ''):
        return []
    return_value = []
    return_value.append('#ifndef ' + include_guard_name)
    return_value.append('#define ' + include_guard_name)
    return_value.append(include_guard_name)
    return return_value

def printincludeguardset(include_guard_set):
    print 'First line: ' + include_guard_set[0]
    print 'Second line: ' + include_guard_set[1]
    print 'Last line: ' + include_guard_set[2]
    return

include_guard_begin_identifier = ['#ifndef', '#if !defined']
include_guard_second_identifier = ['#define']
def findincludeguardidentifier(line):
    for begin_identifier in include_guard_begin_identifier:
        line = stringmanipulation.removealloccurances(line,begin_identifier)
    for second_identifier in include_guard_begin_identifier:
        line = stringmanipulation.removealloccurances(line,second_identifier)
    removed_prefix = [True,'']
    line = stringmanipulation.whitespacestoonespace(line)
    while(removed_prefix[0]):
        removed_prefix = stringmanipulation.removeprefix(line,' ')
        line = removed_prefix[1]
    line = stringmanipulation.removealloccurances(line,'(')
    if(line == ''):
        return ''
    word_pos = stringmanipulation.getword(line,0)
    return_value = line[0:word_pos[1]]
    return_value = return_value.rstrip('\r\n')
    return return_value

def findoldincludeguardset(path,filename):
    return_value = []
    full_file_name = path + filename
    file_pointer = open(full_file_name,'r')
    include_guard_name = ''
    for line in file_pointer:
        if (include_guard_name == ''):
            for compare_string in include_guard_begin_identifier:
                if (stringmanipulation.issubstring(compare_string, line) != -1):
                    include_guard_name = findincludeguardidentifier(line)
                    if (include_guard_name == ''):
                        break
                    line = line.rstrip('\r\n')
                    return_value.append(line)
                    break
        else:
            for compare_string in include_guard_second_identifier:
                if (stringmanipulation.issubstring(compare_string, line) != -1):
                    if (stringmanipulation.issubstring(include_guard_name, line) != -1):
                        line = line.rstrip('\r\n')
                        return_value.append(line)
                        return_value.append(include_guard_name)
                        return return_value
            include_guard_name = ''
            return_value = []
    return []

failed_files = []
for index in range(len(files_to_fix)):
    if(commit):
        print (100*index)/len(files_to_fix)
    path_dir = files_to_fix[index][0]
    filename = files_to_fix[index][1]
    is_ignore = False
    for ignore_names in ignore_these:
        if(filename == ignore_names):
            is_ignore = True
            break
    if(is_ignore):
        continue
    old_include_guard_set = findoldincludeguardset(path_dir,filename)
    if (len(old_include_guard_set) != 3) :
        failed_files.append('unable to figure out the include guards for ' + filename)
        continue

    new_include_guard_set = buildnewincludeguardset(path_dir,filename)
    if (len(new_include_guard_set) != 3) :
        failed_files.append('unable to figure out new the include guards for ' + filename)
        continue

    if(not commit):
        print 'old guard: ' + old_include_guard_set[2]
        print 'new guard: ' + new_include_guard_set[2]
        continue

    for index in range(2):
        # enough to only replace for file. However, no function for that
        for extension in extensions:
            filemanagement.replacestringinfolder(path_dir,old_include_guard_set[index],new_include_guard_set[index],extension)
    # special case for last to avoid complications
    for extension in extensions:
        filemanagement.replacestringinfolder(path_dir,' ' + old_include_guard_set[2],' ' + new_include_guard_set[2],extension)
        filemanagement.replacestringinfolder(path_dir,'\\/\\/' + old_include_guard_set[2],'\\/\\/ ' + new_include_guard_set[2],extension)


if(len(failed_files) > 0):
    print 'Following failures should be investigated manually:'
for line in failed_files:
    print line
