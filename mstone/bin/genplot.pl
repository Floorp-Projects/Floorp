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

# This file deals with the graphs data only
# Interfaces to gnuplot to generate gifs for HTML inclusion.

# Type of images to hold plots (e.g. png, gif, jpeg, svg, tiff, pbm, etc)
unless ($params{IMAGETYPE}) {
    # Should probe gnuplot and see if it can generate one of:
    #	png, gif, jpeg, svg, tiff, or pbm.
    # Not all the programs that use args.pl need this (or have gnuplot, yet)
    my $outfile = "$tmpbase/termtypes.out";
    my %types = ();		# hash of interesting types that we spot

    unless (open(NEW, "<$outfile")) { # re-use old file if present
	($params{DEBUG}) &&
	    print "Asking gnuplot what output types it supports\n";

	my $infile = "$tmpbase/setterm.gpt";
	open (NEW, ">$infile") ||
	    die "genPlot: Could not open output $infile: $!";
	print NEW "set terminal";
	close (NEW);
	# Redirect STDIN, and STDERR
	open SAVEIN, "<STDIN";
	open STDIN, "<$infile"
	    || die "Coundn't open $infile for input\n";
	open SAVEERR, ">&STDERR";
	open STDERR, ">$outfile"
	    || die "Couldnt open $outfile for output\n";
	system $params{GNUPLOT};	# gnuplot < $infile 2> $outfile
	# Restore STDERR and STDIN
	close STDERR;
	open STDERR, ">&SAVEERR";
	open STDIN, "<SAVEIN";

	open(NEW, "<$outfile") ||
	    die ": Could not open gnuplot output for parsing ($outfile): $!";

	unlink ($infile);	# clean up
    }

    # now check through the output for terminal types we can use.
    # I havent verified the jpeg or tiff types.  e-mail me success or failure
    while (<NEW>) {
	(/\spng\s/) && ++$types{"png"} && next;
	(/\sgif\s/) && ++$types{"gif"} && next;
	(/\sjpeg\s/) && ++$types{"jpeg"} && next;
	(/\ssvg\s/) && ++$types{"svg"} && next;
	(/\stiff\s/) && ++$types{"tiff"} && next;
	(/\spbm\s/) && ++$types{"pbm"} && next;
    }
    close (NEW);

    ($params{DEBUG}) &&
	print "Found these gnuplot types: " . (join " ", (keys %types)) . "\n";

    # The ordering here determines our preferences
    # This list is by likely browser compatibility and image compactness
    # png is about 10x smaller than gif
    # svg is smooth and scalable but usually requires a plug-in
    # jpeg will probably look bad, but will be directly supported
    if ($types{"png"}) { $params{IMAGETYPE}="png"; }
    elsif ($types{"gif"}) { $params{IMAGETYPE}="gif"; }
    elsif ($types{"jpeg"}) { $params{IMAGETYPE}="jpeg"; }
    elsif ($types{"svg"}) { $params{IMAGETYPE}="svg"; }
    elsif ($types{"tiff"}) { $params{IMAGETYPE}="tiff"; }
    elsif ($types{"pbm"}) { $params{IMAGETYPE}="pbm"; }
    else {
	die "Gnuplot doesn't support any good image types.  Check $outfile.\n";
    }
    # leave the output file around to speed up repeat runs
}

