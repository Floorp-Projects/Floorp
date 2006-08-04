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

=head2 Modifying the Database

Sometimes you'll want to modify the database. In fact, that's mostly
what checksetup does, is upgrade old Bugzilla databases to the modern
format.

If you'd like to know how to make changes to the datbase, see
the information in the Bugzilla Developer's Guide, at:
L<http:E<sol>E<sol>www.bugzilla.orgE<sol>docsE<sol>developer.html#sql-schema>

Also see L<Bugzilla::DB/"Schema Modification Methods"> and 
L<Bugzilla::DB/"Schema Information Methods">.

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

L<Bugzilla::Install::Requirements>,
L<Bugzilla::Install::Localconfig>,
L<Bugzilla::Install::Filesystem>,
L<Bugzilla::Install::DB>,
L<Bugzilla::Install>,
L<Bugzilla::Config/update_params>, and
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
# Changes to the fielddefs --TABLE--
###########################################################################

# Calling Bugzilla::Field::create_or_update depends on the
# fielddefs table having a modern definition. So, we have to make
# these particular schema changes before we make any other schema changes.
Bugzilla::Install::DB::update_fielddefs_definition();

Bugzilla::Field::populate_field_definitions();

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
# Update the tables to the current definition --TABLE--
###########################################################################

Bugzilla::Install::DB::update_table_definitions();        

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
