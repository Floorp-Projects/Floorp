#!/usr/bin/perl -w
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#                 Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>
#                 Zach Lipton  <zach@zachlipton.com>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Tobias Burnus <burnus@net-b.de>
#                 Shane H. W. Travis <travis@sedsystems.ca>
#                 Gervase Markham <gerv@gerv.net>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Dave Lawrence <dkl@redhat.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Joel Peshkin <bugreport@peshkin.net>
#                 Lance Larsh <lance.larsh@oracle.com>
#                 A. Karl Kornel <karl@kornel.name>
#                 Marc Schumann <wurblzap@gmail.com>

=head1 NAME

checksetup.pl - A do-it-all upgrade and installation script for Bugzilla.

=head1 SYNOPSIS

 ./checksetup.pl [--help|--check-modules]
 ./checksetup.pl [SCRIPT [--verbose]] [--no-templates|-t]

=head1 OPTIONS

=over

=item F<SCRIPT>

Name of script to drive non-interactive mode. This script should
define an C<%answer> hash whose keys are variable names and the
values answers to all the questions checksetup.pl asks. For details
on the format of this script, do C<perldoc checksetup.pl> and look for
the L</"RUNNING CHECKSETUP NON-INTERACTIVELY"> section.

=item B<--help>

Display this help text

=item B<--check-modules>

Only check for correct module dependencies and quit afterward.

=item B<--no-templates> (B<-t>) 

Don't compile the templates at all. Existing compiled templates will 
remain; missing compiled templates will not be created. (Used primarily
by developers to speed up checksetup.) Use this switch at your own risk.

=item B<--verbose>

Output results of SCRIPT being processed.

=back

=head1 DESCRIPTION

Hey, what's this?

F<checksetup.pl> is a script that is supposed to run during 
installation time and also after every upgrade.

The goal of this script is to make the installation even easier.
It does this by doing things for you as well as testing for problems
in advance.

You can run the script whenever you like. You SHOULD run it after
you update Bugzilla, because it may then update your SQL table
definitions to resync them with the code.

Currently, this script does the following:

=over

=item * 

Check for required perl modules

=item * 

Set defaults for local configuration variables

=item * 

Create and populate the F<data> directory after installation

=item * 

Set the proper rights for the F<*.cgi>, F<*.html>, etc. files

=item * 

Verify that the code can access the database server

=item * 

Creates the database C<bugs> if it does not exist

=item * 

Creates the tables inside the database if they don't exist

=item * 

Automatically changes the table definitions if they are from
an older version of Bugzilla

=item * 

Populates the groups

=item * 

Puts the first user into all groups so that the system can
be administered

=item * 

...And a whole lot more.

=back

=head1 MODIFYING CHECKSETUP

There should be no need for Bugzilla Administrators to modify
this script; all user-configurable stuff has been moved 
into a local configuration file called F<localconfig>. When that file
in changed and F<checksetup.pl> is run, then the user's changes
will be reflected back into the database.

Developers, however, have to modify this file at various places. To
make this easier, there are some special tags for which one
can search.

 To                                               Search for

 add/delete local configuration variables         --LOCAL--
 check for more required modules                  --MODULES--
 add more database-related checks                 --DATABASE--
 change the defaults for local configuration vars --DATA--
 update the assigned file permissions             --CHMOD--
 change table definitions                         --TABLE--
 add more groups                                  --GROUPS--
 add user-adjustable settings                     --SETTINGS--
 create initial administrator account             --ADMIN--

Note: sometimes those special comments occur more than once. For
example, C<--LOCAL--> is used at least 3 times in this code!  C<--TABLE-->
is also used more than once, so search for each and every occurrence!

=head1 RUNNING CHECKSETUP NON-INTERACTIVELY

To operate checksetup non-interactively, run it with a single argument
specifying a filename that contains the information usually obtained by
prompting the user or by editing localconfig.

The format of that file is as follows:

 $answer{'db_host'}   = 'localhost';
 $answer{'db_driver'} = 'mydbdriver';
 $answer{'db_port'}   = 0;
 $answer{'db_name'}   = 'mydbname';
 $answer{'db_user'}   = 'mydbuser';
 $answer{'db_pass'}   = 'mydbpass';

 $answer{'urlbase'} = 'http://bugzilla.mydomain.com/';

 (Any localconfig variable or parameter can be specified as above.)

 $answer{'ADMIN_OK'} = 'Y';
 $answer{'ADMIN_EMAIL'} = 'myadmin@mydomain.net';
 $answer{'ADMIN_PASSWORD'} = 'fooey';
 $answer{'ADMIN_REALNAME'} = 'Joel Peshkin';

 $answer{'SMTP_SERVER'} = 'mail.mydomain.net';

Note: Only information that supersedes defaults from C<LocalVar()>
function calls needs to be specified in this file.

=head1 SEE ALSO

L<Bugzilla::Install::Requirements>

L<Bugzilla::Install::Localconfig>

L<Bugzilla::Install::Filesystem>

L<Bugzilla::Config/update_params>

L<Bugzilla::DB/CONNECTION>

=cut

######################################################################
# Initialization
######################################################################

use strict;
use 5.008;
use File::Basename;
use Getopt::Long qw(:config bundling);
use Pod::Usage;
use POSIX ();
use Safe;

BEGIN { chdir dirname($0); }
use lib ".";
use Bugzilla::Constants;
use Bugzilla::Install::Requirements;

require 5.008001 if ON_WINDOWS; # for CGI 2.93 or higher

######################################################################
# Subroutines
######################################################################

sub read_answers_file {
    my %hash;
    if ($ARGV[0]) {
        my $s = new Safe;
        $s->rdo($ARGV[0]);

        die "Error reading $ARGV[0]: $!" if $!;
        die "Error evaluating $ARGV[0]: $@" if $@;

        # Now read the param back out from the sandbox
        %hash = %{$s->varglob('answer')};
    }
    return \%hash;
}

######################################################################
# Live Code
######################################################################

my %switch;
GetOptions(\%switch, 'help|h|?', 'check-modules', 'no-templates|t',
                     'verbose|v|no-silent');

# Print the help message if that switch was selected.
pod2usage({-verbose => 1, -exitval => 1}) if $switch{'help'};

# Read in the "answers" file if it exists, for running in 
# non-interactive mode.
our %answer = %{read_answers_file()};
my $silent = scalar(keys %answer) && !$switch{'verbose'};

# Display version information
unless ($silent) {
    printf "\n* This is Bugzilla " . BUGZILLA_VERSION . " on perl %vd\n",
           $^V;
    my @os_details = POSIX::uname;
    # 0 is the name of the OS, 2 is the major version,
    my $os_name = $os_details[0] . ' ' . $os_details[2];
    if (ON_WINDOWS) {
        require Win32;
        $os_name = Win32::GetOSName();
    }
    # 3 is the minor version.
    print "* Running on $os_name $os_details[3]\n"
}

