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

# for each protocol, store rate over time (figure derivative), and final count
# commands
# readMessages
# writeMessages
# readBytes
# writeBytes
# connections
# these are already 0 based over time, and average
# connectDelay
# transactionTime

#require "perl-5.005";

#use Cwd;

print "Combining client results:\t", scalar (localtime), "\n";

# Basic sanity check
unless ($testsecs > 0) {
    die "Test time is 0!\n";
}

$startTime = 0;			# these are timeInSeconds/$timeStep
$endTime = 0;

# keep graphs with somewhat more precision than sample rate
# this is supposed to help deal with time skew between clients
#$timeStep = int ($params{FREQUENCY} / 2);
#if ($timeStep < 1) { $timeStep = 1; }
$timeStep = int ($params{FREQUENCY});

# global results initialization
$reportingClients = 0;
$totalProcs = 0;		# number of clients started

my $maxTimeStep = $params{FREQUENCY}*2;
$maxTimeStep = 10 if ($maxTimeStep < 10);


# Fill in graph values
# Usage: updateGraph (graph, lastTimeStep, TimeStep, timeSecs,
#	lastValue, value);
sub updateGraph {
    #SLOW: my ($gp, $lastTime, $time, $timeD, $lastValue, $value) = @_;
    my $gp = shift;		# gp: graph hash to fill in over time
    my $lastTime = shift;	# lastTime: time values (already divided by timestep)
    my $time = shift;
    my $timeD = shift;		# timeD: time delta in seconds
    my $lastValue = shift;
    my $value = shift;
    return unless ($timeD);	# initial case

    #print "updateGraph: time='$time' timeD='$timeD' lastValue='$lastValue' value='$value'\n";
    my $v = ($value - $lastValue) / $timeD; # figure update / step
    for ($i = $lastTime; $i < $time; $i++) { # fill in graph
	$gp->{$i} += $v;
    }
}

# Fill in graph values, figuring the MIN
# Usage: updateMinGraph (graph, lastTimeStep, TimeStep, value);
sub updateMinGraph {
    my $gp = shift;		# graph hash to fill in over time
    my $lastTime = shift;	# time values (already divided by timestep)
    my $time = shift;
    my $v = shift;
    return unless ($lastTime);	# initial case
    return unless ($v > 0);		# 0 min is considered no information

    #print "updateMinGraph: time='$time' lastTime='$lastTime' value='$v'\n";
    for ($i = $lastTime; $i < $time; $i++) { # fill in graph
	$gp->{$i} = $v if (!($gp->{$i}));
	$gp->{$i} = $v if ($v < $gp->{$i});
    }
}

# Fill in graph values, figuring the MAX
# Usage: updateMaxGraph (graph, lastTimeStep, TimeStep, value);
sub updateMaxGraph {
    my $gp = shift;		# graph hash to fill in over time
    my $lastTime = shift;	# time values (already divided by timestep)
    my $time = shift;
    my $v = shift;
    return unless ($lastTime);	# initial case

    #print "updateMaxGraph: time='$time' lastTime='$lastTime' value='$v'\n";
    for ($i = $lastTime; $i < $time; $i++) { # fill in graph
	$gp->{$i} = $v if (!($gp->{$i}));
	$gp->{$i} = $v if ($v > $gp->{$i});
    }
}

# Turn one of the disadvantages of interpreted code to an advantage
#  by writing optimal code on the fly.  
#  You must be taller than Kenny the kangaroo to edit this code.
# Create a function that will parse timers (all timer must be identical)
# Timer are built out of positional assignments (not attr=value pairs)
sub CreateFastTimerParser {
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string
    
    my @tlist = @{$ltype->{"$fstr:SEPS"}};
    my @nlist = @{$ltype->{"$fstr:NAMES"}};
	
				# function preamble
    my $fn = "sub FastTimerParser {\n";

    $fn .= 'my $sc = shift; my $gp = shift;
    my $lt = shift; my $t = shift; my $td = shift;
    my $ltype = shift; my $fstr = shift;
    my $ln = shift;' . "\n";

    #$fn .= 'print "(fastTimerParser) $fstr $lt $t $td\n";' . "\n";
    $fn .= 'if ($ln =~ m/^';

    foreach $s (@tlist) {	# write pattern match
	$fn .= "(.+)$s";
    }
    $fn .= '(.+)$/) {' . "\n";		# last field in match
    #$fn .= 'print "$ln =\t$1 + $2 / $3 + $4 / $5 [ $6 , $7 ] $8\n";' . "\n";

    my $n = 0;
    foreach $v (@nlist) {		# write update calls
	$n++;
	if ($v =~ /Min$/) {
	    $fn .= 'updateMinGraph ($gp->{"' . $v . '"}, $lt, $t, $' . $n . ");\n";
	    # Handle never defined case first.
	    $fn .= '$sc->{"' . $v . '"} = 0 if (!($sc->{"' . $v . '"}));' . "\n";
	    # Do the Min update. 0 is not a valid number
	    $fn .= '$sc->{"' . $v . '"} = $' . $n . ' if (($' . $n . ' > 0) && (($sc->{"' . $v . '"} == 0) || ($' . $n . ' < $sc->{"' . $v . '"})));' . "\n";
	} elsif ($v =~ /Max$/) {
	    $fn .= 'updateMaxGraph ($gp->{"' . $v . '"}, $lt, $t, $' . $n . ");\n";
	    $fn .= '$sc->{"' . $v . '"} = $' . $n . ' if (!($sc->{"' . $v . '"}) || ($' . $n . ' > $sc->{"' . $v . '"}));' . "\n";
	} else {
	    $fn .= 'updateGraph ($gp->{"' . $v . '"}, $lt, $t, $td, $sc->{"' . $v . '"}, $' . $n . ");\n";
	    $fn .= '$sc->{"' . $v . '"} = $' . $n . ";\n";
	}
    }
    $fn .= "\n} else {\n";
    $fn .= 'die "Error parsing timer from: $ln\n";';
    $fn .= "\n}\n}\n";
    #($params{DEBUG}) && print "Created timer parse function: $fn\n";
    eval $fn;			# create the function
    $timerParser = \&FastTimerParser;
}
# All this quoted stuff will screw up Emacs' formatter/colorizer.  Oh well.

