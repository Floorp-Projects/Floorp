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
# Copyright (C) 1999-2000 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s):	Dan Christian <robodan@netscape.com>
#			Marcel DePaolis <marcel@netcape.com>
#			Jim Salter <jsalter@netscape.com>
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

# usage: setup.pl setup|cleanup|checktime|timesync -m machine_file
# message files are expected to be in ./data/ and end with ".msg"

print "Netscape Mailstone.\nCopyright (c) 1997-2000 Netscape Communications Corp.\n";

$mode = shift;			# mode must be first

# this parses the command line for -m machinefile
# also sets many defaults
do 'args.pl'|| die $@;
parseArgs();			# parse command line

setConfigDefaults();		# setup RSH and RCP

$cpcmd = "cp";			# copy files... dir
$rmcmd = "rm -f";		# remove files...

die "Must specify workload file" unless (@workload);

# Add or change client machines
sub configClients {
    print "\n    You can enter multiple machines like this: host1,host2\n";
    my @d = <bin/*/bin>;
    if (@d) {
	my @d2;
	foreach (@d2 = @d) { s/^bin\/// }
	foreach (@d = @d2) { s/\/bin$// }
	print "    These OS versions are available:\n@d\n";
    }

    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	my $arch = "default OS";
	$arch = $section->{ARCH} if ($section->{ARCH});
	print "\nWhat is the name of the client(s) for $arch [$slist]: ";
	my $ans = <STDIN>; chomp $ans;
	if ($ans) {
	    $ans =~ s/\s//g;	# strip any whitespace
	    fileReplaceText ($params{WORKLOAD}, "<CLIENT", $slist, $ans);
	}
    }
    while (1) {
	print "\nWhat additional client(s) [none]: ";
	my $ans = <STDIN>; chomp $ans;
	last unless ($ans);	# done
	last if ($ans =~ /^none$/i);
	$ans =~ s/\s//g;	# strip any whitespace
	my $block = "\n<CLIENT HOSTS=$ans>\n";

	print "What OS type [default]: ";
	my $ans = <STDIN>; chomp $ans;
	$block .= "  Arch\t$ans\n" if ($ans && !($ans =~ /^default$/i));

	$block .= "</CLIENT>\n";
	fileInsertAfter ($params{WORKLOAD}, "^</CLIENT>", $block);
    }
}

# Create a user ldif file
sub configUserLdif {
    my $name = "conf/$defaultSection->{SERVER}.ldif";
    print "\nWhat file to you want to create [$name]? ";
    $ans = <STDIN>; chomp $ans;
    $name = $ans if ($ans);

    my $mode = "users";
    print "\nDo you want to create a broadcast account [y]? ";
    $ans = <STDIN>;
    $mode .= " broadcast" unless ($ans =~ /^n/i);

    my $basedn = $defaultSection->{SERVER};	# pick a default
    $basedn =~ s/^.*?\.//;	# strip off before first dot
    $basedn = "o=$basedn";

    print "\nWhat is LDAP base DN [$basedn]? ";
    $ans = <STDIN>; chomp $ans;
    $basedn = $ans if ($ans);

    my $args = $params{MAKEUSERSARGS};

    print "\n    Common additional makeusers arguments:\n";
    print "\t-s storeName -x storeCount \tMultiple store partitions\n";
    print "\t[-3|-4]                    \tConfigure for NSMS 3.x or 4.x\n";
    print "Any extra arguments to makeusers [$args]? ";
    $ans = <STDIN>; chomp $ans;
    $args = $ans if ($ans);

    my $perlbin = "/usr/bin/perl";

    $params{DEBUG} &&
	print "$perlbin -Ibin -- bin/makeusers.pl $mode -w $params{WORKLOAD} -b '$basedn' -o $name $args\n";

    print "\nGenerating $name (this can take a while)\n";
    system "$perlbin -Ibin -- bin/makeusers.pl $mode -w $params{WORKLOAD} -b '$basedn' -o $name $args"
	|| warn "$@";
    print "LDIF generation complete.  See $name\n";
    print "\tSee the manual or INSTALL to create users using the LDIF file.\n";
}

# This uses a match pattern plus text to text replacements.  
# Could make all changes and then write out new workload
# 	You would have to be carefull about sections with multi-line support.
sub configWorkload {
    my $ans;

    print "\nWhat is the name of the mail host [$defaultSection->{SERVER}]: ";
    $ans = <STDIN>; chomp $ans;
    if ($ans) {
	fileReplaceText ($params{WORKLOAD},
			 "(SERVER|SMTPMAILFROM|ADDRESSFORMAT)", 
			 $defaultSection->{SERVER}, $ans);
	$defaultSection->{SERVER} = $ans; # needed for ldif generation
    }
    print "\nWhat is the user name pattern [$defaultSection->{LOGINFORMAT}]: ";
    $ans = <STDIN>; chomp $ans;
    if ($ans) {
	fileReplaceText ($params{WORKLOAD},
			 "(LOGINFORMAT|ADDRESSFORMAT)",
			 $defaultSection->{LOGINFORMAT}, $ans);
	$ans =~ s/%ld/0/;	# create smtpMailFrom user
	my $olduser = $defaultSection->{SMTPMAILFROM};
	$olduser =~ s/@.*$//;	# strip off after @
	fileReplaceText ($params{WORKLOAD},
			 "SMTPMAILFROM",
			 $olduser, $ans);
    }

    print "\nWhat is the password pattern [$defaultSection->{PASSWDFORMAT}]: ";
    $ans = <STDIN>; chomp $ans;
    fileReplaceText ($params{WORKLOAD}, "PASSWDFORMAT", 
		     $defaultSection->{PASSWDFORMAT}, $ans);

    $defaultSection->{NUMLOGINS} = 100 unless ($defaultSection->{NUMLOGINS});
    print "\nHow many users [$defaultSection->{NUMLOGINS}]: ";
    $ans = <STDIN>; chomp $ans;
    fileReplaceText ($params{WORKLOAD}, "(NUMADDRESSES|NUMLOGINS)",
		     $defaultSection->{NUMLOGINS}, $ans);

    $defaultSection->{FIRSTLOGIN} = 0 unless ($defaultSection->{FIRSTLOGIN});
    print "\nWhat is the first user number [$defaultSection->{FIRSTLOGIN}]: ";
    $ans = <STDIN>; chomp $ans;
    fileReplaceText ($params{WORKLOAD}, "(FIRSTADDRESS|FIRSTLOGIN)",
		     $defaultSection->{FIRSTLOGIN}, $ans);

    unless ($params{NT}) {
	configClients ();
    }

    print "\nDo you want to view the edited $params{WORKLOAD} [y]? ";
    $ans = <STDIN>;
    unless ($ans =~ /^n/i) {
	print "Here is the edited $params{WORKLOAD}:\n\n";
	fileShow ($params{WORKLOAD});
	print "\n";
    }

    print "\nDo you want to generate a user LDIF file [y]? ";
    $ans = <STDIN>;
    unless ($ans =~ /^n/i) {
	configUserLdif ();
    }
}

# See if license file has been displayed
if (($mode ne "cleanup") && (! -f ".license" )) {
    fileShow ("LICENSE");
    print "\nDo you agree to the terms of the license? (yes/no) ";
    my $ans = <STDIN>;
    print "\n";
    unless ($ans =~ /^yes$/i) {
	print "License not agreed to.\n";
	exit 0;
    }
    my ($sec, $min, $hour, $mday, $mon, $year) = localtime;
    open (LIC, ">.license");
    printf LIC "%04d$mon$mday$hour$min\n", $year+1900;
    close (LIC);
}

if ($mode eq "config") {	# re-run config
    configWorkload ();

    print "\nMake any additional changes to $params{WORKLOAD} and then re-run 'setup'\n";
    exit 0;
} elsif ($mode ne "cleanup") {	# check if configured
    my $unconf = 0; # see if default values are in use
    foreach $section (@workload) {
	($section->{SERVER})
	    && ($section->{SERVER} =~ /example\.com$/)
		&& $unconf++;
	($section->{SMTPMAILFROM})
	    && ($section->{SMTPMAILFROM} =~ /example\.com$/)
		&& $unconf++;
	($section->{ADDRESSFORMAT})
	    && ($section->{ADDRESSFORMAT} =~ /example\.com$/)
		&& $unconf++;
	last if ($unconf > 0);
    }
    if ($unconf > 0) {
	print "Server has not been configured (example.com is an invalid address).\n";
	print "Do you want to setup a simple configuration now [y]?";
	my $ans = <STDIN>;
	if ($ans =~ /^n/i) {
	    print "Re-run setup when you have edited the configuration.\n";
	    exit 0;
	}
	configWorkload ();

	print "\nMake any additional changes to $params{WORKLOAD} and then re-run 'setup'\n";
	exit 0;
    }
}

if ($mode eq "timesync") {
    if ($params{NT}) {
	print "Timesync has no effect on NT\n";
	exit 0;
    }
    my ($sec, $min, $hour, $mday, $mon, $year) = localtime;
    $mon += 1;			# adjust from 0 based to std
    $systime = sprintf ("%02d%02d%02d%02d%04d.%02d",
		     $mon, $mday, $hour, $min, 1900+$year, $sec);
} elsif ($mode eq "checktime") {
    if ($params{NT}) {		# if running on NT, then only single client
	print "Checktime not needed on NT\n";
	exit 0;
    }
    mkdir ("$resultbase", 0775);
    mkdir ("$tmpbase", 0775);
    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	foreach $cli (split /[\s,]/, $slist) {
	    open MAKEIT, ">$tmpbase/$cli.tim";
	    close MAKEIT;
	}
    }
} elsif (($mode eq "setup") || ($mode eq "cleanup")) {
    @msgs = <data/*.msg>;
    foreach (@files = @msgs) { s/data\/// }
    print "Found these message files:\n@files\n\n";
    if ($params{NT}) {		# handle NT localhost here
	exit 0 if ($mode =~ /cleanup$/);
	my $clipath = "bin/WINNT4.0/bin/mailclient.exe";
	print "Copying $clipath and message files to $cli\n";
	system "copy $clipath $params{TEMPDIR}";
	foreach $f (@files) {
	    system "copy $f $params{TEMPDIR}";
	}
	exit 0;			# without perl:fork, no more to do
    }
}

# iterate over every client in the testbed, complete the cmd and rsh
foreach $section (@workload) {
    next unless ($section->{sectionTitle} =~ /CLIENT/o);
    my $slist = $section->{sectionParams};
    $slist =~ s/HOSTS=\s*//; # strip off initial bit
    foreach $cli (split /[\s,]/, $slist) {
	my $rsh = ($section->{RSH}) ? $section->{RSH} : $params{RSH};
	my $rcp = ($section->{RCP}) ? $section->{RCP} : $params{RCP};
	my $tempdir;
	if ($section->{TEMPDIR}) {
	    $tempdir = $section->{TEMPDIR};
	} elsif ($params{TEMPDIR}) {
	    $tempdir = $params{TEMPDIR};
	}
	# most time critical first
	if ($mode eq "timesync") {
	    next if ($cli =~ /^localhost$/i); # dont reset our own time
	    # run all these in parallel to minimize skew
	    next if ($section->{ARCH} eq "NT4.0");
	    forkproc ($rsh, $cli, "date $systime");
	}

	elsif ($mode eq "checktime") {
	    # run all these in parallel to minimize skew
	    forkproc ($rsh, $cli, ($section->{ARCH} eq "NT4.0")
		      ? "time" : "date",
		      "/dev/null", "$tmpbase/$cli.tim");
	}

	elsif ($mode eq "setup") {
	    my ($clibin) = split /\s/, (($section->{COMMAND})
					? $section->{COMMAND}
					: $params{CLIENTCOMMAND});
	    my $clipath = "bin/$clibin";
	    if ($section->{ARCH}) {
		if (-x "bin/$section->{ARCH}/bin/$clibin") {
		    $clipath="bin/$section->{ARCH}/bin/$clibin";
		} else {
		    print "Requested OS $section->{ARCH} $cli not found. Using default.\n";
		}
	    }
	    my $rdir = ($tempdir) ?  "$tempdir/" : ".";
	    # chmod so that the remote files can be easily cleaned up
	    my $rcmd = "chmod g+w @files $clibin; uname -a";
	    $rcmd = "cd $tempdir; " . $rcmd if ($tempdir);
	    $rdir =~ s!/!\\!g if ($section->{ARCH} eq "NT4.0");
	    if ($cli =~ /^localhost$/i) {
		die "TEMPDIR must be set for 'localhost'\n"
		    unless ($tempdir);
		die "Invalid local NT copy.  Should never get here.\n"
		    if ($section->{ARCH} eq "NT4.0"); # should never happen
		print "Copying $clipath and message files to $rdir\n";
		system ("$cpcmd @msgs $clipath $rdir");
		system ($rcmd);
	    } else {
		print "$rcp $clipath @msgs $cli:$rdir\n" if ($params{DEBUG});
		print "Copying $clipath and message files to $cli:$rdir\n";
		system (split (/\s+/, $rcp), $clipath, @msgs, "$cli:$rdir");
		next if ($section->{ARCH} eq "NT4.0"); # chmod not valid
		print "rcmd='$rcmd'\n" if ($params{DEBUG});
		system (split (/\s+/, $rsh), $cli, $rcmd);
	    }
	    print "\n";
	}

	elsif ($mode eq "cleanup") {
	    if ($params{DEBUG}) {	# get debug files
		print "Cleaning up debug files on $cli\n";
		my $rcmd = ($section->{ARCH} eq "NT4.0") ? "DEL" : "$rmcmd";
		$rmcmd .= " mstone-debug.[0-9]*";
		$rcmd = "cd $tempdir; " . $rcmd if ($tempdir);
		$rcmd =~ s/;/&&/g if ($section->{ARCH} eq "NT4.0");
		if ($cli =~ /^localhost$/i) {
		    die "TEMPDIR must be set for 'localhost'\n"
			unless ($tempdir);
		    system ($rcmd);
		} else {
		    system (split (/\s+/, $rsh), $cli, $rcmd);
		}
	    } else {
		print "Cleaning $cli\n";
		my $rcmd = ($section->{ARCH} eq "NT4.0") ? "DEL" : "$rmcmd";
		$rcmd .= " $clibin @files";
		$rcmd = "cd $tempdir; " . $rcmd if ($tempdir);
		$rcmd =~ s/;/&&/g if ($section->{ARCH} eq "NT4.0");
		if ($cli =~ /^localhost$/i) {
		    die "TEMPDIR must be set for 'localhost'\n"
			unless ($tempdir);
		    system ($rcmd);
		} else {
		    system (split (/\s+/, $rsh), $cli, $rcmd);
		}
	    }
	}

	else {
	    die "Couldn't recognize mode $mode!\n";
	}
    }
}

# wait for children to finish
if (($mode eq "timesync") || ($mode eq "checktime")) {
    $pid = wait();
    while ($pid != -1) {
	$pid = wait();
    }
}

# Print the results of the time checks
if ($mode eq "checktime") {
    print "Time from each client:\n";
    foreach $section (@workload) {
	next unless ($section->{sectionTitle} =~ /CLIENT/o);
	my $slist = $section->{sectionParams};
	$slist =~ s/HOSTS=\s*//; # strip off initial bit
	foreach $cli (split /[\s,]/, $slist) {
	    open TIMEFILE, "$tmpbase/$cli.tim"
		|| warn "Counldn't open $tmpbase/$cli.tim\n";
	    printf "%32s: ", $cli;
	    while (<TIMEFILE>) { print; last;} # single line (2 on NT)
	    close(TIMEFILE);
	    unlink "$tmpbase/$cli.tim";
	}
    }
}