# Check required --MODULES--
my $module_results = check_requirements(!$silent);
exit if !$module_results->{pass};
# Break out if checking the modules is all we have been asked to do.
exit if $switch{'check-modules'};

###########################################################################
# Load Bugzilla Modules
###########################################################################

# It's never safe to "use" a Bugzilla module in checksetup. If a module
# prerequisite is missing, and you "use" a module that requires it,
# then instead of our nice normal checksetup message, the user would
# get a cryptic perl error about the missing module.

# We need $::ENV{'PATH'} to remain defined.
my $env = $::ENV{'PATH'};
require Bugzilla;
$::ENV{'PATH'} = $env;

require Bugzilla::Config;
import Bugzilla::Config qw(:admin);

require Bugzilla::User::Setting;
import Bugzilla::User::Setting qw(add_setting);

require Bugzilla::Util;
import Bugzilla::Util qw(bz_crypt trim html_quote is_7bit_clean
                         clean_text url_quote);

require Bugzilla::User;
import Bugzilla::User qw(insert_new_user);

require Bugzilla::Bug;
import Bugzilla::Bug qw(is_open_state);

require Bugzilla::Install::Localconfig;
import Bugzilla::Install::Localconfig qw(read_localconfig update_localconfig);

require Bugzilla::Install::Filesystem;
import Bugzilla::Install::Filesystem qw(update_filesystem create_htaccess
                                        fix_all_file_permissions);
require Bugzilla::Install::DB;

require Bugzilla::DB;
require Bugzilla::Template;
require Bugzilla::Field;
require Bugzilla::Install;

###########################################################################
# Check and update --LOCAL-- configuration
###########################################################################

print "Reading " .  bz_locations()->{'localconfig'} . "...\n" unless $silent;
update_localconfig({ output => !$silent, answer => \%answer });
my $lc_hash = read_localconfig();

# XXX Eventually this variable can be eliminated, but it is
# used more than once throughout checksetup right now.
my $my_webservergroup = $lc_hash->{'webservergroup'};

###########################################################################
# Check --DATABASE-- setup
###########################################################################

# At this point, localconfig is defined and is readable. So we know
# everything we need to create the DB. We have to create it early,
# because some data required to populate data/params is stored in the DB.

Bugzilla::DB::bz_check_requirements(!$silent);
Bugzilla::DB::bz_create_database() if $lc_hash->{'db_check'};

# now get a handle to the database:
my $dbh = Bugzilla->dbh;

###########################################################################
# Create tables
###########################################################################

# Note: --TABLE-- definitions are now in Bugzilla::DB::Schema.
$dbh->bz_setup_database();

# Populate the tables that hold the values for the <select> fields.
$dbh->bz_populate_enum_tables();

###########################################################################
# Check --DATA-- directory
###########################################################################

update_filesystem({ index_html => $lc_hash->{'index_html'} });
create_htaccess() if $lc_hash->{'create_htaccess'};

# XXX Some parts of checksetup still need these, right now.
my $datadir   = bz_locations()->{'datadir'};

# Remove parameters from the params file that no longer exist in Bugzilla,
# and set the defaults for new ones
update_params({ answer => \%answer});

###########################################################################
# Pre-compile --TEMPLATE-- code
###########################################################################

Bugzilla::Template::precompile_templates(!$silent)
    unless $switch{'no-templates'};

###########################################################################
# Set proper rights (--CHMOD--)
###########################################################################

fix_all_file_permissions(!$silent);

###########################################################################
# Check GraphViz setup
###########################################################################

#
# If we are using a local 'dot' binary, verify the specified binary exists
# and that the generated images are accessible.
#
check_graphviz(!$silent) if Bugzilla->params->{'webdotbase'};

###########################################################################
# Populate groups table
###########################################################################

sub GroupDoesExist
{
    my ($name) = @_;
    my $sth = $dbh->prepare("SELECT name FROM groups WHERE name='$name'");
    $sth->execute;
    if ($sth->rows) {
        return 1;
    }
    return 0;
}


#
# This subroutine ensures that a group exists. If not, it will be created 
# automatically, and given the next available groupid
#

