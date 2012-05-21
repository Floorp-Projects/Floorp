#!/usr/bin/perl
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
use DBI;    
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use URLTimingDataSet;
use File::Copy ();
use strict;

use vars qw($dbh $arc $dbroot); # current db, and db/archive

use constant STALE_AGE => 5 * 60; # seconds

# show a chart of this run; turned off in automated tests, and where
# an installation hasn't set up the required modules and libraries
use constant SHOW_CHART => 0;

sub createArchiveMetaTable {
    my $table = "tMetaTable";
    return if -e "$dbroot/archive/$table"; # don't create it if it exists
    warn "createMetaTable:\t$dbroot/archive/$table";
    mkdir "$dbroot/archive" unless -d "$dbroot/archive";
    my ($sth, $sql);
    $sql = qq{
	CREATE TABLE tMetaTable
	    (DATETIME CHAR(14),      LASTPING CHAR(14),
	     ID CHAR(8),             INDEX INTEGER,
	     CUR_IDX INTEGER,        CUR_CYC INTEGER,
	     CUR_CONTENT CHAR(128),  STATE INTEGER,
	     BLESSED INTEGER,        MAXCYC INTEGER,
	     MAXIDX INTEGER,         REPLACE INTEGER,
	     NOCACHE INTEGER,        DELAY INTEGER,
	     REMOTE_USER CHAR(16),   HTTP_USER_AGENT CHAR(128),
	     REMOTE_ADDR CHAR(15),   USER_EMAIL CHAR(32),
	     USER_COMMENT CHAR(256)
	     )
	    };
    $sth = $arc->prepare($sql);
    $sth->execute();
    $sth->finish();
    warn 'created archive meta table';
    return 1;
}


sub purgeStaleEntries {
    my $id = shift;
    my $metatable = "tMetaTable";

    # first, remove dead stuff
    my $sql = qq{SELECT * FROM $metatable 
                   WHERE STATE = "INIT" OR STATE = "OPEN"};
    my $sth = $dbh->prepare($sql);
    $sth->execute();
    my $now = time();
    my $status;
    while (my @data = $sth->fetchrow_array()) {
	my $age = $now - timestamp2Time($data[1]);
	# if OPEN or INIT, and not heard from in 10 minutes, then it's never coming
	# back here again. Delete the entry. Whine in the error_log.
	if ($age > STALE_AGE) {
	    warn "deleting stale record+table, id = $data[2], last = $data[1], @data";
	    $dbh->do( qq(DELETE FROM $metatable WHERE ID = "$data[2]") );
	    $dbh->do("DROP TABLE t" . $data[2]);
	}
	$status .= "$age @data\n";
    }
    $sth->finish();

    # now move any COMPLETE records to archive
    $sql = qq{SELECT * FROM $metatable};
    $sth = $dbh->prepare($sql);
    $sth->execute();
    $now = time();
    while (my @data = $sth->fetchrow_array()) {
	my $age = $now - timestamp2Time($data[1]);
	# This keeps the "live" entries from growing too slow. 
	# If COMPLETE and older than 10 minutes, move to archive.
	if ($age > STALE_AGE) {
	    warn "moving COMPLETE record+table, id = $data[2], last = $data[1], @data";
	    moveRecordToArchive($data[2], \@data);
	    $dbh->do( qq(DELETE FROM $metatable WHERE ID = "$data[2]") );
	}
    }
    $sth->finish();


    if (!SHOW_CHART) {
	# Don't move it if showing a chart. (Otherwise, if showing a 
	# a chart, I'd have to do a little extra work to make sure I 
	# didn't yank the record away from the IMG request)
	$sql = qq{SELECT * FROM $metatable WHERE ID = "$id"};
	$sth = $dbh->prepare($sql);
	$sth->execute();
	while (my @data = $sth->fetchrow_array()) {
	    warn "moving COMPLETE record+table, id = $id, @data\n";
	    moveRecordToArchive($data[2], \@data);
	    $dbh->do( qq(DELETE FROM $metatable WHERE ID = "$data[2]") );
	}
    }
    $sth->finish();
}


sub moveRecordToArchive {
    my $id      = shift || die "no id";
    my $dataref = shift || die "no dataref";
    createArchiveMetaTable(); # if it doesn't exist
    insertIntoMetaTable($dataref);
    File::Copy::move("$dbroot/t$id", "$dbroot/archive/t$id");
}


sub insertIntoMetaTable {
    my $dataref = shift || die "no dataref";
    my $table = "tMetaTable";
    my ($sth, $sql);
    $sql = qq{
        INSERT INTO $table
             (DATETIME,         LASTPING,     ID,
              INDEX,            CUR_IDX,      CUR_CYC,
              CUR_CONTENT,      STATE,        BLESSED,
              MAXCYC,           MAXIDX,       REPLACE,
              NOCACHE,          DELAY,        REMOTE_USER,
              HTTP_USER_AGENT,  REMOTE_ADDR,  USER_EMAIL,
              USER_COMMENT
              )
          VALUES (?,?,?,?,
                  ?,?,?,?,
                  ?,?,?,?,
                  ?,?,?,?,
                  ?,?,?)
        };
    $sth = $arc->prepare($sql);
    $sth->execute(@$dataref);
    $sth->finish();
}


