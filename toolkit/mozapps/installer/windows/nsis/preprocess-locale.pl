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
# The Original Code is Mozilla installer build scripts.
#
# The Initial Developer of the Original Code is Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert Strong <robert.bugzilla@gmail.com>
#  Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

my $mozsrcdir = "$ARGV[0]";    # dir that contains the toolkit source
my $appLocaleDir = "$ARGV[1]";
my $AB_CD = "$ARGV[2]";
my $configDir = "$ARGV[3]";
my $nsisVer = "v6";
# Set the language ID to 0 to make this locale the default locale. An actual
# ID will need to be used to create a multi-language installer (e.g. for CD
# distributions, etc.).
my $langID = 0;
my $nsisCP = "-";
my $fontName = "-";
my $fontSize = "-";
my $RTL = "-";
my $line;
my $lnum;

# Read the codepage for the locale and the optional font name, font size, and
# whether the locale is right to left from locales.nsi.
my $inFile = "$mozsrcdir/toolkit/mozapps/installer/windows/nsis/locales.nsi";
open(locales, "<$inFile");
if (!-e $inFile) {
  die "Error $inFile does not exist!";
}

$lnum = 1;
while( $line = <locales> ) {
  $line =~ s/[\r\n]*//g;    # remove \r and \n
  if ($line =~ m|^!define $AB_CD\_rtl|) {
    $RTL = "RTL";
  }
  $lnum++;
}
close locales;

# Create the main NSIS language file with just the codepage, font, and
# RTL information
open(outfile, ">$configDir/nlf.in");
print outfile "# Header, don't edit\r\nNLF $nsisVer\r\n# Start editing here\r\n";
print outfile "# Language ID\r\n$langID\r\n";
print outfile "# Font and size - dash (-) means default\r\n$fontName\r\n$fontSize\r\n";
print outfile "# Codepage - dash (-) means ANSI code page\r\n$nsisCP\r\n";
print outfile "# RTL - anything else than RTL means LTR\r\n$RTL\r\n";
close outfile;
&cpConvert("nlf.in", "baseLocale.nlf");


# Create the main NSIS language file
$inFile = "$appLocaleDir/override.properties";
if (!-e $inFile) {
  die "Error $inFile does not exist!";
}
open(infile, "<$inFile");
open(outfile, ">$configDir/override.properties");
$lnum = 1;
while( $line = <infile> ) {
  $line =~ s/[\r\n]*//g;    # remove \r and \n
  next if ($line =~ m|^#|);
  my @values = split('=', $line, 2);
  next if (@values[0] eq undef) || (@values[1] eq undef);
  my $value = @values[1];
  $value =~ s/^\s+//; # trim whitespace from the beginning of the string
  $value =~ s/\s+$//; # trim whitespace from the end of the string
  $value =~ s/^"(.*)"$/$1/g; # remove " at the beginning and end of the value
  $value =~ s/(")/\$\\$1/g;  # prefix " with $\
  $value =~ s/(\\[rnt])/\$$1/g; # prefix all occurences of \r, \n and \t with $
  print outfile "LangString  ^@values[0] $langID \"$value\"\r\n";
  $lnum++;
}
close infile;
close outfile;
&cpConvert("override.properties", "overrideLocale.nsh");


# Create the main Modern User Interface language file
$inFile = "$appLocaleDir/mui.properties";
if (!-e $inFile) {
  die "Error $inFile does not exist!";
}
open(infile, "<$inFile");
open(outfile, ">$configDir/mui.properties");
print outfile ";NSIS Modern User Interface - Language File\r\n";
print outfile ";Compatible with Modern UI 1.68\r\n";
print outfile ";Language: baseLocale ($langID)\r\n";
print outfile "!insertmacro MOZ_MUI_LANGUAGEFILE_BEGIN \"baseLocale\"\r\n";
print outfile "!define MUI_LANGNAME \"baseLocale\"\r\n";

$lnum = 1;
while( $line = <infile> ) {
  $line =~ s/[\r\n]*//g;    # remove \r and \n
  next if ($line =~ m|^#|) || ($line eq "");
  my @values = split('=', $line, 2);
  next if (@values[0] eq undef) || (@values[1] eq undef);
  my $value = @values[1];
  $value =~ s/(")/\$\\$1/g;  # prefix " with $\
  $value =~ s/(\\n)/\\r$1/g; # insert \r before each occurence of \n
  $value =~ s/(\\r)\\r/$1/g; # replace all ocurrences of \r\r with \r
  $value =~ s/(\\[rnt])/\$$1/g; # prefix all occurences of \r, \n and \t with $
  print outfile "!define @values[0] \"$value\"\r\n";
  $lnum++;
}
print outfile "!insertmacro MOZ_MUI_LANGUAGEFILE_END\r\n";
close infile;
close outfile;
&cpConvert("mui.properties", "baseLocale.nsh");


# Create the custom language file for our custom strings
$inFile = "$appLocaleDir/custom.properties";
if (!-e $inFile) {
  die "Error $inFile does not exist!";
}
open(infile, "<$inFile");
open(outfile, ">$configDir/custom.properties");

$lnum = 1;
while( $line = <infile> ) {
  $line =~ s/[\r\n]*//g;    # remove \r and \n
  next if ($line =~ m|^#|) || ($line eq "");
  my @values = split('=', $line, 2);
  next if (@values[0] eq undef) || (@values[1] eq undef);
  my $string = @values[1];
  $string =~ s/"/\$\\"/g;       # replace " with $\"
  $string =~ s/(\\n)/\\r$1/g;   # insert \r before each occurence of \n
  $string =~ s/(\\r)\\r/$1/g;   # replace all ocurrences of \r\r with \r
  $string =~ s/(\\[rnt])/\$$1/g; # prefix all occurences of \r, \n and \t with $
  print outfile "LangString @values[0] $langID \"$string\"\r\n";
  $lnum++;
}
close infile;
close outfile;
&cpConvert("custom.properties", "customLocale.nsh");


# Converts a file's codepage
sub cpConvert
{
  my $srcFile = $_[0];
  my $targetFile = $_[1];
  my $prependBOM = "cat $mozsrcdir/toolkit/mozapps/installer/windows/nsis/utf16-le-bom.bin -";
  print "iconv -f UTF-8 -t UTF-16LE $configDir/$srcFile | $prependBOM > $configDir/$targetFile\n";
  system("iconv -f UTF-8 -t UTF-16LE $configDir/$srcFile | $prependBOM > $configDir/$targetFile") &&
    die "Error converting codepage to UTF-16LE for $configDir/$srcFile";
  unlink <$configDir/$srcFile>;
}
