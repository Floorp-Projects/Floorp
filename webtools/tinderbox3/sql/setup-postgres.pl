#!/usr/bin/perl -I.
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
                   'drop!', 'defaults!', 'create!', 'grant|g:s@',
                   'help|h|?');

my $dbname = shift @ARGV;

#
# Get help
#
if($args{help} || !$dbname || @ARGV) {
	print <<EOM;

setup-postgres.pl [OPTIONS] dbname

  OPTIONS
  --help, -h, -?: Show this message.
  --host:         The server postgres is running on (default: this machine)
  --port:         The port postgres is running on (default: normal Postgres port)
  --username, -u: The postgres username to use (default: current user)
  --password, -p: The postgres password to use
  --nodrop:       Don't perform dropping of tables (default: drop)
  --nocreate:     Don't perform creation of tables (default: create)
  --nodefaults:   Don't populate the system parameters with defaults (default: populate)
  --grant, -g:    A list of users to grant permissions on the tables.  Use multiple -g
                  options to grant permission to multiple users.  (default: none)

If you don't know what dbname to use and you normally connect using "psql", use
your unix username as the dbname.

NOTE: population will not work unless UserLogin is installed.

EOM

	exit(1);
}

#
# Set up defaults, initialize
#
my $create_file = "create_schema_postgres.sql";
my $connect_string = "dbi:Pg:dbname=$dbname";
$connect_string .= ";host=$args{host}" if $args{host};
$connect_string .= ";port=$args{port}" if $args{port};
my $dbh = DBI->connect($connect_string, $args{username}, $args{password}, { RaiseError => 0, AutoCommit => 1 });
my ($tables, $sequences) = read_tables_sequences($create_file);

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
# Grant permissions
#
if($args{grant}) {
	grant_permissions($dbh, $tables, $sequences, @{$args{grant}});
}

#
# Populate data
#
if($args{defaults}) {
	populate_data($dbname, \%args);
}

$dbh->disconnect;


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
	my ($create_file) = @_;

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
		} elsif(/^\s*(\S+)\s*serial/i) {
			my $seq;
			if(length($recent_table) + length($1) > 26) {
				if(length($recent_table) <= 13) {
					$seq = $recent_table . "_" . substr($1, 0, 26 - length($recent_table)) . "_seq";
				} elsif(length($1) <= 13) {
					$seq = substr($recent_table, 0, 26 - length($1)) . "_" . $1 . "_seq";
				} else {
					$seq = substr($recent_table, 0, 13) . "_" . substr($1, 0, 13) . "_seq";
				}
			} else {
				$seq = $recent_table . "_" . $1 . "_seq";
			}

			push @{$sequences{$recent_table}}, lc($seq);
		}
	}
	close IN;

	return (\@tables, \%sequences);
}

#
# Grant permissions to the tables to everyone who needs them
#
sub grant_permissions {
	my ($dbh, $tables, $sequences, @grants) = @_;
	foreach my $grant (@grants) {
		print "Granting permissions to $grant ...\n";
		foreach my $table (@{$tables}) {
			my $sth = $dbh->prepare("GRANT INSERT,UPDATE,DELETE,SELECT ON $table TO $grant");
			# Don't worry if there's an error
			$sth->execute;
		        foreach my $sequence (@{$sequences->{$table}}) {
        			my $sth2 = $dbh->prepare("GRANT INSERT,UPDATE,DELETE,SELECT ON $sequence TO $grant");
        			# Don't worry if there's an error
        			$sth2->execute;
        		}
                }
	}
}

#
# Populate the initial data
#
sub populate_data {
	my ($dbname, $args) = @_;
}

#
# Execute an SQL file in psql
#
#
# Execute an SQL file in mysql
#
sub execute_sql_file {
	# XXX This doesn't respect the password argument
	my ($dbname, $args, $sql_file) = @_;
  my @exec_params = ('psql');
	push @exec_params, ("-h", $args{host}) if $args{host};
	push @exec_params, ("-p", $args{port}) if $args{port};
	push @exec_params, ("-U", $args{username}) if $args{username};
	push @exec_params, ("-f", "$sql_file", $dbname);
	print "Executing " . join(' ', @exec_params) . " ...\n";
	system(@exec_params);
}
