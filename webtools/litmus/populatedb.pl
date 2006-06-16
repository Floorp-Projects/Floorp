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
# Portions created by the Initial Developer are Copyright (C) 2006
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

our \$tr_host = "";
our \$tr_name = "";
our \$tr_user = "";
our \$tr_pass = "";

our \$bugzilla_auth_enabled = 1;
our \$bugzilla_db = "bugzilla";
our \$bugzilla_host = "localhost";
our \$bugzilla_user = "litmus";
our \$bugzilla_pass = "litmus";
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
# Now we need to deal with upgrading old installations by adding new fields 
# and indicies to the schema. To do this, we use the helpful Litmus::DBTools 
# module.
#
# NOTE: anything changed here should also be added to schema.pl for new 
# installations 
use Litmus::DBTools;
my $dbtool = Litmus::DBTools->new($dbh);

# ZLL: Authentication System fields
$dbtool->DropField("users", "is_trusted");
$dbtool->AddField("users", "bugzilla_uid", "int default '1'");
$dbtool->AddField("users", "password", "varchar(255)");
$dbtool->AddField("users", "realname", "varchar(255)");
$dbtool->AddField("users", "is_admin", "tinyint(1) default '0'");
$dbtool->AddField("users", "irc_nickname", "varchar(32)");
$dbtool->DropIndex("users", "email");
$dbtool->DropIndex("users", "irc_nickname");
$dbtool->AddUniqueKey("users","email","(email)");
$dbtool->AddUniqueKey("users","irc_nickname","(irc_nickname)");
$dbtool->AddKey("users","bugzilla_uid","(bugzilla_uid)");
$dbtool->AddKey("users","realname","(realname)");
$dbtool->AddKey("users","is_admin","(is_admin)");

# replace enums with more portable and flexible formats:
$dbtool->ChangeFieldType("products", "enabled", 'tinyint(1) default "1"');

$dbtool->DropField("test_result_logs", "log_path");
$dbtool->AddField("test_result_logs", "log_text", "longtext");
$dbtool->AddKey("test_result_logs", "log_text", "(log_text(255))");

$dbtool->AddField("test_results", "valid", "tinyint(1) default '1'");
$dbtool->AddField("test_results", "vetted", "tinyint(1) default '0'");
$dbtool->AddField("test_results", "validated_by_user_id", "int(11) default '0'");
$dbtool->AddField("test_results", "vetted_by_user_id", "int(11) default '0'");
$dbtool->AddField("test_results", "validated_timestamp", "datetime");
$dbtool->AddField("test_results", "vetted_timestamp", "datetime");
$dbtool->AddKey("test_results", "valid", "(valid)");
$dbtool->AddKey("test_results", "vetted", "(vetted)");
$dbtool->AddKey("test_results", "validated_by_user_id", "(validated_by_user_id)");
$dbtool->AddKey("test_results", "vetted_by_user_id", "(vetted_by_user_id)");
$dbtool->AddKey("test_results", "validated_timestamp", "(valid)");
$dbtool->AddKey("test_results", "vetted_timestamp", "(vetted)");
$dbtool->DropField("test_results", "validity_id");
$dbtool->DropField("test_results", "vetting_status_id");
$dbtool->DropTable("validity_lookup");
$dbtool->DropTable("vetting_status_lookup");

$dbtool->RenameTable("test_groups","testgroups");
$dbtool->AddField("testgroups", "enabled", "tinyint(1) NO NULL default '1'");
$dbtool->AddKey("testgroups","enabled","(enabled)");
$dbtool->DropField("testgroups", "obsolete");
$dbtool->DropField("testgroups", "expiration_days");

$dbtool->AddField("subgroups", "enabled", "tinyint(1) NOT NULL default '1'");
$dbtool->AddKey("subgroups","enabled","(enabled)");
$dbtool->AddField("subgroups", "product_id", "tinyint(4) NOT NULL");
$dbtool->AddKey("subgroups","product_id","(product_id)");
$dbtool->DropField("subgroups", "testgroup_id");

$dbtool->AddField("users", "enabled", "tinyint(1) NOT NULL default '1'");
$dbtool->AddKey("users","enabled","(enabled)");
$dbtool->DropField("users", "disabled");

$dbtool->DropTable("test_status_lookup");

# Remove reference to test_status_lookup
$dbtool->RenameTable("tests","testcases");
$dbtool->AddField("testcases", "enabled", "tinyint(1) NOT NULL default '1'");
$dbtool->AddKey("testcases","enabled","(enabled)");
$dbtool->DropIndex("testcases", "test_id");
$dbtool->RenameField("testcases", "test_id", "testcase_id");
$dbtool->AddKey("testcases","testcase_id","(testcase_id)");
$dbtool->AddKey("testcases","summary_2","(summary, steps, expected_results)");
$dbtool->ChangeFieldType("testcases", "community_enabled", 'tinyint(1) default "1"');
$dbtool->AddField("testcases", "product_id", "tinyint(4) NOT NULL");
$dbtool->AddKey("testcases","product_id","(product_id)");
$dbtool->DropField("testcases", "subgroup_id");

$dbtool->DropIndex("test_results", "test_id");
$dbtool->RenameField("test_results", "test_id", "testcase_id");
$dbtool->AddKey("test_results","testcase_id","(testcase_id)");
$dbtool->RenameField("test_results", "buildid", "build_id");
$dbtool->ChangeFieldType("test_results", "build_id", 'int(10) unsigned');
$dbtool->AddKey("test_results","build_id","(build_id)");
$dbtool->DropIndex("test_results", "result_id");
$dbtool->RenameField("test_results", "result_id", "result_status_id");
$dbtool->AddKey("test_results","result_status_id","(result_status_id)");
$dbtool->DropField("test_results", "platform_id");

$dbtool->AddField("branches", "enabled", "tinyint(1) NOT NULL default '1'");
$dbtool->AddKey("branches","enabled","(enabled)");

$dbtool->DropField("platforms", "product_id");

$dbtool->AddField("subgroup_testgroups", "sort_order", "smallint(6) NOT NULL default '1'");
$dbtool->AddKey("subgroup_testgroups","sort_order","(sort_order)");
$dbtool->AddField("testcase_subgroups", "sort_order", "smallint(6) NOT NULL default '1'");
$dbtool->AddKey("testcase_subgroups","sort_order","(sort_order)");
$dbtool->DropField("subgroups", "sort_order");
$dbtool->DropField("testcases", "sort_order");

$dbtool->AddField("users", "authtoken", "varchar(255)");
$dbtool->AddFullText("users", "key", "(email, realname, irc_nickname)");

# zll 2006-06-15: users.irc_nickname cannot have a unique index, since 
# many users have a null nickname:
$dbtool->DropIndex("users", "irc_nickname");
$dbtool->AddKey("users", "irc_nickname", "(irc_nickname)");


print "Schema update complete.\n\n";
print <<EOS;
Due to the schema changes introduced, and depending on the when you last 
updated your schema, you may now need to update/normalize your data as 
well. This will include, but may not be limited to:

 * populating the platform_products table;
 * updating/normalizing platforms;
 * updating/normalizing platform_ids in test_results;
 * updating/normalizing opsyses;
 * updating/normalizing opsys_ids in test_results;
 * populating the subgroup_testgroups table;
 * populating the testcase_subgroups table;
 * filling in product_ids for each testcase/subgroup;
 * populating the testgroup_branches table

EOS

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