sub timestamp2Time ($) {
    my $str = shift;
    use Time::Local ();
    my @datetime = reverse unpack 'A4A2A2A2A2A2', $str;
    --$datetime[4]; # month: 0-11
    return Time::Local::timelocal(@datetime);
}


sub serializeDataSet {
    # package up this data for storage elsewhere
    my $rs = shift;
    my $data = "avgmedian|" . $rs->{avgmedian};
    $data .=   "|average|"  . $rs->{average};
    $data .=   "|minimum|"  . $rs->{minimum};
    $data .=   "|maximum|"  . $rs->{maximum};
    $_ = $rs->as_string;
    s/^\s+//gs;
    s/\s+\n$//gs;
    s/\s*\n/\|/gs; # fold newlines
    s/\|\s+/\|/gs; 
    s/\s+/;/gs;
    return $data . ":" . $_;
}

#
# handle the request
#
my $request = new CGI::Request;
my $id = $request->param('id'); #XXX need to check for valid parameter id
my $rs = URLTimingDataSet->new($id);

print "Content-type: text/html\n\n";

# This sucks: we'll let the test time out to avoid crash-on-shutdown bugs
print "<html><body onload='window.close();'>";
#
# dump some stats for tinderbox to snarf
#
print "<script>\n";
print "if (window.dump) dump('";
print "Starting Page Load Test\\n\\\n";
print "Test id: $id\\n\\\n";
print "Avg. Median : ", $rs->{avgmedian}, " msec\\n\\\n";
print "Average     : ", $rs->{average}, " msec\\n\\\n";
print "Minimum     : ", $rs->{minimum}, " msec\\n\\\n";
print "Maximum     : ", $rs->{maximum}, " msec\\n\\\n";
print "IDX PATH                           AVG    MED    MAX    MIN  TIMES ...\\n\\\n";
if ($request->param('sort')) {
    $_ = $rs->as_string_sorted();
} else {
    $_ = $rs->as_string();
}
#
# Terminate raw newlines with '\n\' so we don't have an unterminated string literal.
#
s/\n/\\n\\\n/g;
print $_;
print "(tinderbox dropping follows)\\n\\\n";
print "_x_x_mozilla_page_load," , $rs->{avgmedian}, ",", $rs->{maximum}, ",", $rs->{minimum}, "\\n\\\n";
#
# package up this data for storage elsewhere
#
my $data = serializeDataSet($rs);
print "_x_x_mozilla_page_load_details,", $data, "\\n\\\n";
#
# average median
#
#print "TinderboxPrint:<a title=\"Avg. of the median per url pageload time.\" href=\"http://tegu.mozilla.org/graph/query.cgi?tbox=spider&testname=pageload&autoscale=1&days=7&avg=1\">Tp:", $rs->{avgmedian}, "ms</a>", "\\n\\\n";
print "');";
print "</script></body></html>\n";


#
# If this is SurfingSafari, then catch a wave and you're sitting on top of the world!!
# (and also blat this out to tegu, cause we got no 'dump' statement.
#
if ($request->cgi->var("HTTP_USER_AGENT") =~ /Safari/) {
    my %machineMap = 
	(
	 "10.169.105.26" => "boxset",
	 "10.169.105.21" => "pawn"
	 );
    my $ip = $request->cgi->var('REMOTE_ADDR');
    my $machine = $machineMap{$ip};
    my $res = eval q{
	use LWP::UserAgent;
	use HTTP::Request::Common qw(POST);
	my $ua = LWP::UserAgent->new;
	$ua->timeout(10); # seconds
	my $req = POST('http://tegu.mozilla.org/graph/collect.cgi',
		       [testname => 'pageload',
			tbox     => "$machine" . "-aux",
			value    => $rs->{avgmedian},
			data     => $data]);
	my $res = $ua->request($req);
	return $res;
    };
    if ($@) {
	warn "Failed to submit startup results: $@";
    } else {
	warn "Startup results submitted to server: \n",
	$res->status_line, "\n", $res->content, "\n";
    }
}


if ($request->param('purge')) {
    # now move any old stuff into archive and clean stale entries
    # just going with the simple approach of "whoever sees old entries
    # first, cleans em up, whether they 'own' them or not". Hopefully,
    # the default locking will be sufficient to prevent a race.
    close(STDOUT);
    sleep(1);
    $dbroot = "db";
    $dbh = DBI->connect("DBI:CSV:f_dir=./$dbroot", 
                        {RaiseError => 1, AutoCommit => 1})
	|| die "Cannot connect: " . $DBI::errstr;
    $arc = DBI->connect("DBI:CSV:f_dir=./$dbroot/archive", 
                        {RaiseError => 1, AutoCommit => 1})
	|| die "Cannot connect: " . $DBI::errstr;
    purgeStaleEntries($id);
    $dbh->disconnect();
    $arc->disconnect();
}

exit 0;
