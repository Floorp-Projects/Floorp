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
$|++;

BEGIN {
	unless (-e "data/") {
    	system("mkdir data/");
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
EOT
        close(OUT);
        print "Go edit 'localconfig' with your configuration and \n";
        print "run this script again.";
        exit;
    }
}

use Getopt::Long;
use Litmus::Config;

my $reset_db;
my $help;
GetOptions('help|?' => \$help,'r|resetdb' => \$reset_db);

if ($help) {
  &usage;
  exit;
}

if ($reset_db) {
  my $schema_file = "createdb.sql";
  my $data_file = "populatedb.sql";
 
  print "Creating tables...";
  my $cmd = "mysql --user=$Litmus::Config::db_user --password=$Litmus::Config::db_pass < $schema_file";
  my $rv = system($cmd);
  if ($rv) {
    die "Error creating database $Litmus::Config::db_name";
  }
  print "done.\n";

  print "Populating tables...";
  $cmd = "mysql --user=$Litmus::Config::db_user --password=$Litmus::Config::db_pass < $data_file";
  $rv = system($cmd);
  if ($rv) {
    die "Error populating database $Litmus::Config::db_name";
  }
  print "done.\n";
}


# javascript cache                  
print "Rebuilding JS cache...";
use Litmus::Cache;
rebuildCache(); 
print "done.\n";

exit;

#########################################################################
sub usage() {
  print "./populatedb.pl [-h|--help] [-r|--resetdb]\n";
}
