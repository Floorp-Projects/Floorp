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
# This perl script builds the xpi, config.ini, and js files.
#

# Make sure there are at least four arguments
if($#ARGV < 3)
{
  die "usage: $0 <default version> <URL path> <staging path> <dist install path>

       default version   : julian based date version
                           ie: 5.0.0.99257

       URL path          : URL path to where the .xpi files will be staged at.
                           Either ftp:// or http:// can be used.  Nothing will be
                           copied to there by this script.  It is used for config.ini.

       staging path      : full path to where the components are staged at

       dist install path : full path to where the mozilla/dist/win32_o.obj/install is at.
       \n";
}

$inDefaultVersion = $ARGV[0];
$inURLPath        = $ARGV[1];
$inStagePath      = $ARGV[2];
$inDistPath       = $ARGV[3];

# Check for existance of staging path
if(!(-e "$inStagePath"))
{
  die "invalid path: $inStagePath\n";
}

# Make sure inDestPath exists
if(!(-e "$inDistPath"))
{
  system("mkdir /s $inDestPath");
}

MakeConfigFile();
MakeJsFile();

# Make all xpi files
MakeXpiFile("core");
MakeXpiFile("mail");
MakeXpiFile("editor");

# The jre122.exe is as is.  There is no need to create a .xpi file for it.
# It just simply needs to be copied to $inDistPath\xpi.
system("xcopy /f $inStagePath\\oji $inDistPath\\xpi");

if(-e "$inDistPath\\setup")
{
  unlink <$inDistPath\\setup\\*>;
}
else
{
  system("mkdir /s $inDistPath\\setup");
}

# Copy the setup files to the dist setup directory.
system("xcopy /f config.ini                 $inDistPath");
system("xcopy /f config.ini                 $inDistPath\\setup");
system("xcopy /f $inDistPath\\setup.exe     $inDistPath\\setup");
system("xcopy /f $inDistPath\\setuprsc.dll  $inDistPath\\setup");

# build the self-extracting .exe file.
printf "/nbuilding self-extracting .exe installer...";
system("$inDistPath\\nszip.exe $inDistPath\\nsinstall.exe $inDistPath\\setup\\*.* $inDistPath\\xpi\\*.*");

print " done!\n";

# end of script
exit(0);

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inStagePath $inURLPath") != 0)
  {
    exit(1);
  }
}

sub MakeJsFile
{
  # Make .js files
  @jstFiles = <*.jst>;
  foreach $jst (@jstFiles)
  {
    if(system("perl makejs.pl $jst $inDefaultVersion") != 0)
    {
      exit(1);
    }
  }
}

sub MakeXpiFile
{
  my($componentName) = @_;

  # Make .xpi file
  if(system("perl makexpi.pl $componentName $inStagePath $inDistPath\\xpi") != 0)
  {
    exit(1);
  }
}

