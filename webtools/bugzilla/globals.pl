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
#                 Jake <jake@acutex.net>
#                 Bradley Baetz <bbaetz@cs.mcgill.ca>
#                 Christopher Aillon <christopher@aillon.com>

# Contains some global variables and routines used throughout bugzilla.

use strict;

use Bugzilla::Util;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
    $zz = @main::SqlStateStack;
    $zz = @main::chooseone;
    $zz = $main::contenttypes;
    $zz = @main::default_column_list;
    $zz = $main::defaultqueryname;
    $zz = @main::dontchange;
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
    $zz = %main::param_type;
    $zz = %main::proddesc;
    $zz = @main::prodmaxvotes;
    $zz = $main::superusergroupset;
    $zz = $main::template;
    $zz = $main::userid;
    $zz = $main::vars;
}

#
# Here are the --LOCAL-- variables defined in 'localconfig' that we'll use
# here
# 

$::db_host = "localhost";
$::db_port = 3306;
$::db_name = "bugs";
$::db_user = "bugs";
$::db_pass = "";

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

# Contains the version string for the current running Bugzilla.
$::param{'version'} = '2.17';

$::dontchange = "--do_not_change--";
$::chooseone = "--Choose_one:--";
$::defaultqueryname = "(Default query)";
$::unconfirmedstate = "UNCONFIRMED";
$::dbwritesallowed = 1;

# Adding a global variable for the value of the superuser groupset.
# Joe Robins, 7/5/00
$::superusergroupset = "9223372036854775807";

#sub die_with_dignity {
#    my ($err_msg) = @_;
#    print $err_msg;
#    confess($err_msg);
#}
#$::SIG{__DIE__} = \&die_with_dignity;

# Some files in the data directory must be world readable iff we don't have
# a webserver group. Call this function to do this.
sub ChmodDataFile($$) {
    my ($file, $mask) = @_;
    my $perm = 0770;
    if ((stat('data'))[2] & 0002) {
        $perm = 0777;
    }
    $perm = $perm & $mask;
    chmod $perm,$file;
}

sub ConnectToDatabase {
    my ($useshadow) = (@_);
    if (!defined $::db) {
        my $name = $::db_name;
        if ($useshadow && Param("shadowdb") && Param("queryagainstshadowdb")) {
            $name = Param("shadowdb");
            $::dbwritesallowed = 0;
        }
        $::db = DBI->connect("DBI:mysql:host=$::db_host;database=$name;port=$::db_port", $::db_user, $::db_pass)
            || die "Bugzilla is currently broken. Please try again later. " . 
      "If the problem persists, please contact " . Param("maintainer") .
      ". The error you should quote is: " . $DBI::errstr;
    }
}

sub ReconnectToShadowDatabase {
    if (Param("shadowdb") && Param("queryagainstshadowdb")) {
        SendSQL("USE " . Param("shadowdb"));
        $::dbwritesallowed = 0;
    }
}

my $shadowchanges = 0;
sub SyncAnyPendingShadowChanges {
    if ($shadowchanges) {
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
        die "Evil code attempted to write stuff to the shadow database.";
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

    

@::default_column_list = ("severity", "priority", "platform", "owner",
                          "status", "resolution", "summary");

sub AppendComment {
    my ($bugid,$who,$comment,$isprivate) = (@_);
    $comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
    $comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
    if ($comment =~ /^\s*$/) {  # Nothin' but whitespace.
        return;
    }

    my $whoid = DBNameToIdAndCheck($who);
    my $privacyval = $isprivate ? 1 : 0 ;
    SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext, isprivate) " .
        "VALUES($bugid, $whoid, now(), " . SqlQuote($comment) . ", " . 
        $privacyval . ")");

    SendSQL("UPDATE bugs SET delta_ts = now() WHERE bug_id = $bugid");
}

