#!/usr/bin/perl
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#     Samir Gehani <sgehani@netscape.com>
#
#==============================================================================
# usage: perl deliver.pl version URLPath stubName blobName buildWizard
# e.g.   perl deliver.pl 5.0.0.1 ftp://foo/ mozilla-installer mozilla-installer
#
# Delivers the stub and blob installers to mozilla/installer/stub 
# and mozilla/installer/sea, respectively.  Also, delivers the .xpis
# to mozilla/installer/raw/xpi.
#
# NOTE:
# -----
#   * all args are optional 
#   * version is used by xpinstall and needs to bumped every build
#   * URLPath must have a trailing slash
#   * if you are not building a release version no need to pass any args
#   * pass in "buildwizard" as the last arg to build the wizard too
#   * you must be in deliver.pl's dir when calling it since it assumes DEPTH
#==============================================================================

use Cwd;
use File::Path;

$inXpiURL = "";
$inRedirIniURL = "";
$inConfigFiles = "";
$inSrcDir = "";
$inObjDir = "";

ParseArgv(@ARGV);
if ($inXpiURL eq "")
{
  # archive url not supplied, set it to default values
    $inXpiURL      = "ftp://not.supplied.invalid";
    print "Warning: Remote XPI URL not set. Using $inXpiURL instead!\n";
}
if($inRedirIniURL eq "")
{
  # redirect url not supplied, set it to default value.
    $inRedirIniURL = $inXpiURL;
}
if($inConfigFiles eq "")
{
  print "Error: Packager Manifests and Install Script Location not supplied! Use -config\n";
  exit(1);
}
if ($inSrcDir eq "")
{
    print "Error: Mozilla source directory must be specified with -srcDir\n";
    exit (1);
}
if ($inObjDir eq "")
{
    print "Objdir not specified, using $inSrcDir\n";
    $inObjDir = $inSrcDir;
}

ParseInstallerCfg();

$win32 = ($^O =~ / ((MS)?win32)|cygwin|os2/i) ? 1 : 0;
if ($win32) {
    $platform = 'dos';
} else {
    $platform = 'unix';
}

ParseInstallerCfg();

$STAGE  = "$inObjDir/stage";
$DIST   = "$inObjDir/dist";

$verPartial = "5.0.0.";
$ver        = $verPartial . GetVersion($inObjDir);

# Set up the stage directory
if (-d $STAGE) {
    system("rm -rf $STAGE");
}

mkdir($STAGE, 0775);

#-------------------------------------------------------------------------
#   Make .xpis
#-------------------------------------------------------------------------
#// call pkgcp.pl
chdir("$inSrcDir/xpinstall/packager");
system("perl pkgcp.pl -o $platform -s $DIST -d $STAGE -f $inConfigFiles/$ENV{WIZ_packagesFile} -v");
spew("Completed copying build files");

#// call xptlink.pl to make big .xpt files/component
system("perl xptlink.pl -o $platform -s $DIST -d $STAGE -v");
spew("Completed xptlinking"); 

#// call makeall.pl tunneling args (delivers .xpis to $inObjDir/installer/stage)
chdir("$inSrcDir/toolkit/mozapps/installer");
spew("perl makeall.pl $ver -config $inConfigFiles -aurl $inXpiURL -rurl $inRedirIniURL -objDir $inObjDir");
system("perl makeall.pl $ver -config $inConfigFiles -aurl $inXpiURL -rurl $inRedirIniURL -objDir $inObjDir");
spew("Completed making .xpis");

spew("Installers built (see $inObjDir/dist/install/{stub,sea})");


#-------------------------------------------------------------------------
#   Utilities
#-------------------------------------------------------------------------
sub spew 
{
    print "+++ deliver.pl: ".$_[0]."\n";
}

sub copy
{
    if (! -e $_[0])
    {
        die "--- deliver.pl: couldn't cp cause ".$_[0]." doesn't exist: $!";
    }
    system ("cp ".$_[0]." ".$_[1]);
}

# Initialize global installer environment variables.  This information comes
# from the application's installer.cfg file which specifies such things as name
# and installer executable file names.

