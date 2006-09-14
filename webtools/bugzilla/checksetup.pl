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
 ./checksetup.pl --make-admin=user@domain.com

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

=item B<--make-admin>=username@domain.com

Makes the specified user into a Bugzilla administrator. This is
in case you accidentally lock yourself out of the Bugzilla administrative
interface.

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
                     'verbose|v|no-silent', 'make-admin=s');

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

require Bugzilla::Bug;
import Bugzilla::Bug qw(is_open_state);

require Bugzilla::Install::Localconfig;
import Bugzilla::Install::Localconfig qw(update_localconfig);

require Bugzilla::Install::Filesystem;
import Bugzilla::Install::Filesystem qw(update_filesystem create_htaccess
                                        fix_all_file_permissions);
require Bugzilla::Install::DB;

require Bugzilla::DB;
require Bugzilla::Template;
require Bugzilla::Field;
require Bugzilla::Install;

Bugzilla->usage_mode(USAGE_MODE_CMDLINE);

###########################################################################
# Check and update --LOCAL-- configuration
###########################################################################

print "Reading " .  bz_locations()->{'localconfig'} . "...\n" unless $silent;
update_localconfig({ output => !$silent, answer => \%answer });
my $lc_hash = Bugzilla->localconfig;

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
# Changes to the fielddefs --TABLE--
###########################################################################

# Calling Bugzilla::Field::create_or_update depends on the
# fielddefs table having a modern definition. So, we have to make
# these particular schema changes before we make any other schema changes.
Bugzilla::Install::DB::update_fielddefs_definition();

Bugzilla::Field::populate_field_definitions();

###########################################################################
# Update the tables to the current definition --TABLE--
###########################################################################

Bugzilla::Install::DB::update_table_definitions();        

###########################################################################
# Bugzilla uses --GROUPS-- to assign various rights to its users.
###########################################################################

Bugzilla::Install::update_system_groups();

###########################################################################
# Create --SETTINGS-- users can adjust
###########################################################################

Bugzilla::Install::update_settings();

###########################################################################
# Create Administrator  --ADMIN--
###########################################################################

Bugzilla::Install::make_admin($switch{'make-admin'}) if $switch{'make-admin'};
Bugzilla::Install::create_admin({ answer => \%answer });

###########################################################################
# Create default Product and Classification
###########################################################################

Bugzilla::Install::create_default_product();

###########################################################################
# Final checks
###########################################################################

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
