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

