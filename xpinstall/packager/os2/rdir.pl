#!c:\perl\bin\perl
# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
# Sean Su <ssu@netscape.com>
# 

use Cwd;

if($#ARGV < 0)
{
    print_usage();
    exit(1);
}

print "removing directory:\n";
for($i = 0; $i <= $#ARGV; $i++)
{
    print "  $ARGV[$i]";
    remove_dir_structure($ARGV[$i]);
    print "\n";
}

exit(0);
# end

sub remove_dir_structure
{
    my($curr_dir) = @_;
    $save_cwd     = cwd();
    $save_cwd     =~ s/\//\\/g;
    if((-e "$curr_dir") && (-d "$curr_dir"))
    {
        remove_all_dir($curr_dir);
        chdir($save_cwd);
        remove_directory($curr_dir);
        print " done!";
    }
    else
    {
        if(!(-e "$curr_dir"))
        {
            print "\n";
            print "$curr_dir does not exist!";
        }
        elsif(!(-d "$curr_dir"))
        {
            print "\n";
            print "$curr_dir is not a valid directory!";
        }
    }
}

sub remove_all_dir
{
    my($curr_dir) = @_;
    my(@dirlist);
    my($dir);

    chdir("$curr_dir");
    @dirlist = <*>;
    foreach $dir (@dirlist)
    {
        if(-d "$dir")
        {
            print ".";
            remove_all_dir($dir);
        }
    }
    chdir("..");
    remove_directory($curr_dir);
}

sub remove_directory
{
    my($directory) = @_;
    my($save_cwd);

    $save_cwd = cwd();
    $save_cwd =~ s/\//\\/g;

    if(-e "$directory")
    {
        chdir($directory);
#        @files = <*>;
        chmod 0777, <*>;
        unlink <*>; # remove files
        chdir($save_cwd);
        rmdir $directory;         # remove directory
    }
}

sub print_usage
{
    print "usage: $0 <dir1> [dir2 dir3...]\n";
}
