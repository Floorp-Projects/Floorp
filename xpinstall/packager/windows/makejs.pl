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
# This perl script parses the input file for special variables
# in the format of $Variable$ and replace it with the appropriate
# value(s).
#
# Input: .jst file              - which is a .js template
#        default version        - a julian date in the form of:
#                                 major.minor.release.yydoy
#                                 ie: 5.0.0.99256
#        user agent             - user agent of product
#        component staging path - path to where the components are staged at
#
#        ie: perl makejs.pl core.jst 5.0.0.99256
#

# Make sure there are at least two arguments
if($#ARGV < 3)
{
  die "usage: $0 <.jst file> <default version> <UserAgent> <staging path>

       .jst file              : .js template input file
       default version        : default julian base version number to use in the
                                form of: major.minor.release.yydoy
                                ie: 5.0.0.99256
       user agent             : user agent of product (5.0b1 [en])
       component staging path : path to where this component is staged at
                                ie: z:\\stage\\windows\\32bit\\en\\5.0\\core
       \n";
}

$inJstFile        = $ARGV[0];
$inVersion        = $ARGV[1];
$inUserAgent      = $ARGV[2];
$inStagePath      = $ARGV[3];

# Get the name of the file replacing the .jst extension with a .js extension
@inJstFileSplit   = split(/\./,$inJstFile);
$outJsFile        = $inJstFileSplit[0];
$outJsFile       .= ".js";
$outTempFile      = $inJstFileSplit[0];
$outTempFile     .= ".template";

system("copy ..\\common\\share.t $outTempFile");
system("cat $inJstFile >> $outTempFile");

# Open the input .template file
open(fpInTemplate, $outTempFile) || die "\ncould not open $outTempFile: $!\n";

# Open the output .js file
open(fpOutJs, ">$outJsFile") || die "\nCould not open $outJsFile: $!\n";

# While loop to read each line from input file
while($line = <fpInTemplate>)
{
  # For each line read, search and replace $Version$ with the version passed in
  if($line =~ /\$Version\$/i)
  {
    $line =~ s/\$Version\$/$inVersion/i;
  }
  elsif($line =~ /\$UserAgent\$/i)
  {
    $line =~ s/\$UserAgent\$/$inUserAgent/i;
  }
  elsif($line =~ /\$SpaceRequired\$/i) # For each line read, search and replace $InstallSize$ with the calculated size
  {
    $spaceRequired = 0;

    # split read line by ":" deliminator
    @colonSplit = split(/:/, $line);
    if($#colonSplit > 0)
    {
      @semiColonSplit = split(/;/, $colonSplit[1]);
      $subDir         = $semiColonSplit[0];
      $spaceRequired  = GetSpaceRequired("$inStagePath\\$subDir");
      $line =~ s/\$SpaceRequired\$:$subDir/$spaceRequired/i;
    }
    else
    {
      $spaceRequired = GetSpaceRequired("$inStagePath");
      $line =~ s/\$SpaceRequired\$/$spaceRequired/i;
    }
  }

  print fpOutJs $line;
}

sub GetSpaceRequired()
{
  my($inPath) = @_;
  my($spaceRequired);

  print "   calulating size for $inPath\n";
  $spaceRequired    = `ds32.exe /D /L0 /A /S /C 32768 $inPath`;
  $spaceRequired    = int($spaceRequired / 1024);
  $spaceRequired   += 1;
  return($spaceRequired);
}