sub ParseInstallerCfg
{
    $ENV{WIZ_distSubdir} = "bin";
    $ENV{WIZ_packagesFile} = "packages-static";

    open(fpInstallCfg, "$inConfigFiles/installer.cfg") || die"\ncould not open $inConfigFiles/installer.cfg: $!\n";

    while ($line = <fpInstallCfg>)
    {

      if (substr($line, -2, 2) eq "\r\n") {
        $line = substr($line, 0, length($line) - 2) . "\n";
      }
      ($prop, $value) = ($line =~ m/(\w*)\s+=\s+(.*)\n/);

      if ($prop eq "VersionLanguage") {
        $ENV{WIZ_versionLanguage} = $value;
      }
      elsif ($prop eq "NameCompany") {
        $ENV{WIZ_nameCompany} = $value;
      }
      elsif ($prop eq "NameProduct") {
        $ENV{WIZ_nameProduct} = $value;
      }
      elsif ($prop eq "ShortNameProduct") {
        $ENV{WIZ_shortNameProduct} = $value;
      }
      elsif ($prop eq "NameProductInternal") {
        $ENV{WIZ_nameProductInternal} = $value;
      }
      elsif ($prop eq "VersionProduct") {
        $ENV{WIZ_versionProduct} = $value;
      }
      elsif ($prop eq "FileInstallerEXE") {
        $ENV{WIZ_fileInstallerExe} = $value;
      }
      elsif ($prop eq "FileUninstall") {
        $ENV{WIZ_fileUninstall} = $value;
      }
      elsif ($prop eq "FileUninstallZIP") {
        $ENV{WIZ_fileUninstallZip} = $value;
      }
      elsif ($prop eq "FileMainEXE") {
        $ENV{WIZ_fileMainExe} = $value;
      }
      elsif ($prop eq "FileInstallerNETRoot") {
        $ENV{WIZ_fileNetStubRootName} = $value;
      }
      elsif ($prop eq "ComponentList") {
        $ENV{WIZ_componentList} = $value;
      }
      elsif ($prop eq "7ZipSFXModule") {
        $ENV{WIZ_sfxModule} = $value;
      }
      elsif ($prop eq "DistSubdir") {
        $ENV{WIZ_distSubdir} = $value;
      }
      elsif ($prop eq "packagesFile") {
        $ENV{WIZ_packagesFile} = $value;
      }
    }

    close(fpInstallCfg);
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
	elsif($myArgv[$counter] =~ /^[-,\/]srcDir$/i)
	{
	    if ($#myArgv >= ($counter + 1))
	    {
		++$counter;
		$inSrcDir = $myArgv[$counter];
	    }
	}
	elsif($myArgv[$counter] =~ /^[-,\/]objDir$/i)
	{
	    if ($#myArgv >= ($counter + 1))
	    {
		++$counter;
		$inObjDir = $myArgv[$counter];
	    }
	}
    }
}

sub GetVersion
{
    my($depthPath) = @_;
    my($fileMozilla);
    my($fileMozillaVer);
    my($distWinPathName);
    my($monAdjusted);
    my($yy);
    my($mm);
    my($dd);
    my($hh);

    $distWinPathName = "dist";

    $fileMozilla = "$depthPath/$distWinPathName/$ENV{WIZ_distSubdir}/$ENV{WIZ_fileMainExe}";
    
  # verify the existance of file
  if(!(-e "$fileMozilla"))
  {
      print "file not found: $fileMozilla\n";
      exit(1);
  }

  ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, $ctime,
   $blksize, $blocks) = stat $fileMozilla;
  ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime($mtime);

  # calculate year
  # localtime() returns year 2000 as 100, we mod 100 to get at the last 2 digits
    $yy = $year % 100;
    $yy = "20" . sprintf("%.2d", $yy);

  # calculate month
    $monAdjusted = $mon + 1;
    $mm = sprintf("%.2d", $monAdjusted);

  # calculate month day
    $dd = sprintf("%.2d", $mday);

  # calculate day hour
    $hh = sprintf("%.2d", $hour);

    $fileMozillaVer = "$yy$mm$dd$hh";
    print "y2k compliant version string for $fileMozilla: $fileMozillaVer\n";
    return($fileMozillaVer);
}