# Similar to above.
# This seems to be a smaller gain (about 10%) than the timers.
sub CreateFastProtocolParser {
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string

    my @tlist = @{$ltype->{"$fstr:SEPS"}};
    my @nlist = @{$lst = $ltype->{"$fstr:NAMES"}};

    #print "CreateFastProtocolParser: $fstr\n";

    my $nm = $fstr;
    $nm =~ s/^.*://;
    my $fn = "sub FastProtocolParser$nm {\n";
    $fn .= 'my $sc = shift; my $gp = shift;
    my $lt = shift; my $t = shift; my $td = shift;
    my $ltype = shift; my $fstr = shift;
    my $ln = shift;' . "\n";

    #$fn .= 'print "(FastProtocolParser' . $nm . ' ) $fstr $lt $t $td\n";' . "\n";
    $fn .= 'if ($ln =~ m/^';
    my $posttext = pop @tlist;	# save last bit of text
    foreach $s (@tlist) {	# write pattern match
	$fn .= "$s(.+)";
    }
    $fn .= $posttext . '$/) {' . "\n"; # last text in match
    my $n = 0;
    foreach $v (@nlist) {	# write update calls
	$n++;
	if ($v =~ m/^\[(.+)\]$/) { # timer
	    $fn .= 'FastTimerParser ($sc->{"' . $1 . '"}, $gp->{"' . $1 . '"}, $lt, $t, $td, $ltype, "TIMERS:' . $1 . '", $' . $n . ');' . "\n";
	} else {		# direct assignment
	    $fn .= 'updateGraph ($gp->{"' . $v . '"}, $lt, $t, $td, $sc->{"' . $v . '"}, $' . $n . ');' . "\n";
	    $fn .= '$sc->{"' . $v . '"} = $' . $n . ";\n";
	}
    }
    $fn .= "\n} else {\n";
    $fn .= 'die "Error parsing protocol ' . $nm . ' from: $ln\n";';
    $fn .= "\n}\n}\n";
    $fn .= '$cliLines{"SUMMARY-TIME"}->{"' . $fstr . ':PROTOPARSE"} = \&FastProtocolParser' . $nm . ";\n";
    #print "Created proto parse function: $fn\n";
    eval $fn;			# create the function
}

# This is the slow verson of timer parsing.  Kept for possible debugging
# There are only 4 levels to the parse hierarchy: line, protocol, timer, value
# each level consists of 1 or more elements from lower levels, plus text

# Given a timer format description, break string into name,value chunks 
# Timer are built out of positional assignments (not attr=value pairs)
# Usage: parseTimer (...lineHash, format, line)
sub parseTimer {
    my $subcli = shift;		# client hash to update
    my $gp = shift;		# graph hash to fill in over time
    my $lastTime = shift;	# time values (already divided by timestep)
    my $time = shift;
    my $timeD = shift;
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string
    my $line = shift		# text line
	|| die "Missing arguments to parseTimer";
    
    my @tlist = @{$ltype->{"$fstr:SEPS"}};
    my @nlist = @{$lst = $ltype->{"$fstr:NAMES"}};
    #print "\nTimer seps ($fstr): @tlist\nNames: @nlist\n";

    my $remln = $line;
	
    while (@nlist) {
	my $chunk;
	my $sepln;
	my $posttext = shift @tlist;
	my $vname = shift @nlist;

	if ($posttext) {	# remln looks like <value><literal text>...
	    ($chunk, ($sepln), $remln) = split /($posttext)/, $remln, 2;
	    #print "chunk='$chunk' sep='$sepln' remln='$remln'\n";
	    die "Error finding string '$posttext' in '$remln' of '$line'"
		unless ($sepln);
	} else {		# remln looks like <value>
	    $chunk = $remln;
	}
	#print "$vname=$chunk ";
	die "updateGraph: Missing graph '$vname'\n" unless ($gp->{$vname});
	if ($vname =~ /Min$/) {
	    updateMinGraph ($gp->{$vname}, $lastTime, $time, $chunk);
	} elsif ($vname =~ /Max$/) {
	    updateMaxGraph ($gp->{$vname}, $lastTime, $time, $chunk);
	} else {
	    updateGraph ($gp->{$vname}, $lastTime, $time, $timeD,
				$subcli->{$vname}, $chunk);
	}
	$subcli->{$vname} = $chunk;
    }
    #print "\n";
}

