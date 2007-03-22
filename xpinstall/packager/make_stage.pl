#!c:\perl\bin\perl -w

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

# This script is used to create the stage area given a product.  This script is
# used in conjunction with the stage_[product name].pl files.
# This script will load the stage_[product name].pl and call its StageProduct()
# function.  Stage_[product name].pl will do the actual creation of the stage
# area.

use Cwd;
use File::Copy;
use File::Path;
use File::Basename;
use IO::Handle;

$DEPTH            = "../../";
$gDirScripts      = dirname($0);  # directory of the running script
$gDirScripts      =~ s/\\/\//g;
$gDirCwd          = cwd();

# ensure that Packager.pm is in @INC, since we might not be called from
# mozilla/xpinstall/packager
push(@INC, $gDirScripts);
require StageUtils;

$inProductName    = undef;
$inStagingScript  = undef;
$inOs             = undef;
ParseArgV(@ARGV);

if(!defined($topobjdir))
{
  chdir($DEPTH);
  $topobjdir = cwd();
  chdir($gDirCwd);
}

$inDirDestStage   = "$topobjdir/stage" if !defined($inDirDestStage);
$inDirSrcDist     = "$topobjdir/dist"  if !defined($inDirSrcDist);

if(defined($ENV{DEBUG_INSTALLER_BUILD}))
{
  print "\n make_stage.pl\n";
  print "   topobjdir     : $topobjdir\n";
  print "   gDirCwd       : $gDirCwd\n";
  print "   inDirDestStage: $inDirDestStage\n";
  print "   inDirSrcDist  : $inDirSrcDist\n";
}

if(!$inProductName || !$inOs)
{
  PrintUsage();
}

if(!$inStagingScript)
{
  # Look for the stage script in cwd first, then look at the
  # mozilla/xpinstall/packager dir.
  $inStagingScript = "$gDirCwd/stage_$inProductName.pl";
}

if(-e $inStagingScript)
{
  require "$inStagingScript";
}
else
{
  # Look in mozilla/xpinstall/packager dir since the stage script
  # was not found in cwd.
  $inStagingScript = "$gDirScripts/stage_$inProductName.pl";
  if(-e $inStagingScript)
  {
    require "$inStagingScript";
  }
  else
  {
    die "\n Could not locate stage_$inProductName.pl in:
   $gDirCwd
   $gDirScripts
\n";
  }
}

# This is where the stage area is actually created.
&StageProduct($inDirSrcDist, $inDirDestStage, $inProductName, $inOs);
chdir($gDirCwd);

exit(0);

sub ParseArgV
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
    elsif($myArgv[$counter] =~ /^[-,\/]dd$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inDirSrcDist = $myArgv[$counter];
        $inDirSrcDist =~ s/\\/\//g;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]sd$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inDirDestStage = $myArgv[$counter];
        $inDirDestStage =~ s/\\/\//g;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]pn$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inProductName = $myArgv[$counter];
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]ss$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inStagingScript = $myArgv[$counter];
        $inStagingScript =~ s/\\/\//g;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]os$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        if(($myArgv[$counter] =~ /^win$/i) ||
           ($myArgv[$counter] =~ /^mac$/i) ||
           ($myArgv[$counter] =~ /^os2$/i) ||
           ($myArgv[$counter] =~ /^unix$/i))
        {
          $inOs = $myArgv[$counter];
        }
      }
    }
  }
}

sub PrintUsage
{
  die "\n usage: $0 [options]

        -h
            Print this usage.

        -dd [dist dir]
            Directory for where the dist dir is at:
              ie: $0 -dd c:/builds/output/dist

            By default if '-dd' is not used, the dist dir is at:
              [mozilla root]/dist

            This path should contain both dist/bin and dist/packages

            *** NOTE ***
            The [dist dir] *cannot* be the parent dir of [stage dir].
            This will cause copy problems during the stage dir
            creation.

        -sd [stage dir]
            Optional stage directory:
              ie: $0 -sd c:/builds/output/stage

            By default if '-sd' is not used, the stage dir will be created at:
              [mozilla root]/stage

            *** NOTE ***
            The [stage dir] *cannot* be a subdir of [dist dir].
            This will cause copy problems during the stage dir
            creation.

        -ss [product's staging script]
            Full path to the script used to create the staging area for the
            product.
              ie: c:/builds/mozilla/xpinstall/packager/stage_mozilla.pl

      * -pn [product name]
            Product name to create the stage dir under.
              ie: [stage dir]/[product name]

            Currently, supported products are:
              gre
              mozilla

      * -os
            win, mac, unix, os2

        '*' indicates required parameters.
\n";
}

