#!/usr/bin/perl
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
#   Ben Goodger <ben@mozilla.org>
#   Brian Ryner <bryner@brianryner.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
# This perl script builds the xpi, config.ini, and js files.
#

use Cwd;
use File::Copy;
use File::Path;
use File::Basename;

$DEPTH = "../../..";
$topsrcdir = GetTopSrcDir();
$win32 = ($^O =~ /((MS)?win32)|cygwin|os2/i) ? 1 : 0;

# platform specific locations and file names
if ($win32) {
    $platform_dir = 'windows';
} else {
    $platform_dir = 'unix';
}

require "$platform_dir/install_sub.pl";

# ensure that Packager.pm is in @INC, since we might not be called from
# mozilla/xpinstall/packager
push(@INC, "$topsrcdir/xpinstall/packager");
require StageUtils;
require "$topsrcdir/config/zipcfunc.pl";

ParseArgv(@ARGV);

$topobjdir                = "$topsrcdir"                 if !defined($topobjdir);
$inStagePath              = "$topobjdir/stage"           if !defined($inStagePath);
$inDistPath               = "$topobjdir/dist"            if !defined($inDistPath);
$inXpiURL                 = "ftp://not.supplied.invalid" if !defined($inXpiURL);
$inRedirIniURL            = $inXpiURL                    if !defined($inRedirIniURL);

if ($inConfigFiles eq "") 
{
  print "Error: Packager Manifests and Install Script Location not supplied! Use -config";
  exit(1);
}  

# Initialize state from environment (which is established in the calling script
# after reading the installer.cfg file. 
$versionLanguage          = $ENV{WIZ_versionLanguage};
@gComponentList           = split(/,/, $ENV{WIZ_componentList});
$seiFileNameGeneric       = "nsinstall".$exe_suffix;
$seiFileNameSpecific      = $ENV{WIZ_fileInstallerExe};
$seiStubRootName          = $ENV{WIZ_fileNetStubRootName};
$seiFileNameSpecificStub  = "$seiStubRootName".$exe_suffix;
$seuFileNameSpecific      = $ENV{WIZ_fileUninstall};
$seuzFileNameSpecific     = $ENV{WIZ_fileUninstallZip};

if(defined($ENV{DEBUG_INSTALLER_BUILD}))
{
  print " makeall.pl\n";
  print "   topobjdir  : $topobjdir\n";
  print "   topsrcdir  : $topsrcdir\n";
  print "   inStagePath: $inStagePath\n";
  print "   inDistPath : $inDistPath\n";
}

$gDefaultProductVersion = $ENV{WIZ_versionProduct};

print "\n";
print " Building $ENV{WIZ_nameProduct} installer\n";
print "  Raw version id   : $gDefaultProductVersion\n";
print "  Display version  : $ENV{WIZ_userAgentShort}\n";
print "  Xpinstall version: $gDefaultProductVersion\n";
print "\n";

$gDirPackager         = "$topsrcdir/xpinstall/packager";
$gDirStageProduct     = $inStagePath;
$gDirDistInstall      = "$inDistPath/install";
$gNGAppsScriptsDir    = "$topsrcdir/toolkit/mozapps/installer";
$gNGAppsPlatformDir   = "$topsrcdir/toolkit/mozapps/installer/$platform_dir";

# The following variables are for displaying version info in the 
# the installer.
$ENV{WIZ_xpinstallVersion}     = "$gDefaultProductVersion";
$ENV{WIZ_distInstallPath}      = "$gDirDistInstall";

print "\n";
print " Building $ENV{WIZ_nameProduct} $ENV{WIZ_userAgent}...\n";
print "\n";

# Check for existence of staging path
if(!(-d "$gDirStageProduct"))
{
  die "\n Invalid path: $gDirStageProduct\n";
}

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

print "Making XPI files...\n";
MakeXpiFile() && die;
print "Making config.ini...\n";
MakeConfigFile() && die;

# Copy the setup files to the dist setup directory.
print "Copying setup files to setup directory\n";
foreach $_ (@wizard_files) {
  copy ("$gDirDistInstall/$_", "$gDirDistInstall/setup") ||
      die "syscopy $gDirDistInstall/$_ $gDirDistInstall/setup: $!\n";

  $mode = (stat("$gDirDistInstall/$_"))[2];
  chmod $mode, "$gDirDistInstall/setup/$_";
}

foreach $_ ("config.ini", "install.ini") {
    copy ("$gDirDistInstall/$_", "$gDirDistInstall/setup") ||
	die "copy $gDirDistInstall/$_ $gDirDistInstall/setup: $!\n";
}

