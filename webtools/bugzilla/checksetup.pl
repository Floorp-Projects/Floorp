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
#                 Dan Mosedale <dmose@mozilla.org>
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
    $db_host $db_port $db_name $db_user $db_pass $db_check
    @severities @priorities @opsys @platforms
);


# Trim whitespace from front and back.

sub trim {
    ($_) = (@_);
    s/^\s+//g;
    s/\s+$//g;
    return $_;
}



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
    "the optional libgd and the Perl modules GD-1.19 and Chart::Base-0.99b, e.g. by\n",
    "running (as root)\n\n",
    "   perl -MCPAN -eshell\n",
    "   install LDS/GD-1.19.tar.gz\n",
    "   install N/NI/NINJAZ/Chart-0.99b.tar.gz";
}





###########################################################################
# Check and update local configuration
###########################################################################

#
# This is quite tricky. But fun!
#
# First we read the file 'localconfig'. Then we check if the variables we
# need are defined. If not, localconfig will be amended by the new settings
# and the user informed to check this. The program then stops.
#
# Why do it this way around?
#
# Assume we will enhance Bugzilla and eventually more local configuration
# stuff arises on the horizon.
#
# But the file 'localconfig' is not in the Bugzilla CVS or tarfile. You
# know, we never want to overwrite your own version of 'localconfig', so
# we can't put it into the CVS/tarfile, can we?
#
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
LocalVar('$db_pass', '
#
# Some people actually use passwords with their MySQL database ...
#
$db_pass = "";
');



LocalVar('$db_check', '
#
# Should checksetup.pl try to check if your MySQL setup is correct?
# (with some combinations of MySQL/Msql-mysql/Perl/moonphase this doesn\'t work)
#
$db_check = 1;
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
    print "\nThis version of Bugzilla contains some variables that you may \n",
          "to change and adapt to your local settings. Please edit the file\n",
          "'localconfig' and rerun checksetup.pl\n\n",
          "The following variables are new to localconfig since you last ran\n",
          "checksetup.pl:  $newstuff\n\n";
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
                'checksetup.pl',
                'syncshadowdb';

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

use DBI;

# get a handle to the low-level DBD driver
my $drh = DBI->install_driver($db_base)
    or die "Can't connect to the $db_base. Is the database installed and up and running?\n";

if ($db_check) {
    # Do we have the database itself?
    my @databases = $drh->func($db_host, $db_port, '_ListDBs');
    unless (grep /^$db_name$/, @databases) {
    print "Creating database $db_name ...\n";
    $drh->func('createdb', $db_name, 'admin')
            or die <<"EOF"

The '$db_name' database is not accessible. This might have several reasons:

* MySQL is not running.
* MySQL is running, but the rights are not set correct. Go and read the
  README file of Bugzilla and all parts of the MySQL documentation.
* There is an subtle problem with Perl, DBI, DBD::mysql and MySQL. Make
  sure all settings in 'localconfig' are correct. If all else fails, set
  '\$db_check' to zero.\n
EOF
    }
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
    fieldid mediumint not null,
    oldvalue tinytext,
    newvalue tinytext,

    index (bug_id),
    index (bug_when),
    index (fieldid)';


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
    bug_status enum("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED", "RESOLVED", "VERIFIED", "CLOSED") not null,
    creation_ts datetime not null,
    delta_ts timestamp,
    short_desc mediumtext,
    op_sys enum($opsys) not null,
    priority enum($priorities) not null,
    product varchar(64) not null,
    rep_platform enum($platforms),
    reporter mediumint not null,
    version varchar(16) not null,
    component varchar(50) not null,
    resolution enum("", "FIXED", "INVALID", "WONTFIX", "LATER", "REMIND", "DUPLICATE", "WORKSFORME") not null,
    target_milestone varchar(20) not null default "---",
    qa_contact mediumint not null,
    status_whiteboard mediumtext not null,
    votes mediumint not null,
    keywords mediumtext not null, ' # Note: keywords field is only a cache;
                                # the real data comes from the keywords table.
    . '
    lastdiffed datetime not null,
    everconfirmed tinyint not null,

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

    index(who),
    unique(bug_id,who)';

$table{watch} =
   'watcher mediumint not null,
    watched mediumint not null,

    index(watched),
    unique(watcher,watched)';


$table{longdescs} = 
   'bug_id mediumint not null,
    who mediumint not null,
    bug_when datetime not null,
    thetext mediumtext,

    index(bug_id),
    index(bug_when)';


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
    votesperuser smallint not null,
    maxvotesperbug smallint not null default 10000,
    votestoconfirm smallint not null,
    defaultmilestone varchar(20) not null default "---"
';


$table{profiles} =
   'userid mediumint not null auto_increment primary key,
    login_name varchar(255) not null,
    password varchar(16),
    cryptpassword varchar(64),
    realname varchar(255),
    groupset bigint not null,
    emailnotification enum("ExcludeSelfChanges", "CConly", "All") not null default "ExcludeSelfChanges",
    disabledtext mediumtext not null,
    newemailtech tinyint not null,
    mybugslink tinyint not null default 1,
    blessgroupset bigint not null,


    unique(login_name)';


$table{profiles_activity} = 
   'userid mediumint not null,
    who mediumint not null,
    profiles_when datetime not null,
    fieldid mediumint not null,
    oldvalue tinytext,
    newvalue tinytext,

    index (userid),
    index (profiles_when),
    index (fieldid)';


$table{namedqueries} =
    'userid mediumint not null,
     name varchar(64) not null,
     watchfordiffs tinyint not null,
     linkinfooter tinyint not null,
     query mediumtext not null,

     unique(userid, name),
     index(watchfordiffs)';

# This isn't quite cooked yet...
#
#  $table{diffprefs} =
#     'userid mediumint not null,
#      fieldid mediumint not null,
#      mailhead tinyint not null,
#      maildiffs tinyint not null,
#
#      index(userid)';

$table{fielddefs} =
   'fieldid mediumint not null auto_increment primary key,
    name varchar(64) not null,
    description mediumtext not null,
    mailhead tinyint not null default 0,
    sortkey smallint not null,

    unique(name),
    index(sortkey)';

$table{versions} =
   'value tinytext,
    program varchar(64) not null';


$table{votes} =
   'who mediumint not null,
    bug_id mediumint not null,
    count smallint not null,

    index(who),
    index(bug_id)';

$table{keywords} =
    'bug_id mediumint not null,
     keywordid smallint not null,

     index(keywordid),
     unique(bug_id,keywordid)';

$table{keyworddefs} =
    'id smallint not null primary key,
     name varchar(64) not null,
     description mediumtext,

     unique(name)';


$table{milestones} =
    'value varchar(20) not null,
     product varchar(64) not null,
     sortkey smallint not null,
     unique (product, value)';

$table{shadowlog} =
    'id int not null auto_increment primary key,
     ts timestamp,
     reflected tinyint not null,
     command mediumtext not null,

     index(reflected)';



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

sub GroupExists ($)
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
# This subroutine checks if a group exist. If not, it will be automatically
# created with the next available bit set
#

sub AddGroup {
    my ($name, $desc, $userregexp) = @_;
    $userregexp ||= "";

    return if GroupExists($name);
    
    # get highest bit number
    my $sth = $dbh->prepare("SELECT bit FROM groups ORDER BY bit DESC");
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
    $sth->execute($bit, $name, $desc, $userregexp);
    return $bit;
}


#
# BugZilla uses --GROUPS-- to assign various rights to its users. 
#

AddGroup 'tweakparams',      'Can tweak operating parameters';
AddGroup 'editusers',      'Can edit or disable users';
AddGroup 'creategroups',     'Can create and destroy groups.';
AddGroup 'editcomponents',   'Can create, destroy, and edit components.';
AddGroup 'editkeywords',   'Can create, destroy, and edit keywords.';

if (!GroupExists("editbugs")) {
    my $id = AddGroup('editbugs',  'Can edit all aspects of any bug.', ".*");
    $dbh->do("UPDATE profiles SET groupset = groupset | $id");
}

if (!GroupExists("canconfirm")) {
    my $id = AddGroup('canconfirm',  'Can confirm a bug.', ".*");
    $dbh->do("UPDATE profiles SET groupset = groupset | $id");
}





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
# Populate the list of fields.
###########################################################################

my $headernum = 1;

sub AddFDef ($$$) {
    my ($name, $description, $mailhead) = (@_);

    $name = $dbh->quote($name);
    $description = $dbh->quote($description);

    my $sth = $dbh->prepare("SELECT fieldid FROM fielddefs " .
                            "WHERE name = $name");
    $sth->execute();
    my ($fieldid) = ($sth->fetchrow_array());
    if (!$fieldid) {
        $fieldid = 'NULL';
    }

    $dbh->do("REPLACE INTO fielddefs " .
             "(fieldid, name, description, mailhead, sortkey) VALUES " .
             "($fieldid, $name, $description, $mailhead, $headernum)");
    $headernum++;
}


AddFDef("bug_id", "Bug \#", 1);
AddFDef("short_desc", "Summary", 1);
AddFDef("product", "Product", 1);
AddFDef("version", "Version", 1);
AddFDef("rep_platform", "Platform", 1);
AddFDef("bug_file_loc", "URL", 1);
AddFDef("op_sys", "OS/Version", 1);
AddFDef("bug_status", "Status", 1);
AddFDef("status_whiteboard", "Status Whiteboard", 1);
AddFDef("keywords", "Keywords", 1);
AddFDef("resolution", "Resolution", 1);
AddFDef("bug_severity", "Severity", 1);
AddFDef("priority", "Priority", 1);
AddFDef("component", "Component", 1);
AddFDef("assigned_to", "AssignedTo", 1);
AddFDef("reporter", "ReportedBy", 1);
AddFDef("votes", "Votes", 0);
AddFDef("qa_contact", "QAContact", 0);
AddFDef("cc", "CC", 0);
AddFDef("dependson", "BugsThisDependsOn", 0);
AddFDef("blocked", "OtherBugsDependingOnThis", 0);
AddFDef("attachments.description", "Attachment description", 0);
AddFDef("attachments.thedata", "Attachment data", 0);
AddFDef("attachments.mimetype", "Attachment mime type", 0);
AddFDef("attachments.ispatch", "Attachment is patch", 0);
AddFDef("target_milestone", "Target Milestone", 0);
AddFDef("delta_ts", "Last changed date", 0);
AddFDef("(to_days(now()) - to_days(bugs.delta_ts))", "Days since bug changed",
        0);
AddFDef("longdesc", "Comment", 0);
    
    


###########################################################################
# Detect changed local settings
###########################################################################

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

sub GetIndexDef ($$)
{
    my ($table, $field) = @_;
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[2] ne $field;
        return $ref;
   }
}

sub CountIndexes ($)
{
    my ($table) = @_;
    
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    if ( $sth->rows == -1 ) {
      die ("Unexpected response while counting indexes in $table:" .
           " \$sth->rows == -1");
    }
    
    return ($sth->rows);
}

sub DropIndexes ($)
{
    my ($table) = @_;
    my %SEEN;

    # get the list of indexes
    #
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    # drop each index
    #
    while ( my $ref = $sth->fetchrow_arrayref) {
      
      # note that some indexes are described by multiple rows in the
      # index table, so we may have already dropped the index described
      # in the current row.
      # 
      next if exists $SEEN{$$ref[2]};

      my $dropSth = $dbh->prepare("ALTER TABLE $table DROP INDEX $$ref[2]");
      $dropSth->execute;
      $dropSth->finish;
      $SEEN{$$ref[2]} = 1;

    }

}
#
# Check if the enums in the bugs table return the same values that are defined
# in the various locally changeable variables. If this is true, then alter the
# table definition.
#

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

    my $oldtype = $ref->[1];
    if ($ref->[4]) {
        $oldtype .= qq{ default "$ref->[4]"};
    }

    if ($oldtype ne $newtype) {
        print "Updating field type $field in table $table ...\n";
        print "old: $oldtype\n";
        print "new: $newtype\n";
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


my $regenerateshadow = 0;




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



# 2000-01-20 Added a new "longdescs" table, which is supposed to have all the
# long descriptions in it, replacing the old long_desc field in the bugs 
# table.  The below hideous code populates this new table with things from
# the old field, with ugly parsing and heuristics.

sub WriteOneDesc {
    my ($id, $who, $when, $buffer) = (@_);
    $buffer = trim($buffer);
    if ($buffer eq '') {
        return;
    }
    $dbh->do("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
             "($id, $who, " .  time2str("'%Y/%m/%d %H:%M:%S'", $when) .
             ", " . $dbh->quote($buffer) . ")");
}


if (GetFieldDef('bugs', 'long_desc')) {
    eval("use Date::Parse");
    eval("use Date::Format");
    my $sth = $dbh->prepare("SELECT count(*) FROM bugs");
    $sth->execute();
    my ($total) = ($sth->fetchrow_array);

    print "Populating new long_desc table.  This is slow.  There are $total\n";
    print "bugs to process; a line of dots will be printed for each 50.\n\n";
    $| = 1;

    $dbh->do("LOCK TABLES bugs write, longdescs write, profiles write");

    $dbh->do('DELETE FROM longdescs');

    $sth = $dbh->prepare("SELECT bug_id, creation_ts, reporter, long_desc " .
                         "FROM bugs ORDER BY bug_id");
    $sth->execute();
    my $count = 0;
    while (1) {
        my ($id, $createtime, $reporterid, $desc) = ($sth->fetchrow_array());
        if (!$id) {
            last;
        }
        print ".";
        $count++;
        if ($count % 10 == 0) {
            print " ";
            if ($count % 50 == 0) {
                print "$count/$total (" . int($count * 100 / $total) . "%)\n";
            }
        }
        $desc =~ s/\r//g;
        my $who = $reporterid;
        my $when = str2time($createtime);
        my $buffer = "";
        foreach my $line (split(/\n/, $desc)) {
            $line =~ s/\s+$//g;       # Trim trailing whitespace.
            if ($line =~ /^------- Additional Comments From ([^\s]+)\s+(\d.+\d)\s+-------$/) {
                my $name = $1;
                my $date = str2time($2);
                $date += 59;    # Oy, what a hack.  The creation time is
                                # accurate to the second.  But we the long
                                # text only contains things accurate to the
                                # minute.  And so, if someone makes a comment
                                # within a minute of the original bug creation,
                                # then the comment can come *before* the
                                # bug creation.  So, we add 59 seconds to
                                # the time of all comments, so that they
                                # are always considered to have happened at
                                # the *end* of the given minute, not the
                                # beginning.
                if ($date >= $when) {
                    WriteOneDesc($id, $who, $when, $buffer);
                    $buffer = "";
                    $when = $date;
                    my $s2 = $dbh->prepare("SELECT userid FROM profiles " .
                                           "WHERE login_name = " .
                                           $dbh->quote($name));
                    $s2->execute();
                    ($who) = ($s2->fetchrow_array());
                    if (!$who) {
                        # This username doesn't exist.  Try a special
                        # netscape-only hack (sorry about that, but I don't
                        # think it will hurt any other installations).  We
                        # have many entries in the bugsystem from an ancient
                        # world where the "@netscape.com" part of the loginname
                        # was omitted.  So, look up the user again with that
                        # appended, and use it if it's there.
                        if ($name !~ /\@/) {
                            my $nsname = $name . "\@netscape.com";
                            $s2 =
                                $dbh->prepare("SELECT userid FROM profiles " .
                                              "WHERE login_name = " .
                                              $dbh->quote($nsname));
                            $s2->execute();
                            ($who) = ($s2->fetchrow_array());
                        }
                    }
                            
                    if (!$who) {
                        # This username doesn't exist.  Maybe someone renamed
                        # him or something.  Invent a new profile entry,
                        # disabled, just to represent him.
                        $dbh->do("INSERT INTO profiles " .
                                 "(login_name, password, cryptpassword," .
                                 " disabledtext) VALUES (" .
                                 $dbh->quote($name) .
                                 ", 'okthen', encrypt('okthen'), " .
                                 "'Account created only to maintain database integrity')");
                        $s2 = $dbh->prepare("SELECT LAST_INSERT_ID()");
                        $s2->execute();
                        ($who) = ($s2->fetchrow_array());
                    }
                    next;
                } else {
#                    print "\nDecided this line of bug $id has a date of " .
#                        time2str("'%Y/%m/%d %H:%M:%S'", $date) .
#                            "\nwhich is less than previous line:\n$line\n\n";
                }

            }
            $buffer .= $line . "\n";
        }
        WriteOneDesc($id, $who, $when, $buffer);
    }
                

    print "\n\n";

    DropField('bugs', 'long_desc');

    $dbh->do("UNLOCK TABLES");
    $regenerateshadow = 1;

}


# 2000-01-18 Added a new table fielddefs that records information about the
# different fields we keep an activity log on.  The bugs_activity table
# now has a pointer into that table instead of recording the name directly.

if (GetFieldDef('bugs_activity', 'field')) {
    AddField('bugs_activity', 'fieldid',
             'mediumint not null, ADD INDEX (fieldid)');
    print "Populating new fieldid field ...\n";

    $dbh->do("LOCK TABLES bugs_activity WRITE, fielddefs WRITE");

    my $sth = $dbh->prepare('SELECT DISTINCT field FROM bugs_activity');
    $sth->execute();
    my %ids;
    while (my ($f) = ($sth->fetchrow_array())) {
        my $q = $dbh->quote($f);
        my $s2 =
            $dbh->prepare("SELECT fieldid FROM fielddefs WHERE name = $q");
        $s2->execute();
        my ($id) = ($s2->fetchrow_array());
        if (!$id) {
            $dbh->do("INSERT INTO fielddefs (name, description) VALUES " .
                     "($q, $q)");
            $s2 = $dbh->prepare("SELECT LAST_INSERT_ID()");
            $s2->execute();
            ($id) = ($s2->fetchrow_array());
        }
        $dbh->do("UPDATE bugs_activity SET fieldid = $id WHERE field = $q");
    }
    $dbh->do("UNLOCK TABLES");

    DropField('bugs_activity', 'field');
}
        

# 2000-01-18 New email-notification scheme uses a new field in the bug to 
# record when email notifications were last sent about this bug.  Also,
# added a user pref whether a user wants to use the brand new experimental
# stuff.

if (!GetFieldDef('bugs', 'lastdiffed')) {
    AddField('bugs', 'lastdiffed', 'datetime not null');
    $dbh->do('UPDATE bugs SET lastdiffed = now(), delta_ts = delta_ts');
}

AddField('profiles', 'newemailtech', 'tinyint not null');


# 2000-01-22 The "login_name" field in the "profiles" table was not
# declared to be unique.  Sure enough, somehow, I got 22 duplicated entries
# in my database.  This code detects that, cleans up the duplicates, and
# then tweaks the table to declare the field to be unique.  What a pain.

if (GetIndexDef('profiles', 'login_name')->[1]) {
    print "Searching for duplicate entries in the profiles table ...\n";
    while (1) {
        # This code is weird in that it loops around and keeps doing this
        # select again.  That's because I'm paranoid about deleting entries
        # out from under us in the profiles table.  Things get weird if
        # there are *three* or more entries for the same user...
        $sth = $dbh->prepare("SELECT p1.userid, p2.userid, p1.login_name " .
                             "FROM profiles AS p1, profiles AS p2 " .
                             "WHERE p1.userid < p2.userid " .
                             "AND p1.login_name = p2.login_name " .
                             "ORDER BY p1.login_name");
        $sth->execute();
        my ($u1, $u2, $n) = ($sth->fetchrow_array);
        if (!$u1) {
            last;
        }
        print "Both $u1 & $u2 are ids for $n!  Merging $u2 into $u1 ...\n";
        foreach my $i (["bugs", "reporter"],
                       ["bugs", "assigned_to"],
                       ["bugs", "qa_contact"],
                       ["attachments", "submitter_id"],
                       ["bugs_activity", "who"],
                       ["cc", "who"],
                       ["votes", "who"],
                       ["longdescs", "who"]) {
            my ($table, $field) = (@$i);
            print "   Updating $table.$field ...\n";
            my $extra = "";
            if ($table eq "bugs") {
                $extra = ", delta_ts = delta_ts";
            }
            $dbh->do("UPDATE $table SET $field = $u1 $extra " .
                     "WHERE $field = $u2");
        }
        $dbh->do("DELETE FROM profiles WHERE userid = $u2");
    }
    print "OK, changing index type to prevent duplicates in the future ...\n";
    
    $dbh->do("ALTER TABLE profiles DROP INDEX login_name");
    $dbh->do("ALTER TABLE profiles ADD UNIQUE (login_name)");

}    


# 2000-01-24 Added a new field to let people control whether the "My
# bugs" link appears at the bottom of each page.  Also can control
# whether each named query should show up there.

AddField('profiles', 'mybugslink', 'tinyint not null default 1');
AddField('namedqueries', 'linkinfooter', 'tinyint not null');


# 2000-02-12 Added a new state to bugs, UNCONFIRMED.  Added ability to confirm
# a vote via bugs.  Added user bits to control which users can confirm bugs
# by themselves, and which users can edit bugs without their names on them.
# Added a user field which controls which groups a user can put other users 
# into.

my @states = ("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED", "RESOLVED",
              "VERIFIED", "CLOSED");
CheckEnumField('bugs', 'bug_status', @states);
if (!GetFieldDef('bugs', 'everconfirmed')) {
    AddField('bugs', 'everconfirmed',  'tinyint not null');
    $dbh->do("UPDATE bugs SET everconfirmed = 1, delta_ts = delta_ts");
}
AddField('products', 'maxvotesperbug', 'smallint not null default 10000');
AddField('products', 'votestoconfirm', 'smallint not null');
AddField('profiles', 'blessgroupset', 'bigint not null');

# 2000-03-21 Adding a table for target milestones to 
# database - matthew@zeroknowledge.com

$sth = $dbh->prepare("SELECT count(*) from milestones");
$sth->execute();
if (!($sth->fetchrow_arrayref()->[0])) {
    print "Replacing blank milestones...\n";
    $dbh->do("UPDATE bugs SET target_milestone = '---', delta_ts=delta_ts WHERE target_milestone = ' '");
    
# Populate milestone table with all exisiting values in database
    $sth = $dbh->prepare("SELECT DISTINCT target_milestone, product FROM bugs");
    $sth->execute();
    
    print "Populating milestones table...\n";
    
    my $value;
    my $product;
    while(($value, $product) = $sth->fetchrow_array())
    {
        # check if the value already exists
        my $sortkey = substr($value, 1);
        if ($sortkey !~ /^\d+$/) {
            $sortkey = 0;
        } else {
            $sortkey *= 10;
        }
        $value = $dbh->quote($value);
        $product = $dbh->quote($product);
        my $s2 = $dbh->prepare("SELECT value FROM milestones WHERE value = $value AND product = $product");
        $s2->execute();
        
        if(!$s2->fetchrow_array())
        {
            $dbh->do("INSERT INTO milestones(value, product, sortkey) VALUES($value, $product, $sortkey)");
        }
    }
}

# 2000-03-22 Changed the default value for target_milestone to be "---"
# (which is still not quite correct, but much better than what it was 
# doing), and made the size of the value field in the milestones table match
# the size of the target_milestone field in the bugs table.

ChangeFieldType('bugs', 'target_milestone',
                'varchar(20) default "---"');
ChangeFieldType('milestones', 'value', 'varchar(20)');


# 2000-03-23 Added a defaultmilestone field to the products table, so that
# we know which milestone to initially assign bugs to.

if (!GetFieldDef('products', 'defaultmilestone')) {
    AddField('products', 'defaultmilestone',
             'varchar(20) not null default "---"');
    $sth = $dbh->prepare("SELECT product, defaultmilestone FROM products");
    $sth->execute();
    while (my ($product, $defaultmilestone) = $sth->fetchrow_array()) {
        $product = $dbh->quote($product);
        $defaultmilestone = $dbh->quote($defaultmilestone);
        my $s2 = $dbh->prepare("SELECT value FROM milestones " .
                               "WHERE value = $defaultmilestone " .
                               "AND product = $product");
        $s2->execute();
        if (!$s2->fetchrow_array()) {
            $dbh->do("INSERT INTO milestones(value, product) " .
                     "VALUES ($defaultmilestone, $product)");
        }
    }
}

# 2000-03-24 Added unique indexes into the cc and keyword tables.  This
# prevents certain database inconsistencies, and, moreover, is required for
# new generalized list code to work.

if ( CountIndexes('cc') != 3 ) {

    # XXX should eliminate duplicate entries before altering
    #
    print "Recreating indexes on cc table.\n";
    DropIndexes('cc');
    $dbh->do("ALTER TABLE cc ADD UNIQUE (bug_id,who)");
    $dbh->do("ALTER TABLE cc ADD INDEX (who)");

    $regenerateshadow=1; # cc fields no longer have spaces in them
}    

if ( CountIndexes('keywords') != 3 ) {

    # XXX should eliminate duplicate entries before altering
    #
    print "Recreating indexes on keywords table.\n";
    DropIndexes('keywords');
    $dbh->do("ALTER TABLE keywords ADD INDEX (keywordid)");
    $dbh->do("ALTER TABLE keywords ADD UNIQUE (bug_id,keywordid)");

}    

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
#
# Final checks...
if ($regenerateshadow) {
    print "Now regenerating the shadow database for all bugs.\n";
    system("./processmail", "regenerate");
}

unlink "data/versioncache";
print "Reminder: Bugzilla now requires version 3.22.5 or later of MySQL.\n";
print "Reminder: Bugzilla now requires version 8.7 or later of sendmail.\n";
