# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Contains some global variables and routines used throughout bugzilla.

use diagnostics;
use strict;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
    $zz = @main::chooseone;
    $zz = @main::default_column_list;
    $zz = $main::defaultqueryname;
    $zz = @main::dontchange;
    $zz = %main::keywordsbyname;
    $zz = @main::legal_bug_status;
    $zz = @main::legal_components;
    $zz = @main::legal_keywords;
    $zz = @main::legal_opsys;
    $zz = @main::legal_platform;
    $zz = @main::legal_priority;
    $zz = @main::legal_product;
    $zz = @main::legal_severity;
    $zz = @main::legal_target_milestone;
    $zz = @main::legal_versions;
    $zz = @main::milestoneurl;
    $zz = @main::prodmaxvotes;
}

#
# Here are the --LOCAL-- variables defined in 'localconfig' that we'll use
# here
# 

my $db_host = "localhost";
my $db_name = "bugs";
my $db_user = "bugs";
my $db_pass = "";

do 'localconfig';

use Mysql;

use Date::Format;               # For time2str().
use Date::Parse;               # For str2time().
# use Carp;                       # for confess

# Contains the version string for the current running Bugzilla.
$::param{'version'} = '2.9';

$::dontchange = "--do_not_change--";
$::chooseone = "--Choose_one:--";
$::defaultqueryname = "(Default query)";
$::unconfirmedstate = "UNCONFIRMED";
$::dbwritesallowed = 1;

sub ConnectToDatabase {
    my ($useshadow) = (@_);
    if (!defined $::db) {
        my $name = $db_name;
        if ($useshadow && Param("shadowdb")) {
            $name = Param("shadowdb");
            $::dbwritesallowed = 0;
        }
	$::db = Mysql->Connect($db_host, $name, $db_user, $db_pass)
            || die "Can't connect to database server.";
    }
}

sub ReconnectToShadowDatabase {
    if (Param("shadowdb")) {
        SendSQL("USE " . Param("shadowdb"));
        $::dbwritesallowed = 0;
    }
}

my $shadowchanges = 0;
sub SyncAnyPendingShadowChanges {
    if ($shadowchanges) {
        system("./syncshadowdb &");
        $shadowchanges = 0;
    }
}
    

my $dosqllog = (-e "data/sqllog") && (-w "data/sqllog");

sub SqlLog {
    if ($dosqllog) {
        my ($str) = (@_);
        open(SQLLOGFID, ">>data/sqllog") || die "Can't write to data/sqllog";
        if (flock(SQLLOGFID,2)) { # 2 is magic 'exclusive lock' const.
            print SQLLOGFID time2str("%D %H:%M:%S $$", time()) . ": $str\n";
        }
        flock(SQLLOGFID,8);     # '8' is magic 'unlock' const.
        close SQLLOGFID;
    }
}
    



sub SendSQL {
    my ($str, $dontshadow) = (@_);
    my $iswrite =  ($str =~ /^(INSERT|REPLACE|UPDATE|DELETE)/i);
    if ($iswrite && !$::dbwritesallowed) {
        die "Evil code attempted to write stuff to the shadow database.";
    }
    if ($str =~ /^LOCK TABLES/i && $str !~ /shadowlog/) {
        $str =~ s/^LOCK TABLES/LOCK TABLES shadowlog WRITE, /i;
    }
    SqlLog($str);
    $::currentquery = $::db->query($str)
	|| die "$str: " . $::db->errmsg;
    SqlLog("Done");
    if (!$dontshadow && $iswrite && Param("shadowdb")) {
        my $q = SqlQuote($str);
        my $insertid;
        if ($str =~ /^(INSERT|REPLACE)/i) {
            SendSQL("SELECT LAST_INSERT_ID()");
            $insertid = FetchOneColumn();
        }
        SendSQL("INSERT INTO shadowlog (command) VALUES ($q)", 1);
        if ($insertid) {
            SendSQL("SET LAST_INSERT_ID = $insertid");
        }
        $shadowchanges++;
    }
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
    $comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
    $comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
    if ($comment =~ /^\s*$/) {  # Nothin' but whitespace.
        return;
    }

    my $whoid = DBNameToIdAndCheck($who);

    SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) " .
            "VALUES($bugid, $whoid, now(), " . SqlQuote($comment) . ")");

    SendSQL("UPDATE bugs SET delta_ts = now() WHERE bug_id = $bugid");
}

