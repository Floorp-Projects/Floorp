#!/usr/bin/perl
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Mailstone utility, 
# released March 17, 2000.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1997-2000 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s):	Dan Christian <robodan@netscape.com>
#			Marcel DePaolis <marcel@netcape.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#####################################################

# Generate reports independently of mailmaster
# Can be used during a run or after mailmaster has finished

print "Netscape Mailstone\n";
print "Copyright (c) 1997-2000 Netscape Communications Corp.\n";

# this parses the command line and config file
do 'args.pl'|| die $@;
parseArgs();			# parse command line
setConfigDefaults();		# pick up any missing defaults

$resultdir = "$resultbase/$params{TSTAMP}";
$tmpdir = "$tmpbase/$params{TSTAMP}";
$resultstxt = "$resultdir/results.txt";
$resultshtml = "$resultdir/results.html";

if ($params{TESTBED}) {		# BACK COMPATIBILITY
    $params{TESTBED} = "$resultdir/testbed.tbd"; # use saved testbed
    # open the saved testbed conf file
    readTestbedFile($params{TESTBED}) || die "$@\n";
}

# Convert old style to new.  Write the complete inclusive workload
writeWorkloadFile ("$resultdir/all.wld", \@workload)
    unless ((-r "$resultdir/all.wld") || (-r "$resultdir/all.wld.gz"));

$testsecs = figureTimeSeconds ($params{TIME}, "minutes");

print "Starting data reduction\n";

sub readClientCSV {
    my $file = shift;
    my @fields;
    my $line;

    print "Reading client summary: $file\n";
    open(CSV, "<$file")	||	# Summary of all clients
	open(CSV, "gunzip -c $file.gz |") || 
	    return 0;		# failed

    # Title line: Verify that arguments are in the same order
    $line = <CSV>;
    unless ($line) {
	print "readClientCSV: Error reading $file. \n";
	return 0;
    }
    chomp $line;		# strip newline
    @fields = split /,/, $line; # turn into an array
    my $cli = shift @fields;	# pull off client header
    my $cn = shift @fields;	# pull off num header
    my $pro = shift @fields;	# pull off protocol header

    # Client array, per variable, per protocol
    foreach $p (@protocols) {
	# This hash will hold the timers
	$clidata{$p} = ArrayInstance->new();
	foreach $t (@timers) {
	    # This hash will hold the values in the timer
	    $clidata{$p}->{$t} = ArrayInstance->new();
	    foreach $f (@{$timerFields{$t}}) {
		# This hash that will hold the actual values per client
		$clidata{$p}->{$t}->{$f} = ArrayInstance->new();
	    }
	}
	foreach $t (@scalarClientFields) { # non-timer fields
	    # This hash that will hold the actual values per client
	    $clidata{$p}->{$t} = ArrayInstance->new();
	}
    }

    foreach $f (@commClientFields) { # proto independent
	my $v = shift @fields;
	if ($v !~ m/$f/i) {
	    print "readClientCSV: Protocol order mismatch '$v', '$f' \n";
	    return 0;
	}
    }
    foreach $t (@timers) {	# timers
	foreach $f (@{$timerFields{$t}}) {
	    my $v = shift @fields;
	    if ($v !~ m/$t:$f/i) {
		print "readClientCSV: Protocol order mismatch '$v', '$t:$f' \n";
		return 0;
	    }
	}
    }
    foreach $t (@scalarClientFields) { # scalars
	my $v = shift @fields;
	if ($v !~ m/$t/i) {
	    print "readClientCSV: Protocol order mismatch '$v', '$t' \n";
	    return 0;
	}
    }

    # Now read actual data
    while (<CSV>) {
	chomp;			# strip newline
	@fields = split /,/;	# turn into an array
	my $cli = shift @fields; # pull off client header
	my $cn = shift @fields;	# pull off num header
	my $p = shift @fields;	# pull off protocol header
	my $cp = $clidata{$p};

	# Create the needed finals arrays
	unless ($finals{$p}) {
	    #print "Creating finals{$p}\n"; # DEBUG
	    $finals{$p} = ArrayInstance->new();
	    foreach $t (@timers) {
		$finals{$p}->{$t} = ArrayInstance->new();
	    }
	}

	foreach $f (@commClientFields) { # proto independent
	    $cliGen{$f}->{$cn} = shift @fields;
	}
	foreach $t (@timers) {	# timers
	    foreach $f (@{$timerFields{$t}}) {
		$cp->{$t}->{$f}->{$cn} = shift @fields;
		$finals{$p}->{$t}->{$f} += $cp->{$t}->{$f}->{$cn};
	    }
	}
	foreach $t (@scalarClientFields) { # scalars
	    $cp->{$t}->{$cn} = shift @fields;
	    $finals{$p}->{$t} += $cp->{$t}->{$cn};
	}

	foreach $section (@workload) { # find thread count for this client
	    next unless ($section->{sectionTitle} =~ /CLIENT/o);
	    next unless ($section->{sectionParams} =~ /$cli/);
	    #print "Process $cli has threads $section->{THREADS}\n";
	    $reportingClients += ($section->{THREADS})
		? $section->{THREADS} : 1;
	    last;
	}
    }

    close (CSV);
    return 1;			# success
}

