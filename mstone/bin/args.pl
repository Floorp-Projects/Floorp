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

# This file does argument processing, file IO, and other utility routines

# Where the online docs live
#$mailstoneURL =
#    "http://home.netscape.com/eng/server/messaging/4.1/mailston/stone.htm";
#$mailstoneURL =
#    "http://developer.iplanet.com/docs/manuals/messaging.html";
#$mailstoneURL =
#    "http://docs.iplanet.com/docs/manuals/messaging/nms415/mailstone/stone.htm"
$mailstoneURL = "http:doc/MailStone.html";

# Subdirs for results (each under a timestamp dir).  Should just hardwire.
$tmpbase = "tmp";
$resultbase = "results";

# This holds everything about the test and system configuration
@workload = ();
# Setup the special CONFIG section
$params{"sectionTitle"} = "CONFIG";
$params{"sectionParams"} = "";
$params{"lineList"} = ();
push @workload, \%params;

# Get the lists discribing the data we process
do 'protoconf.pl' || die "$@\n";

%includedFiles = ();		# array of included files

# Utility functions
# Create a unique hash array. Programming Perl, 2nd edition, p291 (p217?)
package ArrayInstance;
sub new {
    my $type = shift;
    my %params = @_;
    my $self = {};
    return bless $self, $type;
}

package main;

# run a command in the background, return its PID
# Uses fork: will not run on NT in perl 5.004
# if the server is "localhost", ignore the rcmd part
# if stdin, stdout, and/or stderr is set, redirect those for the sub process
sub forkproc {
    my $rcmd = shift;
    my $server = shift;
    my $command = shift;
    my $stdin = shift;
    my $stdout = shift;
    my $stderr = shift;

    if (my $pid = fork()) {
	return $pid;		# parent
    }

    # rest of this is in the child
    if ($stdin) {		# redirect stdin if needed
	close (STDIN);
	open STDIN, "<$stdin"
	    || die "Couldn't open $stdin for input\n";
    }

    if ($stdout) {		# redirect stdout if needed
	close (STDOUT);
	open STDOUT, ">>$stdout"
	    || die "Couldn't open $stdout for output\n";
    }

    if ($stderr) {		# redirect stderr if needed
	close (STDERR);
	open STDERR, ">>$stderr"
	    || die "Couldn't open $stderr for output\n";
    }

    if ($server =~ /^localhost$/i) {
	exec $command;
	die "Coundn't exec $command:$!\n";
    } else {
	exec split (/\s+/, $rcmd), $server, $command;
	die "Coundn't exec $rcmd $server $command:$!\n";
    } 
}

# Relocate file to tmp directory (if it is in the results directory),
#  and put a ~ on the end of it.
# ASSUMES tmp and results are on the same partition (on NT, same drive).
# Usage: fileBackup (filename)
sub fileBackup {
    my $filename = shift;
    my $bfile = $filename;

    (-f $filename) || return 0;	# file doent exist
    $bfile =~ s/$resultbase/$tmpbase/; # move to tmp
    $bfile .= "~";		# indicate that this is a backup
    (-f $bfile) && unlink ($bfile);
    #print "Backing up $filename to $bfile\n"; # DEBUG
    rename ($filename, $bfile) || unlink ($filename);
}

# Insert text into a file after a tagline
# fileInsertAfter (filename, tagstring, newtext)
sub fileInsertAfter {
    my $filename = shift || die "fileInsertAfter: missing filename";
    my $tagline = shift || die "fileInsertAfter: missing tagline";
    my $newtext = shift || die "fileInsertAfter: missing text";
    my $foundit = 0;

    open(OLD, "<$filename") ||
	open(OLD, "gunzip -c $filename |") ||
	    die "fileInsertAfter: Could not open input $filename: $!";
    open(NEW, ">$filename+") ||
	die "fileInsertAfter: Could not open output $filename+: $!";

    while (<OLD>) {
	print NEW $_;		# copy (including tagline)

	next unless (/$tagline/); # matched tagline

	print NEW $newtext;	# insert new text
	$foundit++;
	last;			# only change first occurance
    }

    if ($foundit) {		# copy rest of file
	while (<OLD>) {
	    print NEW $_;
	}
    }

    close (OLD);
    close (NEW);
    if ($foundit) {
	fileBackup ($filename);
	rename ("$filename+", "$filename");
	#print "Updated $filename\n"; # DEBUG
	return $foundit;
    } else {
	($params{DEBUG}) && print "No change to $filename\n"; # DEBUG
	unlink ("$filename+");
	return 0;
    }
}

