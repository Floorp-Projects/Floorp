#!/usr/bonsaitools/bin/perl -w
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
#
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>
#
#
#
# Hey, what's this?
#
# 'checksetup.pl' is a script that is supposed to run during installation
# time and also after every upgrade.
#
# The goal of this script is to make the installation even more easy.
# It does so by doing things for you as well as testing for problems
# early.
#
# And you can re-run it whenever you want. Especially after Bugzilla
# get's updated you SHOULD rerun it. Because then it may update your
# SQL table definitions so that they are again in sync with the code.
#
# So, currently this module does:
#
#     - check for required perl modules
#     - set defaults for local configuration variables
#     - create and populate the data directory after installation
#     - set the proper rights for the *.cgi, *.html ... etc files
#     - check if the code can access MySQL
#     - creates the database 'bugs' if the database does not exist
#     - creates the tables inside the database if they don't exist
#     - automatically changes the table definitions of older BugZilla
#       installations
#     - populates the groups
#     - put the first user into all groups so that the system can
#       be administrated
#     - changes already existing SQL tables if you change your local
#       settings, e.g. when you add a new platform
#
# People that install this module locally are not supposed to modify
# this script. This is done by shifting the user settable stuff intp
# a local configuration file 'localconfig'. When this file get's
# changed and 'checkconfig.pl' will be re-run, then the user changes
# will be reflected back into the database.
#
# Developers however have to modify this file at various places. To
# make this easier, I have added some special comments that one can
# search for.
#
#     To                                               Search for
#
#     add/delete local configuration variables         --LOCAL--
#     check for more prerequired modules               --MODULES--
#     change the defaults for local configuration vars --LOCAL--
#     update the assigned file permissions             --CHMOD--
#     add more MySQL-related checks                    --MYSQL--
#     change table definitions                         --TABLE--
#     add more groups                                  --GROUPS--
#
# Note: sometimes those special comments occur more then once. For
# example, --LOCAL-- is at least 3 times in this code!  --TABLE--
# also is used more than once. So search for every occurence!
#





###########################################################################
# Global definitions
###########################################################################

use diagnostics;
use strict;



#
# This are the --LOCAL-- variables defined in 'localconfig'
# 

use vars qw(
    $webservergroup
    $db_host $db_port $db_name $db_user
    @severities @priorities @opsys @platforms
);





###########################################################################
# Check required module
###########################################################################

#
# Here we check for --MODULES--
#

print "Checking perl modules ...\n";
unless (eval "require 5.004") {
    die "Sorry, you need at least Perl 5.004\n";
}

unless (eval "require DBI") {
    die "Please install the DBI module. You can do this by running (as root)\n\n",
        "       perl -MCPAN -eshell\n",
        "       install DBI\n";
}

unless (eval "require Data::Dumper") {
    die "Please install the Data::Dumper module. You can do this by running (as root)\n\n",
        "       perl -MCPAN -eshell\n",
        "       install Data::Dumper\n";
}

unless (eval "require Mysql") {
    die "Please install the Mysql database driver. You can do this by running (as root)\n\n",
        "       perl -MCPAN -eshell\n",
        "       install Msql-Mysql\n\n",
        "Be sure to enable the Mysql emulation!";
}

unless (eval "require Date::Parse") {
    die "Please install the Date::Parse module. You can do this by running (as root)\n\n",
        "       perl -MCPAN -eshell\n",
        "       install Date::Parse\n";
}

# The following two modules are optional:
my $charts = 0;
$charts++ if eval "require GD";
$charts++ if eval "require Chart::Base";
if ($charts != 2) {
    print "If you you want to see graphical bug dependency charts, you may install\n",
    "the optional libgd and the Perl modules GD and Chart::Base, e.g. by\n",
    "running (as root)\n\n",
    "   perl -MCPAN -eshell\n",
    "   install GD\n",
    "   install Chart::Base\n";
}





###########################################################################
# Check and update local configuration
###########################################################################

#
# This is quite tricky. But fun!
#
# First we read the file 'localconfig'. And then we check if the variables
# we need to be defined are defined. If not, localconfig will be amended by
# the new settings and the user informed to check this. The program then
# stops.
#
# Why do it this way around?
#
# Assume we will enhance Bugzilla and eventually more local configuration
# stuff arises on the horizon.
#
# But the file 'localconfig' is not in the Bugzilla CVS or tarfile. It should
# not be there so that we never overwrite user's local setups accidentally.
# Now, we need a new variable. We simply add the necessary stuff to checksetup.
# The user get's the new version of Bugzilla from the CVS, runs checksetup
# and checksetup finds out "Oh, there is something new". Then it adds some
# default value to the user's local setup and informs the user to check that
# to see if that is what the user wants.
#
# Cute, ey?
#

