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

# Make sure MOZ_SRC is set.
if($ENV{MOZ_SRC} eq "")
{
  print "Error: MOZ_SRC not set!";
  exit(1);
}

# Make sure there are at least three arguments
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

require "$ENV{MOZ_SRC}\\mozilla\\config\\zipcfunc.pl";

$inDefaultVersion     = $ARGV[0];
$inStagePath          = $ARGV[1];
$inDistPath           = $ARGV[2];

$inRedirIniUrl        = "ftp://not.needed.com/because/the/xpi/files/will/be/located/in/the/same/dir/as/the/installer";
$inXpiUrl             = "ftp://not.needed.com/because/the/xpi/files/will/be/located/in/the/same/dir/as/the/installer";

$seiFileNameGeneric   = "nsinstall.exe";
$seiFileNameSpecific  = "mozilla-win32-installer.exe";
$seuFileNameSpecific  = "MozillaUninstall.exe";

# set environment vars for use by other .pl scripts called from this script.
$ENV{WIZ_userAgent}            = "5.0a1 (en)";
$ENV{WIZ_nameCompany}          = "Mozilla";
$ENV{WIZ_nameProduct}          = "Mozilla Seamonkey";
$ENV{WIZ_fileMainExe}          = "Mozilla.exe";
$ENV{WIZ_fileUninstall}        = $seuFileNameSpecific;

# Set the location of the local tmp stage directory
$gLocalTmpStage = "$inDistPath\\tmpstage";

# Check for existance of staging path
if(!(-d "$inStagePath"))
{
  die "\n Invalid path: $inStagePath\n";
}

# List of components for to create xpi files from
@gComponentList = ("xpcom",
                   "browser",
                   "mail",
                   "chatzilla",
                   "deflenus",
                   "langenus");

if(VerifyComponents()) # return value of 0 means no errors encountered
{
  exit(1);
}

# Make sure inDistPath exists
if(!(-d "$inDistPath"))
{
  mkdir ("$inDistPath",0775);
}

if(-d "$inDistPath\\xpi")
{
  unlink <$inDistPath\\xpi\\*>;
}
else
{
  mkdir ("$inDistPath\\xpi",0775);
}

if(-d "$inDistPath\\uninstall")
{
  unlink <$inDistPath\\uninstall\\*>;
}
else
{
  mkdir ("$inDistPath\\uninstall",0775);
}

if(-d "$inDistPath\\setup")
{
  unlink <$inDistPath\\setup\\*>;
}
else
{
  mkdir ("$inDistPath\\setup",0775);
}

# Create the local tmp stage area
if(CreateTmpStage())
{
  exit(1);
}
if(MakeXpiFile())
{
  exit(1);
}
if(MakeUninstall())
{
  exit(1);
}
if(MakeConfigFile())
{
  exit(1);
}

# Remove the local tmp stage area
RemoveLocalTmpStage();

# Copy the setup files to the dist setup directory.
if(system("copy config.ini $inDistPath"))
{
  die "\n Error: copy config.ini $inDistPath\n";
}
if(system("copy config.ini $inDistPath\\setup"))
{
  die "\n Error: copy config.ini $inDistPath\\setup\n";
}
if(system("copy $inDistPath\\setup.exe $inDistPath\\setup"))
{
  die "\n Error: copy $inDistPath\\setup.exe $inDistPath\\setup\n";
}
if(system("copy $inDistPath\\setuprsc.dll $inDistPath\\setup"))
{
  die "\n Error: copy $inDistPath\\setuprsc.dll $inDistPath\\setup\n";
}

