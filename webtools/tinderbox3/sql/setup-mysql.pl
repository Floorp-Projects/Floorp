#!/usr/bin/perl -I..
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
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

use strict;
use Getopt::Long;
use DBI;

#
# Get arguments
#
my %args;
$args{drop} = 1;
$args{create} = 1;
$args{defaults} = 1;
GetOptions(\%args, 'host|h:s', 'port|p:s', 'username|u:s', 'password|p:s',
		   'drop!', 'defaults!', 'create!',
		   'help|h|?');

my $dbname = shift @ARGV;

#
# Get help
#
if($args{help} || !$dbname || @ARGV) {
	print <<EOM;

setup-mysql.pl [OPTIONS] dbname

  OPTIONS
  --help, -h, -?: Show this message.
  --host:         The server mysql is running on (default: this machine)
  --port:         The port mysql is running on (default: normal MySQL port)
  --username, -u: The mysql username to use (default: current user)
  --password, -p: The mysql password to use
  --nodrop:       Don't perform dropping of tables (default: drop)
  --nocreate:     Don't perform creation of tables (default: create)
  --nodefaults:   Don't populate the system parameters with defaults (default: populate)

If you don't know what dbname to use and you normally connect using "mysql", use
your unix username as the dbname.

NOTE: population will not work unless UserLogin is installed.

EOM

	exit(1);
}

#
# Set up defaults, initialize
#
my $create_file = "create_schema_mysql.sql";
generate_create_schema_file("create_schema_postgres.sql", $create_file);
my $connect_string = "dbi:mysql:$dbname";
$connect_string .= ";host=$args{host}" if $args{host};
$connect_string .= ";port=$args{port}" if $args{port};
my $dbh = DBI->connect($connect_string, $args{username}, $args{password}, { RaiseError => 0, AutoCommit => 1 });
my ($tables, $sequences) = read_tables_sequences($create_file, $args{prefix});

#
# Drop tables
#
if($args{drop}) {
	drop_schema($dbh, $tables, $sequences);
}

#
# Create tables
#
if($args{create}) {
	execute_sql_file($dbname, \%args, $create_file);
}

#
# Populate data
#
if($args{defaults}) {
	populate_data($dbname, \%args);
}

$dbh->disconnect;


sub generate_create_schema_file {
  my ($old_create_schema, $new_create_schema) = @_;
  open IN, $old_create_schema;
  open OUT, ">$new_create_schema";
  while (<IN>) {
    s/\bserial\b/int4 not null auto_increment primary key/i;
    s/\bunique\b//i;
    if (/(create\s*table\s*)(\w+)/i) {
      my $new_table_name = lc($2);
      s/(create\s*table\s*)(\w+)/\1$new_table_name/i;
    }
    s/\bboolean\b/bool/i;
    s/\btimestamp\b(\s*\(\s*\d+\s*\))?/datetime/i;
    print OUT;
  }
  close OUT;
  close IN;
}

#
# Actually drop tables and sequences
#
sub drop_schema {
	my ($dbh, $tables, $sequences) = @_;

	foreach my $table (@{$tables}) {
		print "Dropping $table";
		if($sequences->{$table}) {
			print " (seq: " . join(", ", @{$sequences->{$table}}) . ")";
		}
		print " ... \n";
		my $sth;
		foreach my $seq (@{$sequences->{$table}}) {
			$sth = $dbh->prepare("drop sequence $seq");
			# We don't care if there's an error here
			$sth->execute;
		}
		$sth = $dbh->prepare("drop table $table");
		# We don't care if there's an error here
		$sth->execute;
	}
}

#
# Read the list of tables and sequences from the create schema file
#
sub read_tables_sequences {
	my ($create_file, $prefix) = @_;

	my @tables;
	my %sequences;
	my $recent_table;

	#
	# Grab the list of tables and sequences
	#
	open IN, $create_file;
	while(<IN>) {
		if(/^\s*create\s*table\s*(\S+)/i) {
			$recent_table = $1;
			unshift @tables, $recent_table;
		}
	}
	close IN;

	return (\@tables, \%sequences);
}

#
# Populate the initial data
#
sub populate_data {
	my ($dbname, $args) = @_;
}

#
# Execute an SQL file in mysql
#
sub execute_sql_file {
	my ($dbname, $args, $sql_file) = @_;
  my @exec_params = ('mysql');
	push @exec_params, ("-h", $args{host}) if $args{host};
	push @exec_params, ("-P", $args{port}) if $args{port};
	push @exec_params, ("-u", $args{username}) if $args{username};
	push @exec_params, "--password=".$args{password} if $args{password};
  push @exec_params, $dbname;
  my $cmd = join(' ', @exec_params);
  $cmd .= " < $sql_file";
  print "Executing $cmd ...\n";
	system($cmd);
}
