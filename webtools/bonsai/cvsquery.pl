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

require 'globals.pl';
require 'get_line.pl';

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub cvsquery_pl_sillyness {
    my $zz;
    $zz = $::CI_BRANCH;
    $zz = $::CI_CHANGE;
    $zz = $::CI_DATE;
    $zz = $::CI_STICKY;
    $zz = $::TreeID;
    $zz = $::query_debug;
    $zz = $::query_filetype;
    $zz = $::versioninfo;
};

#
# Constants
#
$::CI_CHANGE=0;
$::CI_DATE=1;
$::CI_WHO=2;
$::CI_REPOSITORY=3;
$::CI_DIR=4;
$::CI_FILE=5;
$::CI_REV=6;
$::CI_STICKY=7;
$::CI_BRANCH=8;
$::CI_LINES_ADDED=9;
$::CI_LINES_REMOVED=10;
$::CI_LOG=11;

my $NOT_LOCAL = 1;
my $IS_LOCAL = 2;

chomp($::CVS_ROOT) if defined($::CVS_ROOT);
if (!defined($::CVS_ROOT) || $::CVS_ROOT eq "" ){
    $::CVS_ROOT = pickDefaultRepository();
}

#global variables

$::lines_added = 0;
$::lines_removed = 0;

$::modules = {};

my $CVS_MODULES="$::CVS_ROOT/CVSROOT/modules";

open( MOD, "<$CVS_MODULES") || die "can't open ${CVS_MODULES}";
&parse_modules;
close( MOD );

1;

