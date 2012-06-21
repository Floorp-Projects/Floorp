import fnmatch
import os
import stringmanipulation

def fileexist( file_name ):
    return os.path.isfile(file_name)

def pathexist( path ):
    return os.path.exists(path)

def fixpath( path ):
    return_value = path
    if( return_value[len(return_value) - 1] != '/'):
        return_value = return_value + '/'
    return return_value

def listallfilesinfolder( path, extension ):
    matches = []
    signature = '*' + extension
    for root, dirnames, filenames in os.walk(path):
        for filename in fnmatch.filter(filenames, signature):
            matches.append([fixpath(root), filename])
    return matches

def copyfile(to_file, from_file):
    if(not fileexist(from_file)):
        return
    command = 'cp -f ' + from_file + ' ' + to_file
    os.system(command)
    #print command

def deletefile(file_to_delete):
    if(not fileexist(file_to_delete)):
        return
    os.system('rm ' + file_to_delete)

# very ugly but works, so keep for now
def findstringinfile(path,file_name,search_string):
    command = 'grep \'' + search_string + '\' ' + path + file_name + ' > deleteme.txt'
    return_value = os.system(command)
#    print command
    return (return_value == 0)

def replacestringinfolder( path, old_string, new_string, extension ):
    if(not stringmanipulation.isextension(extension)):
        print 'failed to search and replace'
        return
    if(len(old_string) == 0):
        print 'failed to search and replace'
        return
    find_command = 'ls '+ path + '/*' + extension
    sed_command = 'sed -i \'s/' + old_string + '/' + new_string +\
                     '/g\' *' + extension
    command_string = find_command + ' | xargs ' + sed_command + ' 2> deleteme.txt'
    os.system(command_string)
    #print command_string

#find ./ -name "*.h" -type f  | xargs -P 0 sed -i 's/process_thread_wrapper.h/process_thread.h/g' *.h deleteme.txt
def replacestringinallsubfolders( old_string, new_string, extension):
    if(not stringmanipulation.isextension(extension)):
        print 'failed to search and replace'
        return
    if(len(old_string) == 0):
        print 'failed to search and replace'
        return

    find_command = 'find ./ -name \"*' + extension + '\" -type f'
    sed_command = 'sed -i \'s/' + old_string + '/' + new_string +\
                     '/g\' *' + extension
    command_string = find_command + ' | xargs -P 0 ' + sed_command + ' 2> deleteme.txt'
    os.system(command_string)
    #print command_string
