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

use Cwd;
use File::Copy;
use File::Path;
use File::Basename;

# Make sure there are at least one argument
if($#ARGV < 0)
{
  PrintUsage();
}

$DEPTH = "../../..";
$topsrcdir = GetTopSrcDir();

require "$topsrcdir/config/zipcfunc.pl";

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
#   characters in fields of $ARG[1] for display purposes (mostly to
#   show that we have moved past a certain version by adding a '+'
#   character).  Non-numerics must be stripped out of $inDefaultVersion,
#   however, since this variable is used to identify the the product 
#   for comparison with other installations, so the values in each field 
#   must be numeric only:
$inDefaultVersion =~ s/[^0-9.][^.]*//g;
print "The raw version id is:  $inDefaultVersion\n";

$inStagePath          = "$topsrcdir/stage";
$inDistPath           = "$topsrcdir/dist";
$inXpiURL             = "ftp://not.supplied.invalid";
$inRedirIniURL        = $inXpiURL;

ParseArgv(@ARGV);

$gDirPackager         = "$topsrcdir/xpinstall/packager";
$gDirDistInstall      = "$inDistPath/inst_gre";
$gDirStageProduct     = "$inStagePath/gre";


# Create the stage area here.
# If -sd is not used, the default stage dir will be: $topsrcdir/stage
if(system("perl \"$gDirPackager/make_stage.pl\" -pn gre -os win -sd \"$inStagePath\""))
{
  die "\n Error: perl \"$gDirPackager/make_stage.pl\" -pn gre -os win -sd \"$inStagePath\"\n";
}

$seiFileNameGeneric       = "nsinstall.exe";
$seiFileNameSpecific      = "gre-win32-installer.exe";
$seiStubRootName          = "gre-win32-stub-installer";
$seiFileNameSpecificStub  = "$seiStubRootName.exe";
$seuFileNameSpecific      = "GREUninstall.exe";
$seuzFileNameSpecific     = "greuninstall.zip";

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

$ENV{WIZ_nameCompany}          = "mozilla.org";
$ENV{WIZ_nameProduct}          = "GRE";
$ENV{WIZ_nameProductInternal}  = "GRE"; # product name without the version string
$ENV{WIZ_fileMainExe}          = "none.exe";
$ENV{WIZ_fileUninstall}        = $seuFileNameSpecific;
$ENV{WIZ_fileUninstallZip}     = $seuzFileNameSpecific;
# The following variables are for displaying version info in the 
# the installer.
$ENV{WIZ_userAgent}            = "$versionMain";

# userAgentShort just means it does not have the language string.
#  ie: '1.3b' as opposed to '1.3b (en)'
$ENV{WIZ_userAgentShort}       = "$versionMain";
$ENV{WIZ_xpinstallVersion}     = "$versionMain";
$ENV{WIZ_distInstallPath}      = "$gDirDistInstall";

print "\n";
print "\n";
print " Building $ENV{WIZ_nameProduct} $ENV{WIZ_userAgent}...\n";
print "\n";

# Check for existence of staging path
if(!(-d "$gDirStageProduct"))
{
  die "\n Invalid path: $gDirStageProduct\n";
}

# List of components for to create xpi files from
@gComponentList = ("xpcom",
                   "gre");

if(VerifyComponents()) # return value of 0 means no errors encountered
{
  exit(1);
}

# Make sure gDirDistInstall exists
if(!(-d "$gDirDistInstall"))
{
  mkdir ("$gDirDistInstall",0775);
}

if(-d "$gDirDistInstall/xpi")
{
  unlink <$gDirDistInstall/xpi/*>;
}
else
{
  mkdir ("$gDirDistInstall/xpi",0775);
}

if(-d "$gDirDistInstall/uninstall")
{
  unlink <$gDirDistInstall/uninstall/*>;
}
else
{
  mkdir ("$gDirDistInstall/uninstall",0775);
}

if(-d "$gDirDistInstall/setup")
{
  unlink <$gDirDistInstall/setup/*>;
}
else
{
  mkdir ("$gDirDistInstall/setup",0775);
}

copy("$gDirDistInstall/../install/ds32.exe", "$gDirDistInstall") ||
  die "copy $gDirDistInstall/../install/ds32.exe $gDirDistInstall: $!\n";

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
copy("install.ini", "$gDirDistInstall") ||
  die "copy install.ini $gDirDistInstall: $!\n";

#Get setup.exe from mozilla install
copy("$gDirDistInstall/../install/setup.exe", "$gDirDistInstall") ||
  die "copy $gDirDistInstall/../install/setup.exe $gDirDistInstall: $!\n";

# Get resource file from mozilla install
copy("$gDirDistInstall/../install/setuprsc.dll", "$gDirDistInstall") ||
  die "copy $gDirDistInstall/../install/setuprsc.dll $gDirDistInstall: $!\n";

copy("install.ini", "$gDirDistInstall/setup") ||
  die "copy install.ini $gDirDistInstall/setup: $!\n";
copy("config.ini", "$gDirDistInstall") ||
  die "copy config.ini $gDirDistInstall: $!\n";
copy("config.ini", "$gDirDistInstall/setup") ||
  die "copy config.ini $gDirDistInstall/setup\n";
copy("$gDirDistInstall/setup.exe", "$gDirDistInstall/setup") ||
  die "copy $gDirDistInstall/setup.exe $gDirDistInstall/setup: $!\n";
copy("$gDirDistInstall/setuprsc.dll", "$gDirDistInstall/setup") ||
  die "copy $gDirDistInstall/setuprsc.dll $gDirDistInstall/setup: $!\n";

# copy license file for the installer
copy("$topsrcdir/LICENSE", "$gDirDistInstall/license.txt") ||
  die "copy $topsrcdir/LICENSE $gDirDistInstall/license.txt: $!\n";
copy("$topsrcdir/LICENSE", "$gDirDistInstall/setup/license.txt") ||
  die "copy $topsrcdir/LICENSE $gDirDistInstall/setup/license.txt: $!\n";

# create the self extracting stub installer
print "\n**************************************************************\n";
print "*                                                            *\n";
print "*  creating Self Extracting Executable Stub Install file...  *\n";
print "*                                                            *\n";
print "**************************************************************\n";
print "\n $gDirDistInstall/$seiFileNameSpecificStub\n";

# build the self-extracting .exe (installer) file.
copy("$gDirDistInstall/../install/$seiFileNameGeneric", "$gDirDistInstall/$seiFileNameSpecificStub") ||
  die "copy $gDirDistInstall/../install/$seiFileNameGeneric $gDirDistInstall/$seiFileNameSpecificStub: $!\n";

if(system("$gDirDistInstall/../install/nsztool.exe $gDirDistInstall/$seiFileNameSpecificStub $gDirDistInstall/setup/*.*"))
{
  die "\n Error: $gDirDistInstall/../install/nsztool.exe $gDirDistInstall/$seiFileNameSpecificStub $gDirDistInstall/setup/*.*\n";
}

# copy the lean installer to stub\ dir
print "\n****************************\n";
print "*                          *\n";
print "*  copying Stub files...   *\n";
print "*                          *\n";
print "****************************\n";
print "\n $gDirDistInstall/stub/$seiFileNameSpecificStub\n";

if(-d "$gDirDistInstall/stub")
{
  unlink <$gDirDistInstall/stub/*>;
}
else
{
  mkdir ("$gDirDistInstall/stub",0775);
}
copy("$gDirDistInstall/$seiFileNameSpecificStub", "$gDirDistInstall/stub") ||
  die "copy $gDirDistInstall/$seiFileNameSpecificStub $gDirDistInstall/stub: $!\n";

# create the big self extracting .exe installer
print "\n**************************************************************\n";
print "*                                                            *\n";
print "*  creating Self Extracting Executable Full Install file...  *\n";
print "*                                                            *\n";
print "**************************************************************\n";
print "\n $gDirDistInstall/$seiFileNameSpecific\n";

if(-d "$gDirDistInstall/sea")
{
  unlink <$gDirDistInstall/sea/*>;
}
else
{
  mkdir ("$gDirDistInstall/sea",0775);
}

copy("$gDirDistInstall/../install/$seiFileNameGeneric", "$gDirDistInstall/$seiFileNameSpecific") ||
  die "copy $gDirDistInstall/$seiFileNameGeneric $gDirDistInstall/$seiFileNameSpecific: $!\n";

if(system("$gDirDistInstall/../install/nsztool.exe $gDirDistInstall/$seiFileNameSpecific $gDirDistInstall/setup/*.* $gDirDistInstall/xpi/*.*"))
{
  die "\n Error: $gDirDistInstall/../install/nsztool.exe $gDirDistInstall/$seiFileNameSpecific $gDirDistInstall/setup/*.* $gDirDistInstall/xpi/*.*\n";
}

copy("$gDirDistInstall/$seiFileNameSpecific", "$gDirDistInstall/sea") ||
  die "copy $gDirDistInstall/$seiFileNameSpecific $gDirDistInstall/sea: $!\n";

unlink <$gDirDistInstall/$seiFileNameSpecificStub>;

print " done!\n\n";

exit(0);



sub MakeExeZip
{
  my($aSrcDir, $aExeFile, $aZipFile) = @_;
  my($saveCwdir);

  $saveCwdir = cwd();
  chdir($aSrcDir);
  if(system("zip $gDirDistInstall/xpi/$aZipFile $aExeFile"))
  {
    chdir($saveCwdir);
    die "\n Error: zip $gDirDistInstall/xpi/$aZipFile $aExeFile";
  }
  chdir($saveCwdir);
}

sub PrintUsage
{
  die "usage: $0 <default app version> [options]

       default app version: version to use for GRE
                            ie: 1.3b.0.0

       options include:
           -stagePath <staging path> : Full path to where the GRE components are staged at
                                       Default path, if one is not set, is:
                                         [mozilla]/stage

           -distPath <dist path>     : Full path to where the GRE dist dir is at.
                                       Default path, if one is not set, is:
                                         [mozilla]/dist

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
    elsif($myArgv[$counter] =~ /^[-,\/]stagePath$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inStagePath = $myArgv[$counter];
        $inStagePath =~ s/\\/\//g;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]distPath$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inDistPath = $myArgv[$counter];
        $inDistPath =~ s/\\/\//g;
      }
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
  chdir("$gDirPackager/win_gre");
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inDefaultVersion $gDirStageProduct $gDirDistInstall/xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl config.it $inDefaultVersion $gDirStageProduct $gDirDistInstall/xpi $inRedirIniURL $inXpiURL\n";
    return(1);
  }

  # Make install.ini file
  if(system("perl makecfgini.pl install.it $inDefaultVersion $gDirStageProduct $gDirDistInstall/xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl install.it $inDefaultVersion $gDirStageProduct $gDirDistInstall/xpi $inRedirIniURL $inXpiURL\n";
    return(1);
  }
  return(0);
}

sub MakeUninstall
{
  chdir("$gDirPackager/win_gre");
  if(MakeUninstallIniFile())
  {
    return(1);
  }

  # Copy the uninstall files to the dist uninstall directory.
  copy("uninstall.ini", "$gDirDistInstall") ||
    die "copy uninstall.ini $gDirDistInstall: $!\n";
  copy("uninstall.ini", "$gDirDistInstall/uninstall") ||
    die "copy uninstall.ini $gDirDistInstall/uninstall: $!\n";
  copy("defaults_info.ini", "$gDirDistInstall") ||
    die "copy defaults_info.ini $gDirDistInstall: $!\n";
  copy("defaults_info.ini", "$gDirDistInstall/uninstall") ||
    die "copy defaults_info.ini $gDirDistInstall/uninstall: $!\n";
  # Get uninstaller from mozilla install
  copy("$gDirDistInstall/../install/uninstall.exe", "$gDirDistInstall") ||
    die "copy $gDirDistInstall/../install/uninstall.exe $gDirDistInstall: $!\n";
  copy("$gDirDistInstall/uninstall.exe", "$gDirDistInstall/uninstall") ||
    die "copy $gDirDistInstall/uninstall.exe $gDirDistInstall/uninstall: $!\n";

  # build the self-extracting .exe (uninstaller) file.
  print "\nbuilding self-extracting uninstaller ($seuFileNameSpecific)...\n";
  copy("$gDirDistInstall/../install/$seiFileNameGeneric", "$gDirDistInstall/$seuFileNameSpecific") ||
    die "copy $gDirDistInstall/../install/$seiFileNameGeneric $gDirDistInstall/$seuFileNameSpecific: $!\n";

  if(system("$gDirDistInstall/../install/nsztool.exe $gDirDistInstall/$seuFileNameSpecific $gDirDistInstall/uninstall/*.*"))
  {
    print "\n Error: $gDirDistInstall/../install/nsztool.exe $gDirDistInstall/$seuFileNameSpecific $gDirDistInstall/uninstall/*.*\n";
    return(1);
  }

  MakeExeZip($gDirDistInstall, $seuFileNameSpecific, $seuzFileNameSpecific);
  unlink <$gDirDistInstall/$seuFileNameSpecific>;
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

  chdir("$gDirPackager/win_gre");
  # Make .js file
  if(system("perl makejs.pl $mComponent.jst $inDefaultVersion $gDirStageProduct/$mComponent"))
  {
    print "\n Error: perl makejs.pl $mComponent.jst $inDefaultVersion $gDirStageProduct/$mComponent\n";
    return(1);
  }
  return(0);
}

sub MakeXpiFile
{
  my($mComponent);

  chdir("$gDirPackager/win_gre");
  foreach $mComponent (@gComponentList)
  {
    if($mComponent =~ /xpcom/i)
    {
      # For now, just copy xpcom.jst from ...\packager\windows\xpcom.jst since they
      # are exactly the same.  Do not check in a copy of it into ...\packager\win_gre.
      copy("$gDirPackager/windows/xpcom.jst", "$gDirPackager/win_gre") ||
        die "copy(\"$gDirPackager/windows/xpcom.jst\", \"$gDirPackager/win_gre\"): $!\n";
    }

    # Make .js files
    if(MakeJsFile($mComponent))
    {
      return(1);
    }

    # Make .xpi file
    if(system("perl makexpi.pl $mComponent $gDirStageProduct $gDirDistInstall/xpi"))
    {
      print "\n Error: perl makexpi.pl $mComponent $gDirStageProduct $gDirDistInstall/xpi\n";
      return(1);
    }

    # Put a copy of the xpi with he installer itself
    copy("$gDirDistInstall/xpi/$mComponent.xpi", "$gDirDistInstall") ||
      die "copy $gDirDistInstall/xpi/$mComponent.xpi $gDirDistInstall: $!\n";
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
      print " place holder: $gDirStageProduct/$mComponent\n";
      mkdir("$gDirStageProduct/$mComponent", 775);
    }
    elsif(-d "$gDirStageProduct/$mComponent")
    {
      print "           ok: $gDirStageProduct/$mComponent\n";
    }
    else
    {
      print "        Error: $gDirStageProduct/$mComponent does not exist!\n";
      $mError = 1;
    }
  }
  print "\n";
  return($mError);
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

