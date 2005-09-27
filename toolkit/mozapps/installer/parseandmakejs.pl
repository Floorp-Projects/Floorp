#!/usr/bin/perl
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Samir Gehani <sgehani@netscape.com>
#   Benjamin Smedberg <benjamin@smedbergs.us>
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

use File::Basename;

die ("Usage: $0 <cfgFile> <jstFile> <stageDir>") if (scalar(@ARGV) != 3);

my ($cfgFile, $jstFile, $stageDir) = @ARGV;

# ensure that CFGParser.pm is in @INC, since we might not be called from
# mozilla/toolkit/mozapps/installer
my $top_path = $0;
$top_path =~ s/\\/\//g if $win32;
$top_path = dirname($top_path);

push(@INC, $top_path);

require CFGParser;
CFGParser::ParseInstallerCfg($cfgFile);

$ENV{WIZ_userAgent} = "$ENV{WIZ_userAgentShort} ($ENV{WIZ_versionLanguage})";

system("perl $top_path/makejs.pl $jstFile $ENV{WIZ_versionProduct} $stageDir") == 0 ||
  die("Could not create ini file.");