# This is the slow verson of protocol parsing.  Kept for possible debugging
# Given a format description, break string into name,value chunks 
# Very similar to parseLine, except: no sub protocols, no top level issues
# Usage: parseProtocol (...lineHash, format, line)
sub parseProtocol {
    my $subcli = shift;	# client hash to update
    my $gp = shift;		# graph hash to fill in over time
    my $lastTime = shift;	# time values (already divided by timestep)
    my $time = shift;
    my $timeD = shift;
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string
    my $line = shift		# text line
	|| die "Missing arguments to parseProtocol";

    #print "\nparseProtocol format='$fstr' line='$line'\n";
    
    my @tlist = @{$ltype->{"$fstr:SEPS"}};
    my @nlist = @{$lst = $ltype->{"$fstr:NAMES"}};
    #print "\nProtocol seps ($fstr): @tlist\nNames: @nlist\n";

    # remove first part of literal text
    my $posttext = shift @tlist;
    my ($chunk, $sepln, $remln) = split /($posttext)/, $line, 2;
    die "Error parsing initial string '$posttext' from '$line'"
	unless ($sepln);
	
    # progressively split $fstr into literal chunks
    # back to back timers or protocols (with no literal space) is not allowed
    while (@nlist) {
	# remln always looks like <value><literal text>
	my $vname = shift @nlist;
	$posttext = shift @tlist;
	#print "posttext='$posttext'\n";

	($chunk, ($sepln), $remln) = split /($posttext)/, $remln, 2;
	#print "chunk='$chunk'\n";
	#print "remln=$remln\n\n";
	die "Error finding string '$posttext' in '$remln'"
	    unless ($sepln);

	if ($vname =~ m/^\[(.+)\]$/) { # timer
	    #print "TIMER $vname ";
	    die "Unknown timer referenced: $vname for client "
		. $scalar{"client"} . "\n"
		unless ($subcli->{$1});
	    die "Invalid timer specified: $ltype->{TIMERS}->{$vname} in '$line'"
		unless ($ltype->{TIMERS}->{$vname});
	    FastTimerParser ($subcli->{$1}, $gp->{$1},
			$lastTime, $time, $timeD,
			$ltype, "TIMERS:$1", $chunk);
	} else {		# direct assignment
	    #print "$vname='$chunk' ";
	    die "updateGraph: Missing graph '$vname'\n" unless ($gp->{$vname});
	    updateGraph ($gp->{$vname}, $lastTime, $time, $timeD,
				$subcli->{$vname}, $chunk);
	    $subcli->{$vname} = $chunk;
	}
    }
    #print "\n";
}

