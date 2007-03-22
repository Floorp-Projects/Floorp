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
# Sun Microsystems, Inc.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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
($#ARGV > 2) || die "Usage: make_mak.pl <LIST_FILE> <CC_makro_name> <TOP_DIR> <OBJ_DIR> <original_INCLUDES>\n";

#Read CPP's to be recompiled 

if (open (MAK, $ARGV[0])) {
	$empty = 0;	
	undef $/;
	@file_list=split(/\n/, <MAK>);
	close MAK;
} else {
	$empty=1;
}


#Write new makefile

open (MAK, ">".$ARGV[0]) || die "Can't open file $ARGV[0]: $!\n";

print MAK "# This file is generated automatically. Don't edit it !\n\n";
print MAK "TOP_DIR=$ARGV[2]\n";
print MAK "OBJ_DIR=$ARGV[3]\n";
print MAK "INCLUDES=$ARGV[4]\n";
print MAK "OS=$^O\n";
if ($^O !~ /Win32/) {
  print MAK "CC=".(($ENV{"CC"})?$ENV{"CC"}:"gcc")."\n";
  print MAK "C_FLAGS=".(($ENV{"USE_SUN_WS"} eq "1")?"-c -g -KPIC":"-c -fPIC -fno-rtti")."\n"; 
}

$ext = "mk";
if ($^O =~ /Win32/) { $ext = "mak"; }
print MAK "include $ARGV[2]/build/compile.$ext\n";
print MAK "CPP=";
if (!$empty) { print MAK " \\\n"; } else { print MAK "\n"; }
for ($i=0; $file = $file_list[$i]; $i++) {
	chomp $file;
	if ($i < $#file_list) {
		print MAK "\t$file \\\n";
	} else {
		print MAK "\t$file \n";
	}
}

print MAK "all:";
if (!$empty) { print MAK "\n\t\$\($ARGV[1]\) \$\(CPP\)\n"; }
close MAK;
