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
if($#ARGV < 2)
{
  die "usage: $0 <default version> <staging path> <dist install path>

       default version   : y2k compliant based date version.
                           ie: 5.0.0.2000040413

       staging path      : full path to where the components are staged at

       dist install path : full path to where the dist install dir is at.
                           ie: d:\\builds\\mozilla\\dist\\win32_o.obj\\install
       \n";
}

$inDefaultVersion     = $ARGV[0];
$inStagePath          = $ARGV[1];
$inDistPath           = $ARGV[2];

$inRedirIniUrl        = "ftp://not.needed.com/because/the/xpi/files/will/be/located/in/the/same/dir/as/the/installer";
$inXpiUrl             = "ftp://not.needed.com/because/the/xpi/files/will/be/located/in/the/same/dir/as/the/installer";

$seiFileNameGeneric   = "nsinstall.exe";
$seiFileNameSpecific  = "mozilla-win32-installer.exe";
$userAgent            = "5.0b1 (en)";

# Check for existance of staging path
if(!(-e "$inStagePath"))
{
  die "invalid path: $inStagePath\n";
}

# Make sure inDestPath exists
if(!(-e "$inDistPath"))
{
  mkdir ("$inDestPath",0775);
}

# Make .js files
MakeJsFile("core");
MakeJsFile("browser");
MakeJsFile("mail");

# Make .xpi files
MakeXpiFile("core");
MakeXpiFile("browser");
MakeXpiFile("mail");

MakeConfigFile();

if(-e "$inDistPath\\setup")
{
  unlink <$inDistPath\\setup\\*>;
}
else
{
  mkdir ("$inDistPath\\setup",0775);
}

# Copy the setup files to the dist setup directory.
system("xcopy /f config.ini                 $inDistPath\\");
system("xcopy /f config.ini                 $inDistPath\\setup\\");
system("xcopy /f $inDistPath\\setup.exe     $inDistPath\\setup\\");
system("xcopy /f $inDistPath\\setuprsc.dll  $inDistPath\\setup\\");

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
  if(system("perl makecfgini.pl config.it $inDefaultVersion $inStagePath $inDistPath\\xpi $inRedirIniUrl $inXpiUrl") != 0)
  {
    exit(1);
  }
}

sub MakeJsFile
{
  my($componentName) = @_;

  # Make .js file
  if(system("perl makejs.pl $componentName.jst $inDefaultVersion \"$userAgent\" $inStagePath\\$componentName") != 0)
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

