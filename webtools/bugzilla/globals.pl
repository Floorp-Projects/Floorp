# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Contains some global variables and routines used throughout bugzilla.

use diagnostics;
use strict;
use Mysql;

use Date::Format;               # For time2str().
# use Carp;                       # for confess

# Contains the version string for the current running Bugzilla.
$::param{'version'} = '2.3';

$::dontchange = "--do_not_change--";
$::chooseone = "--Choose_one:--";

sub ConnectToDatabase {
    if (!defined $::db) {
	$::db = Mysql->Connect("localhost", "bugs", "bugs", "")
            || die "Can't connect to database server.";
    }
}

sub SendSQL {
    my ($str) = (@_);
    $::currentquery = $::db->query($str)
	|| die "$str: $::db_errstr";
}

sub MoreSQLData {
    if (defined @::fetchahead) {
	return 1;
    }
    if (@::fetchahead = $::currentquery->fetchrow()) {
	return 1;
    }
    return 0;
}

sub FetchSQLData {
    if (defined @::fetchahead) {
	my @result = @::fetchahead;
	undef @::fetchahead;
	return @result;
    }
    return $::currentquery->fetchrow();
}


sub FetchOneColumn {
    my @row = FetchSQLData();
    return $row[0];
}

    

@::default_column_list = ("severity", "priority", "platform", "owner",
                          "status", "resolution", "summary");

sub AppendComment {
    my ($bugid,$who,$comment) = (@_);
    open(DEBUG, ">/tmp/debug");
    print DEBUG "A $comment";
    $comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
    print DEBUG "B $comment";
    $comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
    print DEBUG "C $comment";
    close DEBUG;
    if ($comment =~ /^\s*$/) {  # Nothin' but whitespace.
        return;
    }
    SendSQL("select long_desc from bugs where bug_id = $bugid");
    
    my $desc = FetchOneColumn();
    my $now = time2str("%D %H:%M", time());
    $desc .= "\n\n------- Additional Comments From $who  $now -------\n";
    $desc .= $comment;
    SendSQL("update bugs set long_desc=" . SqlQuote($desc) .
            " where bug_id=$bugid");
}

sub lsearch {
    my ($list,$item) = (@_);
    my $count = 0;
    foreach my $i (@$list) {
        if ($i eq $item) {
            return $count;
        }
        $count++;
    }
    return -1;
}

sub Product_element {
    my ($prod,$onchange) = (@_);
    return make_popup("product", keys %::versions, $prod, 1, $onchange);
}

sub Component_element {
    my ($comp,$prod,$onchange) = (@_);
    my $componentlist;
    if (! defined $::components{$prod}) {
        $componentlist = [];
    } else {
        $componentlist = $::components{$prod};
    }
    my $defcomponent;
    if ($comp ne "" && lsearch($componentlist, $comp) >= 0) {
        $defcomponent = $comp;
    } else {
        $defcomponent = $componentlist->[0];
    }
    return make_popup("component", $componentlist, $defcomponent, 1, "");
}

sub Version_element {
    my ($vers, $prod, $onchange) = (@_);
    my $versionlist;
    if (!defined $::versions{$prod}) {
        $versionlist = [];
    } else {
        $versionlist = $::versions{$prod};
    }
    my $defversion = $versionlist->[0];
    if (lsearch($versionlist,$vers) >= 0) {
        $defversion = $vers;
    }
    return make_popup("version", $versionlist, $defversion, 1, $onchange);
}
        


# Generate a string which, when later interpreted by the Perl compiler, will
# be the same as the given string.

sub PerlQuote {
    my ($str) = (@_);
    return SqlQuote($str);
    
# The below was my first attempt, but I think just using SqlQuote makes more 
# sense...
#     $result = "'";
#     $length = length($str);
#     for (my $i=0 ; $i<$length ; $i++) {
#         my $c = substr($str, $i, 1);
#         if ($c eq "'" || $c eq '\\') {
#             $result .= '\\';
#         }
#         $result .= $c;
#     }
#     $result .= "'";
#     return $result;
}


