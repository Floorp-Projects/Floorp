#!/usr/bin/perl -w
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
# Samir Gehani <sgehani@netscape.com>
# 

#
# This perl script parses the input file for special variables
# in the format of $Variable$ and replace it with the appropriate
# value(s).
#
# Input: .jst file        - which is a .js template
#        default version  - a julian date in the form of:
#                           major.minor.release.yydoy
#                           ie: 5.0.0.99256
#        staging path     - path to where the components are staged at
#
#        ie: perl makejs.pl core.jst 5.0.0.99256 ../../staging_area/core
#

# Make sure there are at least three arguments
if($#ARGV < 2)
{
  die "usage: $0 <.jst file> <default version> <staging path>

       .jst file              : .js template input file
       default version        : default julian base version number to use in the
                                form of: major.minor.release.yydoy
                                ie: 5.0.0.99256
       component staging path : path to where this component is staged at
                                ie: z:\stage\windows\32bit\en\5.0\core
       \n";
}

$inJstFile        = $ARGV[0];
$inVersion        = $ARGV[1];
$inStagePath      = $ARGV[2];

# Get the name of the file replacing the .jst extension with a .js extension
@inJstFileSplit   = split(/\./,$inJstFile);
$outJsFile        = $inJstFileSplit[0];
$outJsFile       .= ".js";
$outTempFile      = $inJstFileSplit[0];
$outTempFile     .= ".template";

system("cp ../common/share.t $outTempFile");
system("cat $inJstFile >> $outTempFile");

# Open the input template file
open(fpInJst, "<$outTempFile") || die "\ncould not open $outTempFile: $!\n";

# Open the output .js file
open(fpOutJs, ">$outJsFile") || die "\nCould not open $outJsFile: $!\n";

# While loop to read each line from input file
while($line = <fpInJst>)
{
  # For each line read, search and replace $Version$ with the version passed in
  if($line =~ /\$Version\$/i)
  {
    $line =~ s/\$Version\$/$inVersion/i;
  }

  # For each line read, search and replace $SpaceRequired$ with the version passed in
  if($line =~ /\$SpaceRequired\$/i) 
  {
    $spaceRequired = 0;

    # split read line by ":" delimiter
    @colonSplit = split(/:/, $line);
    if($#colonSplit > 0)
    {
      @semiColonSplit = split(/;/, $colonSplit[1]);
      $subDir         = $semiColonSplit[0];
      $spaceRequired  = GetSpaceRequired("$inStagePath/$subDir");
      $spaceRequired  = int($spaceRequired/1024) + 1;
      $line =~ s/\$SpaceRequired\$:$subDir/$spaceRequired/i;
    }
    else
    {
      $spaceRequired = GetSpaceRequired("$inStagePath");
      $spaceRequired = int($spaceRequired/1024) + 1;
      $line =~ s/\$SpaceRequired\$/$spaceRequired/i;
    }
  }

  print fpOutJs $line;
}

system("rm $outTempFile");

##
# GetSpaceRequired
#
# Finds the space used by the contents of a dir by recursively
# traversing the subdir hierarchy and counting individual file
# sizes
#
# @param   targetDir  the directory whose space usage to find
# @return  spaceUsed  the number of bytes used by the dir contents
#
sub GetSpaceRequired()
{
    my($targetDir) = $_[0];
    my($spaceUsed) = 0;
    my(@dirEntries) = ();
    my($item) = "";

    @dirEntries = <$targetDir/*>;

    # iterate over all dir entries 
    foreach $item ( @dirEntries ) 
    {
        # if dir entry is dir
        if (-d $item)
        {       
            # add GetSpaceRequired(<dirEntry>) to space used
            $spaceUsed += GetSpaceRequired($item);
        }
        # else if dir entry is file
        elsif (-e $item)
        {
            # add size of file to space used
            $spaceUsed += (-s $item);
        }
    }

    return $spaceUsed;
}
