#!/usr/bin/perl -w

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
# The Original Code is Mozilla Communicator.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corp.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
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

# This script contains functions that aid in the creation of the stage ares
# for a given product.  It helps with copying multiple files and also subdirs.
# It is mainly used by make_stage.pl and the stage_[product name].pl scripts.

package StageUtils;

require 5.004;

use strict;
use Cwd;
use File::Copy;
use File::Path;
use File::Basename;
use IO::Handle;

my($gDirScripts)            = dirname($0);  # directory of the running script
$gDirScripts                =~ s/\\/\//g;
my($DEPTH)                  = "../..";

sub CopySubDirs
{
  my($aSrc, $aDestDir) = @_;
  my($SDIR)            = undef;
  my($file)            = undef;

  if(!(-d "$aDestDir"))
  {
    mkdir("$aDestDir", 755) || die "\n mkdir(\"$aDestDir\", 755): $!\n";
  }

  if(-d $aSrc)
  {
    CopyFiles("$aSrc", "$aDestDir");
    my $SDIR = new IO::Handle;

    opendir($SDIR, "$aSrc") || die "opendir($aSrc): $!\n";
    while ($file = readdir($SDIR))
    {
      next if(-f "$aSrc/$file");
      next if(($file eq "\.\.") || ($file eq "\."));

      if(!(-d "$aDestDir/$file"))
      {
        mkdir("$aDestDir/$file", 755) || die "\n mkdir(\"$aDestDir/$file\", 755): $!\n";
      }

      CopySubDirs("$aSrc/$file", "$aDestDir/$file");
    }
    closedir($SDIR);
  }
  elsif(-f $aSrc)
  {
    copy("$aSrc", "$aDestDir") || die "\n copy($aSrc, $aDestDir): $!\n";
  }
  else
  {
    die "\n error file not found: $aSrc\n";
  }
}

sub CopyFiles
{
  my($aSrc, $aDestDir) = @_;
  my($SDIR)            = undef;
  my($file)            = undef;

  if(!(-d "$aDestDir"))
  {
    mkdir("$aDestDir", 755) || die "\n mkdir(\"$aDestDir\", 755): $!\n";
  }
  if(-d $aSrc)
  {
    my $SDIR = new IO::Handle;

    opendir($SDIR, "$aSrc") || die "opendir($aSrc): $!\n";
    while ($file = readdir($SDIR))
    {
      next if(-d "$aSrc/$file");
      copy("$aSrc/$file", "$aDestDir") || die "\n copy($aSrc/$file, $aDestDir): $!\n";
    }
    closedir($SDIR);
  }
  elsif(-f $aSrc)
  {
    copy("$aSrc", "$aDestDir") || die "\n copy($aSrc, $aDestDir): $!\n";
  }
  else
  {
    die "\n error file not found: $aSrc\n";
  }
}

sub GetAbsPath
{
  my($aWhichPath) = @_;
  my($savedCwd)  = undef;
  my($path)      = undef;
  my($_path)     = undef;

  $savedCwd = cwd();
  chdir($gDirScripts);  # We need to chdir to the scripts dir because $DEPTH is relative to this dir.
  if($aWhichPath eq "moz_dist")
  {
    $_path = "$DEPTH/dist";
  }
  elsif($aWhichPath eq "moz_packager")
  {
    $_path = "$DEPTH/xpinstall/packager";
  }
  elsif($aWhichPath eq "moz_scripts")
  {
    $_path = "$DEPTH/xpinstall/packager/scripts";
  }
  elsif($aWhichPath eq "moz_root")
  {
    $_path = $DEPTH;
  }
  elsif($aWhichPath eq "cwd")
  {
    $_path = cwd();
  }

  # verify the existance of path
  if(!$_path)
  {
    die " StageUtils::GetAbsPath::unrecognized path to locate: $aWhichPath\n";
  }

  if(!(-d $_path))
  {
    die " path not found: $_path\n";
  }

  chdir($_path);
  $path = cwd();
  chdir($savedCwd);
  $path =~ s/\\/\//g;
  return($path);
}

sub CreateStage
{
  my($aDirSrcDist, $aDirStageProductName, $aDirDistPackagesProductName, $aOs) = @_;
  my($savedCwd)       = cwd();
  my($SDIR)           = undef;
  my($file)           = undef;
  my($processedAFile) = undef;
  my($dirMozPackager) = GetAbsPath("moz_packager");

  # The destination cannot be a sub directory of the source.
  # pkgcp.pl will get very unhappy.

  if(!(-d $aDirSrcDist))
  {
    die "\n path not found: $aDirSrcDist\n";
  }

  chdir("$savedCwd");
  if(!(-d "$aDirStageProductName"))
  {
    mkdir("$aDirStageProductName", 755) || die "mkdir($aDirStageProductName, 755): $!\n";
  }
  print("\n Stage area: $aDirStageProductName\n"); 
  if(-d $aDirDistPackagesProductName)
  {
    my $SDIR = new IO::Handle;

    opendir($SDIR, $aDirDistPackagesProductName) || die "opendir($aDirDistPackagesProductName): $!\n";
    while ($file = readdir($SDIR))
    {
      next if(-d "$aDirDistPackagesProductName/$file");
      $processedAFile = 1;
      print "\n\n pkg file: $file (located in $aDirDistPackagesProductName)\n\n";
      system("perl \"$dirMozPackager/pkgcp.pl\" -s \"$aDirSrcDist\" -d \"$aDirStageProductName\" -f \"$aDirDistPackagesProductName/$file\" -o $aOs -v");
    }
    closedir($SDIR);

    if(!$processedAFile)
    {
      die "\n Error: no package(s) to process in $aDirStageProductName\n";
    }
  }
  else
  {
    die "\n path not found: $aDirDistPackagesProductName\n";
  }
}

