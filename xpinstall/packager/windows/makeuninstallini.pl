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
# Input: .it file
#             - which is a .ini template
#
#        version
#             - version to display on the blue background
#
#        UserAgent
#             - user agent to use in the windows registry.  should be the same as the one
#               built into the browser (ie "6.0b2 (en)")
#
#   ie: perl makecfgini.pl uninstall.it 6.0.0.1999120608 "6.0b2 (en)"
#
#

print "argc: $#ARGV\n";

if($#ARGV < 2)
{
  die "usage: $0 <.it file> <version> <UserAgent>

       .it file      : input ini template file

       version       : version to be shown in setup.  Typically the same version
                       as show in mozilla.exe.

       UserAgent     : user agent to use in the windows registry.  should be the same as the one
                       built into the browser (ie \"6.0b2 (en)\")
       \n";
}

$inItFile         = $ARGV[0];
$inVersion        = $ARGV[1];
$inUserAgent      = $ARGV[2];
$uaShort          = ParseUserAgentShort($inUserAgent);

# Get the name of the file replacing the .it extension with a .ini extension
@inItFileSplit    = split(/\./,$inItFile);
$outIniFile       = $inItFileSplit[0];
$outIniFile      .= ".ini";

# Open the input file
open(fpInIt, $inItFile) || die "\ncould not open $ARGV[0]: $!\n";

# Open the output file
open(fpOutIni, ">$outIniFile") || die "\nCould not open $outIniFile: $!\n";

print "\n Making $outIniFile...\n";

# While loop to read each line from input file
while($line = <fpInIt>)
{
  # For each line read, search and replace $Version$ with the version passed in
  $line =~ s/\$Version\$/$inVersion/i;
  $line =~ s/\$UserAgentShort\$/$uaShort/i;
  $line =~ s/\$UserAgent\$/$inUserAgent/i;
  print fpOutIni $line;
}

print " done!\n";

# end of script
exit(0);

sub ParseUserAgentShort()
{
  my($aUserAgent) = @_;
  my($aUserAgentShort);

  @spaceSplit = split(/ /, $aUserAgent);
  if($#spaceSplit >= 0)
  {
    $aUserAgentShort = $spaceSplit[0];
  }

  return($aUserAgentShort);
}