# copy license file for the installer
$licenseLocation = "$topsrcdir/LICENSE";
if ($ENV{WIZ_licenseFile} ne "") {
  $licenseLocation = $ENV{WIZ_licenseFile};
}
copy("$topsrcdir/$licenseLocation", "$gDirDistInstall/license.txt") ||
  die "copy $topsrcdir/$licenseLocation $gDirDistInstall/license.txt: $!\n";
copy("$topsrcdir/$licenseLocation", "$gDirDistInstall/setup/license.txt") ||
  die "copy $licenseLocation $gDirDistInstall/setup/license.txt: $!\n";

BuildPlatformInstaller() && die;

# end of script
exit(0);

sub PrintUsage
{
  die "usage: $0 [options]

       options include:

           -config <path>            : path to where configuration and component script
                                       files are located. 
                                       
           -objDir <path>            : path to the objdir.  default is topsrcdir

           -stagePath <staging path> : full path to where the mozilla components are staged at
                                       Default stage path, if this is not set, is:
                                         [mozilla]/stage

           -distPath <dist path>     : full path to where the mozilla dist dir is at.
                                       Default stage path, if this is not set, is:
                                         [mozilla]/dist

           -aurl <archive url>       : either ftp:// or http:// url to where the
                                       archives (.xpi, .exe, .zip, etc...) reside

           -rurl <redirect.ini url>  : either ftp:// or http:// url to where the
                                       redirec.ini resides.  If not supplied, it
                                       will be assumed to be the same as archive
                                       url.
       \n";
}

sub ParseArgv
{
  my(@myArgv) = @_;
  my($counter);

  for($counter = 0; $counter <= $#myArgv; $counter++)
  {
    if($myArgv[$counter] =~ /^[-,\/]h$/i)
    {
      PrintUsage();
    }
    elsif($myArgv[$counter] =~ /^[-,\/]objDir$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $topobjdir = $myArgv[$counter];
        $topobjdir =~ s/\\/\//g;
      }
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
    elsif($myArgv[$counter] =~ /^[-,\/]config$/i)
    {
      if ($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inConfigFiles = $myArgv[$counter];
      }
    }
  }
}

sub MakeConfigFile
{
  $ENV{WIZ_userAgent} = "$ENV{WIZ_userAgentShort} ({AB_CD})";

  # Make config.ini and other ini files
  chdir($gDirDistInstall);
  foreach $_ ("config.ini", "install.ini") {
      $itFile = $_;
      $itFile =~ s/\.ini$/\.it/;
      copy("$inConfigFiles/$itFile", "$gDirDistInstall/$itFile") || die "copy $inConfigFiles/$itFile $gDirDistInstall/$itFile";

      if(system("perl $gNGAppsScriptsDir/makecfgini.pl $itFile $ENV{WIZ_userAgentShort} $gDirStageProduct $gDirDistInstall/xpi $inRedirIniURL $inXpiURL"))
      {
        print "\n Error: perl $gNGAppsScriptsDir/makecfgini.pl $itFile $ENV{WIZ_userAgentShort} $gDirStageProduct $gDirDistInstall/xpi $inRedirIniURL $inXpiURL\n";
        return(1);
      }

      unlink("$gDirDistInstall/$itFile");
  }
  return(0);
}

sub MakeJsFile
{
  my($mComponent) = @_;

  $ENV{WIZ_userAgent} = "$ENV{WIZ_userAgentShort} ($versionLanguage)";

  # Make .js file
  chdir($inConfigFiles);
  if(system("perl $gNGAppsScriptsDir/makejs.pl $mComponent.jst $gDefaultProductVersion $gDirStageProduct/$mComponent"))
  {
    print "\n Error: perl $gNGAppsScriptsDir/makejs.pl $mComponent.jst $gDefaultProductVersion $gDirStageProduct/$mComponent\n";
    return(1);
  }
  return(0);
}

sub MakeXpiFile
{
  my($mComponent);

  chdir($inConfigFiles);
  foreach $mComponent (@gComponentList)
  {
    # Make .js files
    if(MakeJsFile($mComponent))
    {
      return(1);
    }

    # Make .xpi file
    if(system("perl $gNGAppsScriptsDir/makexpi.pl $mComponent $gDirStageProduct $gDirDistInstall/xpi"))
    {
      print "\n Error: perl $gNGAppsScriptsDir/makexpi.pl $mComponent $gDirStageProduct $gDirDistInstall/xpi\n";
      return(1);
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
      print " place holder: $gDirStageProduct/$mComponent\n";
      mkdir("$gDirStageProduct/$mComponent", 0775);
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