sub GetFieldID {
    my ($f) = (@_);
    SendSQL("SELECT fieldid FROM fielddefs WHERE name = " . SqlQuote($f));
    my $fieldid = FetchOneColumn();
    die "Unknown field id: $f" if !$fieldid;
    return $fieldid;
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
                                    "{" . PerlQuote($k) . "}");
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

    print FID GenerateCode('@::log_columns');
    print FID GenerateCode('%::versions');

    foreach my $i (@list) {
        if (!defined $::components{$i}) {
            $::components{$i} = [];
        }
    }
    @::legal_versions = sort {uc($a) cmp uc($b)} keys(%varray);
    print FID GenerateCode('@::legal_versions');
    print FID GenerateCode('%::components');
    @::legal_components = sort {uc($a) cmp uc($b)} keys(%carray);
    print FID GenerateCode('@::legal_components');
    foreach my $i('product', 'priority', 'severity', 'platform', 'opsys',
                  'bug_status', 'resolution') {
        print FID GenerateCode('@::legal_' . $i);
    }
    print FID GenerateCode('@::settable_resolution');
    print FID GenerateCode('%::proddesc');
    print FID GenerateCode('@::enterable_products');
    print FID GenerateCode('%::prodmaxvotes');

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

        print FID GenerateCode('%::target_milestone');
        print FID GenerateCode('@::legal_target_milestone');
        print FID GenerateCode('%::milestoneurl');
    }

    SendSQL("SELECT id, name FROM keyworddefs ORDER BY name");
    while (MoreSQLData()) {
        my ($id, $name) = FetchSQLData();
        push(@::legal_keywords, $name);
        $name = lc($name);
        $::keywordsbyname{$name} = $id;
    }
    print FID GenerateCode('@::legal_keywords');
    print FID GenerateCode('%::keywordsbyname');

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



# This proc must be called before using legal_product or the versions array.

