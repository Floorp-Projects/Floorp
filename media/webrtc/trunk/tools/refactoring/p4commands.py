import os
import filemanagement

# checks out entire p4 repository
def checkoutallfiles():
    os.system('p4 edit //depotGoogle/...')
    return

# reverts all unchanged files, this is completely innoculus
def revertunchangedfiles():
    os.system('p4 revert -a //depotGoogle/...')
    return

def integratefile( old_name, new_name):
    if(old_name == new_name):
        return
    if(not filemanagement.fileexist(old_name)):
        return
    integrate_command = 'p4 integrate -o -f ' +\
                        old_name +\
                        ' ' +\
                        new_name +\
                        ' > p4summary.txt 2> error.txt'
    os.system(integrate_command)
    #print integrate_command
    delete_command = 'p4 delete -c default ' +\
                     old_name +\
                     ' > p4summary.txt 2> error.txt'
    os.system(delete_command)
    #print delete_command
    return
