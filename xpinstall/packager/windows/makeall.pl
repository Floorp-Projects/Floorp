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

$inDefaultVersion     = $ARGV[0];
$inURLPath            = $ARGV[1];
$inStagePath          = $ARGV[2];
$inDistPath           = $ARGV[3];
$seiFileNameGeneric   = "nsinstall.exe";
$seiFileNameSpecific  = "mozilla-win32-installer.exe";

# Check for existance of staging path
if(!(-e "$inStagePath"))
{
  die "invalid path: $inStagePath\n";
}

# Make sure inDestPath exists
if(!(-e "$inDistPath"))
{
  system("mkdir $inDestPath");
}

# Make .js files
MakeJsFile("core");
MakeJsFile("mail");

# Make .xpi files
MakeXpiFile("core");
MakeXpiFile("mail");

MakeConfigFile();

if(-e "$inDistPath\\setup")
{
  unlink <$inDistPath\\setup\\*>;
}
else
{
  system("mkdir $inDistPath\\setup");
}

# Copy the setup files to the dist setup directory.
system("xcopy /f config.ini                 $inDistPath");
system("xcopy /f config.ini                 $inDistPath\\setup");
system("xcopy /f $inDistPath\\setup.exe     $inDistPath\\setup");
system("xcopy /f $inDistPath\\setuprsc.dll  $inDistPath\\setup");

# build the self-extracting .exe file.
print "\nbuilding self-extracting installer ($seiFileNameSpecific)...\n";
system("copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific");
system("$inDistPath\\nszip.exe $inDistPath\\$seiFileNameSpecific $inDistPath\\setup\\*.* $inDistPath\\xpi\\*.*");

print " done!\n";

# end of script
exit(0);

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inStagePath $inDistPath\\xpi $inURLPath") != 0)
  {
    exit(1);
  }
}

sub MakeJsFile
{
  my($componentName) = @_;

  # Make .js file
  if(system("perl makejs.pl $componentName.jst $inDefaultVersion $inStagePath\\$componentName") != 0)
  {
    exit(1);
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