# sub function to write data files, fire off gnuscript, and clean up
# Uses global startTime and endTime figured above
# genPlot counterName title label \@protocols \@variables
sub genPlot {
    my $name = shift;
    my $title = shift;
    my $label = shift;
    my $protos = shift || die "genPlot: '$name' missing protocols";
    my $f = shift;
    my $vars = shift || die "genPlot: '$name' missing vars";

    my $runlist = "";
    my $totPoints = 0;
    my $totProtos = 0;
    my @realProtos;
    my @goodProtos;

    # user fewer data points than pixels to look good.
    #   on 640x480 gif, the graph is about 579x408
    my $maxData = int (($params{CHARTWIDTH}-60) * 0.9);
    my $averageCnt = int (($endTime - $startTime + ($maxData - 1))/$maxData);
    if ($averageCnt < 1) { $averageCnt = 1; } # must be a positive int

    ($params{DEBUG}) && print "$name: averageCnt=$averageCnt vars = @$vars \n";

    foreach $p (@$protos) { # First see if there is anything to graph
	($p =~ /^Total$/o) && next; # derived if needed
	my $pPoints = 0;
	my $gp = $graphs{$p};
	ALLVAR: foreach $vm (@$vars) {
	    my $vp =  ($f) ? $gp->{$vm}->{$f} : $gp->{$vm};

	    unless (($vp) && (scalar %$vp)) {
		#print "genplot Checking: $p $vm $f => NO\n";
		next;
	    }
	    #print "genplot Checking: $p $vm $f => ";
	    foreach $time (keys %$vp) {
		next unless ($vp->{$time} != 0);
		$totPoints++;
		$pPoints++;
		#print "VALUES\n";
		last ALLVAR;
	    }
	    #print "nothing\n"
	}
	if ($pPoints > 0) {	# count how many protocols have non 0
	    $totProtos++;
	    push @goodProtos, $p;
	}
    }
    ($params{DEBUG}) && print "\tprotocols: @goodProtos\n";

    if ($totPoints == 0) {	# nothing in any protocol
	print "No data for graph '$name', variables '@$vars'\n";
	return 0;
    }

    foreach $p (@$protos) {
	unlink ("$tmpdir/$name.$p"); # remove any previous runs

	(($p =~ /^Total$/o) && ($totProtos <= 1))
	    && next;		# skip Totally if only 1 protocol plus total

	($p !~ /^Total$/o) && push @realProtos, $p; # everything but Total

#  	if ($p =~ /^Total$/o) {	# move from last to first
#  	    $runlist = "\'$name.$p\' with lines, " . $runlist;
#  	    next;
#  	}

	$runlist .= ", " if ($runlist); # later ones

	$runlist .= "\'$name.$p\' with lines";
    }

    $totPoints = 0;
    foreach $p (@realProtos) {	# create the plot data files
	open(DATA, ">$tmpdir/$name.$p") || 
	    die "Can't open $tmpdir/$name.$p:$!";
	my $gp = $graphs{$p};
	my $n = 0;
	my $s = 0.0;
	my $sTime = 0.0;
	my $vp = ($f) ? $gp->{$vars->[0]}->{$f} : $gp->{$vars->[0]};

#	foreach $time (sort numeric keys %$vp) {
	for (my $tm = $startTime; $tm <= $endTime; $tm++) {
	    my $v = 0.0;
	    foreach $vm (@$vars) {
		$vp = ($f) ? $gp->{$vm}->{$f} : $gp->{$vm};

		$totPoints++;

		# due to optimization in updateDelta, 
		#    0 entries are undefined (also avoids warning)
		$v += ($vp->{$tm}) ? $vp->{$tm} : 0;

		# if ($vp->{$tm} < 0) {
		# print $name, ": proto=", $p, " var=", $vm,
		# " value=", $vp->{$tm}, "\n";
		# }
	    }
	    $s += $v;
	    $n += 1;
	    if ($n == 1) {	# NOTE: shifts left in sliding window
		$sTime = $tm-$startTime;
	    }
	    if ($n >= $averageCnt) {
		printf (DATA "%d %f\n", $sTime * $timeStep, $s/$n);
		$n = 0;
		$s = 0.0;
	    }
	}
	if ($n > 0) {		# handle end case
	    printf (DATA "%d %f\n", $sTime * $timeStep, $s/$n);
	}
	close(DATA);
    }
    #($params{DEBUG}) && print "\tpoints: $totPoints\n";

    # need to handle "Total" case
    # read the other files and write out the sum
    # FIX: total my be mis-aligned with data
    if (($#$protos > $#realProtos) && ($totProtos > 1)) { 
	unlink ("$tmpdir/$name.Total");
	open(DATA, ">$tmpdir/$name.Total") || 
	    die "Can't open $tmpdir/$name.Total:$!";

	foreach $r (@goodProtos) { # get file handles
	    open($r, "$tmpdir/$name.$r")
		|| die "Couldn't open $tmpdir/$name.$r: $!";
	}
				# ASSUMES files are identical in order
	my $first = shift @goodProtos;
	# print "First protocol: $first  Rest: @realProtos\n";
	while (<$first>) {
	    my ($t, $s) = split ' ', $_;
	    foreach $r (@goodProtos) { # get file handles
		$l = <$r>;
		if ($l) {
		    my ($tt, $v) = split ' ', $l;
		    $t = $tt unless ($t); # in case first proto missing time
		    $s += $v;
		}
	    }
	    printf (DATA "%d %f\n", $t, $s);
	}

	foreach $r (@goodProtos) { close($r); }
	close (DATA);
    }

    # Create a script to feed to gnuplot, which creates a .gif graph.
    $runTime = ($endTime - $startTime + 1) * $timeStep;
    unlink("$tmpdir/$name.gpt");
    open(SCRIPT, ">$tmpdir/$name.gpt")
	|| die "Can't open $tmpdir/$name.gpt:$!";
    
    $gnuplot = $params{GNUPLOT};
    if ($gnuplot !~ /^\//) {	# if not absolute, adjust for cd $tmpbase
	$gnuplot = "../../$gnuplot"; # ASSUME $tmpbase is single dir
    }
    #print "gnuplot is $gnuplot $params{GNUPLOT}\n";

    my $varstring = "";		# create display version of names
    foreach $t (@$vars) {
	$varstring .= ", " if ($varstring);
	$varstring .= ($timerNames{$t}) ? $timerNames{$t} : $t;
    }

    # Setup output "terminal type"
    if ($params{IMAGETYPE} eq "gif") { # gif type has different arguments
	print SCRIPT "set terminal $params{IMAGETYPE} small size $params{CHARTWIDTH},$params{CHARTHEIGHT}\n";
    } elsif ($params{IMAGETYPE} eq "svg") { # svg type has different args too
	print SCRIPT "set terminal $params{IMAGETYPE} size $params{CHARTWIDTH} $params{CHARTHEIGHT}\n";
    } else {			# most types work like this
	print SCRIPT "set terminal $params{IMAGETYPE} small color\n";
	if (($params{CHARTWIDTH} != 640) || ($params{CHARTHEIGHT} != 480)) {
	    my $xscale = $params{CHARTWIDTH} / 640;
	    my $yscale = $params{CHARTHEIGHT} / 480;
	    print SCRIPT "set size $xscale,$yscale\n";
	}
    }
    print SCRIPT<<"!GROK!THIS!";
set output "../../$resultdir/$name.$params{IMAGETYPE}" # ASSUME $tmpbase is single dir
set autoscale
set xlabel "Test time (seconds)"
set ylabel "$label"
set title "$title ($varstring) -- $params{TSTAMP}"
plot [0:$runTime] $runlist
!GROK!THIS!
    close(SCRIPT);

    # run script from tmpbase to clean up line labeling
#    my $olddir=getcwd();
    chdir $tmpdir;
    system (split (/\s/, $gnuplot), "$name.gpt");
    chdir "../..";
#    chdir $olddir;

    unless ($params{DEBUG}) {
	# cleanup the plot data (or leave around for debugging)
	foreach $p (@$protos) {
	    unlink("$tmpdir/$name.$p");
	}
	unlink("$tmpdir/$name.gpt");
    }
    return 1;
}


return 1;
