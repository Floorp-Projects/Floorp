#!/usr/bin/perl -w
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
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 


use strict;
use Cwd;

if (($#ARGV >= 0) && (-d $ARGV[0])) {
     chdir($ARGV[0]);
} else {
    my $bonsaidir = $0;
    $bonsaidir =~ s:/[^/]*$::;      # Remove last word, and slash before it.
    if ($bonsaidir eq "") {
        $bonsaidir = ".";
    }
    chdir($bonsaidir);
}

my $filename = "data/temp.$$";
unlink($filename);

die "Cannot Open data file (cwd: ". cwd() . "; $filename): $!\n"
     unless (open(FILE, "> $filename"));

while (<STDIN>) {
     print FILE $_;
}
close(FILE);
chmod(0666, $filename);
system("./addcheckin.pl",  $filename);

unlink($filename);

exit;