sub GetFieldID {
    my ($f) = (@_);
    SendSQL("SELECT fieldid FROM fielddefs WHERE name = " . SqlQuote($f));
    my $fieldid = FetchOneColumn();
    if (!$fieldid) {
        my $q = SqlQuote($f);
        SendSQL("REPLACE INTO fielddefs (name, description) VALUES ($q, $q)");
        SendSQL("SELECT LAST_INSERT_ID()");
        $fieldid = FetchOneColumn();
    }
    return $fieldid;
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
    SendSQL("select value, program from components order by value");
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
    SendSQL("select product, description, votesperuser, disallownew$mpart from products");
    $::anyvotesallowed = 0;
    while (@line = FetchSQLData()) {
        my ($p, $d, $votesperuser, $dis, $u) = (@line);
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
        $::prodmaxvotes{$p} = $votesperuser;
        if ($votesperuser > 0) {
            $::anyvotesallowed = 1;
        }
    }
            

    my $cols = LearnAboutColumns("bugs");
    
    @::log_columns = @{$cols->{"-list-"}};
    foreach my $i ("bug_id", "creation_ts", "delta_ts", "lastdiffed") {
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
    print FID GenerateCode('%::prodmaxvotes');
    print FID GenerateCode('$::anyvotesallowed');

    if ($dotargetmilestone) {
        my $last = Param("nummilestones");
        my $i;
        for ($i=1 ; $i<=$last ; $i++) {
            push(@::legal_target_milestone, "M$i");
        }
        print FID GenerateCode('@::legal_target_milestone');
        print FID GenerateCode('%::milestoneurl');
    }

    SendSQL("SELECT id, name FROM keyworddefs ORDER BY name");
    while (MoreSQLData()) {
        my ($id, $name) = FetchSQLData();
        $::keywordsbyname{$name} = $id;
        push(@::legal_keywords, $name);
    }
    print FID GenerateCode('@::legal_keywords');
    print FID GenerateCode('%::keywordsbyname');

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
            die "Can't generate file data/versioncache";
        }
    }
}


sub InsertNewUser {
    my ($username, $realname) = (@_);
    my $password = "";
    for (my $i=0 ; $i<8 ; $i++) {
        $password .= substr("abcdefghijklmnopqrstuvwxyz", int(rand(26)), 1);
    }
    SendSQL("select bit, userregexp from groups where userregexp != ''");
    my $groupset = "0";
    while (MoreSQLData()) {
        my @row = FetchSQLData();
	# Modified -Joe Robins, 2/17/00
	# Making this case insensitive, since usernames are email addresses,
	# and could be any case.
        if ($username =~ m/$row[1]/i) {
            $groupset .= "+ $row[0]"; # Silly hack to let MySQL do the math,
                                      # not Perl, since we're dealing with 64
                                      # bit ints here, and I don't *think* Perl
                                      # does that.
        }
    }
            
    $username = SqlQuote($username);
    $realname = SqlQuote($realname);
    SendSQL("insert into profiles (login_name, realname, password, cryptpassword, groupset) values ($username, $realname, '$password', encrypt('$password'), $groupset)");
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
        InsertNewUser($name, "");
        $result = DBname_to_id($name);
        if ($result > 0) {
            return $result;
        }
        print "Yikes; couldn't create user $name.  Please report problem to " .
            Param("maintainer") ."\n";
    } else {
        print "The name <TT>$name</TT> is not a valid username.  Either you\n";
        print "misspelled it, or the person has not registered for a\n";
        print "Bugzilla account.\n";
        print "<P>Please hit the <B>Back</B> button and try again.\n";
    }
    exit(0);
}

