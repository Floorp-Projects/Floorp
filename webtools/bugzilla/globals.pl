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
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Joel Peshkin <bugreport@peshkin.net> 

# Contains some global variables and routines used throughout bugzilla.

use strict;

use Bugzilla::Constants;
use Bugzilla::Util;
# Bring ChmodDataFile in until this is all moved to the module
use Bugzilla::Config qw(:DEFAULT ChmodDataFile);

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
    $zz = @main::SqlStateStack;
    $zz = $main::contenttypes;
    $zz = @main::default_column_list;
    $zz = $main::defaultqueryname;
    $zz = @main::enterable_products;
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
    $zz = %main::proddesc;
    $zz = @main::prodmaxvotes;
    $zz = $main::template;
    $zz = $main::userid;
    $zz = $main::vars;
}

#
# Here are the --LOCAL-- variables defined in 'localconfig' that we'll use
# here
# 

# XXX - Move this to Bugzilla::Config once code which uses these has moved out
# of globals.pl
do 'localconfig';

use DBI;

use Date::Format;               # For time2str().
use Date::Parse;               # For str2time().
#use Carp;                       # for confess
use RelationSet;

# Use standard Perl libraries for cross-platform file/directory manipulation.
use File::Spec;
    
# Some environment variables are not taint safe
delete @::ENV{'PATH', 'IFS', 'CDPATH', 'ENV', 'BASH_ENV'};

# Cwd.pm in perl 5.6.1 gives a warning if $::ENV{'PATH'} isn't defined
# Set this to '' so that we don't get warnings cluttering the logs on every
# system call
$::ENV{'PATH'} = '';

# Ignore SIGTERM and SIGPIPE - this prevents DB corruption. If the user closes
# their browser window while a script is running, the webserver sends these
# signals, and we don't want to die half way through a write.
$::SIG{TERM} = 'IGNORE';
$::SIG{PIPE} = 'IGNORE';

$::defaultqueryname = "(Default query)"; # This string not exposed in UI
$::unconfirmedstate = "UNCONFIRMED";
$::dbwritesallowed = 1;

#sub die_with_dignity {
#    my ($err_msg) = @_;
#    print $err_msg;
#    confess($err_msg);
#}
#$::SIG{__DIE__} = \&die_with_dignity;

sub ConnectToDatabase {
    my ($useshadow) = (@_);
    $::dbwritesallowed = !$useshadow;
    $useshadow = ($useshadow && Param("shadowdb") &&
                  Param("queryagainstshadowdb"));
    my $useshadow_dbh = ($useshadow && Param("shadowdbhost") ne "");
    my $name = $useshadow ? Param("shadowdb") : $::db_name;
    my $connectstring;

    if ($useshadow_dbh) {
        if (defined $::shadow_dbh) {
            $::db = $::shadow_dbh;
            return;
        }
        $connectstring="DBI:mysql:host=" . Param("shadowdbhost") .
          ";database=$name;port=" . Param("shadowdbport");
        if (Param("shadowdbsock") ne "") {
            $connectstring .= ";mysql_socket=" . Param("shadowdbsock");
        }
    } else {
        if (defined $::main_dbh) {
            $::db = $::main_dbh;
            return;
        }
        $connectstring="DBI:mysql:host=$::db_host;database=$name;port=$::db_port";
        if ($::db_sock ne "") {
            $connectstring .= ";mysql_socket=$::db_sock";
        }
    }
    $::db = DBI->connect($connectstring, $::db_user, $::db_pass)
      || die "Bugzilla is currently broken. Please try again " .
        "later. If the problem persists, please contact " .
        Param("maintainer") . ". The error you should quote is: " .
        $DBI::errstr;

    if ($useshadow_dbh) {
        $::shadow_dbh = $::db;
    } else {
        $::main_dbh = $::db;
    }
}

sub ReconnectToShadowDatabase {
    # This will connect us to the shadowdb if we're not already connected,
    # but if we're using the same dbh for both the main db and the shadowdb,
    # be sure to USE the correct db
    if (Param("shadowdb") && Param("queryagainstshadowdb")) {
        ConnectToDatabase(1);
        if (!Param("shadowdbhost")) {
            SendSQL("USE " . Param("shadowdb"));
        }
    }
}

sub ReconnectToMainDatabase {
    if (Param("shadowdb") && Param("queryagainstshadowdb")) {
        ConnectToDatabase();
        if (!Param("shadowdbhost")) {
            SendSQL("USE $::db_name");
        }
    }
}

my $shadowchanges = 0;
sub SyncAnyPendingShadowChanges {
    if ($shadowchanges && Param("updateshadowdb")) {
        my $pid;
        FORK: {
            if ($pid = fork) { # create a fork
                # parent code runs here
                $shadowchanges = 0;
                return;
            } elsif (defined $pid) {
                # child process code runs here
                my $redir = ($^O =~ /MSWin32/i) ? "NUL" : "/dev/null";
                open STDOUT,">$redir";
                open STDERR,">$redir";
                exec("./syncshadowdb","--") or die "Unable to exec syncshadowdb: $!";
                # the idea was that passing the second parameter tricks it into
                # using execvp instead of running a shell. Not really necessary since
                # there are no shell meta-characters, but it passes our tinderbox
                # test that way. :) http://bugzilla.mozilla.org/show_bug.cgi?id=21253
            } elsif ($! =~ /No more process/) {
                # recoverable fork error, try again in 5 seconds
                sleep 5;
                redo FORK;
            } else {
                # something weird went wrong
                die "Can't create background process to run syncshadowdb: $!";
            }
        }
    }
}


# This is used to manipulate global state used by SendSQL(),
# MoreSQLData() and FetchSQLData().  It provides a way to do another
# SQL query without losing any as-yet-unfetched data from an existing
# query.  Just push the current global state, do your new query and fetch
# any data you need from it, then pop the current global state.
# 
@::SQLStateStack = ();

sub PushGlobalSQLState() {
    push @::SQLStateStack, $::currentquery;
    push @::SQLStateStack, [ @::fetchahead ]; 
}