sub AddGroup {
    my ($name, $desc, $userregexp) = @_;
    $userregexp ||= "";

    return if GroupDoesExist($name);
    
    print "Adding group $name ...\n";
    my $sth = $dbh->prepare('INSERT INTO groups
                          (name, description, userregexp, isbuggroup,
                           last_changed)
                          VALUES (?, ?, ?, ?, NOW())');
    $sth->execute($name, $desc, $userregexp, 0);

    my $last = $dbh->bz_last_key('groups', 'id');
    return $last;
}


###########################################################################
# The list of fields.
###########################################################################

# NOTE: All of these entries are unconditional, from when get_field_id
# used to create an entry if it wasn't found. New fielddef columns should
# be created with their associated schema change.
use constant OLD_FIELD_DEFS => (
    {name => 'bug_id',       desc => 'Bug #',      in_new_bugmail => 1},
    {name => 'short_desc',   desc => 'Summary',    in_new_bugmail => 1},
    {name => 'classification', desc => 'Classification', in_new_bugmail => 1},
    {name => 'product',      desc => 'Product',    in_new_bugmail => 1},
    {name => 'version',      desc => 'Version',    in_new_bugmail => 1},
    {name => 'rep_platform', desc => 'Platform',   in_new_bugmail => 1},
    {name => 'bug_file_loc', desc => 'URL',        in_new_bugmail => 1},
    {name => 'op_sys',       desc => 'OS/Version', in_new_bugmail => 1},
    {name => 'bug_status',   desc => 'Status',     in_new_bugmail => 1},
    {name => 'status_whiteboard', desc => 'Status Whiteboard',
     in_new_bugmail => 1},
    {name => 'keywords',     desc => 'Keywords',   in_new_bugmail => 1},
    {name => 'resolution',   desc => 'Resolution'},
    {name => 'bug_severity', desc => 'Severity',   in_new_bugmail => 1},
    {name => 'priority',     desc => 'Priority',   in_new_bugmail => 1},
    {name => 'component',    desc => 'Component',  in_new_bugmail => 1},
    {name => 'assigned_to',  desc => 'AssignedTo', in_new_bugmail => 1},
    {name => 'reporter',     desc => 'ReportedBy', in_new_bugmail => 1},
    {name => 'votes',        desc => 'Votes'},
    {name => 'qa_contact',   desc => 'QAContact',  in_new_bugmail => 1},
    {name => 'cc',           desc => 'CC',         in_new_bugmail => 1},
    {name => 'dependson',    desc => 'BugsThisDependsOn', in_new_bugmail => 1},
    {name => 'blocked',      desc => 'OtherBugsDependingOnThis',
     in_new_bugmail => 1},

    {name => 'attachments.description', desc => 'Attachment description'},
    {name => 'attachments.filename',    desc => 'Attachment filename'},
    {name => 'attachments.mimetype',    desc => 'Attachment mime type'},
    {name => 'attachments.ispatch',     desc => 'Attachment is patch'},
    {name => 'attachments.isobsolete',  desc => 'Attachment is obsolete'},
    {name => 'attachments.isprivate',   desc => 'Attachment is private'},

    {name => 'target_milestone',      desc => 'Target Milestone'},
    {name => 'creation_ts', desc => 'Creation date', in_new_bugmail => 1},
    {name => 'delta_ts', desc => 'Last changed date', in_new_bugmail => 1},
    {name => 'longdesc',              desc => 'Comment'},
    {name => 'alias',                 desc => 'Alias'},
    {name => 'everconfirmed',         desc => 'Ever Confirmed'},
    {name => 'reporter_accessible',   desc => 'Reporter Accessible'},
    {name => 'cclist_accessible',     desc => 'CC Accessible'},
    {name => 'bug_group',             desc => 'Group'},
    {name => 'estimated_time', desc => 'Estimated Hours', in_new_bugmail => 1},
    {name => 'remaining_time',        desc => 'Remaining Hours'},
    {name => 'deadline',              desc => 'Deadline', in_new_bugmail => 1},
    {name => 'commenter',             desc => 'Commenter'},
    {name => 'flagtypes.name',        desc => 'Flag'},
    {name => 'requestees.login_name', desc => 'Flag Requestee'},
    {name => 'setters.login_name',    desc => 'Flag Setter'},
    {name => 'work_time',             desc => 'Hours Worked'},
    {name => 'percentage_complete',   desc => 'Percentage Complete'},
    {name => 'content',               desc => 'Content'},
    {name => 'attach_data.thedata',   desc => 'Attachment data'},
    {name => 'attachments.isurl',     desc => 'Attachment is a URL'}
);
# Please see comment above before adding any new values to this constant.

###########################################################################
# Changes to the fielddefs --TABLE--
###########################################################################

# Calling Bugzilla::Field::create_or_update depends on the
# fielddefs table having a modern definition. So, we have to make
# these particular schema changes before we make any other schema changes.

# 2005-02-21 - LpSolit@gmail.com - Bug 279910
# qacontact_accessible and assignee_accessible field names no longer exist
# in the 'bugs' table. Their corresponding entries in the 'bugs_activity'
# table should therefore be marked as obsolete, meaning that they cannot
# be used anymore when querying the database - they are not deleted in
# order to keep track of these fields in the activity table.
if (!$dbh->bz_column_info('fielddefs', 'obsolete')) {
    $dbh->bz_add_column('fielddefs', 'obsolete',
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    print "Marking qacontact_accessible and assignee_accessible as obsolete",
          " fields...\n";
    $dbh->do("UPDATE fielddefs SET obsolete = 1
              WHERE name = 'qacontact_accessible'
                 OR name = 'assignee_accessible'");
}

# 2005-08-10 Myk Melez <myk@mozilla.org> bug 287325
# Record each field's type and whether or not it's a custom field in fielddefs.
$dbh->bz_add_column('fielddefs', 'type',
                    { TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0 });
$dbh->bz_add_column('fielddefs', 'custom',
                    { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE' });

$dbh->bz_add_column('fielddefs', 'enter_bug',
    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

# Change the name of the fieldid column to id, so that fielddefs
# can use Bugzilla::Object easily. We have to do this up here, because
# otherwise adding these field definitions will fail.
$dbh->bz_rename_column('fielddefs', 'fieldid', 'id');

# If the largest fielddefs sortkey is less than 100, then
# we're using the old sorting system, and we should convert
# it to the new one before adding any new definitions.
if (!$dbh->selectrow_arrayref(
        'SELECT COUNT(id) FROM fielddefs WHERE sortkey >= 100'))
{
    print "Updating the sortkeys for the fielddefs table...\n";
    my $field_ids = $dbh->selectcol_arrayref(
        'SELECT id FROM fielddefs ORDER BY sortkey');
    my $sortkey = 100;
    foreach my $field_id (@$field_ids) {
        $dbh->do('UPDATE fielddefs SET sortkey = ? WHERE id = ?',
                 undef, $sortkey, $field_id);
        $sortkey += 100;
    }
}

# Create field definitions
foreach my $definition (OLD_FIELD_DEFS) {
    Bugzilla::Field::create_or_update($definition);
}

# Delete or adjust old field definitions.

# Oops. Bug 163299
$dbh->do("DELETE FROM fielddefs WHERE name='cc_accessible'");
# Oops. Bug 215319
$dbh->do("DELETE FROM fielddefs WHERE name='requesters.login_name'");
# This field was never tracked in bugs_activity, so it's safe to delete.
$dbh->do("DELETE FROM fielddefs WHERE name='attachments.thedata'");

# 2005-11-13 LpSolit@gmail.com - Bug 302599
# One of the field names was a fragment of SQL code, which is DB dependent.
# We have to rename it to a real name, which is DB independent.
my $new_field_name = 'days_elapsed';
my $field_description = 'Days since bug changed';

my ($old_field_id, $old_field_name) =
    $dbh->selectrow_array('SELECT id, name
                           FROM fielddefs
                           WHERE description = ?',
                           undef, $field_description);

if ($old_field_id && ($old_field_name ne $new_field_name)) {
    print "SQL fragment found in the 'fielddefs' table...\n";
    print "Old field name: " . $old_field_name . "\n";
    # We have to fix saved searches first. Queries have been escaped
    # before being saved. We have to do the same here to find them.
    $old_field_name = url_quote($old_field_name);
    my $broken_named_queries =
        $dbh->selectall_arrayref('SELECT userid, name, query
                                  FROM namedqueries WHERE ' .
                                  $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                  undef, "%=$old_field_name%");

    my $sth_UpdateQueries = $dbh->prepare('UPDATE namedqueries SET query = ?
                                           WHERE userid = ? AND name = ?');

    print "Fixing saved searches...\n" if scalar(@$broken_named_queries);
    foreach my $named_query (@$broken_named_queries) {
        my ($userid, $name, $query) = @$named_query;
        $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
        $sth_UpdateQueries->execute($query, $userid, $name);
    }

    # We now do the same with saved chart series.
    my $broken_series =
        $dbh->selectall_arrayref('SELECT series_id, query
                                  FROM series WHERE ' .
                                  $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                  undef, "%=$old_field_name%");

    my $sth_UpdateSeries = $dbh->prepare('UPDATE series SET query = ?
                                          WHERE series_id = ?');

    print "Fixing saved chart series...\n" if scalar(@$broken_series);
    foreach my $series (@$broken_series) {
        my ($series_id, $query) = @$series;
        $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
        $sth_UpdateSeries->execute($query, $series_id);
    }

    # Now that saved searches have been fixed, we can fix the field name.
    print "Fixing the 'fielddefs' table...\n";
    print "New field name: " . $new_field_name . "\n";
    $dbh->do('UPDATE fielddefs SET name = ? WHERE id = ?',
              undef, ($new_field_name, $old_field_id));
}


Bugzilla::Field::create_or_update(
    {name => $new_field_name, desc => $field_description});

###########################################################################
# Create initial test product if there are no products present.
###########################################################################
my $sth = $dbh->prepare("SELECT description FROM products");
$sth->execute;
unless ($sth->rows) {
    print "Creating initial dummy product 'TestProduct' ...\n";
    my $test_product_name = 'TestProduct';
    my $test_product_desc = 
        'This is a test product. This ought to be blown away and'
        . ' replaced with real stuff in a finished installation of bugzilla.';
    my $test_product_version = 'other';

    $dbh->do(q{INSERT INTO products(name, description, milestoneurl, 
                           disallownew, votesperuser, votestoconfirm)
               VALUES (?, ?, '', ?, ?, ?)},
               undef, $test_product_name, $test_product_desc, 0, 0, 0);

    # We could probably just assume that this is "1", but better
    # safe than sorry...
    my $product_id = $dbh->bz_last_key('products', 'id');
    
    $dbh->do(q{INSERT INTO versions (value, product_id) 
                VALUES (?, ?)}, 
             undef, $test_product_version, $product_id);
    # note: since admin user is not yet known, components gets a 0 for 
    # initialowner and this is fixed during final checks.
    $dbh->do("INSERT INTO components (name, product_id, description, " .
                                     "initialowner) " .
             "VALUES (" .
             "'TestComponent', $product_id, " .
             "'This is a test component in the test product database.  " .
             "This ought to be blown away and replaced with real stuff in " .
             "a finished installation of Bugzilla.', 0)");
    $dbh->do(q{INSERT INTO milestones (product_id, value, sortkey) 
               VALUES (?,?,?)},
             undef, $product_id, '---', 0);
}

# Create a default classification if one does not exist
my $class_count =
    $dbh->selectrow_array("SELECT COUNT(*) FROM classifications");
if (!$class_count) {
    $dbh->do("INSERT INTO classifications (name,description) " .
             "VALUES('Unclassified','Unassigned to any classifications')");
}

###########################################################################
# Update the tables to the current definition  --TABLE--
###########################################################################

Bugzilla::Install::DB::update_table_definitions();        

# 2005-04-07 - alt@sonic.net, bug 289455
# make classification_id field type be consistent with DB:Schema
$dbh->bz_alter_column('products', 'classification_id',
                      {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '1'});

# initialowner was accidentally NULL when we checked-in Schema,
# when it really should be NOT NULL.
$dbh->bz_alter_column('components', 'initialowner',
                      {TYPE => 'INT3', NOTNULL => 1}, 0);

# 2005-03-28 - bug 238800 - index flags.type_id to make editflagtypes.cgi speedy
$dbh->bz_add_index('flags', 'flags_type_id_idx', [qw(type_id)]);

# For a short time, the flags_type_id_idx was misnamed in upgraded installs.
$dbh->bz_drop_index('flags', 'type_id');

# 2005-04-28 - LpSolit@gmail.com - Bug 7233: add an index to versions
$dbh->bz_alter_column('versions', 'value',
                      {TYPE => 'varchar(64)', NOTNULL => 1});
$dbh->bz_add_index('versions', 'versions_product_id_idx',
                   {TYPE => 'UNIQUE', FIELDS => [qw(product_id value)]});

# Milestone sortkeys get a default just like all other sortkeys.
if (!exists $dbh->bz_column_info('milestones', 'sortkey')->{DEFAULT}) {
    $dbh->bz_alter_column('milestones', 'sortkey', 
                          {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
}

# 2005-06-14 - LpSolit@gmail.com - Bug 292544: only set creation_ts
# when all bug fields have been correctly set.
$dbh->bz_alter_column('bugs', 'creation_ts', {TYPE => 'DATETIME'});

if (!exists $dbh->bz_column_info('whine_queries', 'title')->{DEFAULT}) {

    # The below change actually has nothing to do with the whine_queries
    # change, it just has to be contained within a schema change so that
    # it doesn't run every time we run checksetup.

    # Old Bugzillas have "other" as an OS choice, new ones have "Other"
    # (capital O).
    print "Setting any 'other' op_sys to 'Other'...\n";
    $dbh->do('UPDATE op_sys SET value = ? WHERE value = ?',
             undef, "Other", "other");
    $dbh->do('UPDATE bugs SET op_sys = ? WHERE op_sys = ?',
             undef, "Other", "other");
    if (Bugzilla->params->{'defaultopsys'} eq 'other') {
        # We can't actually fix the param here, because WriteParams() will
        # make $datadir/params unwriteable to the webservergroup.
        # It's too much of an ugly hack to copy the permission-fixing code
        # down to here. (It would create more potential future bugs than
        # it would solve problems.)
        print "WARNING: Your 'defaultopsys' param is set to 'other', but"
            . " Bugzilla now\n"
            . "         uses 'Other' (capital O).\n";
    }

    # Add a DEFAULT to whine_queries stuff so that editwhines.cgi
    # works on PostgreSQL.
    $dbh->bz_alter_column('whine_queries', 'title', {TYPE => 'varchar(128)', 
                          NOTNULL => 1, DEFAULT => "''"});
}

# 2005-06-29 bugreport@peshkin.net, bug 299156
if ($dbh->bz_index_info('attachments', 'attachments_submitter_id_idx')
   && (scalar(@{$dbh->bz_index_info('attachments',
                                    'attachments_submitter_id_idx'
                                   )->{FIELDS}}) < 2)
      ) {
    $dbh->bz_drop_index('attachments', 'attachments_submitter_id_idx');
}
$dbh->bz_add_index('attachments', 'attachments_submitter_id_idx',
                   [qw(submitter_id bug_id)]);

# 2005-08-25 - bugreport@peshkin.net - Bug 305333
if ($dbh->bz_column_info("attachments", "thedata")) {
    print "Migrating attachment data to its own table...\n";
    print "(This may take a very long time)\n";
    $dbh->do("INSERT INTO attach_data (id, thedata) 
                   SELECT attach_id, thedata FROM attachments");
    $dbh->bz_drop_column("attachments", "thedata");    
}

# 2005-11-26 - wurblzap@gmail.com - Bug 300473
# Repair broken automatically generated series queries for non-open bugs.
my $broken_series_indicator =
    'field0-0-0=resolution&type0-0-0=notequals&value0-0-0=---';
my $broken_nonopen_series =
    $dbh->selectall_arrayref("SELECT series_id, query FROM series
                               WHERE query LIKE '$broken_series_indicator%'");
if (@$broken_nonopen_series) {
    print 'Repairing broken series...';
    my $sth_nuke =
        $dbh->prepare('DELETE FROM series_data WHERE series_id = ?');
    # This statement is used to repair a series by replacing the broken query
    # with the correct one.
    my $sth_repair =
        $dbh->prepare('UPDATE series SET query = ? WHERE series_id = ?');
    # The corresponding series for open bugs look like one of these two
    # variations (bug 225687 changed the order of bug states).
    # This depends on the set of bug states representing open bugs not to have
    # changed since series creation.
    my $open_bugs_query_base_old = 
        join("&", map { "bug_status=" . url_quote($_) }
                      ('UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED'));
    my $open_bugs_query_base_new = 
        join("&", map { "bug_status=" . url_quote($_) } BUG_STATE_OPEN);
    my $sth_openbugs_series =
        $dbh->prepare("SELECT series_id FROM series
                        WHERE query IN (?, ?)");
    # Statement to find the series which has collected the most data.
    my $sth_data_collected =
        $dbh->prepare('SELECT count(*) FROM series_data WHERE series_id = ?');
    # Statement to select a broken non-open bugs count data entry.
    my $sth_select_broken_nonopen_data =
        $dbh->prepare('SELECT series_date, series_value FROM series_data' .
                      ' WHERE series_id = ?');
    # Statement to select an open bugs count data entry.
    my $sth_select_open_data =
        $dbh->prepare('SELECT series_value FROM series_data' .
                      ' WHERE series_id = ? AND series_date = ?');
    # Statement to fix a broken non-open bugs count data entry.
    my $sth_fix_broken_nonopen_data =
        $dbh->prepare('UPDATE series_data SET series_value = ?' .
                      ' WHERE series_id = ? AND series_date = ?');
    # Statement to delete an unfixable broken non-open bugs count data entry.
    my $sth_delete_broken_nonopen_data =
        $dbh->prepare('DELETE FROM series_data' .
                      ' WHERE series_id = ? AND series_date = ?');

    foreach (@$broken_nonopen_series) {
        my ($broken_series_id, $nonopen_bugs_query) = @$_;

        # Determine the product-and-component part of the query.
        if ($nonopen_bugs_query =~ /^$broken_series_indicator(.*)$/) {
            my $prodcomp = $1;

            # If there is more than one series for the corresponding open-bugs
            # series, we pick the one with the most data, which should be the
            # one which was generated on creation.
            # It's a pity we can't do subselects.
            $sth_openbugs_series->execute($open_bugs_query_base_old . $prodcomp,
                                          $open_bugs_query_base_new . $prodcomp);
            my ($found_open_series_id, $datacount) = (undef, -1);
            foreach my $open_series_id ($sth_openbugs_series->fetchrow_array()) {
                $sth_data_collected->execute($open_series_id);
                my ($this_datacount) = $sth_data_collected->fetchrow_array();
                if ($this_datacount > $datacount) {
                    $datacount = $this_datacount;
                    $found_open_series_id = $open_series_id;
                }
            }

            if ($found_open_series_id) {
                # Move along corrupted series data and correct it. The
                # corruption consists of it being the number of all bugs
                # instead of the number of non-open bugs, so we calculate the
                # correct count by subtracting the number of open bugs.
                # If there is no corresponding open-bugs count for some reason
                # (shouldn't happen), we drop the data entry.
                print " $broken_series_id...";
                $sth_select_broken_nonopen_data->execute($broken_series_id);
                while (my $rowref =
                       $sth_select_broken_nonopen_data->fetchrow_arrayref()) {
                    my ($date, $broken_value) = @$rowref;
                    my ($openbugs_value) =
                        $dbh->selectrow_array($sth_select_open_data, undef,
                                              $found_open_series_id, $date);
                    if (defined($openbugs_value)) {
                        $sth_fix_broken_nonopen_data->execute
                            ($broken_value - $openbugs_value,
                             $broken_series_id, $date);
                    }
                    else {
                        print "\nWARNING - During repairs of series " .
                              "$broken_series_id, the irreparable data\n" .
                              "entry for date $date was encountered and is " .
                              "being deleted.\n" .
                              "Continuing repairs...";
                        $sth_delete_broken_nonopen_data->execute
                            ($broken_series_id, $date);
                    }
                }

                # Fix the broken query so that it collects correct data in the
                # future.
                $nonopen_bugs_query =~
                    s/^$broken_series_indicator/field0-0-0=resolution&type0-0-0=regexp&value0-0-0=./;
                $sth_repair->execute($nonopen_bugs_query, $broken_series_id);
            }
            else {
                print "\nWARNING - Series $broken_series_id was meant to\n" .
                      "collect non-open bug counts, but it has counted\n" .
                      "all bugs instead. It cannot be repaired\n" .
                      "automatically because no series that collected open\n" .
                      "bug counts was found. You'll probably want to delete\n" .
                      "or repair collected data for series $broken_series_id " .
                      "manually.\n" .
                      "Continuing repairs...";
            }
        }
    }
    print " done.\n";
}

# 2005-09-15 lance.larsh@oracle.com Bug 308717
if ($dbh->bz_column_info("series", "public")) {
    # PUBLIC is a reserved word in Oracle, so renaming the column
    # PUBLIC in table SERIES avoids having to quote the column name
    # in every query against that table
    $dbh->bz_rename_column('series', 'public', 'is_public');
}

# 2005-09-28 bugreport@peshkin.net Bug 149504
$dbh->bz_add_column('attachments', 'isurl',
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 0});

# 2005-10-21 LpSolit@gmail.com - Bug 313020
$dbh->bz_add_column('namedqueries', 'query_type',
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 0});

# 2005-11-04 LpSolit@gmail.com - Bug 305927
$dbh->bz_alter_column('groups', 'userregexp', 
                      {TYPE => 'TINYTEXT', NOTNULL => 1, DEFAULT => "''"});

# 2005-09-26 - olav@bkor.dhs.org - Bug 119524
# Convert logincookies into a varchar
# this allows to store a random token instead of a guessable auto_increment
$dbh->bz_alter_column('logincookies', 'cookie',
                      {TYPE => 'varchar(16)', PRIMARYKEY => 1, NOTNULL => 1});

# Fixup for Bug 101380
# "Newlines, nulls, leading/trailing spaces are getting into summaries"

my $controlchar_bugs =
    $dbh->selectall_arrayref("SELECT short_desc, bug_id FROM bugs WHERE " .
                             $dbh->sql_regexp('short_desc', "'[[:cntrl:]]'"));
if (scalar(@$controlchar_bugs))
{
    my $msg = 'Cleaning control characters from bug summaries...';
    my $found = 0;
    foreach (@$controlchar_bugs) {
        my ($short_desc, $bug_id) = @$_;
        my $clean_short_desc = clean_text($short_desc);
        if ($clean_short_desc ne $short_desc) {
            print $msg if !$found;
            $found = 1;
            print " $bug_id...";
            $dbh->do("UPDATE bugs SET short_desc = ? WHERE bug_id = ?",
                      undef, $clean_short_desc, $bug_id);
        }
    }
    print " done.\n" if $found;
}

# 2005-12-07 altlst@sonic.net -- Bug 225221
$dbh->bz_add_column('longdescs', 'comment_id',
                    {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

# 2006-03-02 LpSolit@gmail.com - Bug 322285
# Do not store inactive flags in the DB anymore.
if ($dbh->bz_column_info('flags', 'id')->{'TYPE'} eq 'INT3') {
    # We first have to remove all existing inactive flags.
    if ($dbh->bz_column_info('flags', 'is_active')) {
        $dbh->do('DELETE FROM flags WHERE is_active = 0');
    }

    # Now we convert the id column to the auto_increment format.
    $dbh->bz_alter_column('flags', 'id',
                          {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    # And finally, we remove the is_active column.
    $dbh->bz_drop_column('flags', 'is_active');
}

# short_desc should not be a mediumtext, fix anything longer than 255 chars.
if($dbh->bz_column_info('bugs', 'short_desc')->{TYPE} eq 'MEDIUMTEXT') {
    # Move extremely long summaries into a comment ("from" the Reporter),
    # and then truncate the summary.
    my $long_summary_bugs = $dbh->selectall_arrayref(
        'SELECT bug_id, short_desc, reporter
           FROM bugs WHERE LENGTH(short_desc) > 255');

    if (@$long_summary_bugs) {
        print <<EOF;

WARNING: Some of your bugs had summaries longer than 255 characters.
They have had their original summary copied into a comment, and then
the summary was truncated to 255 characters. The affected bug numbers were:
EOF
        my $comment_sth = $dbh->prepare(
            'INSERT INTO longdescs (bug_id, who, thetext, bug_when) 
                  VALUES (?, ?, ?, NOW())');
        my $desc_sth = $dbh->prepare('UPDATE bugs SET short_desc = ? 
                                       WHERE bug_id = ?');
        my @affected_bugs;
        foreach my $bug (@$long_summary_bugs) {
            my ($bug_id, $summary, $reporter_id) = @$bug;
            my $summary_comment = "The original summary for this bug"
               . " was longer than 255 characters, and so it was truncated"
               . " when Bugzilla was upgraded. The original summary was:"
               . "\n\n$summary";
            $comment_sth->execute($bug_id, $reporter_id, $summary_comment);
            my $short_summary = substr($summary, 0, 252) . "...";
            $desc_sth->execute($short_summary, $bug_id);
            push(@affected_bugs, $bug_id);
        }
        print join(', ', @affected_bugs) . "\n\n";
    }

    $dbh->bz_alter_column('bugs', 'short_desc', {TYPE => 'varchar(255)', 
                                                 NOTNULL => 1});
}

if (-e "$datadir/versioncache") {
    print "Removing versioncache...\n";
    unlink "$datadir/versioncache";
}

# 2006-07-01 wurblzap@gmail.com -- Bug 69000
$dbh->bz_add_column('namedqueries', 'id',
                    {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
if ($dbh->bz_column_info("namedqueries", "linkinfooter")) {
    # Move link-in-footer information into a table of its own.
    my $sth_read = $dbh->prepare('SELECT id, userid
                                    FROM namedqueries 
                                   WHERE linkinfooter = 1');
    my $sth_write = $dbh->prepare('INSERT INTO namedqueries_link_in_footer
                                   (namedquery_id, user_id) VALUES (?, ?)');
    $sth_read->execute();
    while (my ($id, $userid) = $sth_read->fetchrow_array()) {
        $sth_write->execute($id, $userid);
    }
    $dbh->bz_drop_column("namedqueries", "linkinfooter");    
}

# 2006-07-07 olav@bkor.dhs.org - Bug 277377
# Add a sortkey to the classifications
if (!$dbh->bz_column_info('classifications', 'sortkey')) {
    $dbh->bz_add_column('classifications', 'sortkey',
                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});

    my $class_ids = $dbh->selectcol_arrayref('SELECT id FROM classifications ' .
                                             'ORDER BY name');
    my $sth = $dbh->prepare('UPDATE classifications SET sortkey = ? ' .
                            'WHERE id = ?');
    my $sortkey = 0;
    foreach my $class_id (@$class_ids) {
        $sth->execute($sortkey, $class_id);
        $sortkey += 100;
    }
}

# 2006-07-14 karl@kornel.name - Bug 100953
# If a nomail file exists, move its contents into the DB
$dbh->bz_add_column('profiles', 'disable_mail',
                    { TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE' });
if (-e "$datadir/nomail") {
    # We have a data/nomail file, read it in and delete it
    my %nomail;
    print "Found a data/nomail file.  Moving nomail entries into DB...\n";
    open NOMAIL, '<', "$datadir/nomail";
    while (<NOMAIL>) {
        $nomail{trim($_)} = 1;
    }
    close NOMAIL;

    # Go through each entry read.  If a user exists, set disable_mail.
    my $query = $dbh->prepare('UPDATE profiles
                                  SET disable_mail = 1
                                WHERE userid = ?');
    foreach my $user_to_check (keys %nomail) {
        my $uid;
        if ($uid = Bugzilla::User::login_to_id($user_to_check)) {
            my $user = new Bugzilla::User($uid);
            print "\tDisabling email for user ", $user->email, "\n";
            $query->execute($user->id);
            delete $nomail{$user->email};
        }
    }

    # If there are any nomail entries remaining, move them to nomail.bad
    # and say something to the user.
    if (scalar(keys %nomail)) {
        print 'The following users were listed in data/nomail, but do not ' .
              'have an account here.  The unmatched entries have been moved ' .
              "to $datadir/nomail.bad\n";
        open NOMAIL_BAD, '>>', "$datadir/nomail.bad";
        foreach my $unknown_user (keys %nomail) {
            print "\t$unknown_user\n";
            print NOMAIL_BAD "$unknown_user\n";
            delete $nomail{$unknown_user};
        }
        close NOMAIL_BAD;
    }

    # Now that we don't need it, get rid of the nomail file.
    unlink "$datadir/nomail";
}


# If you had to change the --TABLE-- definition in any way, then add your
# differential change code *** A B O V E *** this comment.
#
# That is: if you add a new field, you first search for the first occurrence
# of --TABLE-- and add your field to into the table hash. This new setting
# would be honored for every new installation. Then add your
# bz_add_field/bz_drop_field/bz_change_field_type/bz_rename_field code above.
# This would then be honored by everyone who updates his Bugzilla installation.
#

#
# Bugzilla uses --GROUPS-- to assign various rights to its users.
#

AddGroup('tweakparams', 'Can tweak operating parameters');
AddGroup('editusers', 'Can edit or disable users');
AddGroup('creategroups', 'Can create and destroy groups.');
AddGroup('editclassifications', 'Can create, destroy, and edit classifications.');
AddGroup('editcomponents', 'Can create, destroy, and edit components.');
AddGroup('editkeywords', 'Can create, destroy, and edit keywords.');
AddGroup('admin', 'Administrators');

if (!GroupDoesExist("editbugs")) {
    my $id = AddGroup('editbugs', 'Can edit all bug fields.', ".*");
    my $sth = $dbh->prepare("SELECT userid FROM profiles");
    $sth->execute();
    while (my ($userid) = $sth->fetchrow_array()) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $id, 0, " . GRANT_DIRECT . ")");
    }
}

if (!GroupDoesExist("canconfirm")) {
    my $id = AddGroup('canconfirm',  'Can confirm a bug.', ".*");
    my $sth = $dbh->prepare("SELECT userid FROM profiles");
    $sth->execute();
    while (my ($userid) = $sth->fetchrow_array()) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $id, 0, " . GRANT_DIRECT . ")");
    }

}

# Create bz_canusewhineatothers and bz_canusewhines
if (!GroupDoesExist('bz_canusewhines')) {
    my $whine_group = AddGroup('bz_canusewhines',
                               'User can configure whine reports for self');
    my $whineatothers_group = AddGroup('bz_canusewhineatothers',
                                       'Can configure whine reports for ' .
                                       'other users');
    my $group_exists = $dbh->selectrow_array(
        q{SELECT 1 FROM group_group_map 
           WHERE member_id = ? AND grantor_id = ? AND grant_type = ?},
        undef, $whineatothers_group, $whine_group, GROUP_MEMBERSHIP);
    $dbh->do("INSERT INTO group_group_map " .
             "(member_id, grantor_id, grant_type) " .
             "VALUES (${whineatothers_group}, ${whine_group}, " .
             GROUP_MEMBERSHIP . ")") unless $group_exists;
}

# 2005-08-14 bugreport@peshkin.net -- Bug 304583
use constant GRANT_DERIVED => 1;
# Get rid of leftover DERIVED group permissions
$dbh->do("DELETE FROM user_group_map WHERE grant_type = " . GRANT_DERIVED);
# Evaluate regexp-based group memberships
$sth = $dbh->prepare("SELECT profiles.userid, profiles.login_name,
                         groups.id, groups.userregexp,
                         user_group_map.group_id
                         FROM (profiles
                         CROSS JOIN groups)
                         LEFT JOIN user_group_map
                         ON user_group_map.user_id = profiles.userid
                         AND user_group_map.group_id = groups.id
                         AND user_group_map.grant_type = ?
                         WHERE (userregexp != ''
                         OR user_group_map.group_id IS NOT NULL)");

my $sth_add = $dbh->prepare("INSERT INTO user_group_map 
                       (user_id, group_id, isbless, grant_type) 
                       VALUES(?, ?, 0, " . GRANT_REGEXP . ")");

my $sth_del = $dbh->prepare("DELETE FROM user_group_map 
                       WHERE user_id  = ? 
                       AND group_id = ? 
                       AND isbless = 0
                       AND grant_type = " . GRANT_REGEXP);

$sth->execute(GRANT_REGEXP);
while (my ($uid, $login, $gid, $rexp, $present) = $sth->fetchrow_array()) {
    if ($login =~ m/$rexp/i) {
        $sth_add->execute($uid, $gid) unless $present;
    } else {
        $sth_del->execute($uid, $gid) if $present;
    }
}

# 2005-10-10 karl@kornel.name -- Bug 204498
if (!GroupDoesExist('bz_sudoers')) {
    my $sudoers_group = AddGroup('bz_sudoers',
                                 'Can perform actions as other users');
    my $sudo_protect_group = AddGroup('bz_sudo_protect',
                                      'Can not be impersonated by other users');
    my ($admin_group) = $dbh->selectrow_array('SELECT id FROM groups
                                               WHERE name = ?', undef, 'admin');
    
    # Admins should be given sudo access
    # Everyone in sudo should be in sudo_protect
    # Admins can grant membership in both groups
    my $sth = $dbh->prepare('INSERT INTO group_group_map 
                             (member_id, grantor_id, grant_type) 
                             VALUES (?, ?, ?)');
    $sth->execute($admin_group, $sudoers_group, GROUP_MEMBERSHIP);
    $sth->execute($sudoers_group, $sudo_protect_group, GROUP_MEMBERSHIP);
    $sth->execute($admin_group, $sudoers_group, GROUP_BLESS);
    $sth->execute($admin_group, $sudo_protect_group, GROUP_BLESS);
}

###########################################################################
# Create --SETTINGS-- users can adjust
###########################################################################

Bugzilla::Install::update_settings();

###########################################################################
# Create Administrator  --ADMIN--
###########################################################################

sub bailout {   # this is just in case we get interrupted while getting passwd
    if ($^O !~ /MSWin32/i) {
        system("stty","echo"); # re-enable input echoing
    }
    exit 1;
}

my @groups = ();
$sth = $dbh->prepare("SELECT id FROM groups");
$sth->execute();
while ( my @row = $sth->fetchrow_array() ) {
    push (@groups, $row[0]);
}

#  Prompt the user for the email address and name of an administrator.  Create
#  that login, if it doesn't exist already, and make it a member of all groups.

$sth = $dbh->prepare("SELECT user_id FROM groups INNER JOIN user_group_map " .
                     "ON id = group_id WHERE name = 'admin'");
$sth->execute;
# when we have no admin users, prompt for admin email address and password ...
if ($sth->rows == 0) {
    my $login = "";
    my $realname = "";
    my $pass1 = "";
    my $pass2 = "*";
    my $admin_ok = 0;
    my $admin_create = 1;
    my $mailcheckexp = "";
    my $mailcheck    = ""; 

    # Here we look to see what the emailregexp is set to so we can 
    # check the email address they enter. Bug 96675. If they have no 
    # params (likely but not always the case), we use the default.
    if (-e "$datadir/params") { 
        require "$datadir/params"; # if they have a params file, use that
    }
    if (Bugzilla->params->{'emailregexp'}) {
        $mailcheckexp = Bugzilla->params->{'emailregexp'};
        $mailcheck    = Bugzilla->params->{'emailregexpdesc'};
    } else {
        $mailcheckexp = '^[\\w\\.\\+\\-=]+@[\\w\\.\\-]+\\.[\\w\\-]+$';
        $mailcheck    = 'A legal address must contain exactly one \'@\', 
        and at least one \'.\' after the @.';
    }

    print "\nLooks like we don't have an administrator set up yet.\n";
    print "Either this is your first time using Bugzilla, or your\n ";
    print "administrator's privileges might have accidentally been deleted.\n";
    while(! $admin_ok ) {
        while( $login eq "" ) {
            print "Enter the e-mail address of the administrator: ";
            $login = $answer{'ADMIN_EMAIL'} 
                || ($silent && die("cant preload ADMIN_EMAIL")) 
                || <STDIN>;
            chomp $login;
            if(! $login ) {
                print "\nYou DO want an administrator, don't you?\n";
            }
            unless ($login =~ /$mailcheckexp/) {
                print "\nThe login address is invalid:\n";
                print "$mailcheck\n";
                print "You can change this test on the params page once checksetup has successfully\n";
                print "completed.\n\n";
                # Go round, and ask them again
                $login = "";
            }
        }
        $sth = $dbh->prepare("SELECT login_name FROM profiles " .
                              "WHERE " . $dbh->sql_istrcmp('login_name', '?'));
        $sth->execute($login);
        if ($sth->rows > 0) {
            print "$login already has an account.\n";
            print "Make this user the administrator? [Y/n] ";
            my $ok = $answer{'ADMIN_OK'} 
                || ($silent && die("cant preload ADMIN_OK")) 
                || <STDIN>;
            chomp $ok;
            if ($ok !~ /^n/i) {
                $admin_ok = 1;
                $admin_create = 0;
            } else {
                print "OK, well, someone has to be the administrator.\n";
                print "Try someone else.\n";
                $login = "";
            }
        } else {
            print "You entered $login.  Is this correct? [Y/n] ";
            my $ok = $answer{'ADMIN_OK'} 
                || ($silent && die("cant preload ADMIN_OK")) 
                || <STDIN>;
            chomp $ok;
            if ($ok !~ /^n/i) {
                $admin_ok = 1;
            } else {
                print "That's okay, typos happen.  Give it another shot.\n";
                $login = "";
            }
        }
    }

    if ($admin_create) {
        while( $realname eq "" ) {
            print "Enter the real name of the administrator: ";
            $realname = $answer{'ADMIN_REALNAME'} 
                || ($silent && die("cant preload ADMIN_REALNAME")) 
                || <STDIN>;
            chomp $realname;
            if(! $realname ) {
                print "\nReally.  We need a full name.\n";
            }
            if(! is_7bit_clean($realname)) {
                print "\nSorry, but at this stage the real name can only " . 
                      "contain standard English\ncharacters.  Once Bugzilla " .
                      "has been installed, you can use the 'Prefs' page\nto " .
                      "update the real name.\n";
                $realname = '';
            }
        }

        # trap a few interrupts so we can fix the echo if we get aborted.
        $SIG{HUP}  = \&bailout;
        $SIG{INT}  = \&bailout;
        $SIG{QUIT} = \&bailout;
        $SIG{TERM} = \&bailout;

        if ($^O !~ /MSWin32/i) {
            system("stty","-echo");  # disable input echoing
        }

        while( $pass1 ne $pass2 ) {
            while( $pass1 eq "" || $pass1 !~ /^[[:print:]]{3,16}$/ ) {
                print "Enter a password for the administrator account: ";
                $pass1 = $answer{'ADMIN_PASSWORD'} 
                    || ($silent && die("cant preload ADMIN_PASSWORD")) 
                    || <STDIN>;
                chomp $pass1;
                if(! $pass1 ) {
                    print "\n\nAn empty password is a security risk. Try again!\n";
                } elsif ( $pass1 !~ /^.{3,16}$/ ) {
                    print "\n\nThe password must be 3-16 characters in length.\n";
                } elsif ( $pass1 !~ /^[[:print:]]{3,16}$/ ) {
                    print "\n\nThe password contains non-printable characters.\n";
                }
            }
            print "\nPlease retype the password to verify: ";
            $pass2 = $answer{'ADMIN_PASSWORD'} 
                || ($silent && die("cant preload ADMIN_PASSWORD")) 
                || <STDIN>;
            chomp $pass2;
            if ($pass1 ne $pass2) {
                print "\n\nPasswords don't match.  Try again!\n";
                $pass1 = "";
                $pass2 = "*";
            }
        }

        if ($^O !~ /MSWin32/i) {
            system("stty","echo"); # re-enable input echoing
        }

        $SIG{HUP}  = 'DEFAULT'; # and remove our interrupt hooks
        $SIG{INT}  = 'DEFAULT';
        $SIG{QUIT} = 'DEFAULT';
        $SIG{TERM} = 'DEFAULT';

        insert_new_user($login, $realname, $pass1);
    }

    # Put the admin in each group if not already    
    my $userid = $dbh->selectrow_array("SELECT userid FROM profiles WHERE " .
                                       $dbh->sql_istrcmp('login_name', '?'),
                                       undef, $login);

    # Admins get explicit membership and bless capability for the admin group
    my ($admingroupid) = $dbh->selectrow_array("SELECT id FROM groups 
                                                WHERE name = 'admin'");
    $dbh->do("INSERT INTO user_group_map 
        (user_id, group_id, isbless, grant_type) 
        VALUES ($userid, $admingroupid, 0, " . GRANT_DIRECT . ")");
    $dbh->do("INSERT INTO user_group_map 
        (user_id, group_id, isbless, grant_type) 
        VALUES ($userid, $admingroupid, 1, " . GRANT_DIRECT . ")");

    # Admins get inherited membership and bless capability for all groups
    foreach my $group ( @groups ) {
        my $sth_check = $dbh->prepare("SELECT member_id FROM group_group_map
                                 WHERE member_id = ?
                                 AND  grantor_id = ?
                                 AND grant_type = ?");
        $sth_check->execute($admingroupid, $group, GROUP_MEMBERSHIP);
        unless ($sth_check->rows) {
            $dbh->do("INSERT INTO group_group_map
                      (member_id, grantor_id, grant_type)
                      VALUES ($admingroupid, $group, " . GROUP_MEMBERSHIP . ")");
        }
        $sth_check->execute($admingroupid, $group, GROUP_BLESS);
        unless ($sth_check->rows) {
            $dbh->do("INSERT INTO group_group_map
                      (member_id, grantor_id, grant_type)
                      VALUES ($admingroupid, $group, " . GROUP_BLESS . ")");
        }
    }

    print "\n$login is now set up as an administrator account.\n";
}


#
# Final checks...

$sth = $dbh->prepare("SELECT user_id " .
                       "FROM groups INNER JOIN user_group_map " .
                       "ON groups.id = user_group_map.group_id " .
                      "WHERE groups.name = 'admin'");
$sth->execute;
my ($adminuid) = $sth->fetchrow_array;
if (!$adminuid) { die "No administrator!" } # should never get here
# when test product was created, admin was unknown
$dbh->do("UPDATE components " .
            "SET initialowner = $adminuid " .
          "WHERE initialowner = 0");

# Check if the default parameter for urlbase is still set, and if so, give
# notification that they should go and visit editparams.cgi 

my @params = Bugzilla::Config::Core::get_param_list();
my $urlbase_default = '';
foreach my $item (@params) {
    next unless $item->{'name'} eq 'urlbase';
    $urlbase_default = $item->{'default'};
    last;
}

if (Bugzilla->params->{'urlbase'} eq $urlbase_default) {
    print "Now that you have installed Bugzilla, you should visit the \n" .
          "'Parameters' page (linked in the footer of the Administrator \n" .
          "account) to ensure it is set up as you wish - this includes \n" .
          "setting the 'urlbase' option to the correct url.\n" unless $silent;
}
################################################################################
