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

# Contains some global variables and routines used throughout bugzilla.

use diagnostics;
use strict;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
    $zz = @main::SqlStateStack;
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
    $zz = $main::superusergroupset;
}

#
# Here are the --LOCAL-- variables defined in 'localconfig' that we'll use
# here
# 

$::db_host = "localhost";
$::db_name = "bugs";
$::db_user = "bugs";
$::db_pass = "";

do 'localconfig';

use DBI;

use Date::Format;               # For time2str().
use Date::Parse;               # For str2time().
#use Carp;                       # for confess
use RelationSet;

# $ENV{PATH} is not taint safe
delete $ENV{PATH};

# Contains the version string for the current running Bugzilla.
$::param{'version'} = '2.15';

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

sub ConnectToDatabase {
    my ($useshadow) = (@_);
    if (!defined $::db) {
        my $name = $::db_name;
        if ($useshadow && Param("shadowdb") && Param("queryagainstshadowdb")) {
            $name = Param("shadowdb");
            $::dbwritesallowed = 0;
        }
        $::db = DBI->connect("DBI:mysql:host=$::db_host;database=$name", $::db_user, $::db_pass)
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
        
sub Milestone_element {
    my ($tm, $prod, $onchange) = (@_);
    my $tmlist;
    if (!defined $::target_milestone{$prod}) {
        $tmlist = [];
    } else {
        $tmlist = $::target_milestone{$prod};
    }

    my $deftm = $tmlist->[0];

    if (lsearch($tmlist, $tm) >= 0) {
        $deftm = $tm;
    }

    return make_popup("target_milestone", $tmlist, $deftm, 1, $onchange);
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

    my $dotargetmilestone = 1;  # This used to check the param, but there's
                                # enough code that wants to pretend we're using
                                # target milestones, even if they don't get
                                # shown to the user.  So we cache all the data
                                # about them anyway.

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
    mkdir("data", 0777);
    chmod 0777, "data";
    my $tmpname = "data/versioncache.$$";
    open(FID, ">$tmpname") || die "Can't create $tmpname";

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
    print FID GenerateCode('%::prodmaxvotes');
    print FID GenerateCode('$::anyvotesallowed');

    if ($dotargetmilestone) {
        # reading target milestones in from the database - matthew@zeroknowledge.com
        SendSQL("SELECT value, product FROM milestones ORDER BY sortkey, value");
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
    chmod 0666, "data/versioncache";
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
    my ($name, $forceok) = (@_);
    $name = html_quote($name);
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
        print "\n";  # http://bugzilla.mozilla.org/show_bug.cgi?id=80045
        print "The name <TT>$name</TT> is not a valid username.  Either you\n";
        print "misspelled it, or the person has not registered for a\n";
        print "Bugzilla account.\n";
        print "<P>Please hit the <B>Back</B> button and try again.\n";
    }
    exit(0);
}

# Use detaint_string() when you know that there is no way that the data
# in a scalar can be tainted, but taint mode still bails on it.
# WARNING!! Using this routine on data that really could be tainted
#           defeats the purpose of taint mode.  It should only be
#           used on variables that cannot be touched by users.

sub detaint_string {
    my ($str) = @_;
    $str =~ m/^(.*)$/s;
    $str = $1;
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
        $item = GetBugLink($num, $item);
        $things[$count++] = $item;
    }
    while ($text =~ s/\battachment(\s|%\#)*(\d+)/"##$count##"/ei) {
        my $item = $&;
        my $num = $2;
        $item = value_quote($item); # Not really necessary, since we know
                                    # there's no special chars in it.
        $item = qq{<A HREF="showattachment.cgi?attach_id=$num">$item</A>};
        $things[$count++] = $item;
    }
    while ($text =~ s/\*\*\* This bug has been marked as a duplicate of (\d+) \*\*\*/"##$count##"/ei) {
        my $item = $&;
        my $num = $1;
        my $bug_link;
        $bug_link = GetBugLink($num, $num);
        $item =~ s@\d+@$bug_link@;
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
    $text =~ s/\&#013;/\n/g;

    # Stuff everything back from the array.
    for (my $i=0 ; $i<$count ; $i++) {
        $text =~ s/##$i##/$things[$i]/e;
    }

    # And undo the quoting of "#" characters.
    $text =~ s/%#/#/g;

    return $text;
}

# This is a new subroutine written 12/20/00 for the purpose of processing a
# link to a bug.  It can be called using "GetBugLink (<BugNumber>, <LinkText>);"
# Where <BugNumber> is the number of the bug and <LinkText> is what apprears
# between '<a>' and '</a>'.

sub GetBugLink {
    my ($bug_num, $link_text) = (@_);
    my ($link_return) = "";

    # TODO - Add caching capabilites... possibly use a global variable in the form
    # of $buglink{$bug_num} that contains the text returned by this sub.  If that
    # variable is defined, simply return it's value rather than running the SQL
    # query.  This would cut down on the number of SQL calls when the same bug is
    # referenced multiple times.
    
    # Make sure any unfetched data from a currently running query
    # is saved off rather than overwritten
    PushGlobalSQLState();
    
    # Get this bug's info from the SQL Database
    SendSQL("select bugs.bug_status, resolution, short_desc, groupset
             from bugs where bugs.bug_id = $bug_num");
    my ($bug_stat, $bug_res, $bug_desc, $bug_grp) = (FetchSQLData());
    
    # Format the retrieved information into a link
    if ($bug_stat eq "UNCONFIRMED") { $link_return .= "<i>" }
    if ($bug_res ne "") { $link_return .= "<strike>" }
    $bug_desc = value_quote($bug_desc);
    $link_text = value_quote($link_text);
    $link_return .= qq{<a href="show_bug.cgi?id=$bug_num" title="$bug_stat};
    if ($bug_res ne "") {$link_return .= " $bug_res"}
    if ($bug_grp == 0) { $link_return .= " - $bug_desc" }
    $link_return .= qq{">$link_text</a>};
    if ($bug_res ne "") { $link_return .= "</strike>" }
    if ($bug_stat eq "UNCONFIRMED") { $link_return .= "</i>"}
    
    # Put back any query in progress
    PopGlobalSQLState();

    return $link_return; 

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
            $result .= "\n\n------- Additional Comments From $who".Param('emailsuffix')."  ".
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

    my ($query) = ("SELECT profiles.realname, profiles.login_name, longdescs.bug_when, " .
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
        my ($who, $email, $when, $text) = (FetchSQLData());
        $email .= Param('emailsuffix');
        if ($count) {
            $result .= "<BR><BR><I>------- Additional Comments From ";
              if ($who) {
                  $result .= qq{<A HREF="mailto:$email">$who</A> } .
                      time2str("%Y-%m-%d %H:%M", str2time($when)) .
                          " -------</I><BR>\n";
              } else {
                  $result .= qq{<A HREF="mailto:$email">$email</A> } .
                      time2str("%Y-%m-%d %H:%M", str2time($when)) .
                          " -------</I><BR>\n";
              }
        }
        $result .= "<PRE>" . quoteUrls(\%knownattachments, $text) . "</PRE>\n";
        $count++;
    }

    return $result;
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
    $str = detaint_string($str);
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
    ConnectToDatabase();
    SendSQL("select count(*) from groups where name=" . SqlQuote($groupname));
    my $count = FetchOneColumn();
    return $count;
}

# Given the name of an existing group, returns the bit associated with it.
# If the group does not exist, returns 0.
# !!! Remove this function when the new group system is implemented!
sub GroupNameToBit {
    my ($groupname) = (@_);
    ConnectToDatabase();
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
    ConnectToDatabase();
    SendSQL("select isactive from groups where bit=$groupbit");
    my $isactive = FetchOneColumn();
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
    ConnectToDatabase();
    my $whopart = "";
    if ($who) {
        $whopart = " AND votes.who = $who";
    }
    SendSQL("SELECT profiles.login_name, profiles.userid, votes.count, " .
            "products.votesperuser, products.maxvotesperbug " .
            "FROM profiles " . 
            "LEFT JOIN votes ON profiles.userid = votes.who " .
            "LEFT JOIN bugs USING(bug_id) " .
            "LEFT JOIN products USING(product)" .
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
            if (open(SENDMAIL, "|/usr/lib/sendmail $sendmailparm -t")) {
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


# Trim whitespace from front and back.

sub trim {
    ($_) = (@_);
    s/^\s+//g;
    s/\s+$//g;
    return $_;
}

1;
