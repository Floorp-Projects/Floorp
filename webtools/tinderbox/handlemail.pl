#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

# Figure out which directory tinderbox is in by looking at argv[0].  Unless
# there is a command line argument; if there is, just use that.

$tinderboxdir = $0;
$tinderboxdir =~ s:/[^/]*$::;      # Remove last word, and slash before it.
if ($tinderboxdir eq "") {
    $tinderboxdir = ".";
}

if (@ARGV > 0) {
    $tinderboxdir = $ARGV[0];
}

print "tinderbox = $tinderboxdir\n"; 

chdir $tinderboxdir || die "Couldn't chdir to $tinderboxdir"; 


open(DF, ">data/tbx.$$") || die "could not open data/tbx.$$";
while(<STDIN>){
    print DF $_;
}
close(DF);

$err = system("./processbuild.pl", "data/tbx.$$");

if( $err ) {
    die "processbuild.pl returned an error\n";
}