# This routine quoteUrls contains inspirations from the HTML::FromText CPAN
# module by Gareth Rees <garethr@cre.canon.co.uk>.  It has been heavily hacked,
# all that is really recognizable from the original is bits of the regular
# expressions.

sub quoteUrls {
    my ($knownattachments, $text) = (@_);
    return $text unless $text;

    my $base = Param('urlbase');

    my $protocol = join '|',
    qw(afs cid ftp gopher http https mid news nntp prospero telnet wais);

    my %options = ( metachars => 1, @_ );

    my $count = 0;

    # Now, quote any "#" characters so they won't confuse stuff later
    $text =~ s/#/%#/g;

    # Next, find anything that looks like a URL or an email address and
    # pull them out the the text, replacing them with a "##<digits>##
    # marker, and writing them into an array.  All this confusion is
    # necessary so that we don't match on something we've already replaced,
    # which can happen if you do multiple s///g operations.

    my @things;
    while ($text =~ s%((mailto:)?([\w\.\-\+\=]+\@[\w\-]+(?:\.[\w\-]+)+)\b|
                    (\b((?:$protocol):[^ \t\n<>"]+[\w/])))%"##$count##"%exo) {
        my $item = $&;

        $item = value_quote($item);

        if ($item !~ m/^$protocol:/o && $item !~ /^mailto:/) {
            # We must have grabbed this one because it looks like an email
            # address.
            $item = qq{<A HREF="mailto:$item">$item</A>};
        } else {
            $item = qq{<A HREF="$item">$item</A>};
        }

        $things[$count++] = $item;
    }
    while ($text =~ s/\bbug(\s|%\#)*(\d+)/"##$count##"/ei) {
        my $item = $&;
        my $num = $2;
        $item = value_quote($item); # Not really necessary, since we know
                                    # there's no special chars in it.
        $item = qq{<A HREF="show_bug.cgi?id=$num">$item</A>};
        $things[$count++] = $item;
    }
    while ($text =~ s/\*\*\* This bug has been marked as a duplicate of (\d+) \*\*\*/"##$count##"/ei) {
        my $item = $&;
        my $num = $1;
        $item =~ s@\d+@<A HREF="show_bug.cgi?id=$num">$num</A>@;
        $things[$count++] = $item;
    }
    while ($text =~ s/Created an attachment \(id=(\d+)\)/"##$count##"/e) {
        my $item = $&;
        my $num = $1;
        if ($knownattachments->{$num}) {
            $item = qq{<A HREF="showattachment.cgi?attach_id=$num">$item</A>};
        }
        $things[$count++] = $item;
    }

    $text = value_quote($text);
    $text =~ s/\&#010;/\n/g;

    # Stuff everything back from the array.
    for (my $i=0 ; $i<$count ; $i++) {
        $text =~ s/##$i##/$things[$i]/e;
    }

    # And undo the quoting of "#" characters.
    $text =~ s/%#/#/g;

    return $text;
}

sub GetLongDescriptionAsText {
    my ($id, $start, $end) = (@_);
    my $result = "";
    my $count = 0;
    my ($query) = ("SELECT profiles.login_name, longdescs.bug_when, " .
                   "       longdescs.thetext " .
                   "FROM longdescs, profiles " .
                   "WHERE profiles.userid = longdescs.who " .
                   "      AND longdescs.bug_id = $id ");

    if ($start && $start =~ /[1-9]/) {
        # If the start is all zeros, then don't do this (because we want to
        # not emit a leading "Additional Comments" line in that case.)
        $query .= "AND longdescs.bug_when > '$start'";
        $count = 1;
    }
    if ($end) {
        $query .= "AND longdescs.bug_when <= '$end'";
    }

    $query .= "ORDER BY longdescs.bug_when";
    SendSQL($query);
    while (MoreSQLData()) {
        my ($who, $when, $text) = (FetchSQLData());
        if ($count) {
            $result .= "\n\n------- Additional Comments From $who  " .
                time2str("%Y-%m-%d %H:%M", str2time($when)) . " -------\n";
        }
        $result .= $text;
        $count++;
    }

    return $result;
}


sub GetLongDescriptionAsHTML {
    my ($id, $start, $end) = (@_);
    my $result = "";
    my $count = 0;
    my %knownattachments;
    SendSQL("SELECT attach_id FROM attachments WHERE bug_id = $id");
    while (MoreSQLData()) {
        $knownattachments{FetchOneColumn()} = 1;
    }

    my ($query) = ("SELECT profiles.login_name, longdescs.bug_when, " .
                   "       longdescs.thetext " .
                   "FROM longdescs, profiles " .
                   "WHERE profiles.userid = longdescs.who " .
                   "      AND longdescs.bug_id = $id ");

    if ($start && $start =~ /[1-9]/) {
        # If the start is all zeros, then don't do this (because we want to
        # not emit a leading "Additional Comments" line in that case.)
        $query .= "AND longdescs.bug_when > '$start'";
        $count = 1;
    }
    if ($end) {
        $query .= "AND longdescs.bug_when <= '$end'";
    }

    $query .= "ORDER BY longdescs.bug_when";
    SendSQL($query);
    while (MoreSQLData()) {
        my ($who, $when, $text) = (FetchSQLData());
        if ($count) {
            $result .= "<BR><BR><I>------- Additional Comments From " .
                qq{<A HREF="mailto:$who">$who</A> } .
                    time2str("%Y-%m-%d %H:%M", str2time($when)) .
                        " -------</I><BR>\n";
        }
        $result .= "<PRE>" . quoteUrls(\%knownattachments, $text) . "</PRE>\n";
        $count++;
    }

    return $result;
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

sub GroupExists {
    my ($groupname) = (@_);
    ConnectToDatabase();
    SendSQL("select count(*) from groups where name=" . SqlQuote($groupname));
    my $count = FetchOneColumn();
    return $count;
}

# Determines if the given bug_status string represents an "Opened" bug.  This
# routine ought to be paramaterizable somehow, as people tend to introduce
# new states into Bugzilla.

sub IsOpenedState {
    my ($state) = (@_);
    if ($state =~ /^(NEW|REOPENED|ASSIGNED)$/ || $state eq $::unconfirmedstate) {
        return 1;
    }
    return 0;
}


sub RemoveVotes {
    my ($id, $who, $reason) = (@_);
    ConnectToDatabase();
    my $whopart = "";
    if ($who) {
        $whopart = " AND votes.who = $who";
    }
    SendSQL("SELECT profiles.login_name, votes.count " .
            "FROM votes, profiles " .
            "WHERE votes.bug_id = $id " .
            "AND profiles.userid = votes.who" .
            $whopart);
    my @list;
    while (MoreSQLData()) {
        my ($name, $count) = (FetchSQLData());
        push(@list, [$name, $count]);
    }
    if (0 < @list) {
        foreach my $ref (@list) {
            my ($name, $count) = (@$ref);
            if (open(SENDMAIL, "|/usr/lib/sendmail -t")) {
                my %substs;
                $substs{"to"} = $name;
                $substs{"bugid"} = $id;
                $substs{"reason"} = $reason;
                $substs{"count"} = $count;
                my $msg = PerformSubsts(Param("voteremovedmail"),
                                        \%substs);
                print SENDMAIL $msg;
                close SENDMAIL;
            }
        }
        SendSQL("DELETE FROM votes WHERE bug_id = $id" . $whopart);
        SendSQL("SELECT SUM(count) FROM votes WHERE bug_id = $id");
        my $v = FetchOneColumn();
        $v ||= 0;
        SendSQL("UPDATE bugs SET votes = $v, delta_ts = delta_ts " .
                "WHERE bug_id = $id");
    }
}


sub Param {
    my ($value) = (@_);
    if (defined $::param{$value}) {
        return $::param{$value};
    }

    # See if it is a dynamically-determined param (can't be changed by user).
    if ($value eq "commandmenu") {
        return GetCommandMenu();
    }
    if ($value eq "settingsmenu") {
        return GetSettingsMenu();
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