print "Checking user setup ...\n";
do 'localconfig';
my $newstuff = "";
sub LocalVar ($$)
{
    my ($name, $definition) = @_;

    # Is there a cleaner way to test if the variable defined in scalar $name
    # is defined or not?
    my $defined = 0;
    $_ = "\$defined = 1 if defined $name;";
    eval $_;
    return if $defined;

    $newstuff .= " " . $name;
    open FILE, '>>localconfig';
    print FILE $definition, "\n\n";
    close FILE;
}



#
# Set up the defaults for the --LOCAL-- variables below:
#

    
LocalVar('$webservergroup', '
#
# This is the group your web server runs on.
# If you have a windows box, ignore this setting.
# If you do not wish for checksetup to adjust the permissions of anything,
# set this to "".
# If you set this to anything besides "", you will need to run checksetup.pl
# as root.
$webservergroup = "nobody";
');



LocalVar('$db_host', '
#
# How to access the SQL database:
#
$db_host = "localhost";         # where is the database?
$db_port = 3306;                # which port to use
$db_name = "bugs";              # name of the MySQL database
$db_user = "bugs";              # user to attach to the MySQL database
');



LocalVar('@severities', '
#
# Which bug and feature-request severities do you want?
#
@severities = (
        "blocker",
        "critical",
        "major",
        "normal",
        "minor",
        "trivial",
        "enhancement"
);
');



LocalVar('@priorities', '
#
# Which priorities do you want to assign to bugs and feature-request?
#
@priorities = (
        "P1",
        "P2",
        "P3",
        "P4",
        "P5"
);
');



LocalVar('@opsys', '
#
# What operatings systems may your products run on?
#
@opsys = (
        "All",
        "Windows 3.1",
        "Windows 95",
        "Windows 98",
	"Windows 2000",
        "Windows NT",
        "Mac System 7",
        "Mac System 7.5",
        "Mac System 7.6.1",
        "Mac System 8.0",
        "Mac System 8.5",
        "Mac System 8.6",
	"Mac System 9.0",
        "AIX",
        "BSDI",
        "HP-UX",
        "IRIX",
        "Linux",
        "FreeBSD",
        "OSF/1",
        "Solaris",
        "SunOS",
        "Neutrino",
        "OS/2",
        "BeOS",
        "OpenVMS",
        "other"
);
');



LocalVar('@platforms', '
#
# What hardware platforms may your products run on?
#
@platforms = (
        "All",
        "DEC",
        "HP",
        "Macintosh",
        "PC",
        "SGI",
        "Sun",
        "Other"
);
');




if ($newstuff ne "") {
    print "This version of Bugzilla contains some variables that you may \n",
          "to change and adapt to your local settings. Please edit the file\n",
          "'localconfig' and rerun checksetup.pl\n\n",
          "The following variables are new to localconfig since you last ran\n",
          "checksetup.pl:  $newstuff\n";
    exit;
}





###########################################################################
# Check data directory
###########################################################################

#
# Create initial --DATA-- directory and make the initial empty files there:
#

unless (-d 'data') {
    print "Creating data directory ...\n";
    mkdir 'data', 0770;
    if ($webservergroup eq "") {
        chmod 0777, 'data';
    }
    open FILE, '>>data/comments'; close FILE;
    open FILE, '>>data/nomail'; close FILE;
    open FILE, '>>data/mail'; close FILE;
    chmod 0666, glob('data/*');
}


# Just to be sure ...
unlink "data/versioncache";





###########################################################################
# Set proper rights
###########################################################################

#
# Here we use --CHMOD-- and friends to set the file permissions
#
# The rationale is that the web server generally runs as nobody and so the cgi
# scripts should not be writable for nobody, otherwise someone may be possible
# to change the cgi's when exploiting some security flaw somewhere (not
# necessarily in Bugzilla!)
#
# Also, some *.pl files are executable, some are not.
#
# +++ Can anybody tell me what a Windows Perl would do with this code?
#

if ($webservergroup) {
    mkdir 'shadow', 0770 unless -d 'shadow';
    # Funny! getgrname returns the GID if fed with NAME ...
    my $webservergid = getgrnam($webservergroup);
    chown 0, $webservergid, glob('*');
    chmod 0640, glob('*');

    chmod 0750, glob('*.cgi'),
                'processmail',
                'whineatnews.pl',
                'collectstats.pl',
                'checksetup.pl';

    chmod 0770, 'data', 'shadow';
    chmod 0666, glob('data/*');
}





###########################################################################
# Check MySQL setup
###########################################################################

#
# Check if we have access to --MYSQL--
#

# This settings are not yet changeable, because other code depends on
# the fact that we use MySQL and not, say, PostgreSQL.

my $db_base = 'mysql';
my $db_pass = '';               # Password to attach to the MySQL database

use DBI;

# get a handle to the low-level DBD driver
my $drh = DBI->install_driver($db_base)
    or die "Can't connect to the $db_base. Is the database installed and up and running?\n";

# Do we have the database itself?
my @databases = $drh->func($db_host, $db_port, '_ListDBs');
unless (grep /^$db_name$/, @databases) {
    print "Creating database $db_name ...\n";
    $drh->func('createdb', $db_name, 'admin')
        or die "The '$db_name' database does not exist. I tried to create the database,\n",
               "but that didn't work, probably because of access rigths. Read the README\n",
               "file and the documentation of $db_base to make sure that everything is\n",
               "set up correctly.\n";
}

# now get a handle to the database:
my $connectstring = "dbi:$db_base:$db_name:host=$db_host:port=$db_port";
my $dbh = DBI->connect($connectstring, $db_user, $db_pass)
    or die "Can't connect to the table '$connectstring'.\n",
           "Have you read Bugzilla's README?  Have you read the doc of '$db_name'?\n";

END { $dbh->disconnect if $dbh }





###########################################################################
# Table definitions
###########################################################################

#
# The following hash stores all --TABLE-- definitions. This will be used
# to automatically create those tables that don't exist. The code is
# safer than the make*.sh shell scripts used to be, because they won't
# delete existing tables.
#
# If you want intentionally do this, yon can always drop a table and re-run
# checksetup, e.g. like this:
#
#    $ mysql bugs
#    mysql> drop table votes;
#    mysql> exit;
#    $ ./checksetup.pl
#
# If you change one of those field definitions, then also go below to the
# next occurence of the string --TABLE-- (near the end of this file) to
# add the code that updates older installations automatically.
#


my %table;

$table{bugs_activity} = 
   'bug_id mediumint not null,
    who mediumint not null,
    bug_when datetime not null,
    field varchar(64) not null,
    oldvalue tinytext,
    newvalue tinytext,

    index (bug_id),
    index (bug_when),
    index (field)';


$table{attachments} =
   'attach_id mediumint not null auto_increment primary key,
    bug_id mediumint not null,
    creation_ts timestamp,
    description mediumtext not null,
    mimetype mediumtext not null,
    ispatch tinyint,
    filename mediumtext not null,
    thedata longblob not null,
    submitter_id mediumint not null,

    index(bug_id),
    index(creation_ts)';


$table{bugs} =
   'bug_id mediumint not null auto_increment primary key,
    groupset bigint not null,
    assigned_to mediumint not null, # This is a comment.
    bug_file_loc text,
    bug_severity enum($severities) not null,
    bug_status enum("NEW", "ASSIGNED", "REOPENED", "RESOLVED", "VERIFIED", "CLOSED") not null,
    creation_ts datetime not null,
    delta_ts timestamp,
    short_desc mediumtext,
    long_desc mediumtext,
    op_sys enum($opsys) not null,
    priority enum($priorities) not null,
    product varchar(64) not null,
    rep_platform enum($platforms),
    reporter mediumint not null,
    version varchar(16) not null,
    component varchar(50) not null,
    resolution enum("", "FIXED", "INVALID", "WONTFIX", "LATER", "REMIND", "DUPLICATE", "WORKSFORME") not null,
    target_milestone varchar(20) not null,
    qa_contact mediumint not null,
    status_whiteboard mediumtext not null,
    votes mediumint not null,
    keywords mediumtext not null, ' # Note: keywords field is only a cache;
                                # the real data comes from the keywords table.
    . '

    index (assigned_to),
    index (creation_ts),
    index (delta_ts),
    index (bug_severity),
    index (bug_status),
    index (op_sys),
    index (priority),
    index (product),
    index (reporter),
    index (version),
    index (component),
    index (resolution),
    index (target_milestone),
    index (qa_contact),
    index (votes)';


$table{cc} =
   'bug_id mediumint not null,
    who mediumint not null,

    index(bug_id),
    index(who)';


$table{components} =
   'value tinytext,
    program varchar(64),
    initialowner tinytext not null,     # Should arguably be a mediumint!
    initialqacontact tinytext not null, # Should arguably be a mediumint!
    description mediumtext not null';


$table{dependencies} =
   'blocked mediumint not null,
    dependson mediumint not null,

    index(blocked),
    index(dependson)';


# Group bits must be a power of two. Groups are identified by a bit; sets of
# groups are indicated by or-ing these values together.
#
# isbuggroup is nonzero if this is a group that controls access to a set
# of bugs.  In otherword, the groupset field in the bugs table should only
# have this group's bit set if isbuggroup is nonzero.
#
# User regexp is which email addresses are initially put into this group.
# This is only used when an email account is created; otherwise, profiles
# may be individually tweaked to add them in and out of groups.

$table{groups} =
   'bit bigint not null,
    name varchar(255) not null,
    description text not null,
    isbuggroup tinyint not null,
    userregexp tinytext not null,

    unique(bit),
    unique(name)';


$table{logincookies} =
   'cookie mediumint not null auto_increment primary key,
    userid mediumint not null,
    cryptpassword varchar(64),
    hostname varchar(128),
    lastused timestamp,

    index(lastused)';


$table{products} =
   'product varchar(64),
    description mediumtext,
    milestoneurl tinytext not null,
    disallownew tinyint not null,
    votesperuser smallint not null';


$table{profiles} =
   'userid mediumint not null auto_increment primary key,
    login_name varchar(255) not null,
    password varchar(16),
    cryptpassword varchar(64),
    realname varchar(255),
    groupset bigint not null,
    emailnotification enum("ExcludeSelfChanges", "CConly", "All") not null default "ExcludeSelfChanges",
    disabledtext mediumtext not null,

    index(login_name)';


$table{versions} =
   'value tinytext,
    program varchar(64)';


$table{votes} =
   'who mediumint not null,
    bug_id mediumint not null,
    count smallint not null,

    index(who),
    index(bug_id)';

$table{keywords} =
    'bug_id mediumint not null,
     keywordid smallint not null,

     index(bug_id),
     index(keywordid)';

$table{keyworddefs} =
    'id smallint not null primary key,
     name varchar(64) not null,
     description mediumtext,

     unique(name)';



###########################################################################
# Create tables
###########################################################################

# The current DBI::mysql tells me to use this:
#my @tables = map { $_ =~ s/.*\.//; $_ } $dbh->tables();
# but that doesn't work on a freshly created database, so I still use
my @tables = $dbh->func('_ListTables');
#print 'Tables: ', join " ", @tables, "\n";

# add lines here if you add more --LOCAL-- config vars that end up in the enums:

my $severities = '"' . join('", "', @severities) . '"';
my $priorities = '"' . join('", "', @priorities) . '"';
my $opsys      = '"' . join('", "', @opsys)      . '"';
my $platforms  = '"' . join('", "', @platforms)  . '"';

# go throught our %table hash and create missing tables
while (my ($tabname, $fielddef) = each %table) {
    next if grep /^$tabname$/, @tables;
    print "Creating table $tabname ...\n";

    # add lines here if you add more --LOCAL-- config vars that end up in
    # the enums:

    $fielddef =~ s/\$severities/$severities/;
    $fielddef =~ s/\$priorities/$priorities/;
    $fielddef =~ s/\$opsys/$opsys/;
    $fielddef =~ s/\$platforms/$platforms/;

    $dbh->do("CREATE TABLE $tabname (\n$fielddef\n)")
        or die "Could not create table '$tabname'. Please check your '$db_base' access.\n";
}





###########################################################################
# Populate groups table
###########################################################################

#
# This subroutine checks if a group exist. If not, it will be automatically
# created with the next available bit set
#

sub AddGroup ($$)
{
    my ($name, $desc) = @_;

    # does the group exist?
    my $sth = $dbh->prepare("SELECT name FROM groups WHERE name='$name'");
    $sth->execute;
    return if $sth->rows;
    
    # get highest bit number
    $sth = $dbh->prepare("SELECT bit FROM groups ORDER BY bit DESC");
    $sth->execute;
    my @row = $sth->fetchrow_array;

    # normalize bits
    my $bit;
    if (defined $row[0]) {
        $bit = $row[0] << 1;
    } else {
        $bit = 1;
    }

   
    print "Adding group $name ...\n";
    $sth = $dbh->prepare('INSERT INTO groups
                          (bit, name, description, userregexp)
                          VALUES (?, ?, ?, ?)');
    $sth->execute($bit, $name, $desc, "");
}


#
# BugZilla uses --GROUPS-- to assign various rights to it's users. 
#

AddGroup 'tweakparams',      'Can tweak operating parameters';
AddGroup 'editusers',      'Can edit or disable users';
AddGroup 'editgroupmembers', 'Can put people in and out of groups that they are members of.';
AddGroup 'creategroups',     'Can create and destroy groups.';
AddGroup 'editcomponents',   'Can create, destroy, and edit components.';
AddGroup 'editkeywords',   'Can create, destroy, and edit keywords.';





###########################################################################
# Create initial test product if there are no products present.
###########################################################################

my $sth = $dbh->prepare("SELECT product FROM products");
$sth->execute;
unless ($sth->rows) {
    print "Creating initial dummy product 'TestProduct' ...\n";
    $dbh->do('INSERT INTO products(product, description) VALUES ("TestProduct",
              "This is a test product.  This ought to be blown away and ' .
             'replaced with real stuff in a finished installation of ' .
             'bugzilla.")');
    $dbh->do('INSERT INTO versions (value, program) VALUES ("other", "TestProduct")');
    $dbh->do('INSERT INTO components (value, program, description) VALUES (' .
             '"TestComponent", "TestProduct", ' .
             '"This is a test component in the test product database.  ' .
             'This ought to be blown away and replaced with real stuff in ' .
             'a finished installation of bugzilla.")');
}





###########################################################################
# Detect changed local settings
###########################################################################

#
# Check if the enums in the bugs table return the same values that are defined
# in the various locally changeable variables. If this is true, then alter the
# table definition.
#

sub GetFieldDef ($$)
{
    my ($table, $field) = @_;
    my $sth = $dbh->prepare("SHOW COLUMNS FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[0] ne $field;
        return $ref;
   }
}

sub CheckEnumField ($$@)
{
    my ($table, $field, @against) = @_;

    my $ref = GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";
    
    $_ = "enum('" . join("','", @against) . "')";
    if ($$ref[1] ne $_) {
        print "Updating field $field in table $table ...\n";
        $_ .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $field $_");
    }
}



#
# This code changes the enum types of some SQL tables whenever you change
# some --LOCAL-- variables
#

CheckEnumField('bugs', 'bug_severity', @severities);
CheckEnumField('bugs', 'priority',     @priorities);
CheckEnumField('bugs', 'op_sys',       @opsys);
CheckEnumField('bugs', 'rep_platform', @platforms);





###########################################################################
# Promote first user into every group
###########################################################################

#
# Assume you just logged in. Now how can you administrate the system? Just
# execute checksetup.pl again. If there is only 1 user in bugzilla, then
# this user is promoted into every group.
#

$sth = $dbh->prepare("SELECT login_name FROM profiles");
$sth->execute;
# when we have exactly one user ...
if ($sth->rows == 1) {
    my @row = $sth->fetchrow_array;
    print "Putting user $row[0] into every group ...\n";
    # are this enought f's for now?  :-)
    $dbh->do("update profiles set groupset=0xffffffffffff");
}






###########################################################################
# Update the tables to the current definition
###########################################################################

#
# As time passes, fields in tables get deleted, added, changed and so on.
# So we need some helper subroutines to make this possible:
#

sub ChangeFieldType ($$$)
{
    my ($table, $field, $newtype) = @_;

    my $ref = GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    if ($$ref[1] ne $newtype) {
        print "Updating field type $field in table $table ...\n";
        $newtype .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $field $newtype");
    }
}

sub RenameField ($$$)
{
    my ($table, $field, $newname) = @_;

    my $ref = GetFieldDef($table, $field);
    return unless $ref; # already fixed?
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    if ($$ref[1] ne $newname) {
        print "Updating field $field in table $table ...\n";
        my $type = $$ref[1];
        $type .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $newname $type");
    }
}

sub AddField ($$$)
{
    my ($table, $field, $definition) = @_;

    my $ref = GetFieldDef($table, $field);
    return if $ref; # already added?

    print "Adding new field $field to table $table ...\n";
    $dbh->do("ALTER TABLE $table
              ADD COLUMN $field $definition");
}

sub DropField ($$)
{
    my ($table, $field) = @_;

    my $ref = GetFieldDef($table, $field);
    return unless $ref; # already dropped?

    print "Deleting unused field $field from table $table ...\n";
    $dbh->do("ALTER TABLE $table
              DROP COLUMN $field");
}



# 1999-05-12 Added a pref to control how much email you get.  This needs a new
# column in the profiles table, so feed the following to mysql:

AddField('profiles', 'emailnotification', 'enum("ExcludeSelfChanges", "CConly", 
                      "All") not null default "ExcludeSelfChanges"');



# 1999-06-22 Added an entry to the attachments table to record who the
# submitter was.  Nothing uses this yet, but it still should be recorded.

AddField('attachments', 'submitter_id', 'mediumint not null');

#
# One could even populate this field automatically, e.g. with
#
# unless (GetField('attachments', 'submitter_id') {
#    AddField ...
#    populate
# }
#
# For now I was too lazy, so you should read the README :-)



# 1999-9-15 Apparently, newer alphas of MySQL won't allow you to have "when"
# as a column name.  So, I have had to rename a column in the bugs_activity
# table.

RenameField ('bugs_activity', 'when', 'bug_when');



# 1999-10-11 Restructured voting database to add a cached value in each bug
# recording how many total votes that bug has.  While I'm at it, I removed
# the unused "area" field from the bugs database.  It is distressing to
# realize that the bugs table has reached the maximum number of indices
# allowed by MySQL (16), which may make future enhancements awkward.
# (P.S. All is not lost; it appears that the latest betas of MySQL support
# a new table format which will allow 32 indices.)

DropField('bugs', 'area');
AddField('bugs',     'votes',        'mediumint not null, add index (votes)');
AddField('products', 'votesperuser', 'mediumint not null');



# The product name used to be very different in various tables.
#
# It was   varchar(16)   in bugs
#          tinytext      in components
#          tinytext      in products
#          tinytext      in versions
#
# tinytext is equivalent to varchar(255), which is quite huge, so I change
# them all to varchar(64).

ChangeFieldType ('bugs',       'product', 'varchar(64)');
ChangeFieldType ('components', 'program', 'varchar(64)');
ChangeFieldType ('products',   'product', 'varchar(64)');
ChangeFieldType ('versions',   'program', 'varchar(64)');

# 2000-01-16 Added a "keywords" field to the bugs table, which
# contains a string copy of the entries of the keywords table for this
# bug.  This is so that I can easily sort and display a keywords
# column in bug lists.

if (!GetFieldDef('bugs', 'keywords')) {
    AddField('bugs', 'keywords', 'mediumtext not null');

    my @kwords;
    print "Making sure 'keywords' field of table 'bugs' is empty ...\n";
    $dbh->do("UPDATE bugs SET delta_ts = delta_ts, keywords = '' " .
             "WHERE keywords != ''");
    print "Repopulating 'keywords' field of table 'bugs' ...\n";
    my $sth = $dbh->prepare("SELECT keywords.bug_id, keyworddefs.name " .
                            "FROM keywords, keyworddefs " .
                            "WHERE keyworddefs.id = keywords.keywordid " .
                            "ORDER BY keywords.bug_id, keyworddefs.name");
    $sth->execute;
    my @list;
    my $bugid = 0;
    my @row;
    while (1) {
        my ($b, $k) = ($sth->fetchrow_array());
        if (!defined $b || $b ne $bugid) {
            if (@list) {
                $dbh->do("UPDATE bugs SET delta_ts = delta_ts, keywords = " .
                         $dbh->quote(join(', ', @list)) .
                         " WHERE bug_id = $bugid");
            }
            if (!$b) {
                last;
            }
            $bugid = $b;
            @list = ();
        }
        push(@list, $k);
    }
}


# 2000-01-18 Added a "disabledtext" field to the profiles table.  If not
# empty, then this account has been disabled, and this field is to contain
# text describing why.

AddField('profiles', 'disabledtext',  'mediumtext not null');



#
# If you had to change the --TABLE-- definition in any way, then add your
# differential change code *** A B O V E *** this comment.
#
# That is: if you add a new field, you first search for the first occurence
# of --TABLE-- and add your field to into the table hash. This new setting
# would be honored for every new installation. Then add your
# AddField/DropField/ChangeFieldType/RenameField code above. This would then
# be honored by everyone who updates his Bugzilla installation.
#
