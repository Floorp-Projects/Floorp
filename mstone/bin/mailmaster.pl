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
#			Mike Blakely
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

#  see setup.pl for full usage
#	mailmaster [-d] [-c <config file>] ...

# This script reads in the client configuration files and will
# fork children to rsh the mailclient process on network clients,
# each child will write test results to /mailstone directory before
# dying.  The parent will the read and combine the results.
#
# Make sure the user running this script has rsh privilege across
# all client machines

print "Netscape Mailstone version 4.2\n";
print "Copyright (c) Netscape Communications Corp. 1997-2000\n";

# this parses the command line and config file
do 'args.pl'|| die "$@\n";
parseArgs();			# parse command line

{				# get unique date string
    my ($sec, $min, $hour, $mday, $mon, $year) = localtime;
    my $tstamp = sprintf ("%04d%02d%02d.%02d%02d",
		       1900+$year, 1+$mon, $mday, $hour, $min);

    if ( -d "$resultbase/$tstamp") { # check for runs within a minute
	my $tail = 'a';
	while ( -d "$resultbase/$tstamp$tail" ) { $tail++; }
	$tstamp .= $tail;
    }
    $params{TSTAMP} = $tstamp;
}

$resultdir = "$resultbase/$params{TSTAMP}";
$tmpdir = "$tmpbase/$params{TSTAMP}";
$resultstxt = "$resultdir/results.txt";
$resultshtml = "$resultdir/results.html";
mkdir ("$resultbase", 0775);
mkdir ("$tmpbase", 0775);
mkdir ("$resultdir", 0775);
mkdir ("$tmpdir", 0775);

# Make sure we have everything
die "Must specify the test time" unless $params{TIME};
die "Must specify a workload file" unless $params{WORKLOAD};

if ($params{TESTBED}) {		# BACK COMPATIBILITY
    readTestbedFile ($params{TESTBED}) || die "Error reading testbed: $@\n";
}

$testsecs = figureTimeSeconds ($params{TIME}, "seconds");

# figure out the processes and thread, given the desired number
# takes into account all the constraints.  todo can be a float.
sub figurePT {
    my $sec = shift;
    my $todo = shift;
    my $p = 1;			# first guess
    my $t = 1;
    my $start = 1;		# initial process guess
    my $end = 250;		# highest process guess

    if ($todo < 1) {		# mark this client as inactive
	$sec->{PROCESSES} = 0;
	$sec->{THREADS} = 0;
	return 0;
    }

    if (($section->{MAXCLIENTS}) && ($todo > $section->{MAXCLIENTS})) {
	$todo = $section->{MAXCLIENTS};	# trim to max client per host
    }
    if ($section->{PROCESSES}) { # they set this part already
	$start = int ($section->{PROCESSES});
	$end = $start;
	$p = $start;
	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	print "Using specified $p processes for clients $slist\n";
    }

    $end = int ($section->{MAXPROCESSES})
	if ($section->{MAXPROCESSES}); # they set a max
    
    if (($params{NT}) || ($section->{ARCH} eq "NT4.0")) {
	$end = 1;		#  # NT is currently limited to 1 process
	$start = 1;
	$p = 1;
    }

    # step through some process counts
    # it should first reduce errors due to MAXTHREADS,
    # the it will reduce errors due to integer math.
    # not optimal, just good enough
    my $misses = 0;
    for (my $n = $start; $n <= $end; $n++) { # try some process counts
	my $tryt = int ($todo / $n);
	if (($sec->{MAXTHREADS}) && ($tryt > $sec->{MAXTHREADS})) {
	    $tryt = $sec->{MAXTHREADS};
	}
	# see if this is a better match than the last one
	if (abs ($todo - ($n * $tryt)) < abs ($todo - ($p * $t))) {
	    $p = $n;
	    $t = $tryt;
	    $misses = 0;
	} else {
	    $misses++;
	    last if ($misses > 1); # getting worse
	}
    }
    $sec->{PROCESSES} = $p;
    $sec->{THREADS} = $t;
    return $p * $t;
}

