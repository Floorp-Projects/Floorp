#!/usr/bin/env python

import stringmanipulation
import filemanagement
import p4commands
import sys

extensions = ['.h', '.cpp', '.cc', '.gyp']

ignore_these = ['list_no_stl.h','map_no_stl.h','constructor_magic.h']

exceptions = [
['GIPSRWLock.h','rw_lock.h'],
['GIPSCriticalsection.h','critical_section.h'],
]

if((len(sys.argv) != 4) and (len(sys.argv) != 5)):
    print 'parameters are: parent directory extension new extension [--commit]'
    quit()

directory = sys.argv[1];
if(not filemanagement.pathexist(directory)):
    print 'path ' + directory + ' does not exist'
    quit()

old_extension = sys.argv[2]
if(not stringmanipulation.isextension(old_extension)):
    print old_extension + ' is not a valid extension'
    quit()

new_extension = sys.argv[3]
if(not stringmanipulation.isextension(new_extension)):
    print new_extension + ' is not a valid extension'
    quit()

if((len(sys.argv) == 5) and (sys.argv[4] != '--commit')):
    print 'parameters are: parent directory extension new extension [--commit]'
    quit()

commit = False
if(len(sys.argv) == 5):
    commit = True

files_to_integrate = filemanagement.listallfilesinfolder(directory,\
                                                         old_extension)

if(commit):
    p4commands.checkoutallfiles()
for index in range(len(files_to_integrate)):
    if(commit):
        print (100*index)/len(files_to_integrate)
    path_dir = files_to_integrate[index][0]
    filename = files_to_integrate[index][1]
    is_ignore = False
    for ignore_names in ignore_these:
        if(filename == ignore_names):
            is_ignore = True
            break
    if(is_ignore):
        continue

    new_file_name = ''
    is_exception = False
    for exception_name,exception_name_new in exceptions:
        if(filename == exception_name):
            is_exception = True
            new_file_name = exception_name_new
            break

    if(not is_exception):
        new_file_name = filename

        new_file_name = stringmanipulation.removeallprefix(new_file_name,\
                                                       'gips')
        new_file_name = stringmanipulation.removealloccurances(new_file_name,\
                                                       'module')
        new_file_name = stringmanipulation.changeextension(new_file_name,\
                                           old_extension,\
                                           new_extension)
        new_file_name = stringmanipulation.fixabbreviations( new_file_name )
        new_file_name = stringmanipulation.lowercasewithunderscore(new_file_name)
    if(not commit):
        print 'File ' + filename + ' will be replaced with ' + new_file_name
        continue
    full_new_file_name = path_dir + new_file_name
    full_old_file_name = path_dir + filename
    if(full_new_file_name != full_old_file_name):
        p4commands.integratefile(full_old_file_name,full_new_file_name)
    else:
        print 'skipping ' + new_file_name + ' due to no change'
    for extension in extensions:
        print 'replacing ' + filename
        if (extension == ".gyp"):
            filemanagement.replacestringinallsubfolders(
                filename,new_file_name,extension)
        else:
            filemanagement.replacestringinallsubfolders(
                '\"' + filename + '\"', '\"' + new_file_name + '\"', extension)
if(commit):
    p4commands.revertunchangedfiles()
