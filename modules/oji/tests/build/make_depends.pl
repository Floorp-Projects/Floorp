# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 
($#ARGV > 0)  || die "Usage: make_depends.pl <OBJ_dir> <CC_command_macro_name> <CPP_files_list>\n";

open(OUT, ">depend.mak") || die "Can't create depend.mak file: $!\n";

print OUT "# This file is generated automatically. Don't edit it !\n\n";
$ext = "o";
if ($^O =~ /Win32/) { $ext = "obj"; }
for ($i=2; $i<$#ARGV+1; $i++) {
	if ($ARGV[$i] =~ /(.*?).cpp/) {
		$name = $1;
		#print "Creating rule for file $name.cpp\n";
		print OUT "$ARGV[0]/$name.$ext: $name.cpp\n\t\$\($ARGV[1]\) \$\? \>\> cmd.mak\n\n";
	}
}
close(OUT);