sub PopGlobalSQLState() {
    die ("PopGlobalSQLState: stack underflow") if ( $#::SQLStateStack < 1 );
    @::fetchahead = @{pop @::SQLStateStack};
    $::currentquery = pop @::SQLStateStack;
}

sub SavedSQLStates() {
    return ($#::SqlStateStack + 1) / 2;
}


my $dosqllog = (-e "data/sqllog") && (-w "data/sqllog");

sub SqlLog {
    if ($dosqllog) {
        my ($str) = (@_);
        open(SQLLOGFID, ">>data/sqllog") || die "Can't write to data/sqllog";
        if (flock(SQLLOGFID,2)) { # 2 is magic 'exclusive lock' const.

            # if we're a subquery (ie there's pushed global state around)
            # indent to indicate the level of subquery-hood
            #
            for (my $i = SavedSQLStates() ; $i > 0 ; $i--) {
                print SQLLOGFID "\t";
            }

            print SQLLOGFID time2str("%D %H:%M:%S $$", time()) . ": $str\n";
        }
        flock(SQLLOGFID,8);     # '8' is magic 'unlock' const.
        close SQLLOGFID;
    }
}

sub SendSQL {
    my ($str, $dontshadow) = (@_);

    # Don't use DBI's taint stuff yet, because:
    # a) We don't want out vars to be tainted (yet)
    # b) We want to know who called SendSQL...
    # Is there a better way to do b?
    if (is_tainted($str)) {
        die "Attempted to send tainted string '$str' to the database";
    }

    my $iswrite =  ($str =~ /^(INSERT|REPLACE|UPDATE|DELETE)/i);
    if ($iswrite && !$::dbwritesallowed) {
        die "Evil code attempted to write '$str' to the shadow database";
    }
    if ($str =~ /^LOCK TABLES/i && $str !~ /shadowlog/ && $::dbwritesallowed) {
        $str =~ s/^LOCK TABLES/LOCK TABLES shadowlog WRITE, /i;
    }
    # If we are shutdown, we don't want to run queries except in special cases
    if (Param('shutdownhtml')) {
        if ($0 =~ m:[\\/]((do)?editparams.cgi|syncshadowdb)$:) {
            $::ignorequery = 0;
        } else {
            $::ignorequery = 1;
            return;
        }
    }
    SqlLog($str);
    $::currentquery = $::db->prepare($str);
    if (!$::currentquery->execute) {
        my $errstr = $::db->errstr;
        # Cut down the error string to a reasonable.size
        $errstr = substr($errstr, 0, 2000) . ' ... ' . substr($errstr, -2000)
                if length($errstr) > 4000;
        die "$str: " . $errstr;
    }
    SqlLog("Done");
    if (!$dontshadow && $iswrite && Param("shadowdb") && Param("updateshadowdb")) {
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
    # $::ignorequery is set in SendSQL
    if ($::ignorequery) {
        return 0;
    }
    if (defined @::fetchahead) {
        return 1;
    }
    if (@::fetchahead = $::currentquery->fetchrow_array) {
        return 1;
    }
    return 0;
}

sub FetchSQLData {
    # $::ignorequery is set in SendSQL
    if ($::ignorequery) {
        return;
    }
    if (defined @::fetchahead) {
        my @result = @::fetchahead;
        undef @::fetchahead;
        return @result;
    }
    return $::currentquery->fetchrow_array;
}


sub FetchOneColumn {
    my @row = FetchSQLData();
    return $row[0];
}

    

@::default_column_list = ("bug_severity", "priority", "rep_platform", 
                          "assigned_to", "bug_status", "resolution",
                          "short_short_desc");

sub AppendComment {
    my ($bugid, $who, $comment, $isprivate, $timestamp, $work_time) = @_;
    $work_time ||= 0;
    
    # Use the date/time we were given if possible (allowing calling code
    # to synchronize the comment's timestamp with those of other records).
    $timestamp = ($timestamp ? SqlQuote($timestamp) : "NOW()");
    
    $comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
    $comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.

    # allowing negatives though so people can back out errors in time reporting
    if (defined $work_time) {
       # regexp verifies one or more digits, optionally followed by a period and
       # zero or more digits, OR we have a period followed by one or more digits
       if ($work_time !~ /^-?(?:\d+(?:\.\d*)?|\.\d+)$/) { 
          ThrowUserError("need_numeric_value");
          return;
       }
    } else { $work_time = 0 };

    if ($comment =~ /^\s*$/) {  # Nothin' but whitespace
        return;
    }

    my $whoid = DBNameToIdAndCheck($who);
    my $privacyval = $isprivate ? 1 : 0 ;
    SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext, isprivate, work_time) " .
        "VALUES($bugid, $whoid, $timestamp, " . SqlQuote($comment) . ", " . 
        $privacyval . ", " . SqlQuote($work_time) . ")");

    SendSQL("UPDATE bugs SET delta_ts = now() WHERE bug_id = $bugid");
}

sub GetFieldID {
    my ($f) = (@_);
    SendSQL("SELECT fieldid FROM fielddefs WHERE name = " . SqlQuote($f));
    my $fieldid = FetchOneColumn();
    die "Unknown field id: $f" if !$fieldid;
    return $fieldid;
}

# XXXX - this needs to go away
sub GenerateVersionTable {
    SendSQL("SELECT versions.value, products.name " .
            "FROM versions, products " .
            "WHERE products.id = versions.product_id " .
            "ORDER BY versions.value");
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
    SendSQL("SELECT components.name, products.name " .
            "FROM components, products " .
            "WHERE products.id = components.product_id " .
            "ORDER BY components.name");
    while (@line = FetchSQLData()) {
        my ($c,$p) = (@line);
        if (!defined $::components{$p}) {
            $::components{$p} = [];
        }
        my $ref = $::components{$p};
        push @$ref, $c;
        $carray{$c} = 1;
    }

    my $dotargetmilestone = 1;  # This used to check the param, but there's
                                # enough code that wants to pretend we're using
                                # target milestones, even if they don't get
                                # shown to the user.  So we cache all the data
                                # about them anyway.

    my $mpart = $dotargetmilestone ? ", milestoneurl" : "";
    SendSQL("select name, description, votesperuser, disallownew$mpart from products ORDER BY name");
    while (@line = FetchSQLData()) {
        my ($p, $d, $votesperuser, $dis, $u) = (@line);
        $::proddesc{$p} = $d;
        if (!$dis) {
            push @::enterable_products, $p;
        }
        if ($dotargetmilestone) {
            $::milestoneurl{$p} = $u;
        }
        $::prodmaxvotes{$p} = $votesperuser;
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

    # 'settable_resolution' is the list of resolutions that may be set 
    # directly by hand in the bug form. Start with the list of legal 
    # resolutions and remove 'MOVED' and 'DUPLICATE' because setting 
    # bugs to those resolutions requires a special process.
    #
    @::settable_resolution = @::legal_resolution;
    my $w = lsearch(\@::settable_resolution, "DUPLICATE");
    if ($w >= 0) {
        splice(@::settable_resolution, $w, 1);
    }
    my $z = lsearch(\@::settable_resolution, "MOVED");
    if ($z >= 0) {
        splice(@::settable_resolution, $z, 1);
    }

    my @list = sort { uc($a) cmp uc($b)} keys(%::versions);
    @::legal_product = @list;
    my $tmpname = "data/versioncache.$$";
    open(FID, ">$tmpname") || die "Can't create $tmpname";

    print FID "#\n";
    print FID "# DO NOT EDIT!\n";
    print FID "# This file is automatically generated at least once every\n";
    print FID "# hour by the GenerateVersionTable() sub in globals.pl.\n";
    print FID "# Any changes you make will be overwritten.\n";
    print FID "#\n";

    require Data::Dumper;
    print FID Data::Dumper->Dump([\@::log_columns, \%::versions],
                                 ['*::log_columns', '*::versions']);

    foreach my $i (@list) {
        if (!defined $::components{$i}) {
            $::components{$i} = [];
        }
    }
    @::legal_versions = sort {uc($a) cmp uc($b)} keys(%varray);
    print FID Data::Dumper->Dump([\@::legal_versions, \%::components],
                                 ['*::legal_versions', '*::components']);
    @::legal_components = sort {uc($a) cmp uc($b)} keys(%carray);

    print FID Data::Dumper->Dump([\@::legal_components, \@::legal_product,
                                  \@::legal_priority, \@::legal_severity,
                                  \@::legal_platform, \@::legal_opsys,
                                  \@::legal_bug_status, \@::legal_resolution],
                                 ['*::legal_components', '*::legal_product',
                                  '*::legal_priority', '*::legal_severity',
                                  '*::legal_platform', '*::legal_opsys',
                                  '*::legal_bug_status', '*::legal_resolution']);

    print FID Data::Dumper->Dump([\@::settable_resolution, \%::proddesc,
                                  \@::enterable_products, \%::prodmaxvotes],
                                 ['*::settable_resolution', '*::proddesc',
                                  '*::enterable_products', '*::prodmaxvotes']);

    if ($dotargetmilestone) {
        # reading target milestones in from the database - matthew@zeroknowledge.com
        SendSQL("SELECT milestones.value, products.name " .
                "FROM milestones, products " .
                "WHERE products.id = milestones.product_id " .
                "ORDER BY milestones.sortkey, milestones.value");
        my @line;
        my %tmarray;
        @::legal_target_milestone = ();
        while(@line = FetchSQLData()) {
            my ($tm, $pr) = (@line);
            if (!defined $::target_milestone{$pr}) {
                $::target_milestone{$pr} = [];
            }
            push @{$::target_milestone{$pr}}, $tm;
            if (!exists $tmarray{$tm}) {
                $tmarray{$tm} = 1;
                push(@::legal_target_milestone, $tm);
            }
        }

        print FID Data::Dumper->Dump([\%::target_milestone,
                                      \@::legal_target_milestone,
                                      \%::milestoneurl],
                                     ['*::target_milestone',
                                      '*::legal_target_milestone',
                                      '*::milestoneurl']);
    }

    SendSQL("SELECT id, name FROM keyworddefs ORDER BY name");
    while (MoreSQLData()) {
        my ($id, $name) = FetchSQLData();
        push(@::legal_keywords, $name);
        $name = lc($name);
        $::keywordsbyname{$name} = $id;
    }

    print FID Data::Dumper->Dump([\@::legal_keywords, \%::keywordsbyname],
                                 ['*::legal_keywords', '*::keywordsbyname']);

    print FID "1;\n";
    close FID;

    rename $tmpname, "data/versioncache" || die "Can't rename $tmpname to versioncache";
    ChmodDataFile('data/versioncache', 0666);
}


sub GetKeywordIdFromName {
    my ($name) = (@_);
    $name = lc($name);
    return $::keywordsbyname{$name};
}




# Returns the modification time of a file.

sub ModTime {
    my ($filename) = (@_);
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks)
        = stat($filename);
    return $mtime;
}

$::VersionTableLoaded = 0;
sub GetVersionTable {
    return if $::VersionTableLoaded;
    my $mtime = ModTime("data/versioncache");
    if (!defined $mtime || $mtime eq "") {
        $mtime = 0;
    }
    if (time() - $mtime > 3600) {
        use Token;
        Token::CleanTokenTable() if $::dbwritesallowed;
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
    $::VersionTableLoaded = 1;
}

# Validates a given username as a new username
# returns 1 if valid, 0 if invalid
sub ValidateNewUser {
    my ($username, $old_username) = @_;

    if(DBname_to_id($username) != 0) {
        return 0;
    }

    my $sqluname = SqlQuote($username);

    # Reject if the new login is part of an email change which is 
    # still in progress
    #
    # substring/locate stuff: bug 165221; this used to use regexes, but that
    # was unsafe and required weird escaping; using substring to pull out
    # the new/old email addresses and locate() to find the delimeter (':')
    # is cleaner/safer
    SendSQL("SELECT eventdata FROM tokens WHERE tokentype = 'emailold' 
     AND SUBSTRING(eventdata, 1, (LOCATE(':', eventdata) - 1)) = $sqluname 
     OR SUBSTRING(eventdata, (LOCATE(':', eventdata) + 1)) = $sqluname");

    if (my ($eventdata) = FetchSQLData()) {
        # Allow thru owner of token
        if($old_username && ($eventdata eq "$old_username:$username")) {
            return 1;
        }
        return 0;
    }

    return 1;
}

sub InsertNewUser {
    my ($username, $realname) = (@_);

    # Generate a new random password for the user.
    my $password = GenerateRandomPassword();
    my $cryptpassword = Crypt($password);


    # Insert the new user record into the database.            
    $username = SqlQuote($username);
    $realname = SqlQuote($realname);
    $cryptpassword = SqlQuote($cryptpassword);
    PushGlobalSQLState();
    SendSQL("INSERT INTO profiles (login_name, realname, cryptpassword) 
             VALUES ($username, $realname, $cryptpassword)");
    PopGlobalSQLState();

    # Return the password to the calling code so it can be included 
    # in an email sent to the user.
    return $password;
}

# Removes all entries from logincookies for $userid, except for the
# optional $keep, which refers the logincookies.cookie primary key.
# (This is useful so that a user changing their password stays logged in)
sub InvalidateLogins {
    my ($userid, $keep) = @_;

    my $remove = "DELETE FROM logincookies WHERE userid = $userid";
    if (defined $keep) {
        $remove .= " AND cookie != " . SqlQuote($keep);
    }
    SendSQL($remove);
}

sub GenerateRandomPassword {
    my ($size) = @_;

    # Generated passwords are eight characters long by default.
    $size ||= 8;

    # The list of characters that can appear in a randomly generated password.
    # Note that users can put any character into a password they choose themselves.
    my @pwchars = (0..9, 'A'..'Z', 'a'..'z', '-', '_', '!', '@', '#', '$', '%', '^', '&', '*');

    # The number of characters in the list.
    my $pwcharslen = scalar(@pwchars);

    # Generate the password.
    my $password = "";
    for ( my $i=0 ; $i<$size ; $i++ ) {
        $password .= $pwchars[rand($pwcharslen)];
    }

    # Return the password.
    return $password;
}

#
# This function checks if there are any entry groups defined.
# If called with no arguments, it identifies
# entry groups for all products.  If called with a product
# id argument, it checks for entry groups associated with 
# one particular product.
sub AnyEntryGroups {
    my $product_id = shift;
    $product_id = 0 unless ($product_id);
    return $::CachedAnyEntryGroups{$product_id} 
        if defined($::CachedAnyEntryGroups{$product_id});
    PushGlobalSQLState();
    my $query = "SELECT 1 FROM group_control_map WHERE entry != 0";
    $query .= " AND product_id = $product_id" if ($product_id);
    $query .= " LIMIT 1";
    SendSQL($query);
    $::CachedAnyEntryGroups{$product_id} = MoreSQLData();
    FetchSQLData();
    PopGlobalSQLState();
    return $::CachedAnyEntryGroups{$product_id};
}

#
# This function checks if there are any default groups defined.
# If so, then groups may have to be changed when bugs move from
# one bug to another.
sub AnyDefaultGroups {
    return $::CachedAnyDefaultGroups if defined($::CachedAnyDefaultGroups);
    PushGlobalSQLState();
    SendSQL("SELECT 1 FROM group_control_map, groups WHERE " .
            "groups.id = group_control_map.group_id " .
            "AND isactive != 0 AND " .
            "(membercontrol = " . CONTROLMAPDEFAULT .
            " OR othercontrol = " . CONTROLMAPDEFAULT .
            ") LIMIT 1");
    $::CachedAnyDefaultGroups = MoreSQLData();
    FetchSQLData();
    PopGlobalSQLState();
    return $::CachedAnyDefaultGroups;
}

#
# This function checks if, given a product id, the user can edit
# bugs in this product at all.
sub CanEditProductId {
    my ($productid) = @_;
    my $query = "SELECT group_id FROM group_control_map " .
                "WHERE product_id = $productid " .
                "AND canedit != 0 "; 
    if ((defined @{$::vars->{user}{groupids}}) 
        && (@{$::vars->{user}{groupids}} > 0)) {
        $query .= "AND group_id NOT IN(" . 
                   join(',',@{$::vars->{user}{groupids}}) . ") ";
    }
    $query .= "LIMIT 1";
    PushGlobalSQLState();
    SendSQL($query);
    my ($result) = FetchSQLData();
    PopGlobalSQLState();
    return (!defined($result));
}

#
# This function determines if a user can enter bugs in the named
# product.
sub CanEnterProduct {
    my ($productname) = @_;
    my $query = "SELECT group_id IS NULL " .
                "FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id " .
                "AND group_control_map.entry != 0 ";
    if ((defined @{$::vars->{user}{groupids}}) 
        && (@{$::vars->{user}{groupids}} > 0)) {
        $query .= "AND group_id NOT IN(" . 
                   join(',',@{$::vars->{user}{groupids}}) . ") ";
    }
    $query .= "WHERE products.name = " . SqlQuote($productname) . " LIMIT 1";
    PushGlobalSQLState();
    SendSQL($query);
    my ($ret) = FetchSQLData();
    PopGlobalSQLState();
    return ($ret);
}

#
# This function returns an alphabetical list of product names to which
# the user can enter bugs.
sub GetEnterableProducts {
    my $query = "SELECT name " .
                "FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id " .
                "AND group_control_map.entry != 0 ";
    if ((defined @{$::vars->{user}{groupids}}) 
        && (@{$::vars->{user}{groupids}} > 0)) {
        $query .= "AND group_id NOT IN(" . 
                   join(',',@{$::vars->{user}{groupids}}) . ") ";
    }
    $query .= "WHERE group_id IS NULL ORDER BY name";
    PushGlobalSQLState();
    SendSQL($query);
    my @products = ();
    while (MoreSQLData()) {
        push @products,FetchOneColumn();
    }
    PopGlobalSQLState();
    return (@products);
}

# GetEnterableProductHash
# returns a hash containing 
# legal_products => an enterable product list
# legal_components => the list of components of enterable products
# components => a hash of component lists for each enterable product
sub GetEnterableProductHash {
    my $query = "SELECT products.name, components.name " .
                "FROM products " .
                "LEFT JOIN components " .
                "ON components.product_id = products.id " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id " .
                "AND group_control_map.entry != 0 ";
    if ((defined @{$::vars->{user}{groupids}}) 
        && (@{$::vars->{user}{groupids}} > 0)) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', @{$::vars->{user}{groupids}}) . ") ";
    }
    $query .= "WHERE group_id IS NULL " .
              "ORDER BY products.name, components.name";
    PushGlobalSQLState();
    SendSQL($query);
    my @products = ();
    my %components = ();
    my %components_by_product = ();
    while (MoreSQLData()) {
        my ($product, $component) = FetchSQLData();
        if (!grep($_ eq $product, @products)) {
            push @products, $product;
        }
        if ($component) {
            $components{$component} = 1;
            push @{$components_by_product{$product}}, $component;
        }
    }
    PopGlobalSQLState();
    my @componentlist = (sort keys %components);
    return {
        legal_products => \@products,
        legal_components => \@componentlist,
        components => \%components_by_product,
    };
}


sub CanSeeBug {

    my ($id, $userid) = @_;

    # Query the database for the bug, retrieving a boolean value that
    # represents whether or not the user is authorized to access the bug.

    # if no groups are found --> user is permitted to access
    # if no user is found for any group --> user is not permitted to access
    my $query = "SELECT bugs.bug_id, reporter, assigned_to, qa_contact," .
        " reporter_accessible, cclist_accessible," .
        " cc.who IS NOT NULL," .
        " COUNT(DISTINCT(bug_group_map.group_id)) as cntbugingroups," .
        " COUNT(DISTINCT(user_group_map.group_id)) as cntuseringroups" .
        " FROM bugs" .
        " LEFT JOIN cc ON bugs.bug_id = cc.bug_id" .
        " AND cc.who = $userid" .
        " LEFT JOIN bug_group_map ON bugs.bug_id = bug_group_map.bug_id" .
        " LEFT JOIN user_group_map ON" .
        " user_group_map.group_id = bug_group_map.group_id" .
        " AND user_group_map.isbless = 0" .
        " AND user_group_map.user_id = $userid" .
        " WHERE bugs.bug_id = $id GROUP BY bugs.bug_id";
    PushGlobalSQLState();
    SendSQL($query);
    my ($found_id, $reporter, $assigned_to, $qa_contact,
        $rep_access, $cc_access,
        $found_cc, $found_groups, $found_members) 
        = FetchSQLData();
    PopGlobalSQLState();
    return (
               ($found_groups == 0) 
               || (($userid > 0) && 
                  (
                       ($assigned_to == $userid) 
                    || ($qa_contact == $userid)
                    || (($reporter == $userid) && $rep_access) 
                    || ($found_cc && $cc_access) 
                    || ($found_groups == $found_members)
                  ))
           );
}

sub ValidatePassword {
    # Determines whether or not a password is valid (i.e. meets Bugzilla's
    # requirements for length and content).    
    # If a second password is passed in, this function also verifies that
    # the two passwords match.
    my ($password, $matchpassword) = @_;
    
    if (length($password) < 3) {
        ThrowUserError("password_too_short");
    } elsif (length($password) > 16) {
        ThrowUserError("password_too_long");
    } elsif ($matchpassword && $password ne $matchpassword) { 
        ThrowUserError("passwords_dont_match");
    }
}


sub Crypt {
    # Crypts a password, generating a random salt to do it.
    # Random salts are generated because the alternative is usually
    # to use the first two characters of the password itself, and since
    # the salt appears in plaintext at the beginning of the crypted
    # password string this has the effect of revealing the first two
    # characters of the password to anyone who views the crypted version.

    my ($password) = @_;

    # The list of characters that can appear in a salt.  Salts and hashes
    # are both encoded as a sequence of characters from a set containing
    # 64 characters, each one of which represents 6 bits of the salt/hash.
    # The encoding is similar to BASE64, the difference being that the
    # BASE64 plus sign (+) is replaced with a forward slash (/).
    my @saltchars = (0..9, 'A'..'Z', 'a'..'z', '.', '/');

    # Generate the salt.  We use an 8 character (48 bit) salt for maximum
    # security on systems whose crypt uses MD5.  Systems with older
    # versions of crypt will just use the first two characters of the salt.
    my $salt = '';
    for ( my $i=0 ; $i < 8 ; ++$i ) {
        $salt .= $saltchars[rand(64)];
    }

    # Crypt the password.
    my $cryptedpassword = crypt($password, $salt);

    # Return the crypted password.
    return $cryptedpassword;
}

# ConfirmGroup(userid) is called prior to any activity that relies
# on user_group_map to ensure that derived group permissions are up-to-date.
# Permissions must be rederived if ANY groups have a last_changed newer
# than the profiles.refreshed_when value.
sub ConfirmGroup {
    my ($user) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT userid FROM profiles, groups WHERE userid = $user " .
            "AND profiles.refreshed_when <= groups.last_changed ");
    my $ret = FetchSQLData();
    PopGlobalSQLState();
    if ($ret) {
        DeriveGroup($user);
    }
}

# DeriveGroup removes and rederives all derived group permissions for
# the specified user.
sub DeriveGroup {
    my ($user) = (@_);
    PushGlobalSQLState();

    SendSQL("LOCK TABLES profiles WRITE, user_group_map WRITE, group_group_map READ, groups READ");

    # avoid races,  we are only as up to date as the BEGINNING of this process
    SendSQL("SELECT login_name, NOW() FROM profiles WHERE userid = $user");
    my ($login, $starttime) = FetchSQLData();
    
    # first remove any old derived stuff for this user
    SendSQL("DELETE FROM user_group_map WHERE user_id = $user " .
            "AND isderived = 1");

    my %groupidsadded = ();
    # add derived records for any matching regexps
    SendSQL("SELECT id, userregexp FROM groups WHERE userregexp != ''");
    while (MoreSQLData()) {
        my ($groupid, $rexp) = FetchSQLData();
        if ($login =~ m/$rexp/i) {        
            PushGlobalSQLState();
            $groupidsadded{$groupid} = 1;
            SendSQL("INSERT INTO user_group_map " .
                    "(user_id, group_id, isbless, isderived) " .
                    "VALUES ($user, $groupid, 0, 1)");
            PopGlobalSQLState();

        }
    }

    # Get a list of the groups of which the user is a member.
    my %groupidschecked = ();
    my @groupidstocheck = ();
    SendSQL("SELECT group_id FROM user_group_map WHERE user_id = $user
             AND NOT isbless");
    while (MoreSQLData()) {
        my ($groupid) = FetchSQLData();
        push(@groupidstocheck,$groupid);
    }

    # Each group needs to be checked for inherited memberships once.
    while (@groupidstocheck) {
        my $group = shift @groupidstocheck;
        if (!defined($groupidschecked{"$group"})) {
            $groupidschecked{"$group"} = 1;
            SendSQL("SELECT grantor_id FROM group_group_map WHERE"
                 . " member_id = $group AND NOT isbless");
            while (MoreSQLData()) {
                my ($groupid) = FetchSQLData();
                if (!defined($groupidschecked{"$groupid"})) {
                    push(@groupidstocheck,$groupid);
                }
                if (!$groupidsadded{$groupid}) {
                    $groupidsadded{$groupid} = 1;
                    PushGlobalSQLState();
                    SendSQL("INSERT INTO user_group_map"
                         . " (user_id, group_id, isbless, isderived)"
                         . " VALUES ($user, $groupid, 0, 1)");
                    PopGlobalSQLState();
                }
            }
        }
    }
            
    SendSQL("UPDATE profiles SET refreshed_when = " .
            SqlQuote($starttime) . "WHERE userid = $user");
    SendSQL("UNLOCK TABLES");
    PopGlobalSQLState();
};


sub DBID_to_real_or_loginname {
    my ($id) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT login_name,realname FROM profiles WHERE userid = $id");
    my ($l, $r) = FetchSQLData();
    PopGlobalSQLState();
    if (!defined $r || $r eq "") {
        return $l;
    } else {
        return "$l ($r)";
    }
}

sub DBID_to_name {
    my ($id) = (@_);
    # $id should always be a positive integer
    if ($id =~ m/^([1-9][0-9]*)$/) {
        $id = $1;
    } else {
        $::cachedNameArray{$id} = "__UNKNOWN__";
    }
    if (!defined $::cachedNameArray{$id}) {
        PushGlobalSQLState();
        SendSQL("select login_name from profiles where userid = $id");
        my $r = FetchOneColumn();
        PopGlobalSQLState();
        if (!defined $r || $r eq "") {
            $r = "__UNKNOWN__";
        }
        $::cachedNameArray{$id} = $r;
    }
    return $::cachedNameArray{$id};
}

sub DBname_to_id {
    my ($name) = (@_);
    PushGlobalSQLState();
    SendSQL("select userid from profiles where login_name = @{[SqlQuote($name)]}");
    my $r = FetchOneColumn();
    PopGlobalSQLState();
    # $r should be a positive integer, this makes Taint mode happy
    if (defined $r && $r =~ m/^([1-9][0-9]*)$/) {
        return $1;
    } else {
        return 0;
    }
}


sub DBNameToIdAndCheck {
    my ($name) = (@_);
    my $result = DBname_to_id($name);
    if ($result > 0) {
        return $result;
    }

    $::vars->{'name'} = $name;
    ThrowUserError("invalid_username");
}




sub get_product_id {
    my ($prod) = @_;
    PushGlobalSQLState();
    SendSQL("SELECT id FROM products WHERE name = " . SqlQuote($prod));
    my ($prod_id) = FetchSQLData();
    PopGlobalSQLState();
    return $prod_id;
}

sub get_product_name {
    my ($prod_id) = @_;
    die "non-numeric prod_id '$prod_id' passed to get_product_name"
      unless ($prod_id =~ /^\d+$/);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM products WHERE id = $prod_id");
    my ($prod) = FetchSQLData();
    PopGlobalSQLState();
    return $prod;
}

sub get_component_id {
    my ($prod_id, $comp) = @_;
    return undef unless ($prod_id =~ /^\d+$/);
    PushGlobalSQLState();
    SendSQL("SELECT id FROM components " .
            "WHERE product_id = $prod_id AND name = " . SqlQuote($comp));
    my ($comp_id) = FetchSQLData();
    PopGlobalSQLState();
    return $comp_id;
}

sub get_component_name {
    my ($comp_id) = @_;
    die "non-numeric comp_id '$comp_id' passed to get_component_name"
      unless ($comp_id =~ /^\d+$/);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM components WHERE id = $comp_id");
    my ($comp) = FetchSQLData();
    PopGlobalSQLState();
    return $comp;
}

# This routine quoteUrls contains inspirations from the HTML::FromText CPAN
# module by Gareth Rees <garethr@cre.canon.co.uk>.  It has been heavily hacked,
# all that is really recognizable from the original is bits of the regular
# expressions.
# This has been rewritten to be faster, mainly by substituting 'as we go'.
# If you want to modify this routine, read the comments carefully

sub quoteUrls {
    my ($text) = (@_);
    return $text unless $text;

    # We use /g for speed, but uris can have other things inside them
    # (http://foo/bug#3 for example). Filtering that out filters valid
    # bug refs out, so we have to do replacements.
    # mailto can't contain space or #, so we don't have to bother for that
    # Do this by escaping \0 to \1\0, and replacing matches with \0\0$count\0\0
    # \0 is used because its unliklely to occur in the text, so the cost of
    # doing this should be very small
    # Also, \0 won't appear in the value_quote'd bug title, so we don't have
    # to worry about bogus substitutions from there

    # escape the 2nd escape char we're using
    my $chr1 = chr(1);
    $text =~ s/\0/$chr1\0/g;

    # However, note that adding the title (for buglinks) can affect things
    # In particular, attachment matches go before bug titles, so that titles
    # with 'attachment 1' don't double match.
    # Dupe checks go afterwards, because that uses ^ and \Z, which won't occur
    # if it was subsituted as a bug title (since that always involve leading
    # and trailing text)

    # Because of entities, its easier (and quicker) to do this before escaping

    my @things;
    my $count = 0;
    my $tmp;

    # non-mailto protocols
    my $protocol_re = qr/(afs|cid|ftp|gopher|http|https|mid|news|nntp|prospero|telnet|wais)/i;

    $text =~ s~\b(${protocol_re}:  # The protocol:
                  [^\s<>\"]+       # Any non-whitespace
                  [\w\/])          # so that we end in \w or /
              ~($tmp = html_quote($1)) &&
               ($things[$count++] = "<a href=\"$tmp\">$tmp</a>") &&
               ("\0\0" . ($count-1) . "\0\0")
              ~egox;

    # We have to quote now, otherwise our html is itsself escaped
    # THIS MEANS THAT A LITERAL ", <, >, ' MUST BE ESCAPED FOR A MATCH

    $text = html_quote($text);

    # mailto:
    # Use |<nothing> so that $1 is defined regardless
    $text =~ s~\b(mailto:|)?([\w\.\-\+\=]+\@[\w\-]+(?:\.[\w\-]+)+)\b
              ~<a href=\"mailto:$2\">$1$2</a>~igx;

    # attachment links - handle both cases separatly for simplicity
    $text =~ s~((?:^Created\ an\ |\b)attachment\s*\(id=(\d+)\))
              ~<a href=\"attachment.cgi?id=$2&amp;action=view\">$1</a>~igx;

    $text =~ s~\b(attachment\s*\#?\s*(\d+))
              ~<a href=\"attachment.cgi?id=$2&amp;action=view\">$1</a>~igx;

    # This handles bug a, comment b type stuff. Because we're using /g
    # we have to do this in one pattern, and so this is semi-messy.
    # Also, we can't use $bug_re?$comment_re? because that will match the
    # empty string
    my $bug_re = qr/bug\s*\#?\s*(\d+)/i;
    my $comment_re = qr/comment\s*\#?\s*(\d+)/i;
    $text =~ s~\b($bug_re(?:\s*,?\s*$comment_re)?|$comment_re)
              ~ # We have several choices. $1 here is the link, and $2-4 are set
                # depending on which part matched
               (defined($2) ? GetBugLink($2,$1,$3) :
                              "<a href=\"#c$4\">$1</a>")
              ~egox;

    # Duplicate markers
    $text =~ s~(?<=^\*\*\*\ This\ bug\ has\ been\ marked\ as\ a\ duplicate\ of\ )
               (\d+)
               (?=\ \*\*\*\Z)
              ~GetBugLink($1, $1)
              ~egmx;

    # Now remove the encoding hacks
    $text =~ s/\0\0(\d+)\0\0/$things[$1]/eg;
    $text =~ s/$chr1\0/\0/g;

    return $text;
}

# GetBugLink creates a link to a bug, including its title.
# It takes either two or three parameters:
#  - The bug number
#  - The link text, to place between the <a>..</a>
#  - An optional comment number, for linking to a particular
#    comment in the bug

sub GetBugLink {
    my ($bug_num, $link_text, $comment_num) = @_;
    detaint_natural($bug_num) || die "GetBugLink() called with non-integer bug number";

    # If we've run GetBugLink() for this bug number before, %::buglink
    # will contain an anonymous array ref of relevent values, if not
    # we need to get the information from the database.
    if (! defined $::buglink{$bug_num}) {
        # Make sure any unfetched data from a currently running query
        # is saved off rather than overwritten
        PushGlobalSQLState();

        SendSQL("SELECT bugs.bug_status, resolution, short_desc " .
                "FROM bugs WHERE bugs.bug_id = $bug_num");

        # If the bug exists, save its data off for use later in the sub
        if (MoreSQLData()) {
            my ($bug_state, $bug_res, $bug_desc) = FetchSQLData();
            # Initialize these variables to be "" so that we don't get warnings
            # if we don't change them below (which is highly likely).
            my ($pre, $title, $post) = ("", "", "");

            $title = $bug_state;
            if ($bug_state eq $::unconfirmedstate) {
                $pre = "<i>";
                $post = "</i>";
            }
            elsif (! IsOpenedState($bug_state)) {
                $pre = "<strike>";
                $title .= " $bug_res";
                $post = "</strike>";
            }
            if (CanSeeBug($bug_num, $::userid)) {
                $title .= " - $bug_desc";
            }
            $::buglink{$bug_num} = [$pre, value_quote($title), $post];
        }
        else {
            # Even if there's nothing in the database, we want to save a blank
            # anonymous array in the %::buglink hash so the query doesn't get
            # run again next time we're called for this bug number.
            $::buglink{$bug_num} = [];
        }
        # All done with this sidetrip
        PopGlobalSQLState();
    }

    # Now that we know we've got all the information we're gonna get, let's
    # return the link (which is the whole reason we were called :)
    my ($pre, $title, $post) = @{$::buglink{$bug_num}};
    # $title will be undefined if the bug didn't exist in the database.
    if (defined $title) {
        my $linkval = "show_bug.cgi?id=$bug_num";
        if (defined $comment_num) {
            $linkval .= "#c$comment_num";
        }
        return qq{$pre<a href="$linkval" title="$title">$link_text</a>$post};
    }
    else {
        return qq{$link_text};
    }
}

sub GetLongDescriptionAsText {
    my ($id, $start, $end) = (@_);
    my $result = "";
    my $count = 0;
    my $anyprivate = 0;
    my ($query) = ("SELECT profiles.login_name, DATE_FORMAT(longdescs.bug_when,'%Y.%d.%m %H:%i'), " .
                   "       longdescs.thetext, longdescs.isprivate " .
                   "FROM   longdescs, profiles " .
                   "WHERE  profiles.userid = longdescs.who " .
                   "AND    longdescs.bug_id = $id ");

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
        my ($who, $when, $text, $isprivate, $work_time) = (FetchSQLData());
        if ($count) {
            $result .= "\n\n------- Additional Comments From $who".Param('emailsuffix')."  ".
                Bugzilla::Util::format_time($when) . " -------\n";
        }
        if (($isprivate > 0) && Param("insidergroup")) {
            $anyprivate = 1;
        }
        $result .= $text;
        $count++;
    }

    return ($result, $anyprivate);
}

sub GetComments {
    my ($id) = (@_);
    my @comments;
    SendSQL("SELECT  profiles.realname, profiles.login_name, 
                     date_format(longdescs.bug_when,'%Y.%m.%d %H:%i'), 
                     longdescs.thetext, longdescs.work_time,
                     isprivate,
                     date_format(longdescs.bug_when,'%Y%m%d%H%i%s') 
            FROM     longdescs, profiles
            WHERE    profiles.userid = longdescs.who 
              AND    longdescs.bug_id = $id 
            ORDER BY longdescs.bug_when");
             
    while (MoreSQLData()) {
        my %comment;
        ($comment{'name'}, $comment{'email'}, $comment{'time'}, 
        $comment{'body'}, $comment{'work_time'},
        $comment{'isprivate'}, $comment{'when'}) = FetchSQLData();
        
        $comment{'email'} .= Param('emailsuffix');
        $comment{'name'} = $comment{'name'} || $comment{'email'};
         
        push (@comments, \%comment);
    }
    
    return \@comments;
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
    # If it's been SqlQuote()ed, then it's safe, so we tell -T that.
    trick_taint($str);
    return "'$str'";
}


# UserInGroup returns information aboout the current user if no second 
# parameter is specified
sub UserInGroup {
    my ($groupname, $userid) = (@_);
    if (!$userid) {
        return $::vars->{'user'}{'groups'}{$_[0]};
    }
    PushGlobalSQLState();
    $userid ||= $::userid;
    SendSQL("SELECT groups.id FROM groups, user_group_map 
        WHERE groups.id = user_group_map.group_id 
        AND user_group_map.user_id = $userid
        AND isbless = 0
        AND groups.name = " . SqlQuote($groupname));
    my $result = FetchOneColumn();
    PopGlobalSQLState();
    return defined($result);
}

sub UserCanBlessGroup {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    # check if user explicitly can bless group
    SendSQL("SELECT groups.id FROM groups, user_group_map 
        WHERE groups.id = user_group_map.group_id 
        AND user_group_map.user_id = $::userid
        AND isbless = 1
        AND groups.name = " . SqlQuote($groupname));
    my $result = FetchOneColumn();
    PopGlobalSQLState();
    if ($result) {
        return 1;
    }
    PushGlobalSQLState();
    # check if user is a member of a group that can bless this group
    # this group does not count
    SendSQL("SELECT groups.id FROM groups, user_group_map, 
        group_group_map 
        WHERE groups.id = grantor_id 
        AND user_group_map.user_id = $::userid
        AND user_group_map.isbless = 0
        AND group_group_map.isbless = 1
        AND user_group_map.group_id = member_id
        AND groups.name = " . SqlQuote($groupname));
    $result = FetchOneColumn();
    PopGlobalSQLState();
    return $result; 
}

sub UserCanBlessAnything {
    PushGlobalSQLState();
    # check if user explicitly can bless a group
    SendSQL("SELECT group_id FROM user_group_map 
        WHERE user_id = $::userid AND isbless = 1");
    my $result = FetchOneColumn();
    PopGlobalSQLState();
    if ($result) {
        return 1;
    }
    PushGlobalSQLState();
    # check if user is a member of a group that can bless this group
    SendSQL("SELECT groups.id FROM groups, user_group_map, 
        group_group_map 
        WHERE groups.id = grantor_id 
        AND user_group_map.user_id = $::userid
        AND group_group_map.isbless = 1
        AND user_group_map.group_id = member_id");
    $result = FetchOneColumn();
    PopGlobalSQLState();
    if ($result) {
        return 1;
    }
    return 0;
}

sub BugInGroup {
    my ($bugid, $groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT bug_group_map.bug_id != 0 FROM bug_group_map, groups 
            WHERE bug_group_map.bug_id = $bugid
            AND bug_group_map.group_id = groups.id
            AND groups.name = " . SqlQuote($groupname));
    my $bugingroup = FetchOneColumn();
    PopGlobalSQLState();
    return $bugingroup;
}

sub BugInGroupId {
    my ($bugid, $groupid) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT bug_id != 0 FROM bug_group_map
            WHERE bug_id = $bugid
            AND group_id = $groupid");
    my $bugingroup = FetchOneColumn();
    PopGlobalSQLState();
    return $bugingroup;
}

sub GroupExists {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT id FROM groups WHERE name=" . SqlQuote($groupname));
    my $id = FetchOneColumn();
    PopGlobalSQLState();
    return $id;
}

sub GroupNameToId {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT id FROM groups WHERE name=" . SqlQuote($groupname));
    my $id = FetchOneColumn();
    PopGlobalSQLState();
    return $id;
}

sub GroupIdToName {
    my ($groupid) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT name FROM groups WHERE id = $groupid");
    my $name = FetchOneColumn();
    PopGlobalSQLState();
    return $name;
}


# Determines whether or not a group is active by checking 
# the "isactive" column for the group in the "groups" table.
# Note: This function selects groups by id rather than by name.
sub GroupIsActive {
    my ($groupid) = (@_);
    $groupid ||= 0;
    PushGlobalSQLState();
    SendSQL("SELECT isactive FROM groups WHERE id=$groupid");
    my $isactive = FetchOneColumn();
    PopGlobalSQLState();
    return $isactive;
}

# Determines if the given bug_status string represents an "Opened" bug.  This
# routine ought to be parameterizable somehow, as people tend to introduce
# new states into Bugzilla.

sub IsOpenedState {
    my ($state) = (@_);
    if (grep($_ eq $state, OpenStates())) {
        return 1;
    }
    return 0;
}

# This sub will return an array containing any status that
# is considered an open bug.

sub OpenStates {
    return ('NEW', 'REOPENED', 'ASSIGNED', $::unconfirmedstate);
}


sub RemoveVotes {
    my ($id, $who, $reason) = (@_);
    my $whopart = "";
    if ($who) {
        $whopart = " AND votes.who = $who";
    }
    SendSQL("SELECT profiles.login_name, profiles.userid, votes.count, " .
            "products.votesperuser, products.maxvotesperbug " .
            "FROM profiles " . 
            "LEFT JOIN votes ON profiles.userid = votes.who " .
            "LEFT JOIN bugs USING(bug_id) " .
            "LEFT JOIN products ON products.id = bugs.product_id " .
            "WHERE votes.bug_id = $id " .
            $whopart);
    my @list;
    while (MoreSQLData()) {
        my ($name, $userid, $oldvotes, $votesperuser, $maxvotesperbug) = (FetchSQLData());
        push(@list, [$name, $userid, $oldvotes, $votesperuser, $maxvotesperbug]);
    }
    if (0 < @list) {
        foreach my $ref (@list) {
            my ($name, $userid, $oldvotes, $votesperuser, $maxvotesperbug) = (@$ref);
            my $s;

            $maxvotesperbug = $votesperuser if ($votesperuser < $maxvotesperbug);

            # If this product allows voting and the user's votes are in
            # the acceptable range, then don't do anything.
            next if $votesperuser && $oldvotes <= $maxvotesperbug;

            # If the user has more votes on this bug than this product
            # allows, then reduce the number of votes so it fits
            my $newvotes = $votesperuser ? $maxvotesperbug : 0;

            my $removedvotes = $oldvotes - $newvotes;

            $s = $oldvotes == 1 ? "" : "s";
            my $oldvotestext = "You had $oldvotes vote$s on this bug.";

            $s = $removedvotes == 1 ? "" : "s";
            my $removedvotestext = "You had $removedvotes vote$s removed from this bug.";

            my $newvotestext;
            if ($newvotes) {
                SendSQL("UPDATE votes SET count = $newvotes " .
                        "WHERE bug_id = $id AND who = $userid");
                $s = $newvotes == 1 ? "" : "s";
                $newvotestext = "You still have $newvotes vote$s on this bug."
            } else {
                SendSQL("DELETE FROM votes WHERE bug_id = $id AND who = $userid");
                $newvotestext = "You have no more votes remaining on this bug.";
            }

            # Notice that we did not make sure that the user fit within the $votesperuser
            # range.  This is considered to be an acceptable alternative to losing votes
            # during product moves.  Then next time the user attempts to change their votes,
            # they will be forced to fit within the $votesperuser limit.

            # Now lets send the e-mail to alert the user to the fact that their votes have
            # been reduced or removed.
            my $sendmailparm = '-ODeliveryMode=deferred';
            if (Param('sendmailnow')) {
               $sendmailparm = '';
            }
            if (open(SENDMAIL, "|/usr/lib/sendmail $sendmailparm -t -i")) {
                my %substs;

                $substs{"to"} = $name;
                $substs{"bugid"} = $id;
                $substs{"reason"} = $reason;

                $substs{"votesremoved"} = $removedvotes;
                $substs{"votesold"} = $oldvotes;
                $substs{"votesnew"} = $newvotes;

                $substs{"votesremovedtext"} = $removedvotestext;
                $substs{"votesoldtext"} = $oldvotestext;
                $substs{"votesnewtext"} = $newvotestext;

                $substs{"count"} = $removedvotes . "\n    " . $newvotestext;

                my $msg = PerformSubsts(Param("voteremovedmail"),
                                        \%substs);
                print SENDMAIL $msg;
                close SENDMAIL;
            }
        }
        SendSQL("SELECT SUM(count) FROM votes WHERE bug_id = $id");
        my $v = FetchOneColumn();
        $v ||= 0;
        SendSQL("UPDATE bugs SET votes = $v, delta_ts = delta_ts " .
                "WHERE bug_id = $id");
    }
}

# Take two comma or space separated strings and return what
# values were removed from or added to the new one.
sub DiffStrings {
    my ($oldstr, $newstr) = @_;

    # Split the old and new strings into arrays containing their values.
    $oldstr =~ s/[\s,]+/ /g;
    $newstr =~ s/[\s,]+/ /g;
    my @old = split(" ", $oldstr);
    my @new = split(" ", $newstr);

    my (@remove, @add) = ();

    # Find values that were removed
    foreach my $value (@old) {
        push (@remove, $value) if !grep($_ eq $value, @new);
    }

    # Find values that were added
    foreach my $value (@new) {
        push (@add, $value) if !grep($_ eq $value, @old);
    }

    my $removed = join (", ", @remove);
    my $added = join (", ", @add);

    return ($removed, $added);
}

sub PerformSubsts {
    my ($str, $substs) = (@_);
    $str =~ s/%([a-z]*)%/(defined $substs->{$1} ? $substs->{$1} : Param($1))/eg;
    return $str;
}

sub FormatTimeUnit {
    # Returns a number with 2 digit precision, unless the last digit is a 0
    # then it returns only 1 digit precision
    my ($time) = (@_);
 
    my $newtime = sprintf("%.2f", $time);

    if ($newtime =~ /0\Z/) {
        $newtime = sprintf("%.1f", $time);
    }

    return $newtime;
    
}
###############################################################################
# Global Templatization Code

# Use the template toolkit (http://www.template-toolkit.org/) to generate
# the user interface using templates in the "template/" subdirectory.
use Template;

# Create the global template object that processes templates and specify
# configuration parameters that apply to all templates processed in this script.

# IMPORTANT - If you make any configuration changes here, make sure to make
# them in t/004.template.t and checksetup.pl. You may also need to change the
# date settings were last changed - see the comments in checksetup.pl for
# details
$::template ||= Template->new(
  {
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "template/en/custom:template/en/default" ,

    # Remove white-space before template directives (PRE_CHOMP) and at the
    # beginning and end of templates and template blocks (TRIM) for better 
    # looking, more compact content.  Use the plus sign at the beginning 
    # of directives to maintain white space (i.e. [%+ DIRECTIVE %]).
    PRE_CHOMP => 1 ,
    TRIM => 1 , 

    COMPILE_DIR => 'data/',

    # Functions for processing text within templates in various ways.
    # IMPORTANT!  When adding a filter here that does not override a
    # built-in filter, please also add a stub filter to checksetup.pl
    # and t/004template.t.
    FILTERS =>
      {
        # Render text in strike-through style.
        strike => sub { return "<strike>" . $_[0] . "</strike>" } ,

        # Returns the text with backslashes, single/double quotes,
        # and newlines/carriage returns escaped for use in JS strings.
        js => sub
        {
            my ($var) = @_;
            $var =~ s/([\\\'\"])/\\$1/g; 
            $var =~ s/\n/\\n/g; 
            $var =~ s/\r/\\r/g; 
            return $var;
        } , 

        # HTML collapses newlines in element attributes to a single space,
        # so form elements which may have whitespace (ie comments) need
        # to be encoded using &#013;
        # See bugs 4928, 22983 and 32000 for more details
        html_linebreak => sub
        {
            my ($var) = @_;
            $var =~ s/\r\n/\&#013;/g;
            $var =~ s/\n\r/\&#013;/g;
            $var =~ s/\r/\&#013;/g;
            $var =~ s/\n/\&#013;/g;
            return $var;
        } ,

        # This subroutine in CGI.pl escapes characters in a variable
        # or value string for use in a query string.  It escapes all
        # characters NOT in the regex set: [a-zA-Z0-9_\-.].  The 'uri'
        # filter should be used for a full URL that may have
        # characters that need encoding.
        url_quote => \&Bugzilla::Util::url_quote ,

        quoteUrls => \&quoteUrls ,

        bug_link => [ sub {
                          my ($context, $bug) = @_;
                          return sub {
                              my $text = shift;
                              return GetBugLink($text, $bug);
                          };
                      },
                      1
                    ],

        # In CSV, quotes are doubled, and we enclose the whole value in quotes
        csv => sub
        {
            my ($var) = @_;
            $var =~ s/"/""/g;
            if ($var !~ /^-?(\d+\.)?\d*$/) {
                $var = "\"$var\"";
            }
            return $var;
        } ,

        # Format a time for display (more info in Bugzilla::Util)
        time => \&Bugzilla::Util::format_time,
      } ,
  }
) || die("Template creation failed: " . Template->error());

# Use the Toolkit Template's Stash module to add utility pseudo-methods
# to template variables.
use Template::Stash;

# Add "contains***" methods to list variables that search for one or more 
# items in a list and return boolean values representing whether or not 
# one/all/any item(s) were found.
$Template::Stash::LIST_OPS->{ contains } =
  sub {
      my ($list, $item) = @_;
      return grep($_ eq $item, @$list);
  };

$Template::Stash::LIST_OPS->{ containsany } =
  sub {
      my ($list, $items) = @_;
      foreach my $item (@$items) { 
          return 1 if grep($_ eq $item, @$list);
      }
      return 0;
  };

# Add a "substr" method to the Template Toolkit's "scalar" object
# that returns a substring of a string.
$Template::Stash::SCALAR_OPS->{ substr } = 
  sub {
      my ($scalar, $offset, $length) = @_;
      return substr($scalar, $offset, $length);
  };
    
# Add a "truncate" method to the Template Toolkit's "scalar" object
# that truncates a string to a certain length.
$Template::Stash::SCALAR_OPS->{ truncate } = 
  sub {
      my ($string, $length, $ellipsis) = @_;
      $ellipsis ||= "";
      
      return $string if !$length || length($string) <= $length;
      
      my $strlen = $length - length($ellipsis);
      my $newstr = substr($string, 0, $strlen) . $ellipsis;
      return $newstr;
  };
    
###############################################################################

# Constructs a format object from URL parameters. You most commonly call it 
# like this:
# my $format = GetFormat("foo/bar", $::FORM{'format'}, $::FORM{'ctype'});
sub GetFormat {
    my ($template, $format, $ctype) = @_;
    
    $ctype ||= "html";
    $format ||= "";
    
    # Security - allow letters and a hyphen only
    $ctype =~ s/[^a-zA-Z\-]//g;
    $format =~ s/[^a-zA-Z\-]//g;
    trick_taint($ctype);
    trick_taint($format);
    
    $template .= ($format ? "-$format" : "");
    $template .= ".$ctype.tmpl";
        
    return 
    { 
        'template'    => $template , 
        'extension'   => $ctype , 
        'ctype' => $::contenttypes->{$ctype} || "text/plain" , 
    };
}

###############################################################################

# Define the global variables and functions that will be passed to the UI
# template.  Additional values may be added to this hash before templates
# are processed.
$::vars =
  {
    # Function for retrieving global parameters.
    'Param' => \&Param ,

    # Function to create date strings
    'time2str' => \&time2str ,

    # Function for processing global parameters that contain references
    # to other global parameters.
    'PerformSubsts' => \&PerformSubsts ,

    # Generic linear search function
    'lsearch' => \&Bugzilla::Util::lsearch ,

    # UserInGroup - you probably want to cache this
    'UserInGroup' => \&UserInGroup ,

    # SyncAnyPendingShadowChanges - called in the footer to sync the shadowdb
    'SyncAnyPendingShadowChanges' => \&SyncAnyPendingShadowChanges ,
    
    # User Agent - useful for detecting in templates
    'user_agent' => $ENV{'HTTP_USER_AGENT'} ,

    # Bugzilla version
    'VERSION' => $Bugzilla::Config::VERSION,
  };

1;

