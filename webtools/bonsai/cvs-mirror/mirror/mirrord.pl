#!/usr/local/bin/perl -w
use Time::HiRes qw(time); 
use Data::Dumper;

use strict;
use Sys::Hostname;
use FindBin;

use lib $FindBin::Bin;

use config;
use proc;
use access;
use DB::Util;
use DB::Select;
use DB::Update;
use DB::Insert;

umask 0;
#
# Need /usr/local/bin in the path since sometimes
# diff3 needs to find the gnu version of diff
#
$ENV{'PATH'}='/usr/local/bin:/usr/bin';
#
# overload the die function to send me email when things go bad
# don't send mail if died in proc.pm since it does it's own error
# catching and email bitching.
#
# TODO: maybe put this in the main loop and get a list of people from
# 		the database to send bitch mail to
#
#
# Trap some signals and send mail to the interested parties
#
$SIG{HUP}  = \&signal_handler;
$SIG{INT}  = \&signal_handler;
$SIG{TERM} = \&signal_handler;
$SIG{QUIT} = \&signal_handler;
$SIG{SEGV} = \&signal_handler;
$SIG{__DIE__} = \&signal_handler;
sub signal_handler {
    my $msg = join "\n--\n", (@_, "mirrord.pl is quitting now.\n");
    unless ($_[0] =~ /^.* failed at .*proc.pm line \d{1,3}\.$/) {
        &proc::notify("[CVS-mirror] FATAL ERROR", $msg);
    }
    die @_;
};
#$SIG{__DIE__} = sub {
#	my $msg = join "\n--\n", (@_, "mirrord.pl is quitting now.\n");
#	unless ($_[0] =~ /^.* failed at .*proc.pm line \d{1,3}\.$/) {
#		&proc::notify("[CVS-mirror] FATAL ERROR", $msg);
#	}
#	die @_;
#};

my $checkin = {};
my $change = {};
my $runtime;
my $totalops;

my $mirror_cmd = $FindBin::Bin . "/mirror.pl";
my $paramref = {
	'tmpdir' => '/tmp/mirror',
	'return' => 'hashref',
#	'nomail' => 1,
	'noop' => 0,
#	'log_stdout' => 1,
};

