#!/usr/bin/perl --
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


# Figure out which directory bonsai is in by looking at argv[0]

$bonsaidir = $0;
$bonsaidir =~ s:/[^/]*$::;      # Remove last word, and slash before it.
if ($bonsaidir eq "") {
    $bonsaidir = ".";
}

chdir $bonsaidir || die "Couldn't chdir to $bonsaidir";
require "utils.pl";

if( $ARGV[0] eq '' ){
    $::CVS_ROOT = pickDefaultRepository();
}
else {
    $::CVS_ROOT = $ARGV[0];
}

$CVS_REPOS_SUFIX = $::CVS_ROOT;
$CVS_REPOS_SUFIX =~ s:/:_:g;


$CHECKIN_DATA_FILE = "$bonsaidir/data/checkinlog${CVS_REPOS_SUFIX}";
$CHECKIN_INDEX_FILE = "$bonsaidir/data/index${CVS_REPOS_SUFIX}";

&build_index;
&print_keys;

sub build_index {
    open(CI, "<$CHECKIN_DATA_FILE") || die "could not open checkin data file\n";

    $file_pos = 0;
    $lastlog = 0;
    $last_date = 0;
    $index = {};
    $now = time;
    while( <CI> ){
        if( /^LOGCOMMENT/ ){
            $done = 0;
            $file_pos += length;
            while( !$done && ($line = <CI>) ){
                if( $line =~ /^:ENDLOGCOMMENT/ ){
                    $done = 1;
                }
                $file_pos += length($line);
            }
        }
        else {
            #print $_ . "\n";
            $ci = [split(/\|/)];
            $d = $ci->[1];

            if( $d < $now && $d > ($last_date + 60*60*4) ){
                $index->{$d} = $file_pos;
                $last_date = $d;
            }
            $file_pos += length;
        }
    }
    close( CI );
}

sub print_keys {
    Lock();
    open(INDEX , ">$CHECKIN_INDEX_FILE");
    for $i (sort {$b <=> $a} keys %{$index}) {
        print INDEX "$index->{$i}|$i\n";
    }
    close(INDEX);
    Unlock();
}