# Given a format description, break string into name,value chunks 
# Figures out all the top level issues of:
#	Figure out what client, what time span,
#	Pass down top level structures
#	Do final special processing (connections graph)
# Usage: parseLine (clientsHash, lineHash, format, line)
sub parseLine {
    my $clients = shift;	# hash of all clients
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string to match
    my $line = shift		# line to parse up
	|| die "Missing arguments to parseLine";
    #print "parseLine: $line\n"; # format='$fstr' 

    my %scalar = ();		# hold scalars until client storage located
    my $subcli;			# client hash to update ($clients->{$n})
    my $lastTime;		# time values (already divided by timestep)
    my $time;
    my $timeD;			# time delta (in seconds)

    my @tlist = @{$ltype->{"$fstr:SEPS"}};
    my @nlist = @{$lst = $ltype->{"$fstr:NAMES"}};
    #print "\nLine seps ($fstr): @tlist\nNames: @nlist\n";

    # remove first part of literal text
    # This should always work, since we pattern matched on this to get here
    my $posttext = shift @tlist;
    my $vname;
    #print "posttext='$posttext'";
    my ($chunk, $sepln, $remln) = split /($posttext)/, $line, 2;
    die "Error parsing initial string '$posttext' from '$line'"
	unless ($sepln);
	
    # progressively split $fstr into literal chunks
    while (@nlist) {
	# remln always looks like <value><literal text>
	$vname = shift @nlist;
	$posttext = shift @tlist;
	#print "\nposttext='$posttext' vname='$vname'\n";

	($chunk, ($sepln), $remln) = split /($posttext)/, $remln, 2;
	#print "chunk='$chunk'\n";
	#print "remln=$remln\n\n";
	die "Error finding string '$posttext' in '$remln'"
	    unless ($sepln);

	if (($vname =~ m/^\[.+\]$/)
	    || ($vname =~ m/^\{.+\}$/)){		# timer or hash

	    unless ($subcli) {	# setup sub client 
		$subcli = $clients->{$scalar{"client"}};
		die "Error parsing client number $scalar{client}"
		    unless ($subcli);
		die "Unknown client referenced: $scalar{client}\n"
		    unless ($subcli);
		$time = $scalar{"time"};
		if ($subcli->{"time"}) {
		    $lastTime = $subcli->{"time"};
		    $timeD = $time - $lastTime; # seconds (not steps)
		    $lastTime = int ($lastTime / $timeStep); # to timesteps
		    if (0 == $timeD) { # final summary case
			$timeD = 1;
			#print "Final update client=$n time=$time\n";
			$time += 1; # so 1 data point gets updated
		    }
		} else {	# first time report
		    $subcli->{"startTime"} = $time;
		    $lastTime = 0;
		    $timeD = 0;	# no time update
		}
		$time = int ($time / $timeStep); # convert to timesteps

		# now store the scalars we have already seen
		#print "(";
		foreach $k (keys %scalar) {
		    #print "$k=$scalar{$k} ";
		    $subcli->{$k} = $scalar{$k};
		    # Dont bother with time graphs here
		}
		#print ") ";
	    }

	    if ($vname =~ m/^\[(.+)\]$/) { # timer
		#print "TIMER $vname '$chunk'\n";
		die "Unknown timer referenced: $vname for client $scalar{client}\n"
		    unless ($subcli->{$1});
		die "Invalid timer specified: $ltype->{TIMERS}->{$vname}"
		    unless ($ltype->{TIMERS}->{$vname});
		FastTimerParser ($subcli->{$1}, $graphs{$1},
			    $lastTime, $time, $timeD,
			    $ltype, "TIMERS:$1", $chunk);
	    } elsif ($vname =~ m/^\{(.+)\}$/) { # protocol
		#print "PROTOCOL $vname '$chunk'\n";
		die "Unknown protocol referenced: $vname for client $scalar{client}\n"
		    unless ($subcli->{$1});
		die "Invalid protocol specified: $ltype->{PROTOCOLS}->{$1}"
		    unless ($ltype->{PROTOCOLS}->{$vname});
		
		#parseProtocol
		&{$ltype->{"PROTOCOLS:$1:PROTOPARSE"}}
		($subcli->{$1}, $graphs{$1},
		 $lastTime, $time, $timeD,
		 $ltype, "PROTOCOLS:$1", $chunk);
	    } else {
		die "parseLine: Unkown separator $posttext\n";
	    }
	} else {		# direct assignment
	    # Note: we dont graph any scalars at the line level
	    #print "$vname='$chunk'\n";
	    $scalar{$vname} = $chunk; # store for later
	    $subcli->{$k} = $chunk if ($subcli);
	} 
    }
    #print "\n";

    return unless ($timeD);	# skip rest if first sample

    # Now do post processing based on the updated $subcli
    foreach $prot (@protocols) { # figure concurrent connections
	next unless ($subcli->{$prot});
	my $pcli = $subcli->{$prot};
	next unless (($pcli->{"conn"}) && ($pcli->{"conn"}->{"Try"}));

	my $connD = $pcli->{"conn"}->{"Try"}; # connections minus logouts
	$connD += $pcli->{"reconn"}->{"Try"} if ($pcli->{"reconn"});
	$connD -= $pcli->{"logout"}->{"Try"};

	$connD -= $pcli->{"total"}->{"Error"}; # subtract sum of errors
	# foreach $k (keys %$pcli) { # subtract errors (which force close)
	# ($k =~ /total/) && next;
	# $connD -= $pcli->{$k}->{"Error"};
	# }

	#print "DERIVED connections {$prot}\n";
	die "updateGraph: Missing graph 'connections'\n"
	    unless ($graphs{$prot}->{"connections"});
	# pass timeD as 1 to avoid time scaling
	updateGraph ($graphs{$prot}->{"connections"},
		     $lastTime, $time, 1, 0, $connD);
    }
}

# Walk a format description (recursively)
# Usage: walkFormatValues (clientHash, formatHash, "LINE", evalExpr)
# Eval expression is call as follows:
#	value in $a
#	field name in $f
#	field path in $p
sub walkFormatValues {
    my $cli = shift;	# hash of all clients
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string to match
    my $doit = shift;		# routine to call
    my $p = shift;		# path to variable (may be empty)
    #print "walkFormatValues ($fstr):  extra: @_\n" if (@_);

    my @nlist = @{$ltype->{"$fstr:NAMES"}};
    #print "walkFormatValues ($fstr):  @nlist";

    if ($p) {			# initialize path
	$p .= ":";
    } else {
	$p = "";
    }

    while (@nlist) {
	my $f = shift @nlist;	# get field name

	if ($f =~ m/^\[(.+)\]$/) { # timer
	    #print "Timer $f:\n";
	    die "Unknown timer referenced: $f for client $scalar{client}\n"
		unless ($cli->{$1});
	    die "Invalid timer specified: $ltype->{TIMERS}->{$f}"
		unless ($ltype->{TIMERS}->{$f});
	    walkFormatValues ($cli->{$1}, $ltype, "TIMERS:$1",
			      $doit, "$p$1", @_);
	} elsif ($f =~ m/^\{(.+)\}$/) { # protocol
	    #print "Protocol $f:\n";
	    die "Unknown protocol referenced: $f for client $scalar{client}\n"
		unless ($cli->{$1});
	    die "Invalid protocol specified: $ltype->{PROTOCOLS}->{$1}"
		unless ($ltype->{PROTOCOLS}->{$f});
	    walkFormatValues ($cli->{$1}, $ltype, "PROTOCOLS:$1",
			      $doit, "$p$1", @_);
	} else {		# direct assignment
	    #print "$p$f=$a ";
	    #too slow???: eval $doit;
	    &$doit ($cli->{$f}, $f, $p, @_);
	} 
    }
    #print "\n";
}