sub readTimeCSV {
    my $file = shift;
    my $p = shift;
    my $gp = $graphs{$p};
    my $line;

    print "Reading time $p summary: $file\n";
    open(CSV, "<$file")	||	# Summary over time
	open(CSV, "gunzip -c $file.gz |") || 
	    return 0;		# failed

    # Verify that arguments are in the same order
    $line = <CSV>;
    unless ($line) {
	print "readTimeCSV: Error reading $file. \n";
	return 0;
    }
    chomp $line;		# strip newline
    @fields = split /,/, $line;
    my $t = shift @fields;	# pull off time header
    foreach $t (@timers) {
	foreach $f (@{$timerFields{$t}}) {
	    my $v = shift @fields;
	    if ($v !~ m/$t:$f/i) {
		print "readTimeCSV: Protocol order mismatch '$v', '$t:$f' \n";
		return 0;
	    }
	}
    }
    foreach $t (@scalarGraphFields) {
	my $v = shift @fields;
	if ($v !~ m/$t/i) {
	    print "readTimeCSV: Protocol order mismatch '$v', '$t' \n";
	    return 0;
	}
    }
    while (<CSV>) {
	chomp;
	#print "LINE: $_\n";
	@fields = split /,/;
	my $tm = shift @fields;	# pull off time header
	#print "t=$t ";
	foreach $t (@timers) {
	    foreach $f (@{$timerFields{$t}}) {
		$gp->{$t}->{$f}->{$tm} = shift @fields;
		#print "$v=$gp->{$v}->{$tm} ";
	    }
	}
	foreach $t (@scalarGraphFields) {
	    $gp->{$t}->{$tm} = shift @fields;
	    #print "$v=$gp->{$v}->{$tm} ";
	}
	#print "\n";
    }
    close (CSV);
    return 1;			# success
}