# Do text for text replacements in a file.
# Perl wildcards are automatically quoted.
# fileReplace (filename, matchPat, oldtext, newtext)
sub fileReplaceText {
    my $filename = shift || die "fileReplaceText: missing filename";
    my $tagline = shift || die "fileReplaceText: missing tagline ($filename)";
    my $oldtext = shift;
    my $newtext = shift;
    my $foundit = 0;

    return if ($newtext eq "");	# nothing to do
    return if ($oldtext eq "");	# nothing can be done

    open(OLD, "<$filename") ||
	open(OLD, "gunzip -c $filename |") ||
	    die "fileReplaceText: Could not open input $filename: $!";
    open(NEW, ">$filename+") ||
	die "fileReplaceText: Could not open output $filename+: $!";

    $oldtext =~ s/([][{}*+?^.\/])/\\$1/g; # quote regex syntax

    while (<OLD>) {
	if (/$tagline/i) { # matched tagline
	    $foundit++;
	    s/$oldtext/$newtext/; # do the replace
	}
	print NEW $_;
    }

    close (OLD);
    close (NEW);
    if ($foundit) {
	fileBackup ($filename);
	rename ("$filename+", "$filename");
	#print "Updated $filename\n"; # DEBUG
	return $foundit;
    } else {
	($params{DEBUG}) && print "No change to $filename\n"; # DEBUG
	unlink ("$filename+");
	return 0;
    }
}

# copy a file to a new name.  Handles possible compression.  OS independent.
# fileCopy (filename, newname)
sub fileCopy {
    my $filename = shift || die "fileReplaceText: missing filename";
    my $newname = shift || die "fileReplaceText: missing newname ($filename)";

    open(OLD, "<$filename") ||
	open(OLD, "gunzip -c $filename |") ||
	    die "fileReplaceText: Could not open input $filename: $!";
    open(NEW, ">$newname") ||
	die "fileReplaceText: Could not open output $newname: $!";

    while (<OLD>) {		# copy it
	print NEW $_;
    }

    close (OLD);
    close (NEW);
    return 0;
}

# display a file to STDOUT.  Handles possible compression
sub fileShow {
    my $filename = shift || die "fileShow: missing filename";
    open(SHOWIT, "<$filename") ||
	open(SHOWIT, "gunzip -c $filename.gz |") ||
	    die "fileShow: Couldn't open $filename: $!";
    while (<SHOWIT>) { print; }
    close(SHOWIT);
}