# Given a timer format description, break string into name,value chunks 
# Usage: allocTimer (clientsHash, graphHash, lineHash, format)
sub allocTimerStorage {
    my $clients = shift;	# hash of this client
    my $graphs = shift;		# hash of graphs
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string to match
    my $sepstr = shift;		# seperator storage to fill in

    #print "parseTimer: format='$fstr'\n\tline='$line'\n";
    my $sepf;
    my $remf = $fstr;
    my $chunk;
    my $sepln;
    my $remln = $line;
    my $vname;
    my @tlist = ();		# list of text tokens
    my @nlist = ();		# list of names

    # progressively split $fstr into literal chunks
    while ($remf) {
	# remf always looks like <tag><literal text>...
	($vname, $sepf, $remf) = split /([][()+\/\,;:|])/, $remf, 2;

	if ($remf) {		# remln looks like <value><literal text>...
	    $sepf =~ s/([][{}*+?^.\/])/\\$1/g; # quote regex syntax
#	    push @tlist, qr/$sepf/; # store compiled match expression
	    push @tlist, $sepf;
	} else {		# remln looks like <value>
	    $chunk = $remln;
	}
	push @nlist, $vname;
	#print "vname='$vname' sep='$sepf'\n";
	unless ($graphs->{$vname}) {
	    #print "$vname ";
	    # This hash that will hold the actual values over time
	    $graphs->{$vname} = ArrayInstance->new();
	}
    }
    #print "Timer sep list: @tlist.  Names: @nlist\n";
    $ltype->{"$sepstr:SEPS"} = \@tlist;
    $ltype->{"$sepstr:NAMES"} = \@nlist;
    CreateFastTimerParser ($ltype, $sepstr) unless ($timerParser);
    # SIDE EFFECT: updates timerFieldsAll
    @timerFieldsAll = @nlist unless (@timerFieldsAll);
}

# Walk a format discription and create storage hierarchy
# Also creates the seperator and field names lists used everywhere else
# 	Some way to precompile the regular expressions (perl 5.005)?
# Usage: allocStorage (clientsHash, graphHash, lineHash, format, "LINE")
sub allocStorage {
    my $clients = shift;	# hash of this client
    my $graphs = shift;		# hash of graphs
    my $ltype = shift;		# hash of line parsing info
    my $fstr = shift;		# format string to match
    my $sepstr = shift;		# seperator storage to fill in
    #print "\nallocStorage format='$fstr'\n";

    my @tlist = ();		# list of text tokens
    my @nlist = ();		# list of name tokens
    # remove first part of literal text
    my ($posttext, $sepf, $remf) = split /([=\{\[])/, $fstr, 2;
    my $nexttext = $posttext . $sepf;
    $posttext .= $sepf if ($sepf =~ m/=/);	# should always happen?
    push @tlist, $posttext;	# store initial text too

    # progressively split $fstr
    my $vname;
    while ($remf) {
	my $lasttext = $nexttext; # last literal text goes with this $vname

	# Get the variable name
	# remf always looks like <tag><literal text>
	#$remf =~ s/^(\w+)//; $vname = $1; # strip value text off line
	($vname, $sepf, $remf) = split /([^\w])/, $remf, 2; # get value text
	unless ($vname) {	# got =[ or ={, go again.
	    #print "+$sepf ";
	    $nexttext .= $sepf;
	    next;
	}
	
	$remf = $sepf . $remf	# put seperator back in to remainder
	    unless ($sepf  =~ /[\]\}]/); # unless after a timer/proto name
	#print "vname='$vname'\n";

	# now get the next bit of literal text
	($posttext, $sepf, $remf) = split /([][={}])/, $remf, 2;
	$nexttext = $posttext . $sepf if ($remf); # unless end of line

	if (($sepf) && ($sepf =~ m/=/)) { # direct assignment
	    $posttext .= $sepf;	# this is part of literal text
	}
	$posttext =~ s/([][{}*+?^.\/])/\\$1/g; # quote regex syntax
	#print "`$lasttext' ";	# this shows some trailing cruft, oops
	#print "remf='$remf'\n";
#	push @tlist, qr/$posttext/;	# save seperator text
	push @tlist, $posttext;	# save seperator text

	if ($lasttext =~ m/[][]/) { # timer
	    #print "[$vname] ";
	    push @nlist, "[$vname]"; # save field name as timer
	    my $nm = "[$vname]";
	    die "Invalid timer specified: $vname in '$fstr'"
		unless ($ltype->{TIMERS}->{$nm});
	    $clients->{$vname} = ArrayInstance->new(); # create sub hash

	    unless ($graphs->{$vname}) { # create graph
		#print "Creating timer graph '$vname':  ";
		$graphs->{$vname} = ArrayInstance->new();
	    } else {
		#print "Timer graph '$vname' already exists.";
	    }
	    allocTimerStorage ($clients->{$vname}, $graphs->{$vname},
			       $ltype,
			       $ltype->{TIMERS}->{$nm}, "TIMERS:$vname");
	    #print "\n"
	} elsif ($lasttext =~ m/[{}]/) { # protocol
	    #print "{$vname}\n";
	    push @nlist, "{$vname}"; # save field name as protocol
	    my $nm = "{$vname}";
	    $clients->{$vname} = ArrayInstance->new(); # create sub hash
	    die "Invalid protocol specified: $vname"
		unless ($ltype->{PROTOCOLS}->{$nm});
	    unless ($graphs->{$vname}) { # create graph protocol
		#print "Creating graph protocol '$vname'\n";
		$graphs->{$vname} = ArrayInstance->new();
				# create generated scalar fields
		$graphs->{$vname}->{"connections"} = ArrayInstance->new();
	    }
	    # recurse into protocol definition
	    # this would allow recursive protocols, 
	    #   but this isnt supported elsewhere
	    allocStorage ($clients->{$vname}, $graphs->{$vname},
			  $ltype,
			  $ltype->{PROTOCOLS}->{$nm}, "PROTOCOLS:$vname");

	    # SIDE EFFECT: updates protocolFields
	    unless ($protocolFields{$vname}) {
		my $lst = $ltype->{"PROTOCOLS:$vname:NAMES"};
		$protocolFields{$vname} = $lst;
		#print "$vname fields: @{$lst}\n"; # DEBUG
	    }
	    CreateFastProtocolParser ($ltype, "PROTOCOLS:$vname")
		unless ($cliLines{"SUMMARY-TIME"}->{"PROTOCOLS:$vname:PROTOPARSE"});
	}  elsif ($lasttext =~ m/=/) { # direct assignment
	    #print "$vname ";
	    push @nlist, $vname;	# save field name
	    #$clients->{$vname} = 0; # preset scalar field
	    unless ($graphs->{$vname}) { # create graph
		#print "Creating value graph '$vname'\n";
		$graphs->{$vname} = ArrayInstance->new();
	    }
	} else {
	    die "Got lost parsing $fstr\n";
	}
    }
    #print "Line/proto sep list: @tlist\nNames: @nlist\n";
    $ltype->{"$sepstr:SEPS"} = \@tlist;
    $ltype->{"$sepstr:NAMES"} = \@nlist;
}

