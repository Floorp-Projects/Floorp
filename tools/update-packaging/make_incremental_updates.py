import os
import shutil
import sha
import sha
from os.path import join, getsize
from stat import *
import re
import sys
import getopt
import time
import datetime
import bz2
import string
import tempfile

class PatchInfo:
    """ Represents the meta-data associated with a patch
        work_dir = working dir where files are stored for this patch
        archive_files = list of files to include in this patch
        manifest = set of patch instructions
        file_exclusion_list = 
        files to exclude from this patch. names without slashes will be
                              excluded anywhere in the directory hiearchy.   names with slashes
                              will only be excluded at that exact path
        """
    def __init__(self, work_dir, file_exclusion_list, path_exclusion_list):
        self.work_dir=work_dir
        self.archive_files=[]
        self.manifest=[]
        self.file_exclusion_list=file_exclusion_list
        self.path_exclusion_list=path_exclusion_list
        
    def append_add_instruction(self, filename):
        """ Appends an add instruction for this patch.   
            if the filename starts with extensions/ adds an add-if instruction
            to test the existence of the subdirectory.  This was ported from
            mozilla/tools/update-packaging/common.sh/make_add_instruction
        """
        if filename.startswith("extensions/"):
            testdir = "extensions/"+filename.split("/")[1]  # Dir immediately following extensions is used for the test
            self.manifest.append('add-if "'+testdir+'" "'+filename+'"')
        else:
            self.manifest.append('add "'+filename+'"')
           
    def append_patch_instruction(self, filename, patchname):
        """ Appends an patch instruction for this patch.   
            
            filename = file to patch
            patchname = patchfile to apply to file
            
            if the filename starts with extensions/ adds a patch-if instruction
            to test the existence of the subdirectory.  
            if the filename starts with searchplugins/ add a add-if instruction for the filename
            This was ported from
            mozilla/tools/update-packaging/common.sh/make_patch_instruction
        """
        if filename.startswith("extensions/"):
            testdir = "extensions/"+filename.split("/")[1]
            self.manifest.append('patch-if "'+testdir+'" "'+patchname+'" "'+filename+'"')
        elif filename.startswith("searchplugins/"):
            self.manifest.append('patch-if "'+filename+'" "'+patchname+'" "'+filename+'"')
        else:
            self.manifest.append('patch "'+patchname+'" "'+filename+'"')
                
    def append_remove_instruction(self, filename):
        """ Appends an remove instruction for this patch.   
            This was ported from
            mozilla/tools/update-packaging/common.sh/make_remove_instruction
        """
        self.manifest.append('remove "'+filename+'"')

    def create_manifest_file(self):
        """ Createst the manifest file into to the root of the work_dir """
        manifest_file_path = os.path.join(self.work_dir,"update.manifest")
        manifest_file = open(manifest_file_path, "w")
        manifest_file.writelines(string.join(self.manifest, '\n'))
        manifest_file.writelines("\n")
        manifest_file.close()    

        bzip_file(manifest_file_path)
        self.archive_files.append('"update.manifest"')


    def build_marfile_entry_hash(self, root_path):
         """ Iterates through the root_path, creating a MarFileEntry for each file
             in that path.  Excluseds any filenames in the file_exclusion_list
         """
         mar_entry_hash = {}
         filename_set = set()
         for root, dirs, files in os.walk(root_path):
             for name in files:
                 # filename is relative path from root directory
                 partial_path = root[len(root_path)+1:]
                 if name not in self.file_exclusion_list:
                     filename = os.path.join(partial_path, name)
                     if "/"+filename not in self.path_exclusion_list:
                         mar_entry_hash[filename]=MarFileEntry(root_path, filename)
                         filename_set.add(filename)
         return mar_entry_hash, filename_set
 
       