# Given the name of a global variable, generate Perl code that, if later
# executed, would restore the variable to its current value.

sub GenerateCode {
    my ($name) = (@_);
    my $result = $name . " = ";
    if ($name =~ /^\$/) {
        my $value = eval($name);
        if (ref($value) eq "ARRAY") {
            $result .= "[" . GenerateArrayCode($value) . "]";
        } else {
            $result .= PerlQuote(eval($name));
        }
    } elsif ($name =~ /^@/) {
        my @value = eval($name);
        $result .= "(" . GenerateArrayCode(\@value) . ")";
    } elsif ($name =~ '%') {
        $result = "";
        foreach my $k (sort { uc($a) cmp uc($b)} eval("keys $name")) {
            $result .= GenerateCode("\$" . substr($name, 1) .
                                    "{'" . $k . "'}");
        }
        return $result;
    } else {
        die "Can't do $name -- unacceptable variable type.";
    }
    $result .= ";\n";
    return $result;
}

sub GenerateArrayCode {
    my ($ref) = (@_);
    my @list;
    foreach my $i (@$ref) {
        push @list, PerlQuote($i);
    }
    return join(',', @list);
}



sub GenerateVersionTable {
    ConnectToDatabase();
    SendSQL("select value, program from versions order by value");
    my @line;
    my %varray;
    my %carray;
    while (@line = FetchSQLData()) {
        my ($v,$p1) = (@line);
        if (!defined $::versions{$p1}) {
            $::versions{$p1} = [];
        }
        push @{$::versions{$p1}}, $v;
        $varray{$v} = 1;
    }
    SendSQL("select value, program from components");
    while (@line = FetchSQLData()) {
        my ($c,$p) = (@line);
        if (!defined $::components{$p}) {
            $::components{$p} = [];
        }
        my $ref = $::components{$p};
        push @$ref, $c;
        $carray{$c} = 1;
    }

    my $dotargetmilestone = Param("usetargetmilestone");

    my $mpart = $dotargetmilestone ? ", milestoneurl" : "";
    SendSQL("select product, description, disallownew$mpart from products");
    while (@line = FetchSQLData()) {
        my ($p, $d, $dis, $u) = (@line);
        $::proddesc{$p} = $d;
        if ($dis) {
            # Special hack.  Stomp on the description and make it "0" if we're
            # not supposed to allow new bugs against this product.  This is
            # checked for in enter_bug.cgi.
            $::proddesc{$p} = "0";
        }
        if ($dotargetmilestone) {
            $::milestoneurl{$p} = $u;
        }
    }
            

    my $cols = LearnAboutColumns("bugs");
    
    @::log_columns = @{$cols->{"-list-"}};
    foreach my $i ("bug_id", "creation_ts", "delta_ts", "long_desc") {
        my $w = lsearch(\@::log_columns, $i);
        if ($w >= 0) {
            splice(@::log_columns, $w, 1);
        }
    }
    @::log_columns = (sort(@::log_columns));

    @::legal_priority = SplitEnumType($cols->{"priority,type"});
    @::legal_severity = SplitEnumType($cols->{"bug_severity,type"});
    @::legal_platform = SplitEnumType($cols->{"rep_platform,type"});
    @::legal_opsys = SplitEnumType($cols->{"op_sys,type"});
    @::legal_bug_status = SplitEnumType($cols->{"bug_status,type"});
    @::legal_resolution = SplitEnumType($cols->{"resolution,type"});
    @::legal_resolution_no_dup = @::legal_resolution;
    my $w = lsearch(\@::legal_resolution_no_dup, "DUPLICATE");
    if ($w >= 0) {
        splice(@::legal_resolution_no_dup, $w, 1);
    }

    my @list = sort { uc($a) cmp uc($b)} keys(%::versions);
    @::legal_product = @list;
    mkdir("data", 0777);
    chmod 0777, "data";
    my $tmpname = "data/versioncache.$$";
    open(FID, ">$tmpname") || die "Can't create $tmpname";

    print FID GenerateCode('@::log_columns');
    print FID GenerateCode('%::versions');

    foreach my $i (@list) {
        if (!defined $::components{$i}) {
            $::components{$i} = "";
        }
    }
    @::legal_versions = sort {uc($a) cmp uc($b)} keys(%varray);
    print FID GenerateCode('@::legal_versions');
    print FID GenerateCode('%::components');
    @::legal_components = sort {uc($a) cmp uc($b)} keys(%carray);
    print FID GenerateCode('@::legal_components');
    foreach my $i('product', 'priority', 'severity', 'platform', 'opsys',
                  'bug_status', 'resolution', 'resolution_no_dup') {
        print FID GenerateCode('@::legal_' . $i);
    }
    print FID GenerateCode('%::proddesc');

    if ($dotargetmilestone) {
        my $last = Param("nummilestones");
        my $i;
        for ($i=1 ; $i<=$last ; $i++) {
            push(@::legal_target_milestone, "M$i");
        }
        print FID GenerateCode('@::legal_target_milestone');
        print FID GenerateCode('%::milestoneurl');
    }
    print FID "1;\n";
    close FID;
    rename $tmpname, "data/versioncache" || die "Can't rename $tmpname to versioncache";
    chmod 0666, "data/versioncache";
}