# These are helper routines to be called by walkFormatValues
# Each gets called at each format node with value, field name, field path
sub walkShowVar {
    my $a = shift; my $f = shift; my $p = shift;
    print CSV "$p$f,";
}
sub walkShowName {
    my $a = shift;
    print CSV "$a,";
}
sub walkCreateFinals {
    my $a = shift; my $f = shift; my $p = shift;
    
    if ($p =~ /(\w+):(\w+):$/) {
	$finals{$1}->{$2}= ArrayInstance->new() unless ($finals{$1}->{$2});
    }
} 
# Add up client data to form total data
sub walkTotalFinals {
    my $a = shift; my $f = shift; my $p = shift;
    if ($p =~ /(\w+):(\w+):$/) { # sub arrray case
	my $pr = $1; my $tm = $2;
	if (!($finals{$pr}->{$tm}->{$f})) { # first assignment
	    $finals{$pr}->{$tm}->{$f} = $a;
	} elsif ($f =~ /Min$/) { # take the new MIN
	    $finals{$pr}->{$tm}->{$f} = $a
		if (($a > 0.0) && ($a < $finals{$pr}->{$tm}->{$f}));
	} elsif ($f =~ /Max$/) { # take the new MAX
	    $finals{$pr}->{$tm}->{$f} = $a if ($a > $finals{$pr}->{$tm}->{$f});
	} else {		# simple sum
	    $finals{$pr}->{$tm}->{$f} += $a;
	}
    } elsif ($p =~ /(\w+):$/) {	# scalar case, simple sum
	$finals{$1}->{$f} += $a;
    }
}

				# Since all timers must be the same
#@timerFieldsAll		# names of all timer fields (in order)
%protocolFields = ();		# protocol as key, hash of lists of fields

