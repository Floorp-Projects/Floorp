package proc;

use strict;
use Data::Dumper;
use Time::HiRes qw(time);
use File::Path;
use FindBin;
use File::Basename;
use Cwd;
use MIME::Lite;

#
# Simulate try-catch-finally block (these subs need to be before the rest
# or perl pitches a bitch. Example syntax:
#
# try {
#	<some perl that can fail or that has an "or die">
# } and catch {
#	<some perl code to execute if the code in the above try block generated an error>
# } or finally {
#	<some code to always execute regardless of errors (i'm not sure when to ever use this)>
# };
#
# the trailing ";" *IS* required, unlike other blocks.
#

sub try (&) {
	my $code = shift;
	eval { &{$code} };
	&notify("[CVS-mirror]: non-fatal error", $@) if $@;
}

sub catch (&) {
	my $code = shift;
	eval { &{$code} };
	return 0;
}

sub finally (&) {
	my $code = shift;
	eval { &{$code} };
}

sub run {
	my ($cmd, $href) = @_;
	my ($return);
	my ($tmpdir, $workdir, $homedir, $debug, $noop, $nomail, $log_always, $log_stdout, $keep_dir, $exit_code);
	print "#-debug-# ",Dumper($href) if $href->{"debug"};
	#
	# define defaults for options
	#
	unless ($tmpdir     = $href->{"tmpdir"}    ) { $tmpdir     = cwd }
	unless ($workdir    = $href->{"workdir"}   ) { $workdir    = $tmpdir."/".$FindBin::Script."-".time."-".$$ }
	unless ($homedir    = $href->{"homedir"}   ) { $homedir    = cwd }
	unless ($debug      = $href->{"debug"}     ) { $debug      = 0 }
	unless ($noop       = $href->{"noop"}      ) { $noop       = 0 }
	unless ($nomail     = $href->{"nomail"}    ) { $nomail     = 0 }
	unless ($log_always = $href->{"log_always"}) { $log_always = 0 }
	unless ($log_stdout = $href->{"log_stdout"}) { $log_stdout = 0 }
	unless ($keep_dir   = $href->{"keep_dir"}  ) { $keep_dir   = 0 }
	unless ($return->{'type'} = $href->{"return"}) { $return->{'type'} = 'array' }
	if ($debug) {
		print "#-debug-# tmpdir:\t$tmpdir\n";
		print "#-debug-# workdir:\t$workdir\n";
		print "#-debug-# homedir:\t$homedir\n";
		print "#-debug-# debug:\t$debug\n";
		print "#-debug-# noop: \t$noop\n";
		print "#-debug-# nomail:\t$nomail\n";
		print "#-debug-# log_always:\t$log_always\n";
		print "#-debug-# log_stdout:\t$log_stdout\n";
		print "#-debug-# keep_dir:\t$keep_dir\n";
		print "#-debug-# return type:\t$return->{'type'}\n";
	}
	#
	# reformat command
	#
	my $ocmd = $cmd;
	$cmd = "echo" if $noop;
	$cmd = "(".$cmd.") 1>stdout 2>stderr";
	#
	# make workdir and chdir into it
	#
	print "#-debug-# started from:\t".cwd."\n" if $debug;
	unless ($href->{'workdir'} && $keep_dir && -d $workdir) {
	try {
		mkpath $workdir, $debug or die "\nfailed to create \"$workdir\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to create \"$workdir\"\n";
		$return->{'exit_value'} += 666;
		goto RETURN;
	};
	}
	try { 
		chdir $workdir or die "failed to chdir to \"$workdir\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to chdir to \"$workdir\": $!\n";
		$return->{'exit_value'} += 666;
		goto RETURN;
	};
	print "#-debug-# working in:\t".cwd."\n" if $debug;
	#
	# execute command and record exit status
	#
	print "#-debug-# executing:\t$cmd\n" if $debug;
	print ("#-debug-# send mail:\t", $nomail?"no":"yes","\n") if $debug;
	unless ($nomail) {
		try {
			$exit_code = system($cmd) and die "\"$cmd\" failed";
		}
	} else {
		$exit_code = system($cmd);
	}
	$return->{'exit_value'}  = $exit_code >> 8;
	$return->{'signal_num'}  = $exit_code & 127;
	$return->{'dumped_core'} = $exit_code & 128;
	#
	# record STDOUT
	#
	try {
		open(OUT, "<stdout") or die "failed to open \"$workdir/stdout\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to open \"$workdir/stdout\"\n";
		$return->{'exit_value'} += 666;
		goto RETURN;
	};
	while (<OUT>) { $return->{'stdout'} .= $_ }
	try {
		close(OUT) or die "failed to close \"$workdir/stdout\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to close \"$workdir/stdout\"\n";
	};
	try {
		unlink "stdout" or die "failed to delete \"$workdir/stdout\"\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to delete \"$workdir/stdout\"\n";
	};
	#
	# record STDERR
	#
	try {
		open(ERR, "<stderr") or die "failed to open \"$workdir/stderr\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to open \"$workdir/stderr\"\n";
		$return->{'exit_value'} += 666;
		goto RETURN;
	};
	while (<ERR>) { $return->{'stderr'} .= $_ }
	try {
		close(ERR) or die "failed to close \"$workdir/stderr\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to close \"$workdir/stderr\"\n";
	};
	try {
		unlink "stderr" or die "failed to delete \"$workdir/stderr\"\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to delete \"$workdir/stderr\"\n";
	};
	#
	# return to homedir
	#
	try {
		chdir $homedir or die "failed to chdir to \"$homedir\": $!\n";
	} and catch {
		$return->{'stderr'} .= "\nfailed to chdir to \"$homedir\"\n";
	};
	print "#-debug-# returned to:\t".cwd."\n" if $debug;
	#
	# cleanup workdir unless we're directed to keep it
	#
	unless ($keep_dir) {
		try { rmtree $workdir or die "failed to delete \"$workdir\": $!\n" };
	}
	#
	# Return stdout, stderr, and exit status in the form requested.
	# Bitch and die, if an invalid form was requested
	#
RETURN:
	if ($log_always) {
		$return->{'log_id'} = &log($ocmd, $return);
	} elsif ($log_stdout &&
			 $return->{'stdout'} ||
			 $return->{'stderr'} ||
			 $return->{'exit_value'} ||
			 $return->{'signal_num'} ||
			 $return->{'dumped_core'}) {
		$return->{'log_id'} = &log($ocmd, $return);
	} elsif ($return->{'stderr'} ||
			 $return->{'exit_value'} ||
			 $return->{'signal_num'} ||
			 $return->{'dumped_core'}) {
		$return->{'log_id'} = &log($ocmd, $return);
	} else {
		# don't log anything
	}
	if ($return->{'type'} =~ /^array$|^list$/i) {
		return (
			$return->{'stdout'},
			$return->{'stderr'},
			$return->{'exit_value'},
			$return->{'signal_num'},
			$return->{'dumped_core'},
			$return->{'log_id'}
		);
	} elsif ($return->{'type'} =~ /^arrayref$|^listref$/i) {
		return [
			$return->{'stdout'},
			$return->{'stderr'},
			$return->{'exit_value'},
			$return->{'signal_num'},
			$return->{'dumped_core'},
			$return->{'log_id'}
		];
	} elsif ($return->{'type'} =~ /^hashref$/i) {
		delete $return->{'type'};
		return $return ;
	} elsif ($return->{'type'} =~ /^hash$/i) {
		delete $return->{'type'};
		return %$return ;
	} else {
		try { die (
			"Invalid return type requested ($return->{'type'}).\n",
			"Valid return types are:\n",
				"\tARRAY or LIST (default)\n",
				"\tHASH\n",
				"\tARRAYREF or LISTREF\n",
				"\tHASHREF\n\n",
		);
		};
	}
}

sub log {
	my ($cmd, $h) = @_;
	return &DB::Insert::exec_log(
		$cmd,
		$h->{'stdout'},
		$h->{'stderr'},
		$h->{'exit_value'},
		$h->{'signal_num'},
		$h->{'dumped_core'}
	);
}

sub notify {
#
# TODO: make the from/to headers variables
#
	my ($subject, $text) = @_;
	my $msg = MIME::Lite->new(
		From      => $default::admin_address,
		To        => $default::pager_address,
		Subject   => $subject,
		Datestamp => 0,
		Data      => $text
	);
	$msg->send();
}

sub addcheckin_count {
    my $count = `ps -ef | grep -c "[a]ddcheckin\.pl"`;
    chomp $count;
    return $count;
}

return 1;