# Returns the modification time of a file.

sub ModTime {
    my ($filename) = (@_);
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks)
        = stat($filename);
    return $mtime;
}



# This proc must be called before using legal_product or the versions array.

sub GetVersionTable {
    my $mtime = ModTime("data/versioncache");
    if (!defined $mtime || $mtime eq "") {
        $mtime = 0;
    }
    if (time() - $mtime > 3600) {
        GenerateVersionTable();
    }
    require 'data/versioncache';
    if (!defined %::versions) {
        GenerateVersionTable();
        do 'data/versioncache';

        if (!defined %::versions) {
            die "Can't generate version info; tell terry.";
        }
    }
}


sub InsertNewUser {
    my ($username) = (@_);
    my $password = "";
    for (my $i=0 ; $i<8 ; $i++) {
        $password .= substr("abcdefghijklmnopqrstuvwxyz", int(rand(26)), 1);
    }
    SendSQL("select bit, userregexp from groups where userregexp != ''");
    my $groupset = "0";
    while (MoreSQLData()) {
        my @row = FetchSQLData();
        if ($username =~ m/$row[1]/) {
            $groupset .= "+ $row[0]"; # Silly hack to let MySQL do the math,
                                      # not Perl, since we're dealing with 64
                                      # bit ints here, and I don't *think* Perl
                                      # does that.
        }
    }
            
    SendSQL("insert into profiles (login_name, password, cryptpassword, groupset) values (@{[SqlQuote($username)]}, '$password', encrypt('$password'), $groupset)");
    return $password;
}


sub DBID_to_name {
    my ($id) = (@_);
    if (!defined $::cachedNameArray{$id}) {
        SendSQL("select login_name from profiles where userid = $id");
        my $r = FetchOneColumn();
        if ($r eq "") {
            $r = "__UNKNOWN__";
        }
        $::cachedNameArray{$id} = $r;
    }
    return $::cachedNameArray{$id};
}

sub DBname_to_id {
    my ($name) = (@_);
    SendSQL("select userid from profiles where login_name = @{[SqlQuote($name)]}");
    my $r = FetchOneColumn();
    if (!defined $r || $r eq "") {
        return 0;
    }
    return $r;
}


