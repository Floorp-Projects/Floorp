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
#        Path to staging area
#             - path on where the seamonkey built bits are staged to
#        URL path
#             - path to where the .xpi files are staged.  can be
#               either ftp:// or http://
#
#   ie: perl makecfgini.pl z:\exposed\windows\32bit\en\5.0 ftp://sweetlou/products/client/seamonkey/windows/32bit/x86/1999-09-13-10-M10
#

# Make sure there are at least two arguments
if($#ARGV < 2)
{
  die "usage: $0 <.it file> <staging path> <URL path>

       .it file     : input ini template file
       staging path : path to where the components are staged at
       URL path     : URL path to where the .xpi files will be staged at
       \n";
}

$inItFile         = $ARGV[0];
$inStagePath      = $ARGV[1];
$inURLPath        = $ARGV[2];

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
  # For each line read, search and replace $InstallSize$ with the calculated size
  if($line =~ /\$InstallSize\$/i)
  {
    $installSize        = 0;
    $installSizeSystem  = 0;

    # split read line by ":" deliminator
    @colonSplit = split(/:/, $line);
    if($#colonSplit >= 0)
    {
      $componentName    = $colonSplit[1];
      chop($componentName);

      $installSize      = OutputInstallSize("$inStagePath\\$componentName");
    }

    # Read the next line to calculate for the "Install Size System="
    if($line = <fpInIt>)
    {
      if($line =~ /\$InstallSizeSystem\$/i)
      {
        $installSizeSystem = OutputInstallSizeSystem($line, "$inStagePath\\$componentName");
      }
    }

    $installSize -= $installSizeSystem;
    print fpOutIni "Install Size=$installSize\n";
    print fpOutIni "Install Size System=$installSizeSystem\n";
  }
  else
  {
    # For each line read, search and replace $InstallSizeSystem$ with the calulated size
    $line =~ s/\$URLPath\$/$inURLPath/i;
    print fpOutIni $line;
  }

}

print " done!\n";

# end of script
exit(0);

sub OutputInstallSize()
{
  my($inPath) = @_;
  my($installSize);

  print "   calulating size for $inPath\n";
  $installSize    = `ds32.exe /L0 /A /S /C 32768 $inPath`;
  $installSize    = int($installSize / 1000);
  $installSize   += 1;
  return($installSize);
}

sub OutputInstallSizeSystem()
{
  my($inLine, $inPath) = @_;
  my($installSizeSystem) = 0;

  # split read line by ":" deliminator
  @colonSplit = split(/:/, $inLine);
  if($#colonSplit >= 0)
  {
    # split line by "," deliminator
    @commaSplit = split(/\,/, $colonSplit[1]);
    if($#commaSplit >= 0)
    {
      foreach(@commaSplit)
      {
        # calculate the size of component installed using ds32.exe in Kbytes
        print "   calulating size for $inPath\\$_\n";
        $installSizeSystem   += `ds32.exe /L0 /A /S /C 32768 $inPath\\$_`;
        $installSizeSystem    = int($installSize / 1000);
        $installSizeSystem   += 1;
      }
    }
  }
  return($installSizeSystem);
}