# This is where the raw data files get read in.
foreach $section (@workload) {
    next unless ($section->{sectionTitle} =~ /CLIENT/o);
    next unless ($section->{PROCESSES}); # unused client

    my $slist = $section->{sectionParams};
    $slist =~ s/HOSTS=\s*//; # strip off initial bit
    foreach $cli (split /[\s,]/, $slist) {
	my $clientfile= getClientFilename ($cli, $section);
	# open the output from this child
	open(CLIENTDATA, "<$clientfile") || 
	    open(CLIENTDATA, "gunzip -c $clientfile.gz |") || 
		warn "Couldn't open $clientfile:$!\n";

	# start writing clients.csv file 
	fileBackup ("$resultdir/client-$cli.csv");
	open(CSV, ">$resultdir/clients-$cli.csv") # Summary of all clients
	    || die "Could not open $resultdir/client-$cli.csv: $!\n";

	($params{DEBUG}) && print "Processing $clientfile\n";
	($params{DEBUG}) && print "\tInput start:\t", scalar (localtime), "\n";
	my $numThisClient=0;
	my $linesThisClient=0;
	my $skippedLines=0;
	my $summaryLines=0;
	my $noticeLines=0;

	%cliLines = ();		# line_name as key, hash of info fields
	my %clidata = ();	# clear client data store

	my $cliTimers = ArrayInstance->new(); # timer, format pairs
	my $cliProtocols = ArrayInstance->new(); # protocol, format pairs


	# read through, looking for connections and throughput over time
	RAWLINE: while (<CLIENTDATA>) {
	    $linesThisClient++;

	    if (/^<FORMAT client=/) { # FORMAT definition
		#print "FORMAT (raw) $_\n";
		s/<\/FORMAT>\s*$//; # strip close format and newline
		s/"//;		# strip quotes (Fix emacs) "
		my ($element, $value) = split />/, $_, 2;
		$element =~ s/^<FORMAT //; # strip initial part
		my ($clipair, $typepair) = split /\s/, $element;
		my ($clijunk, $clinum) = split /=/, $clipair;
		my ($type, $label) = split /=/, $typepair;

		if ($type =~ /TIMER/) {
		    unless ($cliTimers->{$label}) { # already exists
			$cliTimers->{$label} = $value;
			($params{DEBUG}) &&
			    print "TIMER $label, $value\n";
			#print CSV "<FORMAT client=$clinum TIMER=$label>$value</FORMAT>\n";
		    }
		} elsif ($type =~ /PROTOCOL/) {
		    unless ($cliProtocols->{$label}) {
			$cliProtocols->{$label} = $value;
			($params{DEBUG}) &&
			    print "PROTOCOL $label, $value\n";
			#print CSV "<FORMAT client=$clinum PROTOCOL=$label>$value</FORMAT>\n";
		    }
		} elsif ($type =~ /LINE/) {
		    # Note: Line types ($label) must be unique

		    unless ($cliLines{$label}) { # already seen
			my ($spat) = split /=/, $value;
			$spat = "^$spat=";

			($params{DEBUG}) &&
			    print "LINE/$spat/ $value\n";

			#print "LINE short pattern '^$spat='\n";
			# Store all the information about parsing this line
			$cliLines{$label} = ArrayInstance->new();
			$cliLines{$label}->{LINE} = $value;
			# $cliLines{$label}->{PATTERN} = qr/^$spat/;
			$cliLines{$label}->{PATTERN} = "^$spat";
			$cliLines{$label}->{TIMERS} = $cliTimers;
			$cliLines{$label}->{PROTOCOLS} = $cliProtocols;
			#print CSV "<FORMAT client=$clinum LINE=$label>$value</FORMAT>\n";
		    } else {
			#print "Skipping already seen line: $label $value\n";
		    }

		    # Allocate storage for each client number
		    unless ($clidata{$clinum}) {
			$clidata{$clinum} = ArrayInstance->new();
			allocStorage ($clidata{$clinum}, \%graphs,
				      $cliLines{$label},
				      $cliLines{$label}->{LINE}, "LINE");
		    }
		} else {
		    print "Unknown FORMAT: line $_\n";
		}
		next;
	    }

	    # check most common case first
	    if (/$cliLines{"SUMMARY-TIME"}->{PATTERN}/) {
		chomp;		# strip newline
		parseLine (\%clidata,
			   $cliLines{"SUMMARY-TIME"}, "LINE", $_);
		$summaryLines++;
		next;
	    }

	    # Should optimize this dynamic matching, Perl Cookbook 182-185
	    foreach $key (keys %cliLines) {
		#print "Checking $cliLines{$key}->{PATTERN}\n";
		next unless (m/$cliLines{$key}->{PATTERN}/);
		#print "\nMatched dynamic line $key: '$cliLines{$key}->{PATTERN}'\n";
		if ($key =~ /^SUMMARY-TIME/) {
		    warn "SUMMARY-TIME slipped through.  Should never happen\n";
		} elsif ($key =~ /^NOTICE/) {
		    $noticeLines++;
		    next RAWLINE;
		} elsif ($key =~ /^BLOCK-STATISTICS-/) {
				# ignore for now
		    next RAWLINE;
		} else {
		    warn "Found format line without hander $key\n";
		    last;
		}
	    }

	    ($params{DEBUG}) && print "skipping $.: $_";
	    $skippedLines++;
	    next;
	}

	close(CLIENTDATA);
	($params{DEBUG}) && print "\tInput done:\t", scalar (localtime), "\n";

	my $pcount = $section->{PROCESSES};
	my $tcount = ($section->{THREADS}) ? $section->{THREADS} : 1;
	$totalProcs += $pcount * $tcount;

	# Update totals, and dump out client numbers
	my @clist = sort numeric keys %clidata;
	$numThisClient = $#clist+1;

	$reportingClients += $numThisClient * $tcount;

	# Write out format description.  Note can be different for every client
	walkFormatValues ($clidata{0}, $cliLines{"SUMMARY-TIME"}, "LINE",
			  \&walkShowVar);
	print CSV "\n";
	foreach $p (@protocols) { # Create the finals arrays as needed
	    $finals{$p} = ArrayInstance->new() unless ($finals{$p});
	}

	# Create finals arrays (if needed)
	walkFormatValues ($clidata{0}, $cliLines{"SUMMARY-TIME"}, "LINE",
			  \&walkCreateFinals);
	foreach $cnum (@clist) {
	    my $cn = $clidata{$cnum};
	    #print "Total client $cnum\n";

	    # Total finals array.  Handle Min and Max special cases
	    walkFormatValues ($cn, $cliLines{"SUMMARY-TIME"}, "LINE",
			      \&walkTotalFinals);

        }
	($params{DEBUG}) && print "\tTotals done:\t", scalar (localtime), "\n";
	foreach $cnum (@clist) {
	    my $cn = $clidata{$cnum};
	    # Write out CSV
	    walkFormatValues ($cn, $cliLines{"SUMMARY-TIME"}, "LINE",
			      \&walkShowName);
	    print CSV "\n";
	}
	($params{DEBUG}) && print "\tCSV done:\t", scalar (localtime), "\n";
	#print "\n";

	print "Processed: $clientfile: $numThisClient clients\n";
	if (1 || $params{DEBUG}) {
	    foreach $cnum (@clist) {
		printf "\tFirst: %d\tFinal: %d\tDuration: %d\n",
		$clidata{$cnum}->{startTime}, $clidata{$cnum}->{"time"},
		$clidata{$cnum}->{"time"} - $clidata{$cnum}->{startTime};
	    }
	}

	print "\t$summaryLines summaries, $noticeLines notices, $skippedLines unknown.\n";
	print CSV "\n";
	close(CSV);
	($params{DEBUG}) && print "Wrote $resultdir/client-$cli.csv\n";

    }
}