class MarFileEntry:
    """Represents a file inside a Mozilla Archive Format (MAR)
        abs_path = abspath to the the file
        name =  relative path within the mar.  e.g.
          foo.mar/dir/bar.txt extracted into /tmp/foo:
            abs_path=/tmp/foo/dir/bar.txt
            name = dir/bar.txt
    """ 
    def __init__(self, root, name):
        """root = path the the top of the mar
           name = relative path within the mar"""
        self.name=name
        self.abs_path=os.path.join(root,name)
        self.sha_cache=None

    def __str__(self):
        return 'Name: %s FullPath: %s' %(self.name,self.abs_path)

    def calc_file_sha_digest(self, filename):        
        """ Returns sha digest of given filename"""
        file_content = open(filename, 'r').read()
        return sha.new(file_content).digest()

    def sha(self):
        """ Returns sha digest of file repreesnted by this _marfile_entry"""
        if not self.sha_cache:
            self.sha_cache=self.calc_file_sha_digest(self.abs_path)
        return self.sha_cache

def exec_shell_cmd(cmd):
    """Execs shell cmd and raises an exception if the cmd fails"""
    if (os.system(cmd)):
        raise Exception, "cmd failed "+cmd


def copy_file(src_file_abs_path, dst_file_abs_path):
    """ Copies src to dst creating any parent dirs required in dst first """
    dst_file_dir=os.path.dirname(dst_file_abs_path)
    if not os.path.exists(dst_file_dir):
         os.makedirs(dst_file_dir)
    # Copy the file over
    shutil.copy2(src_file_abs_path, dst_file_abs_path)

def bzip_file(filename):
    """ Bzip's the file in place.  The original file is replaced with a bzip'd version of itself
        assumes the path is absolute"""
    exec_shell_cmd('bzip2 -z9 "' + filename+'"')
    os.rename(filename+".bz2",filename)
    
def bunzip_file(filename):
    """ Bzip's the file in palce.   The original file is replaced with a bunzip'd version of itself.
        doesn't matter if the filename ends in .bz2 or not"""
    if not filename.endswith(".bz2"):
        os.rename(filename, filename+".bz2")
        filename=filename+".bz2"
    exec_shell_cmd('bzip2 -d "' + filename+'"') 


def extract_mar(filename, work_dir): 
    """ Extracts the marfile intot he work_dir
        assumes work_dir already exists otherwise will throw osError"""
    print "Extracting "+filename+" to "+work_dir
    saved_path = os.getcwd()
    try:
        os.chdir(work_dir)
        exec_shell_cmd("mar -x "+filename)    
    finally:
        os.chdir(saved_path)

