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
# Input: .jst file        - which is a .js template
#        default version  - a julian date in the form of:
#                           major.minor.release.yydoy
#                           ie: 5.0.0.99256
#
#        ie: perl makejs.pl core.jst 5.0.0.99256
#

# Make sure there are at least two arguments
if($#ARGV < 1)
{
  die "usage: $0 <.jst file> <default version>

       .jst file       : .js template input file
       default version : default julian base version number to use
       \n";
}

$inJstFile        = $ARGV[0];
$inVersion        = $ARGV[1];

# Get the name of the file replacing the .jst extension with a .js extension
@inJstFileSplit   = split(/\./,$inJstFile);
$outJsFile        = $inJstFileSplit[0];
$outJsFile       .= ".js";

# Open the input .jst file
open(fpInJst, $inJstFile) || die "\ncould not open $ARGV[0]: $!\n";

# Open the output .js file
open(fpOutJs, ">$outJsFile") || die "\nCould not open $outJsFile: $!\n";


# While loop to read each line from input file
while($line = <fpInJst>)
{
  # For each line read, search and replace $Version$ with the version passed in
  $line =~ s/\$Version\$/$inVersion/i;
  print fpOutJs $line;
}

