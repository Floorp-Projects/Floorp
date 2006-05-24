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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Samir Gehani <sgehani@netscape.com>
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
use File::Basename;

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

$win32 = ($^O =~ / ((MS)?win32)|cygwin|os2/i) ? 1 : 0;
if ($win32) {
    $platform = 'dos';
} else {
    $platform = 'unix';
}

# ensure that CFGParser.pm is in @INC, since we might not be called from
# mozilla/toolkit/mozapps/installer
my $top_path = $0;
$top_path =~ s/\\/\//g if $win32;
push(@INC, dirname($top_path));

require CFGParser;
CFGParser::ParseInstallerCfg("$inConfigFiles/installer.cfg");

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
system("perl pkgcp.pl -o $platform -s $DIST -d $STAGE -f $inConfigFiles/$ENV{WIZ_packagesFile} -v") &&
  die "pkgcp.pl failed: $!";
spew("Completed copying build files");

#// call xptlink.pl to make big .xpt files/component
system("perl xptlink.pl -s $DIST -d $STAGE -v") &&
  die "xptlink.pl failed: $!";
spew("Completed xptlinking"); 

#// call makeall.pl tunneling args (delivers .xpis to $inObjDir/installer/stage)
chdir("$inSrcDir/toolkit/mozapps/installer");
spew("perl makeall.pl $ver -config $inConfigFiles -aurl $inXpiURL -rurl $inRedirIniURL -objDir $inObjDir");
system("perl makeall.pl $ver -config $inConfigFiles -aurl $inXpiURL -rurl $inRedirIniURL -objDir $inObjDir") &&
  die "makeall.pl failed: $!";
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
