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
#        default version        - a date in the form of:
#                                 major.minor.release.yyyymmdyhr
#                                 ie: 5.0.0.1999120910
#        component staging path - path to where the components are staged at
#
#        ie: perl makejs.pl xpcom.jst 5.0.0.99256
#

# Make sure there are at least two arguments
if($#ARGV < 3)
{
  die "usage: $0 <.jst file> <default version> <staging path>

       component              : name of the component
       js folder              : folder where the .jst file lives and where the 
                                .js file is to be created
       default version        : default julian base version number to use in the
                                form of: major.minor.release.yydoy
                                ie: 5.0.0.99256
       component staging path : path to where this component is staged at
                                ie: z:\\stage\\windows\\32bit\\en\\5.0\\xpcom
       \n";
}

$Component        = $ARGV[0];
$jsDir            = $ARGV[1];
$inVersion        = $ARGV[2];
$inStagePath      = $ARGV[3];

# get environment vars
$userAgent        = $ENV{WIZ_userAgent};
$userAgentShort   = $ENV{WIZ_userAgentShort};
$xpinstallVersion = $ENV{WIZ_xpinstallVersion};
$nameCompany      = $ENV{WIZ_nameCompany};
$nameProduct      = $ENV{WIZ_nameProduct};
$nameProductNoVersion = $ENV{WIZ_nameProductNoVersion};
$nameProductInternal = $ENV{WIZ_nameProductInternal};
$fileMainExe      = $ENV{WIZ_fileMainExe};
$fileUninstall    = $ENV{WIZ_fileUninstall};

# Get the names of the .jst, .js and .template files
$inJstFile        = "$jsDir\\$Component.jst";
$outJsFile       .= "$jsDir\\$Component.js";
$outTempFile     .= "$jsDir\\$Component.template";
$foundLongFiles   = 0;

print "Creating: $outJsFile\n";

if(!(-e "$inJstFile"))
{
  print "ERROR: $inJstFile does not exit.\n";
  exit(1);
}
#print "copy \"$ENV{MOZ_SRC}\\mozilla\\xpinstall\\packager\\common\\share.t\" $outTempFile\n";
system("copy \"$ENV{MOZ_SRC}\\mozilla\\xpinstall\\packager\\common\\share.t\" $outTempFile");
system("type $inJstFile >> $outTempFile");

# Open the input .template file
open(fpInTemplate, $outTempFile) || die "\ncould not open $outTempFile: $!\n";

# Open the output .js file
open(fpOutJs, ">$outJsFile") || die "\nCould not open $outJsFile: $!\n";

# While loop to read each line from input file
while($line = <fpInTemplate>)
{
  if($line =~ /\$SpaceRequired\$/i) # For each line read, search and replace $SpaceRequired$ with the calculated size
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
  else
  {
    $line =~ s/\$Version\$/$inVersion/i;
    $line =~ s/\$UserAgent\$/$userAgent/i;
    $line =~ s/\$UserAgentShort\$/$userAgentShort/i;
    $line =~ s/\$XPInstallVersion\$/$xpinstallVersion/i;
    $line =~ s/\$CompanyName\$/$nameCompany/i;
    $line =~ s/\$ProductName\$/$nameProduct/i;
    $line =~ s/\$ProductNameNoVersion\$/$nameProductNoVersion/i;
    $line =~ s/\$ProductNameInternal\$/$nameProductInternal/i;
    $line =~ s/\$MainExeFile\$/$fileMainExe/i;
    $line =~ s/\$UninstallFile\$/$fileUninstall/i;
  }

  print fpOutJs $line;
}

close(fpInTemplate);
close(fpOutJs);
exit(0);

sub GetSpaceRequired()
{
  my($inPath) = @_;
  my($spaceRequired);

  print "   calculating size for $inPath\n";
  $spaceRequired    = `\"$ENV{MOZ_TOOLS}\\bin\\ds32.exe\" /D /L0 /A /S /C 32768 $inPath`;
  $spaceRequired    = int($spaceRequired / 1024);
  $spaceRequired   += 1;
  return($spaceRequired);
}

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

