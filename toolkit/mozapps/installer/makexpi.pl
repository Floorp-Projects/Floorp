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

#
# This perl script creates .xpi files given component input name
#
# Input: component name
#             - name of the component directory located in the staging path
#        staging path
#             - path to where the built files are staged at
#        dest path
#             - path to where the .xpi files are are to be created at.
#               ** MUST BE AN ABSOLUTE PATH, NOT A RELATIVE PATH **
#
#   ie: perl makexpi.pl xpcom z:\exposed\windows\32bit\en\5.0 d:\build\mozilla\dist\win32_o.obj\install\working
#

use File::Copy;
use File::Basename;
use File::Find;
use Cwd;

# Make sure there are at least three arguments
if($#ARGV < 2)
{
  die "usage: $0 <component name> <staging path> <dest path>

       component name : name of component directory within staging path
       staging path   : path to where the components are staged at
       dest path      : path to where the .xpi files are to be created at
       \n";
}

$DEPTH = "../../..";
$topsrcdir        = GetTopSrcDir();
$inComponentName  = $ARGV[0];
$inStagePath      = $ARGV[1];
$inDestPath       = $ARGV[2];

# check for existance of staging component path
if(!(-e "$inStagePath/$inComponentName"))
{
  die "invalid path: $inStagePath/$inComponentName\n";
}

$win32 = ($^O =~ /((MS)?win32)|cygwin|os2/i) ? 1 : 0;

if($win32 && $inComponentName =~ /xpcom/i)
{
  # copy msvcrt.dll to xpcom dir
  if(-e "$topsrcdir/../redist/microsoft/system/msvcrt.dll")
  {
    copy("$topsrcdir/../redist/microsoft/system/msvcrt.dll", "$inStagePath/$inComponentName");
  }

  # copy msvcirt.dll to xpcom dir
  if(-e "$topsrcdir/../redist/microsoft/system/msvcirt.dll")
  {
    copy("$topsrcdir/../redist/microsoft/system/msvcirt.dll", "$inStagePath/$inComponentName");
  }
}

# check for existance of .js script
if(!(-e "$inComponentName.js"))
{
  die "missing .js script: $inComponentName.js\n";
}

# delete component .xpi file
if(-e "$inDestPath/$inComponentName.xpi")
{
  unlink("$inDestPath/$inComponentName.xpi");
}
if(-e "$inStagePath/$inComponentName/$inComponentName.xpi")
{
  unlink("$inDestPath/$inComponentName.xpi");
}

# delete install.js
if(-e "install.js")
{
  unlink("install.js");
}

# make sure inDestPath exists
if(!(-d "$inDestPath"))
{
  system("mkdir $inDestPath");
}

print "\n Making $inComponentName.xpi...\n";

$saveCwdir = cwd();

copy("$inComponentName.js", "$inStagePath/$inComponentName/install.js");

# change directory to where the files are, else zip will store
# unwanted path information.
chdir("$inStagePath/$inComponentName");

# strip libs on unix
if (!$win32) {
    print "stripping libs in $inStagePath/$inComponentName...\n";
    RecursiveStrip(cwd());
}

$compressionFlags = "";
if ($ENV{MOZ_INSTALLER_USE_7ZIP}) 
{
  $compressionFlags = "-0";
}

if(system("zip -r $compressionFlags $inDestPath/$inComponentName.xpi *"))
{
  chdir("$saveCwdir");
  die "\n Error: zip -r $inDestPath/$inComponentName.xpi *\n";
}

# delete install.js
if(-e "install.js")
{
  unlink("install.js");
}
chdir("$saveCwdir");

print "\n $inComponentName.xpi done!\n";

# end of script
exit(0);

# RecursiveStrip
#
# Strips all libs by recursing into all directories and calling
# the strip utility on all *.so files.
#
# @param   targetDir  the directory to traverse recursively
#
sub RecursiveStrip
{
    my($targetDir) = $_[0];
    my(@dirEntries) = ();
    my($entry) = "";
    my($saveCwd) = cwd();

    undef @libraryList;
    find({ wanted => \&find_libraries, no_chdir => 1 }, $targetDir);
    @dirEntries = <$targetDir/*>;

    # strip all .so files
    system("strip @libraryList") if (defined(@libraryList));
}

sub find_libraries
{
    /\.so$/ && push @libraryList, $File::Find::name;
    /-bin$/ && push @libraryList, $File::Find::name;
    /^xpicleanup$/ && push @libraryList, $File::Find::name;
}

sub GetTopSrcDir
{
  my($rootDir) = dirname($0) . "/$DEPTH";
  my($savedCwdDir) = cwd();

  chdir($rootDir);
  $rootDir = cwd();
  chdir($savedCwdDir);
  return($rootDir);
}

