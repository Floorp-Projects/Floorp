#!/usr/bin/perl
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

#($#ARGV >= 0) || die "Usage: perl mfgen.pl <dir>\n";

$list = `ls *.cpp`;

@filelist = split (/\n/, $list);

print "TOP_DIR = ../../..\nOBJ_DIR = ../obj\nCPP =";
foreach $file (@filelist) {
    if ($file =~ /^.*?\.cpp$/) {
	print " \\\n";
	print "\t$file";	
    }
}

print "\n\nOBJS ="; 
foreach $file (@filelist) {
    if ($file =~ /^(.*?)\.cpp$/) {
	print " \\\n";
	print "\t\$\(OBJ_DIR\)\/$1.o";	
    }
}
print "\n\nINCLUDES = -I..\/include \ninclude ..\/..\/..\/build\/rules.mk \nall: compile\n";