# build the self-extracting .exe (installer) file.
print "\n Building self-extracting installer ($seiFileNameSpecific)...\n";
if(system("copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific\n";
}
if(system("$inDistPath\\nsztool.exe $inDistPath\\$seiFileNameSpecific $inDistPath\\setup\\*.* $inDistPath\\xpi\\*.*"))
{
  die "\n Error: $inDistPath\\nsztool.exe $inDistPath\\$seiFileNameSpecific $inDistPath\\setup\\*.* $inDistPath\\xpi\\*.*\n";
}

print " done!\n\n";

if((!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcrt.dll")) ||
   (!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcirt.dll")))
{
  print "***\n";
  print "**\n";
  print "**  The following required Microsoft redistributable system files were not found\n";
  print "**  in $ENV{MOZ_SRC}\\redist\\microsoft\\system:\n";
  print "**\n";
  if(!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcrt.dll"))
  {
    print "**    msvcrt.dll\n";
  }
  if(!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcirt.dll"))
  {
    print "**    msvcirt.dll\n";
  }
  print "**\n";
  print "**  The above files are required by the installer and the browser.  If you attempt\n";
  print "**  to run the installer, you may encounter the following bug:\n";
  print "**\n";
  print "**    http://bugzilla.mozilla.org/show_bug.cgi?id=27601\n";
  print "**\n";
  print "***\n\n";
}

# end of script
exit(0);

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniUrl $inXpiUrl"))
  {
    print "\n Error: perl makecfgini.pl config.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniUrl $inXpiUrl\n";
    return(1);
  }
  return(0);
}

sub MakeUninstall
{
  if(MakeUninstallIniFile())
  {
    return(1);
  }

  # Copy the uninstall files to the dist uninstall directory.
  if(system("copy uninstall.ini $inDistPath"))
  {
    print "\n Error: copy uninstall.ini $inDistPath\n";
    return(1);
  }
  if(system("copy uninstall.ini $inDistPath\\uninstall"))
  {
    print "\n Error: copy uninstall.ini $inDistPath\\uninstall\n";
    return(1);
  }
  if(system("copy $inDistPath\\uninstall.exe $inDistPath\\uninstall"))
  {
    print "\n Error: copy $inDistPath\\uninstall.exe $inDistPath\\uninstall\n";
    return(1);
  }

  # build the self-extracting .exe (uninstaller) file.
  print "\nbuilding self-extracting uninstaller ($seuFileNameSpecific)...\n";
  if(system("copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seuFileNameSpecific"))
  {
    print "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seuFileNameSpecific\n";
    return(1);
  }
  if(system("$inDistPath\\nsztool.exe $inDistPath\\$seuFileNameSpecific $inDistPath\\uninstall\\*.*"))
  {
    print "\n Error: $inDistPath\\nsztool.exe $inDistPath\\$seuFileNameSpecific $inDistPath\\uninstall\\*.*\n";
    return(1);
  }
  if(system("copy $inDistPath\\$seuFileNameSpecific $inDistPath\\xpi"))
  {
    print "\n Error: copy $inDistPath\\$seuFileNameSpecific $inDistPath\\xpi\n";
    return(1);
  }
  return(0);
}

sub MakeUninstallIniFile
{
  # Make config.ini file
  if(system("perl makeuninstallini.pl uninstall.it $inDefaultVersion"))
  {
    print "\n Error: perl makeuninstallini.pl uninstall.it $inDefaultVersion\n";
    return(1);
  }
  return(0);
}

sub MakeJsFile
{
  my($mComponent) = @_;

  # Make .js file
  if(system("perl makejs.pl $mComponent.jst $inDefaultVersion $gLocalTmpStage\\$mComponent"))
  {
    print "\n Error: perl makejs.pl $mComponent.jst $inDefaultVersion $gLocalTmpStage\\$mComponent\n";
    return(1);
  }
  return(0);
}

sub MakeXpiFile
{
  my($mComponent);

  foreach $mComponent (@gComponentList)
  {
    # Make .js files
    if(MakeJsFile($mComponent))
    {
      return(1);
    }

    # Make .xpi file
    if(system("perl makexpi.pl $mComponent $gLocalTmpStage $inDistPath\\xpi"))
    {
      print "\n Error: perl makexpi.pl $mComponent $gLocalTmpStage $inDistPath\\xpi\n";
      return(1);
    }
  }
  return(0);
}

sub RemoveLocalTmpStage()
{
  # Remove tmpstage area
  if(-d "$gLocalTmpStage")
  {
    system("perl rdir.pl $gLocalTmpStage");
  }
  return(0);
}

sub CreateTmpStage()
{
  my($mComponent);

  # Remove previous tmpstage area if one was left around
  if(-d "$gLocalTmpStage")
  {
    system("perl rdir.pl $gLocalTmpStage");
  }

  print "\n Creating the local TmpStage directory:\n";
  print "   $gLocalTmpStage\n";

  # Copy the component's staging dir locally so that the chrome packages, locales, and skins dirs can be
  # removed prior to creating the .xpi file.
  mkdir("$gLocalTmpStage", 775);

  foreach $mComponent (@gComponentList)
  {
    print "\n Copying $mComponent:\n";
    print " From: $inStagePath\\$mComponent\n";
    print "   To: $gLocalTmpStage\\$mComponent\n\n";
    mkdir("$gLocalTmpStage\\$mComponent", 775);
    if(system("xcopy /s/e $inStagePath\\$mComponent $gLocalTmpStage\\$mComponent\\"))
    {
      print "\n Error: xcopy /s/e $inStagePath\\$mComponent $gLocalTmpStage\\$mComponent\\\n";
      return(1);
    }

    if(-d "$gLocalTmpStage\\$mComponent\\bin\\chrome")
    {
      # Make chrome archive files
      if(&ZipChrome("win32", "noupdate", "$gLocalTmpStage\\$mComponent\\bin\\chrome", "$gLocalTmpStage\\$mComponent\\bin\\chrome"))
      {
        return(1);
      }

      # Remove the locales, packages, and skins dirs if they exist.
      my @dirs = <$gLocalTmpStage\\$mComponent\\bin\\chrome\\*>;
      foreach $d (@dirs) {
          if(-d "$d")
          {
              system("perl rdir.pl $d");
          }
      }
    }
  }
  return(0);
}

sub VerifyComponents()
{
  my($mComponent);
  my($mError) = 0;

  print "\n Verifying existance of required components...\n";
  foreach $mComponent (@gComponentList)
  {
    if(-d "$inStagePath\\$mComponent")
    {
      print "    ok: $inStagePath\\$mComponent\n";
    }
    else
    {
      print " Error: $inStagePath\\$mComponent does not exist!\n";
      $mError = 1;
    }
  }
  return($mError);
}