$::VersionTableLoaded = 0;
sub GetVersionTable {
    return if $::VersionTableLoaded;
    my $mtime = ModTime("data/versioncache");
    if (!defined $mtime || $mtime eq "") {
        $mtime = 0;
    }
    if (time() - $mtime > 3600) {
        use Token;
        Token::CleanTokenTable();
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

    # Reject if the new login is part of an email change which is 
    # still in progress
    SendSQL("SELECT eventdata FROM tokens WHERE tokentype = 'emailold' 
                AND eventdata like '%:$username' 
                 OR eventdata like '$username:%'");
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

    # Determine what groups the user should be in by default
    # and add them to those groups.
    PushGlobalSQLState();
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

    # Insert the new user record into the database.            
    $username = SqlQuote($username);
    $realname = SqlQuote($realname);
    $cryptpassword = SqlQuote($cryptpassword);
    SendSQL("INSERT INTO profiles (login_name, realname, cryptpassword, groupset) 
             VALUES ($username, $realname, $cryptpassword, $groupset)");
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

sub SelectVisible {
    my ($query, $userid, $usergroupset) = @_;

    # Run the SQL $query with the additional restriction that
    # the bugs can be seen by $userid. $usergroupset is provided
    # as an optimisation when this is already known, eg from CGI.pl
    # If not present, it will be obtained from the db.
    # Assumes that 'bugs' is mentioned as a table name. You should
    # also make sure that bug_id is qualified bugs.bug_id!
    # Your query must have a WHERE clause. This is unlikely to be a problem.

    # Also, note that mySQL requires aliases for tables to be locked, as well
    # This means that if you change the name from selectVisible_cc (or add
    # additional tables), you will need to update anywhere which does a
    # LOCK TABLE, and then calls routines which call this

    $usergroupset = 0 unless $userid;

    unless (defined($usergroupset)) {
        PushGlobalSQLState();
        SendSQL("SELECT groupset FROM profiles WHERE userid = $userid");
        $usergroupset = FetchOneColumn();
        PopGlobalSQLState();
    }

    # Users are authorized to access bugs if they are a member of all 
    # groups to which the bug is restricted.  User group membership and 
    # bug restrictions are stored as bits within bitsets, so authorization
    # can be determined by comparing the intersection of the user's
    # bitset with the bug's bitset.  If the result matches the bug's bitset
    # the user is a member of all groups to which the bug is restricted
    # and is authorized to access the bug.

    # A user is also authorized to access a bug if she is the reporter, 
    # or member of the cc: list of the bug and the bug allows users in those
    # roles to see the bug.  The boolean fields reporter_accessible and 
    # cclist_accessible identify whether or not those roles can see the bug.

    # Bit arithmetic is performed by MySQL instead of Perl because bitset
    # fields in the database are 64 bits wide (BIGINT), and Perl installations
    # may or may not support integers larger than 32 bits.  Using bitsets
    # and doing bitset arithmetic is probably not cross-database compatible,
    # however, so these mechanisms are likely to change in the future.

    my $replace = " ";

    if ($userid) {
        $replace .= "LEFT JOIN cc selectVisible_cc ON 
                     bugs.bug_id = selectVisible_cc.bug_id AND 
                     selectVisible_cc.who = $userid "
    }

    $replace .= "WHERE ((bugs.groupset & $usergroupset) = bugs.groupset ";

    if ($userid) {
        # There is a mysql bug affecting v3.22 and 3.23 (at least), where this will
        # cause all rows to be returned! We work arround this by adding an not isnull
        # test to the JOINed cc table. See http://lists.mysql.com/cgi-ez/ezmlm-cgi?9:mss:11417
        # Its needed, even though it shouldn't be
        $replace .= "OR (bugs.reporter_accessible = 1 AND bugs.reporter = $userid)" .
          " OR (bugs.cclist_accessible = 1 AND selectVisible_cc.who = $userid AND not isnull(selectVisible_cc.who))" .
          " OR (bugs.assigned_to = $userid)";
        if (Param("useqacontact")) {
            $replace .= " OR (bugs.qa_contact = $userid)";
        }
    }

    $replace .= ") AND ";

    $query =~ s/\sWHERE\s/$replace/i;

    return $query;
}

sub CanSeeBug {
    # Note that we pass in the usergroupset, since this is known
    # in most cases (ie viewing bugs). Maybe make this an optional
    # parameter?

    my ($id, $userid, $usergroupset) = @_;

    # Query the database for the bug, retrieving a boolean value that
    # represents whether or not the user is authorized to access the bug.

    PushGlobalSQLState();
    SendSQL(SelectVisible("SELECT bugs.bug_id FROM bugs WHERE bugs.bug_id = $id",
                          $userid, $usergroupset));

    my $ret = defined(FetchSQLData());
    PopGlobalSQLState();

    return $ret;
}

sub ValidatePassword {
    # Determines whether or not a password is valid (i.e. meets Bugzilla's
    # requirements for length and content).  If the password is valid, the
    # function returns boolean false.  Otherwise it returns an error message
    # (synonymous with boolean true) that can be displayed to the user.
    
    # If a second password is passed in, this function also verifies that
    # the two passwords match.

    my ($password, $matchpassword) = @_;
    
    if ( length($password) < 3 ) {
        return "The password is less than three characters long.  It must be at least three characters.";
    } elsif ( length($password) > 16 ) {
        return "The password is more than 16 characters long.  It must be no more than 16 characters.";
    } elsif ( $matchpassword && $password ne $matchpassword ) { 
        return "The two passwords do not match.";
    }

    return 0;
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
    die "non-numeric prod_id '$prod_id' passed to get_component_id"
      unless ($prod_id =~ /^\d+$/);
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
# It takes either two or three paramaters:
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

        SendSQL("SELECT bugs.bug_status, resolution, short_desc, groupset " .
                "FROM bugs WHERE bugs.bug_id = $bug_num");

        # If the bug exists, save its data off for use later in the sub
        if (MoreSQLData()) {
            my ($bug_state, $bug_res, $bug_desc, $bug_grp) = FetchSQLData();
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
            if ($bug_grp == 0 || CanSeeBug($bug_num, $::userid, $::usergroupset)) {
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
    my ($query) = ("SELECT profiles.login_name, longdescs.bug_when, " .
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
        my ($who, $when, $text, $isprivate) = (FetchSQLData());
        if ($count) {
            $result .= "\n\n------- Additional Comments From $who".Param('emailsuffix')."  ".
                time2str("%Y-%m-%d %H:%M", str2time($when)) . " -------\n";
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
                     date_format(longdescs.bug_when,'%Y-%m-%d %H:%i'), 
                     longdescs.thetext,
                     isprivate,
                     date_format(longdescs.bug_when,'%Y%m%d%H%i%s') 
            FROM     longdescs, profiles
            WHERE    profiles.userid = longdescs.who 
              AND    longdescs.bug_id = $id 
            ORDER BY longdescs.bug_when");
             
    while (MoreSQLData()) {
        my %comment;
        ($comment{'name'}, $comment{'email'}, $comment{'time'}, $comment{'body'},
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



sub UserInGroup {
    my ($groupname) = (@_);
    if ($::usergroupset eq "0") {
        return 0;
    }
    PushGlobalSQLState();
    SendSQL("select (bit & $::usergroupset) != 0 from groups where name = " . SqlQuote($groupname));
    my $bit = FetchOneColumn();
    PopGlobalSQLState();
    if ($bit) {
        return 1;
    }
    return 0;
}

sub BugInGroup {
    my ($bugid, $groupname) = (@_);
    my $groupbit = GroupNameToBit($groupname);
    PushGlobalSQLState();
    SendSQL("SELECT (bugs.groupset & $groupbit) != 0 FROM bugs WHERE bugs.bug_id = $bugid");
    my $bugingroup = FetchOneColumn();
    PopGlobalSQLState();
    return $bugingroup;
}

sub GroupExists {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("select count(*) from groups where name=" . SqlQuote($groupname));
    my $count = FetchOneColumn();
    PopGlobalSQLState();
    return $count;
}

# Given the name of an existing group, returns the bit associated with it.
# If the group does not exist, returns 0.
# !!! Remove this function when the new group system is implemented!
sub GroupNameToBit {
    my ($groupname) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT bit FROM groups WHERE name = " . SqlQuote($groupname));
    my $bit = FetchOneColumn() || 0;
    PopGlobalSQLState();
    return $bit;
}

# Determines whether or not a group is active by checking 
# the "isactive" column for the group in the "groups" table.
# Note: This function selects groups by bit rather than by name.
sub GroupIsActive {
    my ($groupbit) = (@_);
    $groupbit ||= 0;
    PushGlobalSQLState();
    SendSQL("select isactive from groups where bit=$groupbit");
    my $isactive = FetchOneColumn();
    PopGlobalSQLState();
    return $isactive;
}

# Determines if the given bug_status string represents an "Opened" bug.  This
# routine ought to be paramaterizable somehow, as people tend to introduce
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

sub Param ($) {
    my ($value) = (@_);
    if (! defined $::param{$value}) {
        # Um, maybe we haven't sourced in the params at all yet.
        if (stat("data/params")) {
            # Write down and restore the version # here.  That way, we get 
            # around anyone who maliciously tries to tweak the version number
            # by editing the params file.  Not to mention that in 2.0, there 
            # was a bug that wrote the version number out to the params file...
            my $v = $::param{'version'};
            require "data/params";
            $::param{'version'} = $v;
        }
    }

    if (! defined $::param{$value}) {
        # Well, that didn't help.  Maybe it's a new param, and the user
        # hasn't defined anything for it.  Try and load a default value
        # for it.
        require "defparams.pl";
        WriteParams();
    }

    # If it's still not defined, we're pimped.
    die "Can't find param named $value" if (! defined $::param{$value});

    ## Check to make sure the entry in $::param_type is there; if we don't, we
    ## get 'use of uninitialized constant' errors (see bug 162217). 
    ## Interestingly  enough, placing this check in the die above causes 
    ## deaths on some params (the "languages" param?) because they don't have
    ## a type? Odd... seems like a bug to me... but what do I know? -jpr

    if (defined $::param_type{$value} && $::param_type{$value} eq "m") {
        my $valueList = eval($::param{$value});
        return $valueList if (!($@) && ref($valueList) eq "ARRAY");
        die "Multi-list param '$value' eval() failure ('$@'); data/params is horked";
    }
    else {
        return $::param{$value};
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
        url_quote => \&url_quote ,
      } ,
  }
) || DisplayError("Template creation failed: " . Template->error())
  && exit;

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

sub GetOutputFormats {
    # Builds a set of possible output formats for a script by looking for
    # format files in the appropriate template directories as specified by 
    # the template include path, the sub-directory parameter, and the
    # template name parameter.
    
    # This function is relevant for scripts with one basic function whose
    # results can be represented in multiple formats, f.e. buglist.cgi, 
    # which has one function (query and display of a list of bugs) that can 
    # be represented in multiple formats (i.e. html, rdf, xml, etc.).
    
    # It is *not* relevant for scripts with several functions but only one
    # basic output format, f.e. editattachstatuses.cgi, which not only lists 
    # statuses but also provides adding, editing, and deleting functions.
    # (although it may be possible to make this function applicable under 
    # these circumstances with minimal modification).
    
    # Format files have names that look like SCRIPT-FORMAT.EXT.tmpl, where
    # SCRIPT is the name of the CGI script being invoked, SUBDIR is the name 
    # of the template sub-directory, FORMAT is the name of the format, and EXT 
    # is the filename extension identifying the content type of the output.
     
    # When a format file is found, a record for that format is added to
    # the hash of format records, indexed by format name, with each record
    # containing the name of the format file, its filename extension,
    # and its content type (obtained by reference to the $::contenttypes
    # hash defined in localconfig).
    
    my ($subdir, $script) = @_;

    # A set of output format records, indexed by format name, each record 
    # containing template, extension, and contenttype fields.
    my $formats = {};
    
    # Get the template include path from the template object.
    my $includepath = $::template->context->{ LOAD_TEMPLATES }->[0]->include_path();
    
    # Loop over each include directory in reverse so that format files
    # earlier in the path override files with the same name later in
    # the path (i.e. "custom" formats override "default" ones).
    foreach my $path (reverse @$includepath) {
        # Get the list of files in the given sub-directory if it exists.
        my $dirname = File::Spec->catdir($path, $subdir);
        opendir(SUBDIR, $dirname) || next;
        my @files = readdir SUBDIR;
        closedir SUBDIR;
        
        # Loop over each file in the sub-directory looking for format files
        # (files whose name looks like SCRIPT-FORMAT.EXT.tmpl).
        foreach my $file (@files) {
            if ($file =~ /^\Q$script\E-(.+)\.(.+)\.tmpl$/) {
                # This must be a valid file
                # If an attacker could add a previously unused format
                # type to trick us into running it, then they could just
                # change an existing one...
                # (This implies that running without a webservergroup is
                # insecure, but that is the case anyway)
                trick_taint($file);

                $formats->{$1} = { 
                  'template'    => $file , 
                  'extension'   => $2 , 
                  'contenttype' => $::contenttypes->{$2} || "text/plain" , 
                };
            }
        }
    }
    return $formats;
}

sub ValidateOutputFormat {
    my ($format, $script, $subdir) = @_;
    
    # If the script name is undefined, assume the script currently being
    # executed, deriving its name from Perl's built-in $0 (program name) var.
    if (!defined($script)) {
        my ($volume, $dirs, $filename) = File::Spec->splitpath($0);
        $filename =~ /^(.+)\.cgi$/;
        $script = $1
          || DisplayError("Could not determine the name of the script.")
          && exit;
    }
    
    # If the format name is undefined or the default format is specified,
    # do not do any validation but instead return the default format.
    if (!defined($format) || $format eq "default") {
        return 
          { 
            'template'    => "$script.html.tmpl" , 
            'extension'   => "html" , 
            'contenttype' => "text/html" , 
          };
    }
    
    # If the subdirectory name is undefined, assume the script name.
    $subdir = $script if !defined($subdir);
    
    # Get the list of output formats supported by this script.
    my $formats = GetOutputFormats($subdir, $script);
    
    # Validate the output format requested by the user.
    if (!$formats->{$format}) {
        my $escapedname = html_quote($format);
        DisplayError("The <em>$escapedname</em> output format is not 
          supported by this script.  Supported formats (besides the 
          default HTML format) are <em>" . 
          join("</em>, <em>", map(html_quote($_), keys(%$formats))) . 
          "</em>.");
        exit;
    }
    
    # Return the validated output format.
    return $formats->{$format};
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
  };

1;