# sub function to figure time extents
# (start, end) = dataMinMax counterName \@protocols oldstarttime oldendtime
# Use 0 for uninitialized start or end
sub dataMinMax {
    my $name = shift;
    my $protos = shift;
    my $start = shift;
    my $end = shift;
    
    # make global
    # create the plot script and data files
    # Figure out the encompassing time extent
    foreach $p (@$protos) {	# create the plot data files
	my @times = sort numeric keys %{ $graphs{$p}->{$name}};
	if ($#times <= 0) {
	    next;
	}
	if (($start == 0) || ($times[0] < $start)) {
	    $start = $times[0];
	}
	if (($end == 0) || ($times[0] > $end)) {
	    $end = $times[$#times];
	}
    }
    #printf ("Data $name start=$start end=$end (%d points)...\n",
	#    $end - $start);
    return ($start, $end);
}

# simple function to formatted a number into n, n K, n M, or n G
sub kformat {
    my $n = shift;
    my $r = "";
    if ($n > (1024*1024*1024)) {
	$r = sprintf "%.2fG", $n / (1024*1024*1024);
    } elsif ($n > (1024*1024)) {
	$r = sprintf "%.2fM", $n / (1024*1024);
    } elsif ($n > 1024) {
	$r = sprintf "%.2fK", $n / 1024;
    } else {
	$r = sprintf "%d ", $n;
    }
    return $r;
}

# simple function to formatted a time into Ns, Nms, or Nus
# the goal is to make a table of timss uncluttered and easy to read
# I dont convert to minutes or hours because the non-1000x multipliers
#   are hard to back solve in your head for comparisons
sub tformat {
    my $n = shift;
    my $r = "";
    if ($n == 0.0) {
	$r = "0.0";		# make exactly 0 explicit
    } elsif ($n < 0.001) {
	$r = sprintf "%.2fus", $n * 1000 * 1000;
    } elsif ($n < 1.0) {
	$r = sprintf "%.2fms", $n * 1000;
    } elsif ($n >= 1000.0) {
	$r = sprintf "%.0fs", $n;
    } elsif ($n >= 100.0) {
	$r = sprintf "%.1fs", $n;
    } else {
	$r = sprintf "%.3fs", $n;
    }
    return $r;
}

#Usage: commify (1234567) returns 1,234,567
sub commify {			# perl cookbook p64-65
    my $text = reverse $_[0];
    $text  =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text;
}

# subroutine to enable numeric sorts.  Programming Perl p218
# Use: sort numeric ...
sub numeric { $a <=> $b; }

# on NT, turn slash to backslash, then print.  Else print.
sub pathprint {
    my $str = shift;
    $str =~ s!/!\\!g if ($params{NT});	# turn slash to back slash
    print $str;
}
    
# figureTimeNumber number
# Given an number like: 60m, 1h, 100s, 4d, 200
# Return 60, 1, 100, 4, 200
sub figureTimeNumber {
    my $arg = shift;

    ($arg =~ /([0-9]+)(s|sec|second|seconds|m|min|minute|minutes|h|hr|hour|hours|d|day|days)$/i)
	&& return $1;
    return $arg;			# return default
}

# figureTimeUnits number, default
# Given an number like: 60m, 1h, 100s, 4d
# Return a string of minutes, hours, seconds, days
# Else return the second argument
sub figureTimeUnits {
    my $arg = shift;

    ($arg =~ /(s|sec|second|seconds)$/i)	&& return "seconds";
    ($arg =~ /(m|min|minute|minutes)$/i)	&& return "minutes";
    ($arg =~ /(h|hr|hour|hours)$/i)	&& return "hours";
    ($arg =~ /(d|day|days)$/i)		&& return "days";

    return shift;		# return default
}

# figureTimeSeconds number, defaultUnits
# Given an number like: 60m, 2h, 100s, 4d
# Return 60*60, 2*60*60, 100, 4*24*60*60
sub figureTimeSeconds {
    my $arg = shift;

    ($arg =~ /([0-9]+)(s|sec|second|seconds)$/i)	&& return $1;
    ($arg =~ /([0-9]+)(m|min|minute|minutes)$/i)	&& return (60*$1);
    ($arg =~ /([0-9]+)(h|hr|hour|hours)$/i)	&& return (60*60*$1);
    ($arg =~ /([0-9]+)(d|day|days)$/i)		&& return (24*60*60*$1);

    if ($_) { 
	my $def = shift;
	return $arg * figureTimeSeconds ("1$def"); # return scaled by default
    } else {
	return $arg;		# return it
    }
}

# BACK COMPATIBILITY (everything now in the workload file)
# read the testbed conf file, convert to workload sections
# machine, how many processes, how many threads/proc, arch
# only the first 2 fields are required.  Lines starting with # are ignored.
# You can include other files using <include conf/filename.tbd>
# exampe:
#	client1  5 10 SunOS5.5.1
sub readTestbedFile {
    my $filename = shift;
    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	print "Testbed $filename skipped, clients read in workload\n";
	return 1;			# clients already read in workload
    }

    my $level = 0;
    if ($_) {
	$level = 1 + shift;
	die "Too many nested includes ($level) in $filename!"
	    unless ($level < 100);
    }
    my $handle = "$filename$level";
    open($handle, "<$filename") ||
	open($handle, "gunzip -c $filename.gz |") ||
	    die "Couldn't open testbed $filename: $!";

    while(<$handle>) {
	chomp;

	s/#.*//;		# strip any comments from line

	m/^\s*$/o &&  next; 	# continue if blank line

	# handle include statement
	if (m/^<(include|INCLUDE)\s+([^\s]+)\s*>/o) {
	    #print "Including $2 from $filename\n";
	    readTestbedFile ($2, $level) || die;
	    next;
	}

	# get the server name and number of processes 
	my @line = split(/\s+/);
				# create CLIENT entry in workload
	my $sparm = ArrayInstance->new();
	if ($line[1]) {
	    $sparm->{"sectionTitle"} = "CLIENT";
	    $sparm->{"sectionParams"} = "HOSTS=$line[0]";
	    $sparm->{"PROCESSES"} = $line[1];
	    $sparm->{"THREADS"} = $line[2] if ($line[2]);
	    $sparm->{"ARCH"} = $line[3] if ($line[3]);
	} else {
	    $sparm->{"sectionTitle"} = "MONITOR";
	    $sparm->{"sectionParams"} = "HOSTS=$line[0]";
	    $sparm->{"COMMAND"} = $line[2];
	}
	($params{DEBUG})
	    && print "<$sparm->{sectionTitle} $sparm->{sectionParams}>\n";
	push @workload, $sparm;
    }
    close ($handle);
}

