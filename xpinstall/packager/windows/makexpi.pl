#!c:\perl\bin\perl
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

if($inComponentName =~ /xpcom/i)
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
if(system("zip -r $inDestPath/$inComponentName.xpi *"))
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

sub GetTopSrcDir
{
  my($rootDir) = dirname($0) . "/$DEPTH";
  my($savedCwdDir) = cwd();

  chdir($rootDir);
  $rootDir = cwd();
  chdir($savedCwdDir);
  return($rootDir);
}