%cliLines = ();			# clear storage


unless ($reportingClients > 0) {
    print "No clients reported.  Check $resultdir/stderr\n";
    die "Test Failed!";
}

# Find time extent for a key graph
($startTime, $endTime) = dataMinMax ("blocks", \@protocols,
				     $startTime, $endTime);

$realTestSecs = ($endTime - $startTime) * $timeStep;
$realTestSecs = 1 unless ($realTestSecs); # in case of small MaxBlocks
printf "Reported test duration %d seconds with %d second resolution\n",
	$realTestSecs, $timeStep;
$realTestSecs = $testsecs if ($realTestSecs > $testsecs);

($params{DEBUG})
    && print "Doing statistical data reduction:\t", scalar (localtime), "\n";


# WRONG:  These numbers have already been converted to rate and summed
# Convert Time2 to standard deviation
foreach $p (@protocols) {
    my $gp = $graphs{$p};
    foreach $n (@{$protocolFields{$p}}) { # all timers
	my $t = $n;	# dont modify original list
	if ($t =~ /^\[(\w+)\]$/) { $t = $1; } # strip off brackets
	next unless ($gp->{$t}); # proto doesnt have this timer
	my $sp = $gp->{$t}->{"Time2"}; # sum of time squared graph pointer
	my $tp = $gp->{$t}->{"Time"}; # sum of time graph pointer
	my $np = $gp->{$t}->{"Try"}; # try graph pointer
	next unless (($sp) && ($tp) && ($gp));

	#print "Calculating std dev $t for $p\n";
	foreach $tm (keys %$tp) {
	    my $n = $np->{$tm};
	    my $tot = $sp->{$tm};
	    if ($np->{$tm}) {
		my $var = ($sp->{$tm} - (($tot * $tot) / $n)) / $n;
		print "$p->$t var < 0: Time2=$sp->{$tm} Time=$tot n=$n \@$tm\n"
		    if (($var < 0) && ($params{DEBUG}));
		
		$sp->{$tm} = ($var > 0) ? sqrt ($var) : 0.0;
	    } else {
		print "$p->$t: Time2=$sp->{$tm} Time=$tot w/0 tries \@$tm\n"
		    if (($tp->{$tm}) || ($sp->{$tm})); # internal error check
		$sp->{$tm} = 0;
	    }
	}
    }
}

($params{DEBUG}) && print "Doing final data reduction:\t", scalar (localtime), "\n";

# divide time graphs by number of tries
foreach $p (@protocols) {
    my $gp = $graphs{$p};
    foreach $n (@{$protocolFields{$p}}) { # all timers
	my $t = $n;	# dont modify original list
	if ($t =~ /^\[(\w+)\]$/) { $t = $1; } # strip off brackets
	next unless ($gp->{$t}); # proto doesnt have this timer

	my $tp = $gp->{$t}->{"Time"}; # time graph pointer
	my $np = $gp->{$t}->{"Try"}; # try graph pointer
	next unless (($tp) && ($gp)); # should never happen.

	#print "Ratioing $t for $p\n";
	foreach $tm (keys %$tp) {
	    if ($np->{$tm}) {
		$tp->{$tm} /= $np->{$tm} ;
	    } else {
		print "$p->$t: $tp->{$tm} time with 0 tries \@$tm\n"
		    if ($tp->{$tm}); # internal error check
		$tp->{$tm} = 0;
	    }
	}
    }
}

print "Saving combined graphs:\t", scalar (localtime), "\n";

# Dump graphs into file by protocol
foreach $p (@protocols) {
    my $gp = $graphs{$p};

    # Write out data
    fileBackup ("$resultdir/time-$p.csv"); # if processing as we go, backup
    open(CSV, ">$resultdir/time-$p.csv") # Summary of protocol over time
	|| die "Could not open $resultdir/time-$p.csv: $!\n";

    print CSV "time";
    foreach $t (@{$protocolFields{$p}}) {
	if ($t =~ /^\[(\w+)\]$/) { # Timer case, strip off brackets
	    foreach $f (@timerFieldsAll) {
		print CSV ",$p:$1:$f";
	    }
	} else {
	    print CSV ",$p:$t";
	}
    }
				# note: line print includes initial newline

    for (my $tm = $startTime; $tm <= $endTime; $tm++) {
	print CSV "\n", $tm-$startTime;
	foreach $t (@{$protocolFields{$p}}) {
	    if ($t =~ /^\[(\w+)\]$/) { # Timer case, strip off brackets
		foreach $f (@timerFieldsAll) {
		    if ($gp->{$1}->{$f}->{$tm}) {
			print CSV "," . $gp->{$1}->{$f}->{$tm};
		    } else {
			print CSV ",0";
		    }
		}
	    } else {
		if ($gp->{$t}->{$tm}) {
		    print CSV "," . $gp->{$t}->{$tm};
		} else {
		    print CSV ",0";
		}
	    }
	}
    }
    print CSV "\n";
    close(CSV);
    ($params{DEBUG}) && print "Wrote $resultdir/time-$p.csv\n";

}

return 1;
