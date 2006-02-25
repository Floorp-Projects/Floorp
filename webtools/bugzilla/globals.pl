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
#                 Dave Lawrence <dkl@redhat.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Lance Larsh <lance.larsh@oracle.com>

# Contains some global variables and routines used throughout bugzilla.

use strict;

use Bugzilla::DB qw(:DEFAULT :deprecated);
use Bugzilla::Constants;
use Bugzilla::Util;
# Bring ChmodDataFile in until this is all moved to the module
use Bugzilla::Config qw(:DEFAULT ChmodDataFile $localconfig $datadir);
use Bugzilla::User;
use Bugzilla::Error;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub globals_pl_sillyness {
    my $zz;
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
    $zz = @main::prodmaxvotes;
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

# XXXX - this needs to go away
sub GenerateVersionTable {
    my $dbh = Bugzilla->dbh;

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

    SendSQL("SELECT products.name, classifications.name " .
            "FROM products, classifications " .
            "WHERE classifications.id = products.classification_id " .
            "ORDER BY classifications.name");
    while (@line = FetchSQLData()) {
        my ($p,$c) = (@line);
        if (!defined $::classifications{$c}) {
            $::classifications{$c} = [];
        }
        my $ref = $::classifications{$c};
        push @$ref, $p;
    }

    my $dotargetmilestone = 1;  # This used to check the param, but there's
                                # enough code that wants to pretend we're using
                                # target milestones, even if they don't get
                                # shown to the user.  So we cache all the data
                                # about them anyway.

    my $mpart = $dotargetmilestone ? ", milestoneurl" : "";

    SendSQL("SELECT name, votesperuser, disallownew$mpart " .
            "FROM products ORDER BY name");
    while (@line = FetchSQLData()) {
        my ($p, $votesperuser, $dis, $u) = (@line);
        if (!$dis && scalar($::components{$p})) {
            push @::enterable_products, $p;
        }
        if ($dotargetmilestone) {
            $::milestoneurl{$p} = $u;
        }
        $::prodmaxvotes{$p} = $votesperuser;
    }
            
    @::log_columns = $dbh->bz_table_columns('bugs');
    
    foreach my $i ("bug_id", "creation_ts", "delta_ts", "lastdiffed") {
        my $w = lsearch(\@::log_columns, $i);
        if ($w >= 0) {
            splice(@::log_columns, $w, 1);
        }
    }
    @::log_columns = (sort(@::log_columns));

    @::legal_priority   = get_legal_field_values("priority");
    @::legal_severity   = get_legal_field_values("bug_severity");
    @::legal_platform   = get_legal_field_values("rep_platform");
    @::legal_opsys      = get_legal_field_values("op_sys");
    @::legal_bug_status = get_legal_field_values("bug_status");
    @::legal_resolution = get_legal_field_values("resolution");

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

    print $fh (Data::Dumper->Dump([\@::settable_resolution,
                                   \%::classifications,
                                   \@::enterable_products, \%::prodmaxvotes],
                                  ['*::settable_resolution',
                                   '*::classifications',
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

    rename ($tmpname, "$datadir/versioncache")
        || die "Can't rename $tmpname to versioncache";
    ChmodDataFile("$datadir/versioncache", 0666);
}


sub GetKeywordIdFromName {
    my ($name) = (@_);
    $name = lc($name);
    return $::keywordsbyname{$name};
}


$::VersionTableLoaded = 0;
sub GetVersionTable {
    return if $::VersionTableLoaded;
    my $file_generated = 0;
    if (!-r "$datadir/versioncache") {
        GenerateVersionTable();
        $file_generated = 1;
    }
    require "$datadir/versioncache";
    if (!defined %::versions && !$file_generated) {
        GenerateVersionTable();
        do "$datadir/versioncache";
    }
    if (!defined %::versions) {
        die "Can't generate file $datadir/versioncache";
    }
    $::VersionTableLoaded = 1;
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
    my $dbh = Bugzilla->dbh;
    PushGlobalSQLState();
    my $query = "SELECT 1 FROM group_control_map WHERE entry != 0";
    $query .= " AND product_id = $product_id" if ($product_id);
    $query .= " " . $dbh->sql_limit(1);
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
    my $dbh = Bugzilla->dbh;
    PushGlobalSQLState();
    SendSQL("SELECT 1 FROM group_control_map, groups WHERE " .
            "groups.id = group_control_map.group_id " .
            "AND isactive != 0 AND " .
            "(membercontrol = " . CONTROLMAPDEFAULT .
            " OR othercontrol = " . CONTROLMAPDEFAULT .
            ") " . $dbh->sql_limit(1));
    $::CachedAnyDefaultGroups = MoreSQLData();
    FetchSQLData();
    PopGlobalSQLState();
    return $::CachedAnyDefaultGroups;
}

sub IsInClassification {
    my ($classification,$productname) = @_;

    if (! Param('useclassification')) {
        return 1;
    } else {
        my $query = "SELECT classifications.name " .
          "FROM products,classifications " .
            "WHERE products.classification_id=classifications.id ";
        $query .= "AND products.name = " . SqlQuote($productname);
        PushGlobalSQLState();
        SendSQL($query);
        my ($ret) = FetchSQLData();
        PopGlobalSQLState();
        return ($ret eq $classification);
    }
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

sub DBID_to_name {
    my ($id) = (@_);
    return "__UNKNOWN__" if !defined $id;
    # $id should always be a positive integer
    if ($id =~ m/^([1-9][0-9]*)$/) {
        $id = $1;
    } else {
        $::cachedNameArray{$id} = "__UNKNOWN__";
    }
    if (!defined $::cachedNameArray{$id}) {
        PushGlobalSQLState();
        SendSQL("SELECT login_name FROM profiles WHERE userid = $id");
        my $r = FetchOneColumn();
        PopGlobalSQLState();
        if (!defined $r || $r eq "") {
            $r = "__UNKNOWN__";
        }
        $::cachedNameArray{$id} = $r;
    }
    return $::cachedNameArray{$id};
}

sub DBNameToIdAndCheck {
    my ($name) = (@_);
    my $result = login_to_id($name);
    if ($result > 0) {
        return $result;
    }

    ThrowUserError("invalid_username", { name => $name });
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

# Returns a list of all the legal values for a field that has a
# list of legal values, like rep_platform or resolution.
sub get_legal_field_values {
    my ($field) = @_;
    my $dbh = Bugzilla->dbh;
    my $result_ref = $dbh->selectcol_arrayref(
         "SELECT value FROM $field
           WHERE isactive = ?
        ORDER BY sortkey, value", undef, (1));
    return @$result_ref;
}

sub BugInGroupId {
    my ($bugid, $groupid) = (@_);
    PushGlobalSQLState();
    SendSQL("SELECT CASE WHEN bug_id != 0 THEN 1 ELSE 0 END
            FROM bug_group_map
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
    return ('NEW', 'REOPENED', 'ASSIGNED', 'UNCONFIRMED');
}

############# Live code below here (that is, not subroutine defs) #############

use Bugzilla;

1;