sub DBNameToIdAndCheck {
    my ($name, $forceok) = (@_);
    my $result = DBname_to_id($name);
    if ($result > 0) {
        return $result;
    }
    if ($forceok) {
        InsertNewUser($name);
        $result = DBname_to_id($name);
        if ($result > 0) {
            return $result;
        }
        print "Yikes; couldn't create user $name.  Please report problem to " .
            Param("maintainer") ."\n";
    } else {
        print "The name <TT>$name</TT> is not a valid username.  Please hit\n";
        print "the <B>Back</B> button and try again.\n";
    }
    exit(0);
}

sub GetLongDescription {
    my ($id) = (@_);
    SendSQL("select long_desc from bugs where bug_id = $id");
    return FetchOneColumn();
}


sub ShowCcList {
    my ($num) = (@_);
    my @ccids;
    my @row;
    SendSQL("select who from cc where bug_id = $num");
    while (@row = FetchSQLData()) {
        push(@ccids, $row[0]);
    }
    my @result = ();
    foreach my $i (@ccids) {
        push @result, DBID_to_name($i);
    }

    return join(',', @result);
}



# Fills in a hashtable with info about the columns for the given table in the
# database.  The hashtable has the following entries:
#   -list-  the list of column names
#   <name>,type  the type for the given name

sub LearnAboutColumns {
    my ($table) = (@_);
    my %a;
    SendSQL("show columns from $table");
    my @list = ();
    my @row;
    while (@row = FetchSQLData()) {
        my ($name,$type) = (@row);
        $a{"$name,type"} = $type;
        push @list, $name;
    }
    $a{"-list-"} = \@list;
    return \%a;
}



# If the above returned a enum type, take that type and parse it into the
# list of values.  Assumes that enums don't ever contain an apostrophe!

sub SplitEnumType {
    my ($str) = (@_);
    my @result = ();
    if ($str =~ /^enum\((.*)\)$/) {
        my $guts = $1 . ",";
        while ($guts =~ /^\'([^\']*)\',(.*)$/) {
            push @result, $1;
            $guts = $2;
	}
    }
    return @result;
}


# This routine is largely copied from Mysql.pm.

sub SqlQuote {
    my ($str) = (@_);
#     if (!defined $str) {
#         confess("Undefined passed to SqlQuote");
#     }
    $str =~ s/([\\\'])/\\$1/g;
    $str =~ s/\0/\\0/g;
    return "'$str'";
}



sub UserInGroup {
    my ($groupname) = (@_);
    if ($::usergroupset eq "0") {
        return 0;
    }
    ConnectToDatabase();
    SendSQL("select (bit & $::usergroupset) != 0 from groups where name = " . SqlQuote($groupname));
    my $bit = FetchOneColumn();
    if ($bit) {
        return 1;
    }
    return 0;
}


sub Param {
    my ($value) = (@_);
    if (defined $::param{$value}) {
        return $::param{$value};
    }
    # Um, maybe we haven't sourced in the params at all yet.
    if (stat("data/params")) {
        # Write down and restore the version # here.  That way, we get around
        # anyone who maliciously tries to tweak the version number by editing
        # the params file.  Not to mention that in 2.0, there was a bug that
        # wrote the version number out to the params file...
        my $v = $::param{'version'};
        require "data/params";
        $::param{'version'} = $v;
    }
    if (defined $::param{$value}) {
        return $::param{$value};
    }
    # Well, that didn't help.  Maybe it's a new param, and the user
    # hasn't defined anything for it.  Try and load a default value
    # for it.
    require "defparams.pl";
    WriteParams();
    if (defined $::param{$value}) {
        return $::param{$value};
    }
    # We're pimped.
    die "Can't find param named $value";
}

    
sub PerformSubsts {
    my ($str, $substs) = (@_);
    $str =~ s/%([a-z]*)%/(defined $substs->{$1} ? $substs->{$1} : Param($1))/eg;
    return $str;
}


# Trim whitespace from front and back.

sub trim {
    ($_) = (@_);
    s/^\s+//g;
    s/\s+$//g;
    return $_;
}

1;
