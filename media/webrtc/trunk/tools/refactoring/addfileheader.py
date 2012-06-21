#!/usr/bin/env python

import stringmanipulation
import filemanagement
import sys

extensions = ['.h','.cc','.c','.cpp']

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

files_to_fix = []
for extension in extensions:
    files_to_fix.extend(filemanagement.listallfilesinfolder(directory,\
                                                       extension))

# Just steal the header from the template
def fileheaderasstring():
    template_file_name = 'license_template.txt'
    if (not filemanagement.fileexist(template_file_name)):
        print 'File ' + template_file_name + ' not found!'
        quit()
    template_file = open(template_file_name,'r')
    return_string = ''
    for line in template_file:
        return_string += line
    return return_string

# Just steal the header from the template
def fileheaderasarray():
    template_file_name = 'license_template.txt'
    if (not filemanagement.fileexist(template_file_name)):
        print 'File ' + template_file_name + ' not found!'
        quit()
    template_file = open(template_file_name,'r')
    return_value = []
    for line in template_file:
        return_value.append(line)
    return return_value


def findheader(path, file_name):
    full_file_name = path + file_name
    if (not filemanagement.fileexist(full_file_name)):
        print 'File ' + file_name + ' not found!'
        print 'Unexpected error!'
        quit()
    file_handle = open(full_file_name)
    template_file_content = fileheaderasarray()
    compare_content = []
    # load the same number of lines from file as the fileheader
    for index in range(len(template_file_content)):
        line = file_handle.readline()
        if (line == ''):
            return False
        compare_content.append(line)

    while (True):
        found = True
        for index in range(len(template_file_content)):
            line1 = template_file_content[index]
            line2 = compare_content[index]
            if(line1 != line2):
                found = False
                break
        if (found):
            return True
        compare_content = compare_content[1:len(compare_content)]
        line = file_handle.readline()
        if (line == ''):
            return False
        compare_content.append(line)
    return False

# Used to store temporary result before flushing to real file when finished
def temporaryfilename(old_file_name):
    return old_file_name + '.deleteme'

def updatefile(path, old_file_name):
    full_old_file_name = path + old_file_name
    if (not filemanagement.fileexist(full_old_file_name)):
        print 'File ' + full_old_file_name + ' is not found.'
        print 'Should not happen! Ever!'
        quit()

    full_temporary_file_name = path + temporaryfilename(old_file_name)

    # Make sure that the files are closed by putting them out of scope
    old_file = open(full_old_file_name,'r')
    temporary_file = open(full_temporary_file_name,'w')

    temporary_file.writelines(fileheaderasstring())
    remove_whitespaces = True
    for line in old_file:
        if (remove_whitespaces and (len(line.split()) == 0)):
            continue
        else:
            remove_whitespaces = False
        temporary_file.writelines(line)
    old_file.close()
    temporary_file.close()

    filemanagement.copyfile(full_old_file_name,full_temporary_file_name)
    filemanagement.deletefile(full_temporary_file_name)


failed_files = []
skipped_files = []
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

# Let the word copyright be our sanity, i.e. make sure there is only one
# copy right occurance or report that there will be no change
    if(filemanagement.findstringinfile(path_dir,filename,'Copyright') or
        filemanagement.findstringinfile(path_dir,filename,'copyright') or
        filemanagement.findstringinfile(path_dir,filename,'COPYRIGHT')):
        if(findheader(path_dir,filename)):
            skipped_files.append(path_dir + filename)
        else:
            failed_files.append(path_dir + filename)
        continue

    if (not commit):
        print 'File ' + path_dir + filename + ' will be updated'
        continue
    updatefile(path_dir,filename)

tense = 'will be'
if (commit):
    tense = 'has been'
if (len(skipped_files) > 0):
    print str(len(skipped_files)) + ' file(s) ' + tense + ' skipped since they already have the correct header'

if (len(failed_files) > 0):
    print 'Following files seem to have an invalid file header:'
for line in failed_files:
    print line