# Allocate CLIENTCOUNT to the client machines
# try NOT to turn this into a massive linear programming project
# works best to put bigger machines last
if ($params{CLIENTCOUNT}) {
    my $todo = $params{CLIENTCOUNT};
    my $softcli = 0;		# how many can we play with

    foreach $section (@workload) { # see which are already fixed
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	unless (($section->{PROCESSES}) && ($section->{THREADS})) {
	    $softcli++;
	    next;
	}
	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my @hlist = split /[\s,]/, $slist;
	my $hcnt = (1 + $#hlist);

	# subtract fixed entries
	my $tcount = ($section->{THREADS}) ? $section->{THREADS} : 1;
	$todo -= $tcount * $section->{PROCESSES} * $hcnt;
	$clientProcCount += $section->{PROCESSES} * $hcnt; # total processes
	$params{DEBUG} &&
	    print "Fixed load group with $hcnt hosts: $section->{PROCESSES} x $tcount\n";
    }

    $params{DEBUG} &&
	    print "Allocating $todo clients over $softcli groups\n";
    if ($softcli) {
	foreach $section (@workload) {
	    next unless ($section->{sectionTitle} =~ /CLIENT/o);
	    next if (($section->{PROCESSES}) && ($section->{THREADS}));
	    my $slist = $section->{sectionParams};
	    $slist =~ s/HOSTS=\s*//; # strip off initial bit
	    my @hlist = split /[\s,]/, $slist;
	    my $hcnt = (1 + $#hlist);

	    #print "todo=$todo softcli=$softcli hcnt=$hcnt\n";
	    $todo -= $hcnt * figurePT ($section, $todo / ($softcli * $hcnt));
	    $clientProcCount += $hcnt * $section->{PROCESSES}; # total procs

	    $softcli--;
	    last if ($softcli <= 0); # should not happen
	}
    }
    if ($todo) {
	print "Warning: Could not allocate $todo of $params{CLIENTCOUNT} clients.\n";
	$params{CLIENTCOUNT} -= $todo;
    }
	    
   
} else {			# figure out the client count
    my $cnt = 0;
    foreach $section (@workload) { # see which are already fixed
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	next unless ($section->{PROCESSES});

	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my @hlist = split /[\s,]/, $slist;
	my $hcnt = (1 + $#hlist);

	# subtract fixed entries
	my $tcount = ($section->{THREADS}) ? $section->{THREADS} : 1;
	$cnt += $tcount * $section->{PROCESSES} * $hcnt;
	$clientProcCount += $section->{PROCESSES} * $hcnt; # total processes
    }
    $params{CLIENTCOUNT} = $cnt;
    die "No clients configured!\n" unless ($cnt > 0);
}

# This has to be written into save workload file for later processing
unless ($params{FREQUENCY}) {	# unless frequency set on command line
    my $chartp = ($params{CHARTPOINTS}) ? $params{CHARTPOINTS} : 464;

    # approximate data points for good graphs (up to 2 times this)
    $params{FREQUENCY} = int ($testsecs / $chartp);
    if ($params{FREQUENCY} < 2) { # fastest is every 2 seconds
	$params{FREQUENCY} = 2;
    } elsif ($params{FREQUENCY} > 60) {	# slowest is every minute
	$params{FREQUENCY} = 60;
    }
}

{ # set a unique block id on every section
    my $id = 0;
    my $configSeen = 0;
    my $defaultSeen = 0;
    foreach $section (@workload) {
	if ($section->{"sectionTitle"} =~ /^CONFIG$/) {
	    next if $configSeen;
	    $configSeen++;
	}
	if ($section->{"sectionTitle"} =~ /^DEFAULT$/) {
	    next if $defaultSeen;
	    $defaultSeen++;
	}
	$id++;			# number 1, 2, ...
	if ($section->{"sectionTitle"} =~ /^(CONFIG|CLIENT)$/) {
	    $section->{BLOCKID} = $id;
	} else {
	    push @{$section->{"lineList"}}, "blockID\t$id\n";
	}
    }
}

# Write the version we pass to mailclient
writeWorkloadFile ("$resultdir/work.wld", \@workload,
		   \@scriptWorkloadSections);

# Write the complete inclusive version
writeWorkloadFile ("$resultdir/all.wld", \@workload);

setConfigDefaults();		# pick up any missing defaults

unless ($#protocolsAll > 0) {
    die "No protocols found.  Test Failed!\n";
}

print "Starting: ", scalar(localtime), "\n";

# redirect STDERR
open SAVEERR, ">&STDERR";
open(STDERR, ">$resultdir/stderr") || warn "Can't redirect STDERR:$!\n";

$totalProcs = 0;		# number of clients started

# iterate over every client in the testbed, complete the cmd and rsh
if ($params{NT}) {		# single client on local host 
    pathprint ("Starting clients (errors logged to $resultdir/stderr)\n");

    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	my $tcount = ($section->{THREADS}) ? $section->{THREADS} : 1;


	# Build the initial Mailclient command line
	my $preCmd = ($section->{COMMAND})
	    ? $section->{COMMAND} : $params{CLIENTCOMMAND};
	$preCmd .= " -s -t $params{TIME} -f $params{FREQUENCY}";
	$preCmd .= " -d" if ($params{DEBUG});
	$preCmd .= " -r" if ($params{TELEMETRY});
	$preCmd .= " -R $params{RAMPTIME}" if ($params{RAMPTIME});
	$preCmd .= " -m $params{MAXERRORS}" if ($params{MAXERRORS});
	$preCmd .= " -M $params{MAXBLOCKS}" if ($params{MAXBLOCKS});
	$preCmd .= " -n 1 -N $tcount";
	$preCmd .= ($params{USEGROUPS} && $section->{GROUP})
	    ? " -H $section->{GROUP}" : " -H $cli";

	my $stdout = "$tmpdir/localhost.out";

	$totalProcs += $tcount;
	do 'makeindex.pl' || warn "$@\n";	# html index

	printf "\nTest duration: %d %s.  Rampup time: %d %s.  Number of clients: %d\n",
	figureTimeNumber ($params{TIME}),
	figureTimeUnits ($params{TIME}, "seconds"),
	figureTimeNumber ($params{RAMPTIME}),
	figureTimeUnits ($params{RAMPTIME}, "seconds"),
	$totalProcs;

	print STDERR  "localhost: cd $params{TEMPDIR}; $preCmd\n";

	# Redirect STDIN, and STDOUT
	#open SAVEIN, "<STDIN";
	open STDIN, "<$resultdir/work.wld"
	    || die "Coundn't open $resultdir/work.wld for input\n";
	open SAVEOUT, ">&STDOUT";
	open STDOUT, ">$stdout"
	    || die "Couldnt open $stdout for output\n";

	chdir $params{TEMPDIR} || die "Could not cd $params{TEMPDIR}: $!\n";
	system $preCmd;
	close STDOUT;
	open STDOUT, ">&SAVEOUT";
	printf "Test done.\n";

	chdir $cwd || die "Could not cd $cwd: $!\n";
	last;			# only do the first one
    }
} else {			# not NT (forking works)

    foreach $section (@workload) {	# do pre run commands
	next unless ($section->{sectionTitle} =~ /PRETEST/o);
	unless ($section->{COMMAND}) {
	    print "PreTest with no Command for $section->{sectionParams}\n";
	    next;
	}

	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my $myCmd = $section->{COMMAND};
	$myCmd =~ s/%f/$params{FREQUENCY}/; # fill in frequency variable
	if ($myCmd =~ m/%c/o) { # dont force down if count is used
	    $count = $testsecs / $params{FREQUENCY};
	    $myCmd =~ s/%c/$count/; # fill in count variable
	}
	my $rsh = ($section->{RSH}) ? $section->{RSH} : $params{RSH};

	foreach $cli (split /[\s,]/, $slist) {
	    print "Running pre test command on $cli\n";
	    open PRE, ">>$resultdir/$cli-pre.log";
	    print PRE "========\n";
	    print PRE "$myCmd\n";
	    print PRE "========\n";
	    close PRE;
	    print STDERR "$cli: $myCmd\n"; # log the actual command
	    forkproc ($rsh, $cli, $myCmd,
		      "/dev/null", "$resultdir/$cli-pre.log");
	}
	foreach $cli (split /[\s,]/, $slist) {
	    wait();		# run multiple PRETEST section sequentially
	}
    }

    foreach $section (@workload) { # start monitors
	next unless ($section->{sectionTitle} =~ /MONITOR/o);

	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my $myCmd = ($section->{COMMAND})
	    ? $section->{COMMAND} : $params{MONITORCOMMAND};
	my $forceDown = 0;
	$myCmd =~ s/,/ /g;	# turn commas into spaces BACK COMPATIBIILITY
	$myCmd =~ s/%f/$params{FREQUENCY}/; # fill in frequency variable

	if ($myCmd =~ m/%c/o) { # dont force down if count is used
	    $count = $testsecs / $params{FREQUENCY};
	    $myCmd =~ s/%c/$count/; # fill in count variable
	} else {
	    $forceDown = 1;
	}
	my $rsh = ($section->{RSH}) ? $section->{RSH} : $params{RSH};

	foreach $cli (split /[\s,]/, $slist) {
	    printf "Monitoring on $cli\n";
	    open PRE, ">>$resultdir/$cli-run.log";
	    print PRE "========\n";
	    print PRE "$myCmd\n";
	    print PRE "========\n";
	    close PRE;
	    print STDERR "$cli: $myCmd\n"; # log the actual command
	    $pid = forkproc ($rsh, $cli, $myCmd,
			     "/dev/null", "$resultdir/$cli-run.log");
	    push @forceDownPids, $pid if ($forceDown); # save PID for shutdown
	}
    }

    print "Starting clients (errors logged to $resultdir/stderr)\n";
    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	next unless ($section->{PROCESSES}); # unused client

	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my $rsh = ($section->{RSH}) ? $section->{RSH} : $params{RSH};
	my $pcount = $section->{PROCESSES};
	my $tcount = ($section->{THREADS}) ? $section->{THREADS} : 0;
	my $tempdir;
	if ($section->{TEMPDIR}) {
	    $tempdir = $section->{TEMPDIR};
	} elsif ($params{TEMPDIR}) {
	    $tempdir = $params{TEMPDIR};
	}
	my $preCmd = "./" . (($section->{COMMAND})
			     ? $section->{COMMAND} : $params{CLIENTCOMMAND});
	$preCmd .= " -s -t $params{TIME} -f $params{FREQUENCY}";
	$preCmd .= " -d" if ($params{DEBUG});
	$preCmd .= " -r" if ($params{TELEMETRY});
	$preCmd .= " -R $params{RAMPTIME}" if ($params{RAMPTIME});
	if ($params{MAXERRORS}) {
	    # distribute error count over processes, rounding up
	    my $n = int (($params{MAXERRORS} + $clientProcCount - 1)
			 / $clientProcCount);
	    $n = 1 if ($n < 1);
	    $preCmd .= " -m $n";
	}
	if ($params{MAXBLOCKS}) {
	    # distribute block count over processes, rounding up
	    my $n = int (($params{MAXBLOCKS} + $clientProcCount - 1)
			 / $clientProcCount);
	    $n = 1 if ($n < 1);
	    $preCmd .= " -M $n";
	}
	$preCmd = "cd $tempdir; " . $preCmd if ($tempdir);
	$preCmd .= " -n $pcount";
	$preCmd =~ s!/!\\!g if ($section->{ARCH} eq "NT4.0");
	$preCmd =~ s/;/&&/g if ($section->{ARCH} eq "NT4.0");

	foreach $cli (split /[\s,]/, $slist) {
	    my $stdout = getClientFilename ($cli, $section);
	    my $myCmd = $preCmd;
 	    $myCmd .= ($params{USEGROUPS} && $section->{GROUP})
		? " -H $section->{GROUP}" : " -H $cli";

	    if ($tcount) {
		$myCmd .= " -N $tcount";
		printf "Starting %d x %d on $cli\n", $pcount, $tcount;
		$totalProcs += $pcount * $tcount;
	    } else {
		printf "Starting %d processes on $cli\n", $pcount;
		$totalProcs += $pcount;
	    }

	    print STDERR "$cli: $myCmd\n"; # log the actual command
	    $pid = forkproc ($rsh, $cli, $myCmd,
			     "$resultdir/work.wld", $stdout);
	    push @localPids, $pid if ($cli =~ /^localhost$/i);
	}
    }

    if (@localPids) {
	# print "Trapping extraneous local signals\n";
	# This doesnt trap quite right.  We dont die, but shell returns...
	$SIG{ALRM} = 'IGNORE';	# in case we get an ALRM from the mailclient
    }

    printf "\nTest duration: %d %s.  Rampup time: %d %s.  Number of clients: %d\n",
    figureTimeNumber ($params{TIME}),
    figureTimeUnits ($params{TIME}, "seconds"),
    figureTimeNumber ($params{RAMPTIME}),
    figureTimeUnits ($params{RAMPTIME}, "seconds"),
    $totalProcs;

    do 'makeindex.pl' || warn "$@\n";	# html index

    print "Waiting for test to finish.\n";
    print "Waiting: ", scalar(localtime), "\n";
    # wait for children to finish
    $pid = wait();
    if (@forceDownPids) {		# shut down after the first return.
	print "Shutting down @forceDownPids\n";
	kill 1 => @forceDownPids;	# sigHUP
	# kill 9 => @forceDownPids;	# sigTERM
    }
    while ($pid != -1) {	# wait for all children
	$pid = wait();
    }

    foreach $section (@workload) {	# do post test commands
	next unless ($section->{sectionTitle} =~ /POSTTEST/o);
	unless ($section->{COMMAND}) {
	    print "PostTest with no command for $section->{sectionParams}\n";
	    next;
	}

	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my $myCmd = $section->{COMMAND};
	$myCmd =~ s/%f/$params{FREQUENCY}/; # fill in frequency variable
	if ($myCmd =~ m/%c/o) { # dont force down if count is used
	    $count = $testsecs / $params{FREQUENCY};
	    $myCmd =~ s/%c/$count/; # fill in count variable
	}
	my $rsh = ($section->{RSH}) ? $section->{RSH} : $params{RSH};

	foreach $cli (split /[\s,]/, $slist) {
	    printf "Running post test command on $cli\n";
	    open PRE, ">>$resultdir/$cli-post.log";
	    print PRE "========\n";
	    print PRE "$myCmd\n";
	    print PRE "========\n";
	    close PRE;
	    print STDERR "$cli: $myCmd\n"; # log the actual command
	    forkproc ($rsh, $cli, $myCmd,
		      "/dev/null", "$resultdir/$cli-post.log");
	}
	foreach $cli (split /[\s,]/, $slist) {
	    wait();		# run multiple POSTTEST section sequentially
	}
    }

}

print STDERR "\nDone.\n";
close(STDERR);
open STDERR, ">&SAVEERR";

print "\nClients done: ", scalar(localtime), "\n";
print "Collecting results\n";

do 'reduce.pl' || die "$@\n";	# generate graphs and sums

print "Generating results pages\n";

do 'report.pl' || die "$@\n";

# Now display that data to console
if ($params{VERBOSE}) {
    fileShow ($resultstxt);
    print "\n";
}
print "Processing done: ", scalar (localtime), "\n";

pathprint ("\nResults (text):\t$resultstxt\n");
pathprint (  "Results (HTML):\t$resultshtml\n");
print        "Index of runs: \tfile://$cwd/$resultbase/index.html\n";


# Now check for major problems in the stderr file
if (open(RESULTSTXT, "$resultdir/stderr")) {
    $ERRCNT=0;
    while (<RESULTSTXT>) { $ERRCNT++; }
    close(RESULTSTXT);
    pathprint ("Error log ($ERRCNT lines):\t$resultdir/stderr\n");
}

{				# list user requested logging
    my @logfiles = <$resultdir/*-pre.log>;
    if (@logfiles) {
	foreach $f (@logfiles) {
	    print "Pre test log:   \t$f\n";
	}
    }
    @logfiles = <$resultdir/*-run.log>;
    if (@logfiles) {
	foreach $f (@logfiles) {
	    print "Monitoring log: \t$f\n";
	}
    }
    @logfiles = <$resultdir/*-post.log>;
    if (@logfiles) {
	foreach $f (@logfiles) {
	    print "Post test log:  \t$f\n";
	}
    }
}

print "Mailmaster done: ", scalar(localtime), "\n"; exit 0;
