#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

$cvsroot = substr(<STDIN>, 0, -1);
$flag_debug = 0;
$flag_tagcmd = 0;
$repository = substr(<STDIN>, 0, -1);
$repository_tag = '';

@mailto=();
@changed_files = ();
@added_files = ();
@removed_files = ();
@log_lines = ();
@outlist = ();

$STATE_NONE    = 0;
$STATE_CHANGED = 1;
$STATE_ADDED   = 2;
$STATE_REMOVED = 3;
$STATE_LOG     = 4;

&process_args;

if ($flag_debug ){
    print STDERR "----------------------------------------------\n";
    print STDERR "LOGINFO:\n";
    print STDERR " pwd:" . `pwd` . "\n";
    print STDERR " Args @ARGV\n";
    print STDERR " CVSROOT: $cvsroot\n";                      
    print STDERR " Repository: $repository\n";                      
    print STDERR " mailto: @mailto\n";
    print STDERR "----------------------------------------------\n";
}

if ($flag_tagcmd) {
    &process_tag_command;
} else {
    &get_loginfo;
    &process_cvs_info;
}

if( $flag_debug){
    print STDERR "----------------------------------------------\n";
    print STDERR @outlist;
    print STDERR "----------------------------------------------\n";
}

&mail_notification;

0;

sub process_args {
    while (@ARGV) {
        $arg = shift @ARGV;

        if ($arg eq '-d') {
            $flag_debug = 1;
            print STDERR "Debug turned on...\n";
        } elsif ($arg eq '-t') {
            $flag_tagcmd = 1;
            last;               # Keep the rest in ARGV; they're handled later.
        } else {
            push(@mailto, $arg);
        }
    }
    if( $repository eq '' ){
        open( REP, "<CVS/Repository");
        $repository = <REP>;
        chop($repository);
        close(REP);
    }
    $repository =~ s:^$cvsroot/::;
    
    if (!$flag_tagcmd) {
        if( open( REP, "<CVS/Tag") ) {
            $repository_tag = <REP>;
            chop($repository_tag);
            close(REP);
        }
    }
}

sub get_loginfo {

    if( $flag_debug){
        print STDERR "----------------------------------------------\n";
    }

    # Iterate over the body of the message collecting information.
    #
    $state = $STATE_NONE;
    while (<STDIN>) {
        chop;                   # Drop the newline

        if (/^__BONSAI__SEPARATOR__MAGIC__$/) {
            last;
        }

        if( $flag_debug){
            print STDERR "$_\n";
        }

        if (/^In directory/) {
            next;
        }

        if (/^Modified Files/) { $state = $STATE_CHANGED; next; }
        if (/^Added Files/)    { $state = $STATE_ADDED;   next; }
        if (/^Removed Files/)  { $state = $STATE_REMOVED; next; }
        if (/^Log Message/)    { $state = $STATE_LOG;     next; }

        s/^[ \t\n]+//;          # delete leading whitespace
        s/[ \t\n]+$//;          # delete trailing whitespace
        
        if ($state == $STATE_CHANGED) { push(@changed_files, split); }
        if ($state == $STATE_ADDED)   { push(@added_files,   split); }
        if ($state == $STATE_REMOVED) { push(@removed_files, split); }
        if ($state == $STATE_LOG)     { push(@log_lines,     $_); }
    }
    
    if( $flag_debug){
        print STDERR "----------------------------------------------\n"
                     . "changed files: @changed_files\n"
                     . "added files: @added_files\n"
                     . "removed files: @removed_files\n";
        print STDERR "----------------------------------------------\n";
    }

}

sub process_cvs_info {
    local($d,$fn,$rev,$mod_time,$sticky,$tag,$stat,@d,$rcsfile);
    $time = time;
    while( <STDIN> ){ # Parsing the Entries file, actually.
        chop;
        $fn = "";
        ($d,$fn,$rev,$mod_time,$sticky,$tag) = split(/\//);
        $stat = 'C';
        for $i (@changed_files, "BEATME.NOW", @added_files ) {
            if( $i eq "BEATME.NOW" ){ $stat = 'A'; }
            if($i eq $fn ){
                $rcsfile = "$cvsroot/$repository/$fn,v";
                if( ! -r $rcsfile ){
                    $rcsfile = "$cvsroot/$repository/Attic/$fn,v";
                }
                open(LOG, "/tools/ns/bin/rlog -N -r$rev $rcsfile |") 
                        || print STDERR "dolog.pl: Couldn't run rlog\n";
                $username = "nobody";
                $lines_added = 0;
                $lines_removed = 0;
                while(<LOG>){
                    if (/^date:.* author: ([^;]*);.*/) {
                        $username = $1;
                        if (/lines: \+([0-9]*) -([0-9]*)/) {
                            $lines_added = $1;
                            $lines_removed = $2;
                        }
                    }
                }
                close( LOG );
                push(@outlist, ("$stat|$time|$username|$cvsroot|$repository|$fn|$rev|$sticky|$tag|+$lines_added|-$lines_removed\n"));
            }
        }
    }

    for $i (@removed_files) {
        push( @outlist, ("R|$time|$username|$cvsroot|$repository|$i|||$repository_tag\n"));
    }

    push (@outlist, "LOGCOMMENT\n");
    push (@outlist, join("\n",@log_lines));
    push (@outlist, "\n:ENDLOGCOMMENT\n");
}


sub process_tag_command {
    local($str,$part,$time);
    $time = time;
    $str = "Tag|$cvsroot|$time";
    while (@ARGV) {
        $part = shift @ARGV;
        $str .= "|" . $part;
    }
    push (@outlist, ("$str\n"));
}
        


sub do_commitinfo {
}



sub mail_notification {

    my $filename = "data/temp.$$";
    open(OUT, ">$filename") || die "Couldn't open output file.";
    print OUT "dummy: line preteending to be mail headers\n\n";
    print OUT @outlist, "\n";
    close OUT;
    system "./addcheckin.tcl $filename";
    # rm $filename;
}