# BACK COMPATIBILITY (everything now in the saved workload file)
# This is now only needed to process mailstone4.1 runs
sub readConfigFile {
    my $filename = shift;
    open(CONFIG, "<$filename") ||
	open(CONFIG, "gunzip -c $filename.gz |") ||
	    die "Couldn't open config file $filename: $!";

    while(<CONFIG>) {
	chomp;

	s/#.*//;		# strip any comments from line

	m/^\s*$/o && next; 	# continue if blank line

	# get the property and value
	my @line = split(/=/);
	$params{$line[0]} = $line[1];
    }
    close CONFIG;
}

# read the workload file and store it as a list of hashes
# Each hash always has the fields: sectionTitle and sectionParams
# usage: readWorkloadFile filename, \@list
sub readWorkloadFile {
    my $filename = shift || die "readWorkloadFile: Missing file name";
    my $plist = shift || die "readWorkloadFile: Missing return list";
    my $level = 0;		# file inclusion level
    my @handles;

    my $fh = "$filename$level";

    ($params{DEBUG}) && print "Reading workload from $filename.\n";
    open($fh, "<$filename") ||
	open($fh, "gunzip -c $filename.gz |") ||
	    die "readWorkloadFile Couldn't open testbed $filename: $!";
    $includedFiles{$filename} = 1; # mark file as included

    my $sparm=0;
    my $conline = "";
    
    while($fh) {
	while(<$fh>) {
	    s/#.*//;		# strip any comments from line (quoting?)
	    s/\s*$//;		# strip trailing white space
	    if ($conline) {	# utilize line continue
		$_ = $conline . "\\\n" . $_;
		$conline = "";
	    }
	    if (m/\\$/o) {	# check for quoted line continue
		s/\\$//;	# 
		$conline = $_;
		next;
	    }
	    s/^\s*//;		# strip initial white space
	    m/^$/o &&  next;	# continue if blank line

	    # handle include and includeOnce statements
	    if ((m/^<(include)\s+(\S+)\s*>/i)
		|| (m/^<(includeonce)\s+(\S+)\s*>/i)) {
		my $incfile = $2;
		if (($1 =~ m/^includeonce/i) && ($includedFiles{$incfile})) {
		    ($params{DEBUG})
			&& print "readWorkloadFile:includeOnce $incfile already read.\n";
		    next;
		}
		($params{DEBUG})
		    && print "readWorkloadFile include $incfile from $filename.\n";
		$includedFiles{$incfile} = 1; # mark file
		push @handles, $fh; # push current handle on to stack
		if ($level++ > 99) { # check recursion and make handles unique
		    die "readWorkloadFile: include level too deep: $filename $level\n";
		}
		$fh = "$incfile$level";
		open($fh, "<$incfile") ||
		    open($fh, "gunzip -c $incfile.gz |") ||
			die "readWorkloadFile Couldn't open testbed file $incfile: $!";
		$filename = $incfile;	# for error messages
		next;
	    }

	    if (m!^</(\w+)>$!o) { # end of section
		my $end = $1;
		unless ($sparm->{"sectionTitle"} =~ /$end/i) {
		    die "readWorkloadFile Mismatched section $filename: $. '$sparm->{sectionTitle}' '$end'\n";
		    return 0;
		}
		($params{DEBUG}) && print "</$sparm->{sectionTitle}>\n";
		push @$plist, $sparm;
		$sparm = 0;
		next;
	    }

	    if (m!^<(\w+)\s*(.*)>$!o) { # start of section
		my $sec = $1;
		my $more = $2;

		if ($sparm) {
		    die "readWorkloadFile Missing section end $filename: $. '$sparm->{sectionTitle}'\n";
		}
		if ($sec =~ /CONFIG/i) { # special case, map to existing global
		    $sparm = \%params;
		} elsif ($sec =~ /DEFAULT/i) { # special case, only one DEFAULT
		    if ($defaultSection) { # use existing defaultSection
			$sparm = $defaultSection;
		    } else {	# create a new one
			$sparm = ArrayInstance->new();
			$sparm->{"sectionTitle"} = uc $sec; # ignore case
			$sparm->{"lineList"} = ();
			$defaultSection = $sparm;
		    }
		} else {
		    $sparm = ArrayInstance->new();
		    $sparm->{"sectionTitle"} = uc $sec; # ignore case
		    $sparm->{"lineList"} = ();
		}
		$sparm->{"sectionParams"} = $more; # take newest more info
		($params{DEBUG})
		    && print "<$sparm->{sectionTitle} $sparm->{sectionParams}>\n";
		next;
	    }

	    # must be in a section, get parameters
	    unless ($sparm) {
		die "readWorkloadFile Entry encountered outside a section $filename: $. $_\n";
		return 0;
	    }
	    my ($nm, $val) = split (/[\s=]+/, $_, 2);
	    $nm = uc $nm;	# ignore case
	    ($params{DEBUG}) && print "    $nm = $val\n";
	    if ($nm =~ /ACCOUNTFORMAT/) { # BACK COMPATIBILITY
		print "WARNING: 'accountFormat' is obsolete.  Use 'addressFormat' and 'loginFormat'\n";
		$sparm->{"addressFormat"} = $val;
		push @{$sparm->{"lineList"}}, "addressFormat $val";
		$val =~ s/@.+$//; # strip at and everything after
		$sparm->{"loginFormat"} = $val;
		push @{$sparm->{"lineList"}}, "loginFormat $val";
		next;
	    } elsif ($nm =~ /NUMACCOUNTS/) { # BACK COMPATIBILITY
		print "WARNING: 'numAccounts' is obsolete.  Use 'numAddresses' and 'numLogins'\n";
		$sparm->{"numAddresses"} = $val;
		push @{$sparm->{"lineList"}}, "numAddresses $val";
		$sparm->{"numLogins"} = $val;
		push @{$sparm->{"lineList"}}, "numLogins $val";
		next;
	    } elsif ($nm =~ /BEGINACCOUNTS/) { # BACK COMPATIBILITY
		print "WARNING: 'beginAccounts' is obsolete.  Use 'firstAddress' and 'firstLogin'\n";
		$sparm->{"firstAddress"} = $val;
		push @{$sparm->{"lineList"}}, "firstAddress $val";
		$sparm->{"firstLogin"} = $val;
		push @{$sparm->{"lineList"}}, "firstLogin $val";
		next;
	    } 
	    push @{$sparm->{"lineList"}}, $_; # save lines in original order
	    $sparm->{$nm} = $val;
	    next;
	}
	close ($fh);
	$fh = pop @handles || last; # empty include stack
	$filename = $fh;
	$sparm = 0;		# can only include whole sections
    }
    return 1;			# success
}

