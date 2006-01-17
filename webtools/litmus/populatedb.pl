#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;
use Getopt::Long;
use Litmus::Config;
use DBI;
$|++;

my $reset_db;
my $help;
GetOptions('help|?' => \$help,'r|resetdb' => \$reset_db);

if ($help) {
  &usage;
  exit;
}

unless (-e "data/") {
    system("mkdir", "data/");
}
system("chmod -R 777 data/");
unless (-e "localconfig") {
     open(OUT, ">localconfig");
         print OUT <<EOT;
our \$db_host = "";
our \$db_name = "";
our \$db_user = "";
our \$db_pass = "";

our \$user_cookiename = "litmus_login";
our \$sysconfig_cookiename = "litmustestingconfiguration";

our \$bugzilla_db = "bugzilla";
our \$bugzilla_host = "localhost";
our \$bugzilla_user = "litmus";
out \$bugzilla_pass = "litmus";
EOT
        close(OUT);
        print "Go edit 'localconfig' with your configuration and \n";
        print "run this script again.";
        exit;
} 

# Create database tables, in large part borrowed from the old-school
# --TABLE-- code in checksetup.pl:
print "\nChecking for missing/new database tables...\n";
our %table;
do 'schema.pl'; # import the schema (%table)
my $dbh = DBI->connect("dbi:mysql:;$Litmus::Config::db_host", 
	$Litmus::Config::db_user,
	$Litmus::Config::db_pass) || 
		die "Could not connect to mysql database $Litmus::Config::db_host";

my @databases = $dbh->func('_ListDBs');
    unless (grep($_ eq $Litmus::Config::db_name, @databases)) {
       print "Creating database $Litmus::Config::db_name ...\n";
       if (!$dbh->func('createdb', $Litmus::Config::db_name, 'admin')) {
            my $error = $dbh->errstr;
            die "Could not create database $Litmus::Config::db_name: $error\n";
	}
}
$dbh->disconnect if $dbh;

$dbh = DBI->connect(
	"dbi:mysql:$Litmus::Config::db_name:$Litmus::Config::db_host",
	$Litmus::Config::db_user,
	$Litmus::Config::db_pass);


# Get a list of the existing tables (if any) in the database
my $sth = $dbh->table_info(undef, undef, undef, "TABLE");
my @tables = @{$dbh->selectcol_arrayref($sth, { Columns => [3] })};

# go throught our %table hash and create missing tables
while (my ($tabname, $fielddef) = each %table) {
    next if grep($_ eq $tabname, @tables);
    print "Creating table $tabname ...\n";

    $dbh->do("CREATE TABLE $tabname (\n$fielddef\n) TYPE = MYISAM")
        or die "Could not create table '$tabname'. Please check your database access.\n";
}

# populate the database with the default data in populatedb.sql:
if ($reset_db) {
  my $data_file = "populatedb.sql";

  print "Populating tables with default data...";
  my $cmd = "mysql --user=$Litmus::Config::db_user --password=$Litmus::Config::db_pass $Litmus::Config::db_name < $data_file";
  my $rv = system($cmd);
  if ($rv) {
    die "Error populating database $Litmus::Config::db_name";
  }
  print "done.\n";
}

# UPGRADE THE SCHEMA
# Now we need to deal with upgrading old installations by adding new fields and 
# indicies to the schema. To do this, we use the helpful Litmus::DBTools module
# Note that anything changed here should also be added to schema.pl for new 
# installations 
use Litmus::DBTools;
my $dbtool = Litmus::DBTools->new($dbh);

# ZLL: Authentication System fields
$dbtool->DropField("users", "is_trusted");
$dbtool->AddField("users", "bugzilla_uid", "int default '1'");
$dbtool->AddField("users", "password", "varchar(255)");
$dbtool->AddField("users", "realname", "varchar(255)");
$dbtool->AddField("users", "disabled", "mediumtext");
$dbtool->AddField("users", "is_admin", "tinyint(1) default '0'");

# replace enums with more portable and flexible formats:
$dbtool->ChangeFieldType("products", "enabled", 'tinyint default \'1\'');
$dbtool->ChangeFieldType("test_groups", "obsolete", 'tinyint default \'0\'');

# javascript cache
print "Rebuilding JS cache...";
require Litmus::Cache;
Litmus::Cache::rebuildCache(); 
print "done.\n";

exit;

#########################################################################
sub usage() {
  print "./populatedb.pl [-h|--help] [-r|--resetdb]\n";
}
