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
# Input: .ini file
#             - ini file to extract localizable string from
#
#   ie: perl createp.pl config.ini
#

# Make sure there is at least 1 argument
if($#ARGV < 0)
{
  die "usage: $0 <.ini file>

       .ini file : input ini file
       \n";
}

$inFile = $ARGV[0];

$currentSection;

# Get the name of the file replacing the .it extension with a .properties extension
@inFileSplit = split(/\./,$inFile);
$outFile        = $inFileSplit[0];
$outFile       .= ".properties";

# Open the input file
open(fpIn, $inFile) || die "\ncould not open $ARGV[0]: $!\n";

# Open the output file
open(fpOut, ">$outFile") || die "\nCould not open $outFile: $!\n";

print "\n Making $outFile...\n";

# While loop to read each line from input file
while($line = <fpIn>)
{
  if($line =~ /^\[/)
  {
    chop($line);
    @splitMe = split(/\[/,$line);
    $currentSection = $splitMe[1];
    chop($currentSection);
  }
  elsif($line =~ /\*\*\* LOCALIZE ME BABY \*\*\*/i)
  {
    $line = <fpIn>;
    chop($line);

    print fpOut "$currentSection - $line\n";
  }
}

print " done!\n";

# end of script
exit(0);