# Write out a workload list to a file
# Optionally, pass in a list of sectionTitle's it should ignore
# usage: writeWorkloadFile filename \@list [\@skipList]
sub writeWorkloadFile {
    my $filename = shift || die "writeWorkloadFile: Missing file name";
    my $plist = shift || die "writeWorkloadFile: Missing return list";
    my $skip = shift;
    my @skipH;
    my $configSeen = 0;
    my $defaultSeen = 0;
    my @paramH;

    if ($skip) {
	foreach $s (@$skip) {	# turn list into a hash
	    $skipH{(uc $s)} = $s; # fix case for index
	}
    }
    foreach $s (@workloadParameters) { # turn list into a hash
	$paramH{(uc $s)} = $s;	# fix case for index
    }

    ($params{DEBUG}) && print "Writing workload to $filename.\n";
    unless (open(WORKOUT, ">$filename")) {
	die "Couldn't open testbed $filename: $!";
    }
    
    foreach $sparm (@$plist) {	# each hash reference in the list
	if (($skip)
	    && ($skipH{$sparm->{"sectionTitle"}})) {
	    #($params{DEBUG}) &&
	    #print "Skipping section $sparm->{sectionTitle}\n";
	    next;
	}
	
	# all CONFIG,DEFAULT sections point to the same hash, output once only
	if ($sparm->{"sectionTitle"} =~ /^CONFIG$/) {
	    next if $configSeen;
	    $configSeen++;
	}
	if ($sparm->{"sectionTitle"} =~ /^DEFAULT$/) {
	    next if $defaultSeen;
	    $defaultSeen++;
	}
	if ($sparm->{sectionParams}) { # write section with extra args
	    print WORKOUT "<$sparm->{sectionTitle} $sparm->{sectionParams}>\n";
	} else {
	    print WORKOUT "<$sparm->{sectionTitle}>\n";
	}
	if ($sparm->{"sectionTitle"} =~ /^(CONFIG|CLIENT)$/) {
	    # for Config or Client, output the hash  to get computed config
	    foreach $k (sort keys %$sparm) { # output each parameter
		# skip sectionTitle and sectionParams
		($k =~ /^(sectionTitle|sectionParams|lineList)$/) && next;
		printf WORKOUT "  %s\t%s\n",
		($paramH{$k}) ? $paramH{$k} : $k,
		$sparm->{$k};
	    }
	} else {	# write out the line list
	    foreach $l (@{$sparm->{"lineList"}}) {
		print WORKOUT "  $l\n";
	    }
	}
	print WORKOUT "</$sparm->{sectionTitle}>\n\n";
    }
    close WORKOUT;
}