def create_partial_patch_for_file(from_marfile_entry, to_marfile_entry, shas, patch_info):
    """ Creates the partial patch file and manifest entry for the pair of files passed in
    """
    if not (from_marfile_entry.sha(),to_marfile_entry.sha()) in shas:
        print "diffing: " + from_marfile_entry.name
        
        #bunzip to/from
        bunzip_file(from_marfile_entry.abs_path)
        bunzip_file(to_marfile_entry.abs_path)

        # The patch file will be created in the working directory with the
        # name of the file in the mar + .patch
        patch_file_abs_path = os.path.join(patch_info.work_dir,from_marfile_entry.name+".patch")
        patch_file_dir=os.path.dirname(patch_file_abs_path)
        if not os.path.exists(patch_file_dir):
            os.makedirs(patch_file_dir)

        # Create bzip'd patch file
        exec_shell_cmd("mbsdiff "+from_marfile_entry.abs_path+" "+to_marfile_entry.abs_path+" "+patch_file_abs_path)
        bzip_file(patch_file_abs_path)

        # Create bzip's full file
        full_file_abs_path =  os.path.join(patch_info.work_dir, to_marfile_entry.name)   
        shutil.copy2(to_marfile_entry.abs_path, full_file_abs_path)
        bzip_file(full_file_abs_path)
  
        ## TOODO NEED TO ADD HANDLING FOR FORCED UPDATES
        if os.path.getsize(patch_file_abs_path) < os.path.getsize(full_file_abs_path):
            # Patch is smaller than file.  Remove the file and add patch to manifest
            os.remove(full_file_abs_path)
            file_in_manifest_name = from_marfile_entry.name+".patch"
            file_in_manifest_abspath = patch_file_abs_path
            patch_info.append_patch_instruction(to_marfile_entry.name, file_in_manifest_name)
        else:            
            # File is smaller than patch.  Remove the patch and add file to manifest
            os.remove(patch_file_abs_path)
            file_in_manifest_name = from_marfile_entry.name
            file_in_manifest_abspath = full_file_abs_path
            patch_info.append_add_instruction(file_in_manifest_name)
            
        shas[from_marfile_entry.sha(),to_marfile_entry.sha()] = (file_in_manifest_name,file_in_manifest_abspath)
        patch_info.archive_files.append('"'+file_in_manifest_name+'"')        
    else:
        print "skipping diff: " + from_marfile_entry.name
        filename, src_file_abs_path = shas[from_marfile_entry.sha(),to_marfile_entry.sha()]
        # We've already calculated the patch for this pair of files.   
        if (filename.endswith(".patch")):
            # Patch was smaller than file - add patch instruction to manifest
            file_in_manifest_name = to_marfile_entry.name+'.patch';
            patch_info.append_patch_instruction(to_marfile_entry.name, file_in_manifest_name)
        else:
            # File was smaller than file - add file to manifest
            file_in_manifest_name = to_marfile_entry.name
            patch_info.append_add_instruction(file_in_manifest_name)                
        # Copy the pre-calculated file into our new patch work aread
        copy_file(src_file_abs_path, os.path.join(patch_info.work_dir, file_in_manifest_name))
        patch_info.archive_files.append('"'+file_in_manifest_name+'"')
 
def create_add_patch_for_file(to_marfile_entry, patch_info):           
    """  Copy the file to the working dir, add the add instruction, and add it to the list of archive files """
    print "Adding New File " + to_marfile_entry.name    
    copy_file(to_marfile_entry.abs_path, os.path.join(patch_info.work_dir, to_marfile_entry.name))
    patch_info.append_add_instruction(to_marfile_entry.name)
    patch_info.archive_files.append('"'+to_marfile_entry.name+'"')    

def process_explicit_remove_files(dir_path, patch_info): 
    """ Looks for a 'removed-files' file in the dir_path.  If the removed-files does not exist
    this will throw.  If found adds the removed-files
    found in that file to the patch_info"""

    # Windows and linux have this file at the root of the dir
    list_file_path = os.path.join(dir_path, "removed-files")
    prefix=""
    if not os.path.exists(list_file_path):
        # Mac has is in Contents/MacOS/
        prefix= "Contents/MacOS"
        list_file_path = os.path.join(dir_path, prefix+"/removed-files")

    if (os.path.exists(list_file_path)):
        list_file = bz2.BZ2File(list_file_path,"r") # throws if doesn't exist

        for line in list_file:
            line = line.strip()
            # Exclude any blank lines or any lines ending with a slash, which indicate
            # directories.  The updater doesn't know how to remove entire directories.
            if line and not line.endswith("/"): 
                patch_info.append_remove_instruction(os.path.join(prefix,line))

