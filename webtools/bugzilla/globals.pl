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

use Bugzilla::DB qw(:DEFAULT :deprecated);
use Bugzilla::Constants;
use Bugzilla::Util;
# Bring ChmodDataFile in until this is all moved to the module
use Bugzilla::Config qw(:DEFAULT ChmodDataFile $localconfig $datadir);

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
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
do $localconfig;

use DBI;

use Date::Format;               # For time2str().
use Date::Parse;               # For str2time().
use Bugzilla::RelationSet;

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

# The following subroutine is for debugging purposes only.
# Uncommenting this sub and the $::SIG{__DIE__} trap underneath it will
# cause any fatal errors to result in a call stack trace to help track
# down weird errors.
#sub die_with_dignity {
#    use Carp;  # for confess()
#    my ($err_msg) = @_;
#    print $err_msg;
#    confess($err_msg);
#}
#$::SIG{__DIE__} = \&die_with_dignity;

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
          ThrowUserError("need_numeric_value", {}, "abort");
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
        if (!$dis && scalar($::components{$p})) {
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

    require File::Temp;
    my ($fh, $tmpname) = File::Temp::tempfile("versioncache.XXXXX",
                                              DIR => "$datadir");

    print $fh "#\n";
    print $fh "# DO NOT EDIT!\n";
    print $fh "# This file is automatically generated at least once every\n";
    print $fh "# hour by the GenerateVersionTable() sub in globals.pl.\n";
    print $fh "# Any changes you make will be overwritten.\n";
    print $fh "#\n";

    require Data::Dumper;
    print $fh (Data::Dumper->Dump([\@::log_columns, \%::versions],
                                  ['*::log_columns', '*::versions']));

    foreach my $i (@list) {
        if (!defined $::components{$i}) {
            $::components{$i} = [];
        }
    }
    @::legal_versions = sort {uc($a) cmp uc($b)} keys(%varray);
    print $fh (Data::Dumper->Dump([\@::legal_versions, \%::components],
                                  ['*::legal_versions', '*::components']));
    @::legal_components = sort {uc($a) cmp uc($b)} keys(%carray);

    print $fh (Data::Dumper->Dump([\@::legal_components, \@::legal_product,
                                   \@::legal_priority, \@::legal_severity,
                                   \@::legal_platform, \@::legal_opsys,
                                   \@::legal_bug_status, \@::legal_resolution],
                                  ['*::legal_components', '*::legal_product',
                                   '*::legal_priority', '*::legal_severity',
                                   '*::legal_platform', '*::legal_opsys',
                                   '*::legal_bug_status', '*::legal_resolution']));

    print $fh (Data::Dumper->Dump([\@::settable_resolution, \%::proddesc,
                                   \@::enterable_products, \%::prodmaxvotes],
                                  ['*::settable_resolution', '*::proddesc',
                                   '*::enterable_products', '*::prodmaxvotes']));

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

        print $fh (Data::Dumper->Dump([\%::target_milestone,
                                       \@::legal_target_milestone,
                                       \%::milestoneurl],
                                      ['*::target_milestone',
                                       '*::legal_target_milestone',
                                       '*::milestoneurl']));
    }

    SendSQL("SELECT id, name FROM keyworddefs ORDER BY name");
    while (MoreSQLData()) {
        my ($id, $name) = FetchSQLData();
        push(@::legal_keywords, $name);
        $name = lc($name);
        $::keywordsbyname{$name} = $id;
    }

    print $fh (Data::Dumper->Dump([\@::legal_keywords, \%::keywordsbyname],
                                  ['*::legal_keywords', '*::keywordsbyname']));

    print $fh "1;\n";
    close $fh;

    rename $tmpname, "$datadir/versioncache" || die "Can't rename $tmpname to versioncache";
    ChmodDataFile("$datadir/versioncache", 0666);
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
    my $mtime = ModTime("$datadir/versioncache");
    if (!defined $mtime || $mtime eq "" || !-r "$datadir/versioncache") {
        $mtime = 0;
    }
    if (time() - $mtime > 3600) {
        use Bugzilla::Token;
        Bugzilla::Token::CleanTokenTable() if Bugzilla->dbwritesallowed;
        GenerateVersionTable();
    }
    require "$datadir/versioncache";
    if (!defined %::versions) {
        GenerateVersionTable();
        do "$datadir/versioncache";

        if (!defined %::versions) {
            die "Can't generate file $datadir/versioncache";
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

sub GenerateRandomPassword {
    my $size = (shift or 10); # default to 10 chars if nothing specified
    return join("", map{ ('0'..'9','a'..'z','A'..'Z')[rand 62] } (1..$size));
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
    if (MoreSQLData()) {
       $::CachedAnyEntryGroups{$product_id} = MoreSQLData();
       FetchSQLData();
       PopGlobalSQLState();
       return $::CachedAnyEntryGroups{$product_id};
    } else {
       return undef;
    }
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
    if (defined Bugzilla->user && %{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', values(%{Bugzilla->user->groups})) . ") ";
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
    if (defined Bugzilla->user && %{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', values(%{Bugzilla->user->groups})) . ") ";
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
# the user can enter bugs.  If the $by_id parameter is true, also retrieves IDs
# and pushes them onto the list as id, name [, id, name...] for easy slurping
# into a hash by the calling code.
sub GetSelectableProducts {
    my ($by_id) = @_;

    my $extra_sql = $by_id ? "id, " : "";

    my $query = "SELECT $extra_sql name " .
                "FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id ";
    if (Param('useentrygroupdefault')) {
        $query .= "AND group_control_map.entry != 0 ";
    } else {
        $query .= "AND group_control_map.membercontrol = " .
                  CONTROLMAPMANDATORY . " ";
    }
    if (defined Bugzilla->user && %{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
                   join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= "WHERE group_id IS NULL ORDER BY name";
    PushGlobalSQLState();
    SendSQL($query);
    my @products = ();
    push(@products, FetchSQLData()) while MoreSQLData();
    PopGlobalSQLState();
    return (@products);
}

# GetSelectableProductHash
# returns a hash containing 
# legal_products => an enterable product list
# legal_(components|versions|milestones) =>
#   the list of components, versions, and milestones of enterable products
# (components|versions|milestones)_by_product
#    => a hash of component lists for each enterable product
# Milestones only get returned if the usetargetmilestones parameter is set.
sub GetSelectableProductHash {
    # The hash of selectable products and their attributes that gets returned
    # at the end of this function.
    my $selectables = {};

    my %products = GetSelectableProducts(1);

    $selectables->{legal_products} = [sort values %products];

    # Run queries that retrieve the list of components, versions,
    # and target milestones (if used) for the selectable products.
    my @tables = qw(components versions);
    push(@tables, 'milestones') if Param('usetargetmilestone');

    PushGlobalSQLState();
    foreach my $table (@tables) {
        # Why oh why can't we standardize on these names?!?
        my $fld = ($table eq "components" ? "name" : "value");

        my $query = "SELECT $fld, product_id FROM $table WHERE product_id IN " .
                    "(" . join(",", keys %products) . ") ORDER BY $fld";
        SendSQL($query);

        my %values;
        my %values_by_product;

        while (MoreSQLData()) {
            my ($name, $product_id) = FetchSQLData();
            next unless $name;
            $values{$name} = 1;
            push @{$values_by_product{$products{$product_id}}}, $name;
        }

        $selectables->{"legal_$table"} = [sort keys %values];
        $selectables->{"${table}_by_product"} = \%values_by_product;
    }
    PopGlobalSQLState();

    return $selectables;
}


sub GetFieldDefs {
    my $extra = "";
    if (!UserInGroup(Param('timetrackinggroup'))) {
        $extra = "WHERE name NOT IN ('estimated time', 'remaining_time', " .
                 "'work_time', 'percentage_complete')";
    }

    my @fields;
    PushGlobalSQLState();
    SendSQL("SELECT name, description FROM fielddefs $extra ORDER BY sortkey");
    while (MoreSQLData()) {
        my ($name, $description) = FetchSQLData();
        push(@fields, { name => $name, description => $description });
    }
    PopGlobalSQLState();

    return(@fields);
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
                    || (Param('useqacontact') && $qa_contact == $userid)
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
    } elsif ((defined $matchpassword) && ($password ne $matchpassword)) {
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

    ThrowUserError("invalid_username",
                   { name => $name }, "abort");
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
    return undef unless ($prod_id && ($prod_id =~ /^\d+$/));
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
    my $protocol_re = qr/(afs|cid|ftp|gopher|http|https|irc|mid|news|nntp|prospero|telnet|view-source|wais)/i;

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

    # attachment links - handle both cases separately for simplicity
    $text =~ s~((?:^Created\ an\ |\b)attachment\s*\(id=(\d+)\))
              ~($things[$count++] = GetAttachmentLink($2, $1)) &&
               ("\0\0" . ($count-1) . "\0\0")
              ~egmx;

    $text =~ s~\b(attachment\s*\#?\s*(\d+))
              ~($things[$count++] = GetAttachmentLink($2, $1)) &&
               ("\0\0" . ($count-1) . "\0\0")
              ~egmxi;

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

# GetAttachmentLink creates a link to an attachment,
# including its title.

sub GetAttachmentLink {
    my ($attachid, $link_text) = @_;
    detaint_natural($attachid) ||
        die "GetAttachmentLink() called with non-integer attachment number";

    # If we've run GetAttachmentLink() for this attachment before,
    # %::attachlink will contain an anonymous array ref of relevant
    # values.  If not, we need to get the information from the database.
    if (! defined $::attachlink{$attachid}) {
        # Make sure any unfetched data from a currently running query
        # is saved off rather than overwritten
        PushGlobalSQLState();

        SendSQL("SELECT bug_id, isobsolete, description 
                 FROM attachments WHERE attach_id = $attachid");

        if (MoreSQLData()) {
            my ($bugid, $isobsolete, $desc) = FetchSQLData();
            my $title = "";
            my $className = "";
            if (CanSeeBug($bugid, $::userid)) {
                $title = $desc;
            }
            if ($isobsolete) {
                $className = "bz_obsolete";
            }
            $::attachlink{$attachid} = [value_quote($title), $className];
        }
        else {
            # Even if there's nothing in the database, we want to save a blank
            # anonymous array in the %::attachlink hash so the query doesn't get
            # run again next time we're called for this attachment number.
            $::attachlink{$attachid} = [];
        }
        # All done with this sidetrip
        PopGlobalSQLState();
    }

    # Now that we know we've got all the information we're gonna get, let's
    # return the link (which is the whole reason we were called :)
    my ($title, $className) = @{$::attachlink{$attachid}};
    # $title will be undefined if the bug didn't exist in the database.
    if (defined $title) {
        my $linkval = "attachment.cgi?id=$attachid&amp;action=view";
        return qq{<a href="$linkval" class="$className" title="$title">$link_text</a>};
    }
    else {
        return qq{$link_text};
    }
}

# GetBugLink creates a link to a bug, including its title.
# It takes either two or three parameters:
#  - The bug number
#  - The link text, to place between the <a>..</a>
#  - An optional comment number, for linking to a particular
#    comment in the bug

sub GetBugLink {
    my ($bug_num, $link_text, $comment_num) = @_;
    $bug_num || return "&lt;missing bug number&gt;";
    detaint_natural($bug_num) || return "&lt;invalid bug number&gt;";

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
                $pre = '<span class="bz_closed">';
                $title .= " $bug_res";
                $post = '</span>';
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

# CountOpenDependencies counts the number of open dependent bugs for a
# list of bugs and returns a list of bug_id's and their dependency count
# It takes one parameter:
#  - A list of bug numbers whose dependencies are to be checked

sub CountOpenDependencies {
    my (@bug_list) = @_;
    my @dependencies;
    
    # Make sure any unfetched data from a currently running query
    # is saved off rather than overwritten
    PushGlobalSQLState();
   
    SendSQL("SELECT blocked, count(bug_status) " .
            "FROM bugs, dependencies " .
            "WHERE blocked IN (" . (join "," , @bug_list) . ") " .
            "AND bug_id = dependson " .
            "AND bug_status IN ('" . (join "','", OpenStates())  . "') " .
            "GROUP BY blocked ");
   
    while (MoreSQLData()) {
        my ($bug_id, $dependencies) = FetchSQLData();
        push(@dependencies, { bug_id       => $bug_id, 
                              dependencies => $dependencies });
    }

    # All done with this sidetrip
    PopGlobalSQLState();

    return @dependencies;
}

sub GetLongDescriptionAsText {
    my ($id, $start, $end) = (@_);
    my $result = "";
    my $count = 0;
    my $anyprivate = 0;
    my ($query) = ("SELECT profiles.login_name, DATE_FORMAT(longdescs.bug_when,'%Y.%m.%d %H:%i'), " .
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

sub UserInGroup {
    if ($_[1]) {
        die "UserInGroup no longer takes a second parameter.";
    }
    
    return defined Bugzilla->user && defined Bugzilla->user->groups->{$_[0]};
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
    SendSQL("SELECT profiles.login_name, profiles.userid, votes.vote_count, " .
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
                SendSQL("UPDATE votes SET vote_count = $newvotes " .
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
        SendSQL("SELECT SUM(vote_count) FROM votes WHERE bug_id = $id");
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

    # For each pair of (old, new) entries: 
    # If they're equal, set them to empty. When done, @old contains entries
    # that were removed; @new contains ones that got added.

    foreach my $oldv (@old) {
        foreach my $newv (@new) {
            next if ($newv eq '');
            if ($oldv eq $newv) {
                $newv = $oldv = '';
            }
        }
    }
    my $removed = join (", ", grep { $_ ne '' } @old);
    my $added = join (", ", grep { $_ ne '' } @new);

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

# Constructs a format object from URL parameters. You most commonly call it 
# like this:
# my $format = GetFormat("foo/bar", scalar($cgi->param('format')),
#                        scalar($cgi->param('ctype')));

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

    # Now check that the template actually exists. We only want to check
    # if the template exists; any other errors (eg parse errors) will
    # end up being detected later.
    eval {
        Bugzilla->template->context->template($template);
    };
    # This parsing may seem fragile, but its OK:
    # http://lists.template-toolkit.org/pipermail/templates/2003-March/004370.html
    # Even if it is wrong, any sort of error is going to cause a failure
    # eventually, so the only issue would be an incorrect error message
    if ($@ && $@->info =~ /: not found$/) {
        ThrowUserError("format_not_found", { 'format' => $format,
                                             'ctype' => $ctype,
                                           });
    }

    # Else, just return the info
    return
    {
        'template'    => $template ,
        'extension'   => $ctype ,
        'ctype'       => Bugzilla::Constants::contenttypes->{$ctype} ,
    };
}

############# Live code below here (that is, not subroutine defs) #############

use Bugzilla;

$::template = Bugzilla->template();

$::vars = {};

1;
