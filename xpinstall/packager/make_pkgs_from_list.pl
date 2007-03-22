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

# This script will take in a packages-* type file and generate individual *.pkg
# files for each section that it encounters.

use File::Path;

$inPackageFile    = undef;
$inPlatform       = undef;
$inOutputDir      = undef;
$fpInFile         = undef;
$fpOutFile        = undef;
$section          = undef;
$myFilename       = undef;
$fileCreated      = 0;

ParseArgv(@ARGV);

if(!$inPackageFile || !$inPlatform || !$inOutputDir)
{
  PrintUsage();
}

print "\n";
# Open the input file
open($fpInFile, $inPackageFile) || die "\ncould not open $inPackageFile: $!\n";

# While loop to read each line from input file
while($line = <$fpInFile>)
{
  if($line =~ /^\[.*\]/i)
  {
    if($fpOutFile)
    {
      close($fpOutFile);
    }

    $section = $line;
    chop($section);
    $section =~ s/\[//;
    $section =~ s/\]//;
    if(-e $inOutputDir)
    {
      $myFilename = "$inOutputDir/$section-$inPlatform.pkg";
    }
    else
    {
      $myFilename = "$section-$inPlatform.pkg";
    }

    print " creating $myFilename\n";
    # Open the output file
    open($fpOutFile, ">$myFilename") || die "\nCould not open $myFilename: $!\n";
    $fileCreated = 1;

    printf($fpOutFile "; Package file for $inPlatform\n");
    printf($fpOutFile ";\n");
    printf($fpOutFile "; File format:\n");
    printf($fpOutFile ";\n");
    printf($fpOutFile "; [] designates a toplevel component. Example: [xpcom]\n");
    printf($fpOutFile "; - in front of a file specifies it to be removed from the destination\n");
    printf($fpOutFile "; * wildcard support to recursively copy the entire directory\n");
    printf($fpOutFile "; ; file comment\n");
    printf($fpOutFile ";\n");
    printf($fpOutFile "\n");
    printf($fpOutFile "bin/.autoreg\n");
    printf($fpOutFile "\n");
    printf($fpOutFile "[$section]\n");
  }
  elsif($fileCreated eq 1)
  {
    printf($fpOutFile $line);
  }
}
close($fpOutFile);

# end of script
exit(0);

sub PrintUsage
{
  die "\n  usage: $0 <options>

                  -pkg <package file>  : Packages file such as 'packages-win'
                  -p <platform>        : 'win', 'mac', or 'unix'
                  -o* <output dir>     : Output directory. If not specified,
                                         output directory is cwd.

                  * not required
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
    elsif($myArgv[$counter] =~ /^[-,\/]pkg$/i)
    {
      if($#myArgv ge ($counter + 1))
      {
        ++$counter;
        $inPackageFile = $myArgv[$counter];
        $inPackageFile =~ s/\\/\//g;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]p$/i)
    {
      if($#myArgv ge ($counter + 1))
      {
        ++$counter;
        $inPlatform = $myArgv[$counter];
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]o$/i)
    {
      if($#myArgv ge ($counter + 1))
      {
        ++$counter;
        $inOutputDir = $myArgv[$counter];
        $inOutputDir =~ s/\\/\//g;
      }
    }
  }
}