# Usage: getClientFilename hostname section
sub getClientFilename  {
    my $cli = shift || die "Missing client name";
    my $section = shift || die "Missing section hash";
    return "$tmpdir/$cli-$section->{GROUP}.out"
	if ($params{USEGROUPS} && $section->{GROUP});
    return "$tmpdir/$cli.out"
}

sub setConfigDefaults {		# set CONFIG defaults
    # These are set after writing out the test copy to avoid clutter

    # Path to gnuplot executable
    $params{GNUPLOT}="gnuplot/gnuplot"
	unless ($params{GNUPLOT});

    # This is the directory the client lives in
    $params{TEMPDIR} = "/var/tmp"
	unless($params{TEMPDIR});

    # Set default remote shell
    #$params{RSH} = "/usr/bin/rsh"
    $params{RSH} = "ssh"
	unless($params{RSH});

    # Set default remote copy
    #$params{RCP} = "/usr/bin/rcp"
    $params{RCP} = "scp"
	unless($params{RCP});

    # Size of generated gifs
    $params{CHARTHEIGHT} = 480
	unless($params{CHARTHEIGHT});
    $params{CHARTWIDTH} = 640
	unless($params{CHARTWIDTH});
    $params{CHARTPOINTS} = int (($params{CHARTWIDTH}-60)*0.8)
	unless($params{CHARTPOINTS});


    # The name of the remote executable
    $params{CLIENTCOMMAND} = "mailclient"
	unless ($params{CLIENTCOMMAND});

    # Set default monitoring command
    $params{MONITORCOMMAND} = "vmstat %f"
	unless($params{MONITORCOMMAND});

    # Set default switches to makeusers
    $params{MAKEUSERSARGS} = "-4"
	unless ($params{MAKEUSERSARGS});

    # Figure out @protocols, this sets the report order
    @protocols = ();
    {
	my %skipH;
	foreach $s (@nonProtocolSections) { # turn list into a hash
	    #print "$s ";
	    $skipH{(uc $s)} = $s; # fix case for index
	}
	print "\n";
	foreach $sparm (@workload) {	# each hash reference in the list
	    next if ($skipH{$sparm->{"sectionTitle"}});

	    ($params{DEBUG}) &&
		print "Found protocol ". $sparm->{"sectionTitle"} . "\n";

	    push @protocols, $sparm->{"sectionTitle"};
				# add to skip list so only added once
	    $skipH{(uc $sparm->{"sectionTitle"})} = $sparm->{"sectionTitle"};
	}
    }
    @protocolsAll = @protocols;
    push @protocolsAll, "Total";

    # figure out the graphs ???
}

