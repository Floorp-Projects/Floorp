#!/usr/bin/perl -w
# 
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
#   Samir Gehani <sgehani@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#
# This perl script builds the xpi, config.ini, and js files.
#

# Make sure there are at least four arguments
if($#ARGV < 3)
{
  die "usage: $0 <default version> <URL path> <staging path> <dist install path>

       default version   : julian based date version
                           ie: 5.0.0.99257

       URL path          : URL path to where the .xpi files will be staged at.
                           Either ftp:// or http:// can be used.  Nothing will be
                           copied to there by this script.  It is used for config.ini.

       staging path      : full path to where the components are staged at

       dist install path : full path to target dist area

       \n";
}

$inDefaultVersion = $ARGV[0];
$inURLPath        = $ARGV[1];
$inStagePath      = $ARGV[2];
$inDistPath       = $ARGV[3];

# Check for existance of staging path
if(!(-e "$inStagePath"))
{
  die "invalid path: $inStagePath\n";
}

# Make sure inDistPath exists
if(!(-e "$inDistPath"))
{
  system("mkdir $inDistPath");
}

# Make all xpi files
MakeXpiFile("xpcom");
MakeXpiFile("browser");
MakeXpiFile("psm");
MakeXpiFile("mail");
MakeXpiFile("chatzilla");
MakeXpiFile("talkback");
MakeXpiFile("deflenus");
MakeXpiFile("langenus");
MakeXpiFile("regus");
MakeXpiFile("venkman");
MakeXpiFile("inspector");
MakeXpiFile("spellcheck");

# Make the config.ini file
MakeConfigFile();

print " done!\n";

# end of script
exit(0);

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inDefaultVersion $inStagePath $inDistPath $inURLPath") != 0)
  {
    exit(1);
  }
}

sub MakeXpiFile
{
  my($componentName) = @_;

  # Make .xpi file
  if(system("perl makexpi.pl $componentName $inStagePath $inDistPath $inDefaultVersion") != 0)
  {
    exit(1);
  }
}