&DB::Util::connect(); 
$main::host_id = &DB::Util::id("mh_hostname", Sys::Hostname::hostname());
&DB::Util::disconnect();
#
# Main loop
#
while (1) {
	my $loopstart = time;
	my $lastcommand = defined $runtime->{'command'} ? $runtime->{'command'} : "";
	&DB::Util::connect();
	#
	# fetch some operating parameters from the database
	#
	$runtime = &DB::Select::runtime($main::host_id);
	unless ($::mirror_delay    = $runtime->{'mirror_delay'}   ) { $::mirror_delay    = $default::mirror_delay    };
	unless ($::min_scan_time   = $runtime->{'min_scan_time'}  ) { $::min_scan_time   = $default::min_scan_time   };
	unless ($::throttle_time   = $runtime->{'throttle_time'}  ) { $::throttle_time   = $default::throttle_time   };
	unless ($::max_addcheckins = $runtime->{'max_addcheckins'}) { $::max_addcheckins = $default::max_addcheckins };
	#
	# Send some mail when the command changes
	#
	if (defined $runtime->{'command'} && $runtime->{'command'} ne $lastcommand ) {
        &proc::notify("[CVS-mirror] $runtime->{'command'}", "");
	};
	#
	# log an acknowledgement in the database and shutdown if the 'command' parameter = exit
	#
	if (defined $runtime->{'command'} && $runtime->{'command'} =~ m/exit/i ) {
		&DB::Update::runtime($runtime->{'response'}, $runtime->{'id'}) if defined $runtime->{'id'};
		last;
	};
	#
	# if 'command' != pause, acknowledge and start gathering mirror information
	#
	unless (defined $runtime->{'command'} && $runtime->{'command'} =~ m/pause/i) {
		&DB::Update::runtime($runtime->{'response'}, $runtime->{'id'}) if defined $runtime->{'id'};
#print "running...\nlast_update = $runtime->{'last_update'}\n";
		#
		# Get a copy of the accessconfig from the database. (used later to prevent attempting
		# to mirror to a branch/module/directory/file that is closed)
		#
		my $accessconfig = eval &DB::Util::retrieve("expanded_accessconfig");
		#
		# get a list(ref) of mirrors currently labelled as 'pending'
		#
		my $mirrors = &DB::Select::mirrors('pending');
		#
		# loop over the mirror list
		#
		for my $m (@$mirrors) {
			#
			# extract data from mirror reference and store it in convenience variables
			#
			my $mid = $m->[0];
			my $cid = $m->[1];
			my $to_branch = $m->[2];
			my $to_cvsroot = $m->[3];
			my $offset = $m->[4];
			#
			# gather information about the checkin that produced this mirror object
			# and cache it since it will likely be used by another mirror object in the list
			#
			$checkin->{$cid} = &DB::Select::checkin($cid) unless defined $checkin->{$cid};
			#
			# store the checkin info in convenience variables
			#
			my $directory = $checkin->{$cid}->{'directory'};
			my $user = $checkin->{$cid}->{'user'};
			my $log = $checkin->{$cid}->{'log'};
			my $from_cvsroot = $checkin->{$cid}->{'cvsroot'};
			#
			# gather a list of changes from the source checkin that apply to this mirror
			#
			my $mirror_changes = &DB::Select::mirror_changes("pending", $mid);
			#
			# loop over the changes for this mirror object
			#
			for my $mc (@$mirror_changes) {
				#
				# extract information about the mirror-change, and store in blah blah blah...
				#
				my $chid = $mc->[0];
				my $action = $mc->[1];
				#
				# gather info about this particular change and cache it...
				#
				$change->{$chid} = &DB::Select::change($chid) unless defined $change->{$chid};
				#
				# extract into convenience variables 
				#
				my $file = $change->{$chid}->{'file'};
				my $oldrev = $change->{$chid}->{'oldrev'};
				my $newrev = $change->{$chid}->{'newrev'};
				my $from_branch = $change->{$chid}->{'branch'};
				#
				# Check to see if to_branch has been EOL'd, if so, update the mirror_change status
				# send a friendly reminder to the cvs administrator that he/she sucks. (Oh, and don't
				# mirror this change).
				#
				if (@{&DB::Select::branch_eol($to_cvsroot, $to_branch)}) {
print "EOL BRANCH = ", @{&DB::Select::branch_eol($to_cvsroot, $to_branch)}, "\n";
					&DB::Update::mirror_change($mid, $chid, 'to_branch_eol');
					&proc::notify("[CVS-mirror] warning - to_branch_eol",
						"An attempt was made to mirror $directory/$file from $from_cvsroot:$from_branch " .
						"to $to_cvsroot:$to_branch (oldrev = $oldrev, newrev = $newrev, offset = \"$offset\")." .
						"\n\nYou've likely EOL'd a branch and forgot to update your mirror rules. " .
						"\n\n\tcheckin_id = $cid\n\tchange_id = $chid\n\tmirror_id = $mid" .
						"\n\taction = $action\n\tuser = $user\n\tlog message = $log\n\n" .
						"\n-- Your loving and ever-present MirrorHandler"
					);
					next;
print "--> branch eol\n";
				}
				#
				# Ckeck to see if the repository is open before proceeding (blessed users are NOT mirrored)
				#
#my $pre = time;
				next if &access::closed($accessconfig, $to_cvsroot, $to_branch, $directory, $file);
#my $post = time;
#print "--> $to_cvsroot, $to_branch, $directory, $file -- ", $post - $pre, "\n";
print "--> $to_cvsroot, $to_branch, $directory, $file\n";
				#
				# if we are using the old bonsai on this machine, lets limit the number of
				# addcheckin.pl process that are spawned, since each addcheckin.pl uses about
				# 8MB of RAM.
				#
print "--> addcheckin.pl count: ", &proc::addcheckin_count(), "\n";
				sleep $::throttle_time while (&proc::addcheckin_count() > $::max_addcheckins);
#
# TODO: Check that mirror is still valid before proceeding. There is the possiblilty that the
# mirror rules may have changed between the time of the source checkin and the execution of the
# mirror, if so I should detect it and marke the mirror-change as 'mirror_rule_cancelled' or
# some such.
#
				#
				# build up the command that we will call to mirror this change.
				# We are using 'sudo' so that we can run mirrord.pl as an unprivileged user
				# and still execute cvs and patch/diff as the person who initially performed
				# the checkin.
				# 
				# we pass checkin/mirror metadata (mirror_id and change_id) so that mirror_cmd
				# can update the database appropriately. logging is good.
				#
				my $cmd =
					"sudo -u " . quotemeta($user)
					. " " . quotemeta($mirror_cmd)
					. " --mirror_id=" . quotemeta($mid)
					. " --change_id=" . quotemeta($chid)
					. " --action=" . quotemeta($action)
					. " --user=" . quotemeta($user)
					. " --from_branch=" . quotemeta($from_branch)
					. " --from_cvsroot=" . quotemeta($from_cvsroot)
					. " --to_branch=" . quotemeta($to_branch)
					. " --to_cvsroot=" . quotemeta($to_cvsroot)
					. " --offset=" . quotemeta($offset)
					. " --directory=" . quotemeta($directory)
					. " --file=" . quotemeta($file)
					. " --oldrev=" . quotemeta($oldrev)
					. " --newrev=" . quotemeta($newrev)
					. " --log=" . quotemeta($log)
				;
				#
				# execute cmd using our googfy little system wrapper so that we can trap
				# runtime errors, capture stdout/stderr, send bitch mail, etc.
				#
print "\n$cmd\n\n";
				my $result = &proc::run($cmd, $paramref);
#print Dumper($result);
				#
				# associate the log entry (if produced) with this mirror-change
				#
				&DB::Insert::mirror_change_exec_map( $mid, $chid, $result->{'log_id'}) if defined $result->{'log_id'};
				#
				# mark the mirror-change status as 'error' if a non-zero exit status was rerurned, the process
				# terminated as the result of receiving a signal, or if it core dumped.
				#
				if ($result->{'stderr'} || $result->{'exit_value'} || $result->{'signal_num'} || $result->{'dumped_core'}) {
					&DB::Update::mirror_change($mid, $chid, 'error');
				}
			}
			#
			# get a count of changes that are still marked as pending (likely due to a branch closure) or that
			# experienced an error. If no pending or error changes, mark this mirror object as complete.
			#
			my $not_mirrored = scalar @{&DB::Select::mirror_changes("pending", $mid)};
			my $errors = scalar @{&DB::Select::mirror_changes("error", $mid)};
			&DB::Update::mirror($mid, 'complete') unless ($not_mirrored || $errors);
#print "*** changes still pending =  $not_mirrored\n";
#print "*** changes with errors =  $errors\n";
#$totalops += scalar @$mirror_changes;
#print "changes details -- ", Dumper($checkin);
#print "checkins -- ", scalar keys %$checkin, "\n";
#print "changes -- ", scalar keys %$change, "\n";
#print "mirrors -- ", scalar @$mirrors, "\n" if $mirrors;
#print "totalops -- ", $totalops, "\n" if $totalops;
		}
	} else {
		#
		# acknowledge the 'pause' command
		#
		&DB::Update::runtime($runtime->{'response'}, $runtime->{'id'}) if defined $runtime->{'id'};
print "paused...\n";
	}
	&DB::Util::disconnect();
	my $looptime = time - $loopstart;
print "--> mirror loop duration: $looptime seconds.\n";
	$checkin = {};
	$change = {};
	#
	# sleep a bit, if we finished before the min_scan_time so that we don;t needlessly trash the db and network
	#
	if ($looptime < $::min_scan_time) {	
print "--> Sleeping for ", $::min_scan_time - $looptime, " seconds.\n\n";
		sleep $::min_scan_time - $looptime;
	}
}

print "exiting...\n";
&DB::Util::disconnect();

__END__
