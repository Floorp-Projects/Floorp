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

use File::Copy;
use File::Basename;
use Cwd;

# Make sure MOZ_SRC is set.
if($ENV{MOZ_SRC} eq "")
{
  print "Error: MOZ_SRC not set!";
  exit(1);
}

# Make sure there are at least three arguments
if($#ARGV < 2)
{
  PrintUsage();
}

$inDefaultVersion     = $ARGV[0];
# $ARGV[0] has the form maj.min.release.bld where maj, min, release
#   and bld are numerics representing version information.
# Other variables need to use parts of the version info also so we'll
#   split out the dot seperated values into the array @versionParts
#   such that:
#
#   $versionParts[0] = maj
#   $versionParts[1] = min
#   $versionParts[2] = release
#   $versionParts[3] = bld
@versionParts = split /\./, $inDefaultVersion;

# We allow non-numeric characters to be included as the last 
#   characters in fields of $ARG[0] for display purposes (mostly to
#   show that we have moved past a certain version by adding a '+'
#   character).  Non-numerics must be stripped out of $inDefaultVersion,
#   however, since this variable is used to identify the the product 
#   for comparison with other installations, so the values in each field 
#   must be numeric only:
$inDefaultVersion =~ s/[^0-9.][^.]*//g;
print "The raw version id is:  $inDefaultVersion\n";

$inStagePath          = $ARGV[1];
$inDistPath           = $ARGV[2];

$inXpiURL = "";
$inRedirIniURL = "";

ParseArgv(@ARGV);
if($inXpiURL eq "")
{
  # archive url not supplied, set it to default values
  $inXpiURL      = "ftp://not.supplied.com";
}
if($inRedirIniURL eq "")
{
  # redirect url not supplied, set it to default value.
  $inRedirIniURL = $inXpiURL;
}

$seiFileNameGeneric   = "stubinstall.exe";
$seiFileNameGenericRes   = "stubinstall.res";
$seiFileNameSpecific  = "mozilla-os2-installer.exe";
$seiFileNameSpecificRes  = "mozilla-os2-installer.res";
$seiFileNameSpecificRC  = "mozilla-os2-installer.rc";
$seiStubRootName = "mozilla-os2-stub-installer";
$seiFileNameSpecificStub  = "$seiStubRootName.exe";
$seiFileNameSpecificStubRes  = "$seiStubRootName.res";
$seiFileNameSpecificStubRC  = "$seiStubRootName.rc";
$seuFileNameSpecific  = "MozillaUninstall.exe";
$seuFileNameSpecificRes  = "MozillaUninstall.res";
$seuFileNameSpecificRC  = "MozillaUninstall.rc";
$seuzFileNameSpecific = "mozillauninstall.zip";

# set environment vars for use by other .pl scripts called from this script.
if($versionParts[2] eq "0")
{
   $versionMain = "$versionParts[0]\.$versionParts[1]";
}
else
{
   $versionMain = "$versionParts[0]\.$versionParts[1]\.$versionParts[2]";
}
print "The display version is: $versionMain\n";
$versionLanguage               = "en";
$ENV{WIZ_nameCompany}          = "mozilla.org";
$ENV{WIZ_nameProduct}          = "Mozilla";
$ENV{WIZ_nameProductNoVersion} = "Mozilla";
$ENV{WIZ_fileMainExe}          = "Mozilla.exe";
$ENV{WIZ_fileMainIco}          = "Mozilla.ico";
$ENV{WIZ_fileUninstall}        = $seuFileNameSpecific;
$ENV{WIZ_fileUninstallZip}     = $seuzFileNameSpecific;
# The following variables are for displaying version info in the 
# the installer.
$ENV{WIZ_userAgent}            = "$versionMain ($versionLanguage)";
$ENV{WIZ_userAgentShort}       = "$versionMain";
$ENV{WIZ_xpinstallVersion}     = "$versionMain";

# Set the location of the local tmp stage directory
$gLocalTmpStage = $inStagePath;

# Check for existence of staging path
if(!(-d "$inStagePath"))
{
  die "\n Invalid path: $inStagePath\n";
}

# List of components for to create xpi files from
@gComponentList = ("xpcom",
                   "browser",
                   "mail",
                   "psm",
                   "chatzilla",
                   "deflenus",
                   "langenus",
                   "regus",
                   "venkman",
                   "inspector");

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
  unlink <$inDistPath/xpi/*>;
}
else
{
  mkdir ("$inDistPath\\xpi",0775);
}

if(-d "$inDistPath\\uninstall")
{
  unlink <$inDistPath/uninstall/*>;
}
else
{
  mkdir ("$inDistPath\\uninstall",0775);
}

if(-d "$inDistPath\\setup")
{
  unlink <$inDistPath/setup/*>;
}
else
{
  mkdir ("$inDistPath\\setup",0775);
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

# Copy the setup files to the dist setup directory.
if(system("cp install.ini $inDistPath"))
{
  die "\n Error: copy install.ini $inDistPath\n";
}
if(system("cp install.ini $inDistPath\\setup"))
{
  die "\n Error: copy install.ini $inDistPath\\setup\n";
}
if(system("cp config.ini $inDistPath"))
{
  die "\n Error: copy config.ini $inDistPath\n";
}
if(system("cp config.ini $inDistPath\\setup"))
{
  die "\n Error: copy config.ini $inDistPath\\setup\n";
}
if(system("cp $inDistPath\\setup.exe $inDistPath\\setup"))
{
  die "\n Error: copy $inDistPath\\setup.exe $inDistPath\\setup\n";
}
if(system("cp $inDistPath\\setuprsc.dll $inDistPath\\setup"))
{
  die "\n Error: copy $inDistPath\\setuprsc.dll $inDistPath\\setup\n";
}

# copy license file for the installer
if(system("cp $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\license.txt"))
{
  die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\license.txt\n";
}
if(system("cp $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\setup\\license.txt"))
{
  die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\setup\\license.txt\n";
}

 copy readme for the installer
if(system("cp $ENV{MOZ_SRC}\\mozilla\\README.TXT $inDistPath\\readme.txt"))
{
  die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\README.TXT $inDistPath\\readme.txt\n";
}
if(system("cp $ENV{MOZ_SRC}\\mozilla\\README.TXT $inDistPath\\setup\\readme.txt"))
{
  die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\README.TXT $inDistPath\\setup\\readme.txt\n";
}

# copy the icons
#if(system("cp $inDistPath\\mozilla.ico $inDistPath\\setup\\mozilla.ico"))
#{
#  die "\n Error: copy $inDistPath\\mozilla.ico $inDistPath\\setup\\mozilla.ico\n";
#}

# build the self-extracting .exe (installer) file.
print "\nbuilding self-extracting stub installer ($seiFileNameSpecificStub)...\n";
if(system("cp $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecificStub"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecificStub\n";
}

if(system("cp $inDistPath\\$seiFileNameGenericRes $inDistPath\\$seiFileNameSpecificStubRes"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameGenericRes $inDistPath\\$seiFileNameSpecificStubRes\n";
}

@stubFiles = <$inDistPath/setup/*.*>;

$size = (-s "$inDistPath\\$seiFileNameSpecificStubRes");
truncate("$inDistPath\\$seiFileNameSpecificStubRes", "$size-1");
open(OUTPUTFILE, ">$inDistPath\\$seiFileNameSpecificStubRC");
print OUTPUTFILE "#include <os2.h>\n";
print OUTPUTFILE "STRINGTABLE DISCARDABLE\n";
print OUTPUTFILE "BEGIN\n";
$currentResourceID = 10000+1;
foreach $entry ( @stubFiles ) 
{
  $filename = basename($entry);
  print OUTPUTFILE "$currentResourceID \"$filename\"\n";
  $currentResourceID++;
}
print OUTPUTFILE "END\n";
$currentResourceID = 10000+1;
foreach $entry ( @stubFiles ) 
{
  print OUTPUTFILE "RESOURCE RT_RCDATA $currentResourceID \"$entry\"\n";
  $currentResourceID++;
}
close(OUTPUTFILE);
system("rc -r $inDistPath\\$seiFileNameSpecificStubRC $inDistPath\\temp.res");
system("cat $inDistPath\\$seiFileNameSpecificStubRes $inDistPath\\temp.res > $inDistPath\\new.res");
unlink("$inDistPath\\$seiFileNameSpecificStubRes");
rename("$inDistPath\\new.res", "$inDistPath\\$seiFileNameSpecificStubRes");
unlink("$inDistPath\\temp.res");
system("rc $inDistPath\\$seiFileNameSpecificStubRes $inDistPath\\$seiFileNameSpecificStub");

# copy the lean installer to stub\ dir
print "\n****************************\n";
print "*                          *\n";
print "*  creating Stub files...  *\n";
print "*                          *\n";
print "****************************\n";
if(-d "$inDistPath\\stub")
{
  unlink <$inDistPath/stub/*>;
}
else
{
  mkdir ("$inDistPath/stub",0775);
}
if(system("cp $inDistPath\\$seiFileNameSpecificStub $inDistPath\\stub"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameSpecificStub $inDistPath\\stub\n";
}

# create the xpi for launching the stub installer
print "\n**********************************\n";
print "*                                  *\n";
print "*  creating stub installer xpi...  *\n";
print "*                                  *\n";
print "************************************\n";
if(-d "$inStagePath\\$seiStubRootName")
{
  unlink <$inStagePath\\$seiStubRootName\\*>;
}
else
{
  mkdir ("$inStagePath\\$seiStubRootName",0775);
}
if(system("cp $inDistPath\\stub\\$seiFileNameSpecificStub $gLocalTmpStage\\$seiStubRootName"))
{
  die "\n Error: copy $inDistPath\\stub\\$seiFileNameSpecificStub $gLocalTmpStage\\$seiStubRootName\n";
}

# Make .js files
if(MakeJsFile($seiStubRootName))
{
  return(1);
}

# Make .xpi file
if(system("perl makexpi.pl $seiStubRootName $gLocalTmpStage $inDistPath"))
{
  print "\n Error: perl makexpi.pl $seiStubRootName $gLocalTmpStage $inDistPath\n";
  return(1);
}

# group files for CD
print "\n************************************\n";
print "*                                  *\n";
print "*  creating Compact Disk files...  *\n";
print "*                                  *\n";
print "************************************\n";
if(-d "$inDistPath\\cd")
{
  unlink <$inDistPath\\cd\\*>;
}
else
{
  mkdir ("$inDistPath\\cd",0775);
}
if(system("cp $inDistPath\\$seiFileNameSpecificStub  $inDistPath\\cd"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameSpecificStub $inDistPath\\cd\n";
}
if(system("cp $inDistPath/xpi/* $inDistPath/cd"))
{
  die "\n Error: copy $inDistPath\\xpi $inDistPath\\cd\n";
}

# create the big self extracting .exe installer
print "\n**************************************************************\n";
print "*                                                            *\n";
print "*  creating Self Extracting Executable Full Install file...  *\n";
print "*                                                            *\n";
print "**************************************************************\n";
if(-d "$inDistPath\\sea")
{
  unlink <$inDistPath\\sea\\*>;
}
else
{
  mkdir ("$inDistPath\\sea",0775);
}
if(system("cp $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific\n";
}

if(system("cp $inDistPath\\$seiFileNameGenericRes $inDistPath\\$seiFileNameSpecificRes"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameGenericRes $inDistPath\\$seiFileNameSpecificRes\n";
}

@stubFiles = <$inDistPath/setup/*.*>;
@xpiFiles = <$inDistPath/xpi/*.*>;

$size = (-s "$inDistPath\\$seiFileNameSpecificRes");
truncate("$inDistPath\\$seiFileNameSpecificRes", "$size-1");
open(OUTPUTFILE, ">$inDistPath\\$seiFileNameSpecificRC");
print OUTPUTFILE "#include <os2.h>\n";
print OUTPUTFILE "STRINGTABLE DISCARDABLE\n";
print OUTPUTFILE "BEGIN\n";
$currentResourceID = 10000+1;
foreach $entry ( @stubFiles ) 
{
  $filename = basename($entry);
  print OUTPUTFILE "$currentResourceID \"$filename\"\n";
  $currentResourceID++;
}
foreach $entry ( @xpiFiles ) 
{
  $filename = basename($entry);
  print OUTPUTFILE "$currentResourceID \"$filename\"\n";
  $currentResourceID++;
}
print OUTPUTFILE "END\n";
$currentResourceID = 10000+1;
foreach $entry ( @stubFiles ) 
{
  print OUTPUTFILE "RESOURCE RT_RCDATA $currentResourceID \"$entry\"\n";
  $currentResourceID++;
}
foreach $entry ( @xpiFiles ) 
{
  print OUTPUTFILE "RESOURCE RT_RCDATA $currentResourceID \"$entry\"\n";
  $currentResourceID++;
}
close(OUTPUTFILE);
system("rc -r $inDistPath\\$seiFileNameSpecificRC $inDistPath\\temp.res");
system("cat $inDistPath\\$seiFileNameSpecificRes $inDistPath\\temp.res > $inDistPath\\new.res");
unlink("$inDistPath\\$seiFileNameSpecificRes");
rename("$inDistPath\\new.res", "$inDistPath\\$seiFileNameSpecificRes");
unlink("$inDistPath\\temp.res");
system("rc $inDistPath\\$seiFileNameSpecificRes $inDistPath\\$seiFileNameSpecific");

if(system("cp $inDistPath\\$seiFileNameSpecific $inDistPath\\sea"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameSpecific $inDistPath\\sea\n";
}
#unlink <$inDistPath\\$seiFileNameSpecificStub>;

print " done!\n\n";

# end of script
exit(0);

sub MakeExeZip
{
  my($aSrcDir, $aExeFile, $aZipFile) = @_;
  my($saveCwdir);

  $saveCwdir = cwd();
  chdir($aSrcDir);
  if(system("zip $inDistPath\\xpi\\$aZipFile $aExeFile"))
  {
    chdir($saveCwdir);
    die "\n Error: zip $inDistPath\\xpi\\$aZipFile $aExeFile";
  }
  chdir($saveCwdir);
}

sub PrintUsage
{
  die "usage: $0 <default version> <staging path> <dist install path> [options]

       default version   : y2k compliant based date version.
                           ie: 5.0.0.2000040413

       staging path      : full path to where the components are staged at

       dist install path : full path to where the dist install dir is at.
                           ie: d:\\builds\\mozilla\\dist\\win32_o.obj\\install

       options include:
           -aurl <archive url>      : either ftp:// or http:// url to where the
                                      archives (.xpi, .exe, .zip, etc...) reside

           -rurl <redirect.ini url> : either ftp:// or http:// url to where the
                                      redirec.ini resides.  If not supplied, it
                                      will be assumed to be the same as archive
                                      url.
       \n";
}

sub ParseArgv
{
  my(@myArgv) = @_;
  my($counter);

  # The first 3 arguments are required, so start on the 4th.
  for($counter = 3; $counter <= $#myArgv; $counter++)
  {
    if($myArgv[$counter] =~ /^[-,\/]h$/i)
    {
      PrintUsage();
    }
    elsif($myArgv[$counter] =~ /^[-,\/]aurl$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inXpiURL = $myArgv[$counter];
        $inRedirIniURL = $inXpiURL;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]rurl$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inRedirIniURL = $myArgv[$counter];
      }
    }
  }
}

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl config.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL\n";
    return(1);
  }

  # Make install.ini file
  if(system("perl makecfgini.pl install.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl install.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL\n";
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
  if(system("cp uninstall.ini $inDistPath"))
  {
    print "\n Error: copy uninstall.ini $inDistPath\n";
    return(1);
  }
  if(system("cp uninstall.ini $inDistPath\\uninstall"))
  {
    print "\n Error: copy uninstall.ini $inDistPath\\uninstall\n";
    return(1);
  }
  if(system("cp $inDistPath\\uninstall.exe $inDistPath\\uninstall"))
  {
    print "\n Error: copy $inDistPath\\uninstall.exe $inDistPath\\uninstall\n";
    return(1);
  }

  # build the self-extracting .exe (uninstaller) file.
  print "\nbuilding self-extracting uninstaller ($seuFileNameSpecific)...\n";
  if(system("cp $inDistPath\\$seiFileNameGeneric $inDistPath\\$seuFileNameSpecific"))
  {
    print "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seuFileNameSpecific\n";
    return(1);
  }

  if(system("cp $inDistPath\\$seiFileNameGenericRes $inDistPath\\$seuFileNameSpecificRes"))
  {
    die "\n Error: copy $inDistPath\\$seiFileNameGenericRes $inDistPath\\$seuFileNameSpecificRes\n";
  }

  @stubFiles = <$inDistPath/uninstall/*.*>;

  $size = (-s "$inDistPath\\$seuFileNameSpecificRes");
  truncate("$inDistPath\\$seuFileNameSpecificRes", "$size-1");
  open(OUTPUTFILE, ">$inDistPath\\$seuFileNameSpecificRC");
  print OUTPUTFILE "#include <os2.h>\n";
  print OUTPUTFILE "STRINGTABLE DISCARDABLE\n";
  print OUTPUTFILE "BEGIN\n";
  $currentResourceID = 10000+1;
  foreach $entry ( @stubFiles ) 
  {
    $filename = basename($entry);
    print OUTPUTFILE "$currentResourceID \"$filename\"\n";
    $currentResourceID++;
  }
  print OUTPUTFILE "END\n";
  $currentResourceID = 10000+1;
  foreach $entry ( @stubFiles ) 
  {
    print OUTPUTFILE "RESOURCE RT_RCDATA $currentResourceID \"$entry\"\n";
    $currentResourceID++;
  }
  close(OUTPUTFILE);
  system("rc -r $inDistPath\\$seuFileNameSpecificRC $inDistPath\\temp.res");
  system("cat $inDistPath\\$seuFileNameSpecificRes $inDistPath\\temp.res > $inDistPath\\new.res");
  unlink("$inDistPath\\$seuFileNameSpecificRes");
  rename("$inDistPath\\new.res", "$inDistPath\\$seuFileNameSpecificRes");
  unlink("$inDistPath\\temp.res");
  system("rc $inDistPath\\$seuFileNameSpecificRes $inDistPath\\$seuFileNameSpecific");

  MakeExeZip($inDistPath, $seuFileNameSpecific, $seuzFileNameSpecific);
  unlink <$inDistPath\\$seuFileNameSpecific>;
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

    # If it's not talkback then copy the component over to the local tmp stage.
    # If it is, then skip the copy because there will be nothing at the source.
    # Talkback is a dummy place holder .xpi right now.  Mozilla release team 
    # replaces this place holder .xpi with a real talkback when delivering the
    # build to mozilla.org.
    if(!($mComponent =~ /talkback/i))
    {
      if(system("xcopy /s/e $inStagePath\\$mComponent $gLocalTmpStage\\$mComponent\\"))
      {
        print "\n Error: xcopy /s/e $inStagePath\\$mComponent $gLocalTmpStage\\$mComponent\\\n";
        return(1);
      }
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

  print "\n Verifying existence of required components...\n";
  foreach $mComponent (@gComponentList)
  {
    if($mComponent =~ /talkback/i)
    {
      print " place holder: $inStagePath\\$mComponent\n";
      mkdir("$inStagePath\\$mComponent", 775);
    }
    elsif(-d "$inStagePath\\$mComponent")
    {
      print "           ok: $inStagePath\\$mComponent\n";
    }
    else
    {
      print "        Error: $inStagePath\\$mComponent does not exist!\n";
      $mError = 1;
    }
  }
  return($mError);
}