def create_partial_patch(from_dir_path, to_dir_path, patch_filename, shas, patch_info):
    """ Builds a partial patch by comparing the files in from_dir_path to thoes of to_dir_path"""
    # Cannocolize the paths for safey
    from_dir_path = os.path.abspath(from_dir_path)
    to_dir_path = os.path.abspath(to_dir_path)
    # First create a hashtable of the from  and to directories
    from_dir_hash,from_dir_set = patch_info.build_marfile_entry_hash(from_dir_path)
    to_dir_hash,to_dir_set = patch_info.build_marfile_entry_hash(to_dir_path)
    
    # Files which exist in both sets need to be patched
    patch_filenames = list(from_dir_set.intersection(to_dir_set))
    patch_filenames.sort()
    for filename in patch_filenames:
        from_marfile_entry = from_dir_hash[filename]
        to_marfile_entry = to_dir_hash[filename]
        if from_marfile_entry.sha() != to_marfile_entry.sha():
            # Not the same - calculate a patch
            create_partial_patch_for_file(from_marfile_entry, to_marfile_entry, shas, patch_info)

    # files in from_dir not in to_dir need to be removed
    remove_filenames = list(from_dir_set - to_dir_set)
    remove_filenames.sort()
    for filename in remove_filenames:
        patch_info.append_remove_instruction(from_dir_hash[filename].name)

    # files in to_dir not in from_dir need to added
    add_filenames = list(to_dir_set - from_dir_set)
    add_filenames.sort()
    for filename in add_filenames:
        create_add_patch_for_file(to_dir_hash[filename], patch_info)

    process_explicit_remove_files(to_dir_path, patch_info)
    
    # Construct Manifest file
    patch_info.create_manifest_file()
    
    # And construct the mar
    mar_cmd = 'mar -C '+patch_info.work_dir+' -c output.mar '+string.join(patch_info.archive_files, ' ')    
    exec_shell_cmd(mar_cmd)

    # Copy mar to final destination
    patch_file_dir = os.path.split(patch_filename)[0]
    if not os.path.exists(patch_file_dir):
        os.makedirs(patch_file_dir)
    shutil.copy2(os.path.join(patch_info.work_dir,"output.mar"), patch_filename)
    
def usage():
    print "-h for help"
    print "-f for patchlist_file"

def create_partial_patches(patch_list_file):
    """ Given the patch_list_file generates a set of partial patches"""
    patchlist_file = os.path.abspath(patch_list_file)
    shas = {}
    patches = []
    f = open(patch_list_file, 'r')
    for line in f.readlines():
        patches.append(line)
    f.close()

    work_dir_root = None
    try:
        work_dir_root = tempfile.mkdtemp()
        print "Building patches using work dir: %s" % (work_dir_root)
 
        # Iterate through every patch set in the patch file
        patch_num = 1
        for patch in patches:
            startTime = time.time()

            from_filename,to_filename,patch_filename,forced_updates = patch.split(",")
            from_filename,to_filename,patch_filename = os.path.abspath(from_filename),os.path.abspath(to_filename),os.path.abspath(patch_filename)

            # Each patch iteration uses its own work dir
            work_dir = os.path.join(work_dir_root,str(patch_num))
            os.mkdir(work_dir)

            # Extract from mar into from dir
            work_dir_from =  os.path.join(work_dir,"from");
            os.mkdir(work_dir_from)
            extract_mar(from_filename,work_dir_from)
            
            # Extrat to mar into to dir
            work_dir_to =  os.path.join(work_dir,"to")
            os.mkdir(work_dir_to)
            extract_mar(to_filename, work_dir_to)

            mar_extract_time = time.time()

            create_partial_patch(work_dir_from, work_dir_to, patch_filename, shas, PatchInfo(work_dir, ['channel-prefs.js','update.manifest'],['/readme.txt']))
            print "done with patch %s/%s time (%.2fs/%.2fs/%.2fs) (mar/patch/total)" % (str(patch_num),str(len(patches)),mar_extract_time-startTime,time.time()-mar_extract_time,time.time()-startTime)
            patch_num += 1
    finally:
        # If we fail or get a ctrl-c during run be sure to clean up temp dir
        if (work_dir_root and os.path.exists(work_dir_root)):
            shutil.rmtree(work_dir_root)        

def main(argv):                          
    patchlist_file = None
    try:                                
         opts, args = getopt.getopt(argv, "hf:", ["help", "patchlist_file="])
         for opt, arg in opts:                
            if opt in ("-h", "--help"):      
                usage()                     
                sys.exit()       
            elif opt in ("-f", "--patchlist_file"):
                patchlist_file = arg               
    except getopt.GetoptError:          
          usage()                         
          sys.exit(2)                     
    
    if not patchlist_file:
        usage()
        sys.exit(2)
        
    create_partial_patches(patchlist_file)

if __name__ == "__main__":
    main(sys.argv[1:])