sub CopyAdditionalPackage
{
  my($aFile, $aDirDistPackagesProductName) = @_;

  if(!(-e "$aDirDistPackagesProductName"))
  {
    mkdir("$aDirDistPackagesProductName", 755) || die "\n mkdir($aDirDistPackagesProductName, 755): $!\n";
  }

  if(-e $aFile)
  {
    print " copying $aFile\n";
    copy("$aFile", "$aDirDistPackagesProductName") || die "copy('$aFile', '$aDirDistPackagesProductName'): $!\n";
  }
}

sub CleanupStage
{
  my($aDirStage, $aProductName) = @_;
  my($dirStageProduct) = "$aDirStage/$aProductName";

  rmtree([$dirStageProduct],0,0) if(-d $dirStageProduct);
  if(!(-d "$aDirStage"))
  {
    mkdir("$aDirStage", 755) || die "\n mkdir($aDirStage, 755): $!\n";
  }
}

sub CleanupDistPackages
{
  my($aDirDistPackages, $aProductName) = @_;
  my($dirDistPackagesProductName) = "$aDirDistPackages/$aProductName";

  rmtree([$dirDistPackagesProductName],0,0) if(-d $dirDistPackagesProductName);
  if(!(-d "$aDirDistPackages"))
  {
    mkdir("$aDirDistPackages", 755) || die "\n mkdir($aDirDistPackages, 755): $!\n";
  }
  if(!(-d "$dirDistPackagesProductName"))
  {
    mkdir("$dirDistPackagesProductName", 755) || die "\n mkdir($dirDistPackagesProductName, 755): $!\n";
  }
}

sub GeneratePackagesFromSinglePackage
{
  my($aPlatform, $aSinglePackage, $aDestination) = @_;

  if(!(-d "$aDestination"))
  {
    mkdir("$aDestination", 755) || die "\n mkdir('$aDestination', 755): $!\n";
  }
  system("perl $gDirScripts/make_pkgs_from_list.pl -p $aPlatform -pkg $aSinglePackage -o $aDestination");
}