sub loadCSV {
    my @csvs = <$resultdir/time-*.csv>;
    @csvs = <$resultdir/time-*.csv.gz> unless (@csvs);
    return 0 unless (@csvs);	# no time.csv files

    # stuff normally done from reduce.pl (should all be in protoconf?)
    # Basic sanity check
    return 0 unless ($testsecs > 0);

    $startTime = 0;			# these are timeInSeconds/$timeStep
    $endTime = 0;

    # keep graphs with somewhat more precision than sample rate;
    $timeStep = int ($params{FREQUENCY} / 2);
    if ($timeStep < 1) { $timeStep = 1; }

    # global results initialization
    $reportingClients = 0;
    $totalProcs = 0;		# number of clients started

    foreach $f (@commClientFields) { # protocol independent fields
	$cliGen{$f} = ArrayInstance->new();
    }

    return 0 unless (readClientCSV ("$resultdir/clients.csv")); # client info

    foreach $c (@csvs) { # read time info
	$c =~ s/.gz$//;		# strip .gz extension
	my $p = $c;		# strip down to protocol portion
	$p =~ s/$resultdir\/time-//;
	$p =~ s/.csv$//;
	return 0 unless (readTimeCSV ($c, $p));
    }

    return 0 unless ($reportingClients > 0);

    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my @hlist = split /[\s,]/, $slist;
	my $hcnt = (1 + $#hlist);
	my $pcount = $section->{PROCESSES};
	my $tcount = ($section->{THREADS}) ? $section->{THREADS} : 1;

	$totalProcs += $pcount * $tcount * $hcnt;
    }

    # Find time extent for a key graph
    ($startTime, $endTime) = dataMinMax ("blocks", \@protocols,
					 $startTime, $endTime);

    $realTestSecs = ($endTime - $startTime) * $timeStep;
    $realTestSecs = 1 unless ($realTestSecs); # in case of small MaxBlocks
    printf "Reported test duration %d seconds with %d second resolution\n",
    $realTestSecs, $timeStep;
    $realTestSecs = $testsecs if ($realTestSecs > $testsecs);

    my @newProtos;	# figure real protocol list
    foreach $p (@protocols) {
	my $gp = $graphs{$p};
	my $numValid = 0;
	# See if there is real data here
	CHECKVAL: foreach $t (@timers) {
	    foreach $f (@{$timerFields{$t}}) {
		my $vp = $gp->{$t}->{$f};
		next unless ($vp);	# no data
		next unless (scalar %$vp); # redundant???

		#print "Checking: $p $t $f => ENTRIES\n";
		$numValid++;
		last CHECKVAL;
	    }
	}
	($numValid > 0) || next;

	push @newProtos, $p;
    }
    # update protocol list to only have what was used
    @protocols = @newProtos;
    @protocolsAll = @newProtos;
    push @protocolsAll, "Total";
}


my $doFull = 1;			# re-processing is currently broken

#  if (!((-f "$resultdir/clients.csv")
#        || (-f "$resultdir/clients.csv.gz"))) { # no processing yet
#      $doFull = 1;
#  } else {			# see if any source is newer than csv
#      foreach $section (@workload) {
#  	next unless ($section->{sectionTitle} =~ /CLIENT/o);
#  	my $slist = $section->{sectionParams};
#  	$slist =~ s/HOSTS=\s*//; # strip off initial bit
#  	foreach $cli (split /[\s,]/, $slist) {
#  	    my $fname = getClientFilename ($cli, $section);
#  	    if ((-r $fname)	# raw source exists
#  		&& ((-M "$resultdir/clients.csv")
#  		    > (-M $fname))) { # newer
#  		#print "$fname is newer than $resultdir/clients.csv\n";
#  		$doFull++;
#  		last;
#  	    }
#  	}
#      }
#  }

#  unless ($doFull) {		# do CSV load
#      # if this is a csv only run, then these may not exist yet
#      mkdir ("$tmpbase", 0775);
#      mkdir ("$tmpdir", 0775);
#      unless (-r "$resultbase/index.html") {
#  	do 'makeindex.pl' || warn "$@\n";	# html index
#      }

#      $doFull = 1 unless (loadCSV); # if CSV fails, fall back to full processing
#  }

if ($doFull) {
    do 'reduce.pl' || die "$@\n";
}


print "Generating results pages:\t", scalar (localtime), "\n";

do 'report.pl' || die "$@\n";

# Now display that data to console
if ($params{VERBOSE}) {
    fileShow ($resultstxt);
    print "\n";
}

pathprint ("\nResults (text):\t$resultstxt\n");
pathprint (  "Results (HTML):\t$resultshtml\n");
print        "Index of runs: \tfile://$cwd/$resultbase/index.html\n";

print "Process done:\t", scalar (localtime), "\n";