#
# Actually do the query
#
sub query_checkins {
    my (%mod_map) = @_;
    my ($ci,$result,$lastlog,$rev,$begin_tag,$end_tag);
    my $have_mod_map;
    my @bind_values;

    $::query_module = 'all' unless defined $::query_module;
    if( $::query_module ne 'all' && $::query_module ne 'allrepositories' && @::query_dirs == 0 ){
        $have_mod_map = 1;
        %mod_map = &get_module_map( $::query_module );
    }
    else {
        $have_mod_map = 0;
        %mod_map = ();
    }

    for my $i (@::query_dirs ){
        $i =~ s:^/::;           # Strip leading slash.
        $i =~ s:/$::;           # Strip trailing slash.

        if( !$have_mod_map ){
            %mod_map = ();
            $have_mod_map = 1;
        }
        $mod_map{$i} = $NOT_LOCAL;
    }

    $begin_tag = "";
    $end_tag = "";

    if (defined($::query_begin_tag) && $::query_begin_tag ne '') {
        $begin_tag = load_tag($::query_begin_tag);
    }

    if (defined($::query_end_tag) &&  $::query_end_tag ne '') {
        $end_tag = load_tag($::query_end_tag);
    }


    $result = [];

    &ConnectToDatabase();

    my $qstring = "SELECT type, UNIX_TIMESTAMP(ci_when), people.who, " .
        "repositories.repository, dirs.dir, files.file, revision, " .
        "stickytag, branches.branch, addedlines, removedlines, " .
        "descs.description FROM checkins,people,repositories,dirs,files," .
        "branches,descs WHERE people.id=whoid AND " .
        "repositories.id=repositoryid AND dirs.id=dirid AND " .
        "files.id=fileid AND branches.id=branchid AND descs.id=descid";

    if( $::query_module ne 'allrepositories' ){
        $qstring .= " AND repositories.repository = ?";
        push(@bind_values, $::CVS_ROOT);
    }

    if ($::query_date_min) {
        $qstring .= " AND ci_when >= ?";
        push(@bind_values, &formatSqlTime($::query_date_min));
    }
    if ($::query_date_max) {
        $qstring .= " AND ci_when <= ?";
        push(@bind_values, &formatSqlTime($::query_date_max));
    }
    if ($::query_branch_head) {
        $qstring .= " AND branches.branch = ''";
    } elsif ($::query_branch ne '') {
        if ($::query_branchtype eq 'regexp') {
            $qstring .= " AND branches.branch REGEXP ?";
            push(@bind_values, $::query_branch);
        } elsif ($::query_branchtype eq 'notregexp') {
            if ($::query_branch eq 'HEAD') {
                $qstring .= " AND branches.branch != ''";
            } else {
                $qstring .= " and not (branches.branch REGEXP ?)";
                push(@bind_values, $::query_branch);
            }
        } else {
            $qstring .=
                " AND (branches.branch = ? OR branches.branch = ?)";
            push(@bind_values, $::query_branch);
            push(@bind_values, "T$::query_branch");
        }
    }

    if (0 < @::query_dirs) {
        my @list;
        foreach my $i (@::query_dirs) {
            push(@list, "dirs.dir LIKE ?");
            push(@bind_values, "$i%");
        }
        $qstring .= "AND (" . join(" OR ", @list) . ")";
    }

    if (defined $::query_file && $::query_file ne '') {
        $::query_filetype ||= "exact";
        if ($::query_filetype eq 'regexp') {
            $qstring .= " AND files.file REGEXP ?";
        } elsif ($::query_filetype eq 'notregexp') {
            $qstring .= " AND NOT (files.file REGEXP ?)";
        } else {
            $qstring .= " AND files.file = ?";
        }
        push(@bind_values, $::query_file);
    }
    if (defined $::query_who && $::query_who ne '') {
        $::query_whotype ||= "exact";
        if ($::query_whotype eq 'regexp') {
            $qstring .= " AND people.who REGEXP ?";
        }
        elsif ($::query_whotype eq 'notregexp') {
            $qstring .= " AND NOT (people.who REGEXP ?)";

        } else {
            $qstring .= " AND people.who = ?";
        }
        push(@bind_values, $::query_who);
    }

    if (defined($::query_logexpr) && $::query_logexpr ne '') {
        $qstring .= " AND descs.description regexp ?";
        push(@bind_values, $::query_logexpr);
    }
    
    if ($::query_debug) {
        print "<pre wrap> Query: " . &html_quote($qstring) . "\n";
        print "With values:\n";
        foreach my $v (@bind_values) {
            print "\t" . &html_quote($v) . "\n";
        }
        print "\nTreeID is $::TreeID\n";
        if ($have_mod_map) {
            print "Dump of module map:\n";
            foreach my $k (sort(keys %mod_map)) {
                print value_quote("$k => $mod_map{$k}") . "\n";
            }
            print "\n\nDump of parsed module file:\n";
            foreach my $k(sort(keys %$::modules)) {
                print value_quote("$k => " .
                                  join(",", @{$::modules->{$k}})) . "\n";
            }
        }
        print "</pre>\n";
    }

    &SendSQL($qstring, @bind_values);

    $lastlog = 0;
    my @row;
    while (@row = FetchSQLData()) {
#print "<pre>";
        $ci = [];
        for (my $i=0 ; $i<=$::CI_LOG ; $i++) {
            $ci->[$i] = $row[$i];
#print "$row[$i] ";
        }
#print "</pre>";


        my $key = "$ci->[$::CI_DIR]/$ci->[$::CI_FILE]";
        if (IsHidden("$ci->[$::CI_REPOSITORY]/$key")) {
            next;
        }

        next if ($key =~ m@^CVSROOT/@);

        if( $have_mod_map &&
           !&in_module(\%mod_map, $ci->[$::CI_DIR], $ci->[$::CI_FILE] ) ){
            next;
        }

        if( $begin_tag) {
            $rev = $begin_tag->{$key};
            print "<BR>$key begintag is $rev<BR>\n";
            if ($rev == "" || rev_is_after($ci->[$::CI_REV], $rev)) {
                next;
            }
        }

        if( $end_tag) {
            $rev = $end_tag->{$key};
            print "<BR>$key endtag is $rev<BR>\n";
            if ($rev == "" || rev_is_after($rev, $ci->[$::CI_REV])) {
                next;
            }
        }

        if (defined($::query_logexpr) && 
            $::query_logexpr ne '' &&
            !($ci->[$::CI_LOG] =~ /$::query_logexpr/i) ){
            next;
        }

        push( @$result, $ci );
    }

    for $ci (@{$result}) {
        $::lines_added += $ci->[$::CI_LINES_ADDED];
        $::lines_removed += $ci->[$::CI_LINES_REMOVED];
        $::versioninfo .= "$ci->[$::CI_WHO]|$ci->[$::CI_DIR]|$ci->[$::CI_FILE]|$ci->[$::CI_REV],";
    }
    return $result;
}