# To retrieve a build id ($aDefine) from $aBuildIDFile (normally
# mozilla/config/nsBuildID.h).
sub GetProductBuildID
{
  my($aBuildIDFile, $aDefine) = @_;
  my($line);
  my($buildID);
  my($fpInIt);

  if(!(-e $aBuildIDFile))
  {
    die "\n file not found: $aBuildIDFile\n";
  }

  # Open the input file
  open($fpInIt, $aBuildIDFile) || die "\n open $aBuildIDFile for reading: $!\n";

  # While loop to read each line from input file
  while($line = <$fpInIt>)
  {
    if($line =~ /#define.*$aDefine/)
    {
      $buildID = $line;
      chop($buildID);

      # only get the build id from string, excluding spaces
      $buildID =~ s/..*$aDefine\s+//;
      # strip out any quote characters
      $buildID =~ s/\"//g;
    }
  }
  close($fpInIt);
  return($buildID);
}

# GetGreSpecialID()
#   To build GRE's ID as follows:
#     * Use mozilla's milestone version for 1st 2 numbers of version x.x.x.x.
#     * DO NOT Strip out any non numerical chars from mozilla's milestone
#       version.
#     * Get the y2k ID from mozilla/config/nsBuildID.h.
#     * Build the GRE special ID given the following:
#         mozilla milestone: 1.4a
#         mozilla buildID.h: 2003030510
#         GRE Special ID   : 1.4a_2003030510
sub GetGreSpecialID
{
  my($aDirMozTopSrc)                    = @_;
  my($fileBuildID)                      = "$aDirMozTopSrc/config/nsBuildID.h";
  my($greID)                            = undef;
  my($initEmptyValues)                  = 0;
  my($versionMilestone)                 = GetProductMilestoneVersion($aDirMozTopSrc, $aDirMozTopSrc, $initEmptyValues);

  $greID = GetProductBuildID($fileBuildID, "GRE_BUILD_ID");
  return($versionMilestone);
#  return($greID);
#  return($versionMilestone . "_$greID");
}

# GetGreFileVersion()
#   To build GRE's file version as follows:
#     * Use mozilla's milestone version for 1st 2 numbers of version x.x.x.x.
#     * Strip out any non numerical chars from mozilla's milestone version.
#     * Get the y2k ID from mozilla/config/nsBuildID.h.
#     * Split the y2k ID exactly in 2 equal parts and use them for the last
#       2 numbers of the version x.x.x.x.
#         ie: y2k: 2003030510
#             part1: 20030
#             part2: 30510
#
#  XXX  XXX: Problem with this format! It won't work for dates > 65536.
#              ie: 20030 71608 (July 16, 2003 8am)
#
#            mozilla/config/version_win.pl has the same problem because these
#            two code need to be in sync with each other.
#
#     * Build the GRE file version given a mozilla milestone version of "1.4a"
#         GRE version: 1.4.20030.30510
sub GetGreFileVersion
{
  my($aDirMozTopSrc)                    = @_;
  my($fileBuildID)                      = "$aDirMozTopSrc/config/nsBuildID.h";
  my($initEmptyValues)                  = 1;
  my(@version)                          = undef;
  my($y2kDate)                          = undef;
  my($buildID_hi)                       = undef;
  my($buildID_lo)                       = undef;
  my($versionMilestone)                 = GetProductMilestoneVersion($aDirMozTopSrc, $aDirMozTopSrc, $initEmptyValues);

  $versionMilestone =~ s/[^0-9.][^.]*//g; # Strip out non numerical chars from versionMilestone.
  @version          = split /\./, $versionMilestone;
  $y2kDate          = GetProductBuildID($fileBuildID, "NS_BUILD_ID");

  # If the buildID is 0000000000, it means that it's a non release build.
  # This also means that the GRE version (xpcom.dll's version) will be
  # 0.0.0.0.  We need to match this version for install to proceed
  # correctly.
  if($y2kDate eq "0000000000")
  {
    return("0.0.0.0");
  }

  $buildID_hi       = substr($y2kDate, 0, 5);
  $buildID_hi       =~ s/^0+//; # strip out leading '0's
  $buildID_hi       = 0 if($buildID_hi eq undef); #if buildID_hi happened to be all '0's, then set it to '0'
  $buildID_lo       = substr($y2kDate, 5);
  $buildID_lo       =~ s/^0+//; # strip out leading '0's
  $buildID_lo       = 0 if($buildID_lo eq undef); #if buildID_hi happened to be all '0's, then set it to '0'

  return("$version[0].$version[1].$buildID_hi.$buildID_lo");
}

# Retrieves the product's milestone version (ns or mozilla):
#   ie: "1.4a.0.0"
#
# If the milestone version is simply "1.4a", this function will prefil
# the rest of the 4 unit ids with '0':
#   ie: "1.4a" -> "1.4a.0.0"
#
# The milestone version is acquired from [topsrcdir]/config/milestone.txt
sub GetProductMilestoneVersion
{
  my($aDirMozTopSrc, $aDirConfigTopSrc, $initEmptyValues) = @_;
  my($y2kDate)                          = undef;
  my($versionMilestone)                 = undef;
  my($counter)                          = undef;
  my(@version)                          = undef;
  my($saveCwd)                          = cwd();

  chdir("$aDirMozTopSrc/config");
  $versionMilestone = `perl milestone.pl --topsrcdir $aDirConfigTopSrc`;
  chop($versionMilestone);
  chdir($saveCwd);

  if(($initEmptyValues ne undef) && ($initEmptyValues eq 1))
  {
    @version = split /\./, $versionMilestone;

    # Initialize any missing version blocks (x.x.x.x) to '0'
    for($counter = $#version + 1; $counter <= 3; $counter++)
    {
      $version[$counter] = "0";
    }
    return("$version[0].$version[1].$version[2].$version[3]");
  }
  return($versionMilestone);
}

# Retrieves the products's milestone version from either the ns tree or the
# mozilla tree.
#
# It will also use the y2k compliant build id from mozilla/config/nsBuildID.h
# in the last value:
#   ie: milestone.txt               : 1.4a
#       nsBuildID.h                 : 2003030510
#       product version then will be: 1.4a.0.2003030510
#
# The milestone version is acquired from [topsrcdir]/config/milestone.txt
sub GetProductY2KVersion
{
  my($aDirMozTopSrc, $aDirConfigTopSrc) = @_;
  my($fileBuildID)                      = "$aDirMozTopSrc/config/nsBuildID.h";
  my($initEmptyValues)                  = 1;
  my(@version)                          = undef;
  my($y2kDate)                          = undef;
  my($versionMilestone)                 = GetProductMilestoneVersion($aDirMozTopSrc, $aDirConfigTopSrc, $initEmptyValues);

  @version = split /\./, $versionMilestone;
  $y2kDate = GetProductBuildID($fileBuildID, "NS_BUILD_ID");
  return("$version[0].$version[1].$version[2].$y2kDate");
}