sub parseArgs {			# get args
    while (@ARGV) {
	my $arg = shift(@ARGV);

	if ($arg =~ /^-a$/i) {	# was undocumented feature in 4.1
	    $params{ADDGRAPHS} = shift(@ARGV); # extra graphs
	    next;
	}
	if ($arg =~ /^-b$/i) {
	    $params{TITLE} = shift(@ARGV); # banner
	    next;
	}

	# BACK COMPATIBILITY (everything now in the saved workload file)
	if ($arg =~ /^-c$/i) {	# config file, read when encountered
	    my $configFile = shift(@ARGV);
	    readConfigFile ($configFile);
	    next;
	}

	if ($arg =~ /^-d$/i) {
	    $params{DEBUG}++;	# Debug
	    next;
	}

	if ($arg =~ /^-h$/i) {	# Help
	    print "Usage: -w workfile [-t time] [-r ramptime] [-l load] [-v] [-d]\n";
	    print "\t[-b banner] [-n notes] [-s sysconfigfile] [-a add_graphs_file]\n";
	    print "\t[-c configfile] [-m machinefile] [-z] [PARAM=value]...\n";

	    die "Usage";
	}

	if ($arg =~ /^-l$/i) {	# "load", FIX: naming conventions
	    $params{CLIENTCOUNT} = shift(@ARGV); # desired client count
	    next;
	}

	# BACK COMPATIBILITY (everything now in the saved workload file)
	if ($arg =~ /^-m$/i) {
	    $params{TESTBED} = shift(@ARGV);	# testbed machines file
	    next;
	}

	if ($arg =~ /^-n$/i) {
	    $params{COMMENTS} = shift(@ARGV); # notes
	    next;
	}

	if ($arg =~ /^-r$/i) {
	    $params{RAMPTIME} = shift(@ARGV); # ramptime
	    next;
	}
	if ($arg =~ /^-s$/i) {
	    $params{SYSCONFIG} = shift(@ARGV); # system config html file
	    next;
	}
	if ($arg =~ /^-t$/i) {
	    $params{TIME} = shift(@ARGV); # test time
	    next;
	}

	if ($arg =~ /^-v$/i) {
	    $params{VERBOSE} = 1;	# verbose mode
	    next;
	}

	if ($arg =~ /^-w$/i) {	# workload file (may occur multiple times)
	    my $f = shift(@ARGV);
	    readWorkloadFile ($f, \@workload) || die "Error reading workload: $@\n";
	    $params{WORKLOAD} = $f;
	    next;
	}

	if ($arg =~ /^-z$/i) {
	    $params{NT} = 1;		# NT mode
	    next;
	}

	# any other CONFIG parameter: FIELD=value
	if ($arg =~ /^(\w+)=(\S.*)$/) {
	    my $field = uc $1;
	    $params{$field} = $2;
	    next;
	}
	die "Unknown argument '$arg'";
    }

    if ($params{NT}) {		# should use Cwd module
	$cwd = `cd`;		# NT get current directory
	$cwd = `pwd` unless ($cwd); # in case we are really on UNIX
    } else {
	$cwd = `pwd`;		# in case we are really on UNIX
    }
    chomp $cwd;		# strip NL
}

return 1;