sub load_tag {
    my ($tagname) = @_;

    my $tagfile;
    my $cvssuffix;
    my $s;
    my @line;
    my $time;
    my $cmd;
    my $dir;

    $cvssuffix = $::CVS_ROOT;
    $cvssuffix =~ s/\//_/g;

    $s = $tagname;

    $s =~ s/ /\%20/g;
    $s =~ s/\%/\%25/g;
    $s =~ s/\//\%2f/g;
    $s =~ s/\?/\%3f/g;
    $s =~ s/\*/\%2a/g;

    $tagfile = "data/taginfo/$cvssuffix/$s";

    open(TAG, "<$tagfile") || die "Unknown tag $tagname";
    my $result = {};


print "<br>parsing tag $tagname</br>\n";
    while ( <TAG> ) {
        chop;
        @line = split(/\|/);
        $time = shift @line;
        $cmd = shift @line;
        if ($cmd != "add") {
            # We ought to be able to cope with these... XXX
            next;
        }
        $dir = shift @line;
        $dir =~ s@^$::CVS_ROOT/@@;
        $dir =~ s:^\./::;
        
        while (@line) {
            my $file = shift @line;
            $file = "$dir/$file";
            my $version = shift @line;
            $result->{$file} = $version;
print "<br>Added ($file,$version) for tag $tagname<br>\n";
        }
    }

    return $result;
}



sub rev_is_after {
    my $r1 = shift @_;
    my $r2 = shift @_;

    my @a = split /:/, $r1;
    my @b = split /:/, $r2;

    if (@b > @a) {
        return 1;
    }

    if (@b < @a) {
        return 0;
    }

    for (my $i=0 ; $i<@a ; $i++) {
        if ($a[$i] > $b[$i]) {return 1;}
        if ($a[$i] < $b[$i]) {return 0;}
    }
    return 0;
}
    


sub in_module {
    my ($mod_map, $dirname, $filename ) = @_;
    my ( @path );
    my ( $i, $fp, $local );

    #
    #quick check if it is already in there.
    #
    if( $$mod_map{$dirname} ){
        return 1;
    }

    @path = split(/\//, $dirname);

    $fp = '';

    for( $i = 0; $i < @path; $i++){

        $fp .= ($fp ne '' ? '/' : '') . $path[$i];

        if( $local = $$mod_map{$fp} ){
            if( $local == $IS_LOCAL ){
                if( $i == (@path-1) ){
                    return 1;
                }
            }
            else {
                # Add directories to the map as we encounter them so we go
                #  faster
                 if (!exists($$mod_map{$dirname}) || 
                     $$mod_map{$dirname} == 0) {
                   $$mod_map{$dirname} = $IS_LOCAL;
                }
                return 1;
            }
        }
    }
    if( $$mod_map{ $fp . '/' .  $filename} ) {
        return 1;
    }
    else {
        return 0;
    }
}


sub get_module_map {
    my($name) = @_;
    my(%mod_map);
    &build_map( $name, \%mod_map );
    return %mod_map;
}


sub parse_modules {
    my $l;
    while( $l = &get_line ){
        my ($mod_name, $flag, @params) = split(/[ \t]+/,$l);

        if ( $#params eq -1 ) {
            @params = $flag;
            $flag = "";
        }
	elsif( $flag eq '-d' ){
            my $dummy;
	    ($mod_name, $dummy, $dummy, @params) = split(/[ \t]+/,$l);
	}
        elsif( $flag ne '-a' ){
            next;
        }
        $::modules->{$mod_name} = [@params];
    }
}


sub build_map {
    my ($name,$mod_map) = @_;
    my ($bFound, $local);

    $local = $NOT_LOCAL;
    $bFound = 0;

    for my $i ( @{$::modules->{$name}} ){
        $bFound = 1;
        if( $i eq '-l' ){
            $local = $IS_LOCAL;
        }
        elsif($i eq $name || !build_map($i, $mod_map )){
            $mod_map->{$i} = $local;   
        }
    }
    return $bFound;
}

