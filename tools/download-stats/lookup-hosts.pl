#!/usr/bin/perl

use DBI;
use Socket;
use strict;

$| = 1;

my $verbose = 1;

# Establish a database connection.
my $dsn = "DBI:mysql:host=mecha.mozilla.org;database=logs;port=3306";
my $dbh = DBI->connect($dsn,
                       "logs",
                       "1ssw?w?",
                       { RaiseError => 1,
                         PrintError => 0,
                         ShowErrorStatement => 1 }
                      );

my $sth = $dbh->prepare("UPDATE entries SET client = ? WHERE client = ?");

my $ips = $dbh->selectcol_arrayref('SELECT DISTINCT(client) FROM entries 
                                    WHERE client REGEXP "^[[:digit:]]{1,3}\.[[:digit:]]{1,3}\.[[:digit:]]{1,3}\.[[:digit:]]{1,3}$"');

my $count = scalar(@$ips);

print STDERR "$count total addresses to look up.\n" if $verbose;

my $attempted = 0;
my $succeeded = 0;

my ($ip, $host);
foreach $ip (@$ips) {
    $host = gethostbyaddr(inet_aton($ip), AF_INET);

    if ($host) {
	$sth->execute($host, $ip);
        ++$succeeded;
	#print STDERR "$ip = $host\n" if $verbose;
    }
    else {
	#print STDERR "No match for $ip\n" if $verbose;
    }

    ++$attempted;
    print STDERR "$succeeded/$attempted/$count\n" if ($attempted % 100) == 0 && $verbose;
}
