#!/usr/local/bin/perl -w
#use Time::HiRes;
#$::start = Time::HiRes::time;
#use Cwd;

use strict;
use Sys::Hostname;
use Getopt::Long;
use File::Basename;
use File::Path;
use FindBin;
use MIME::Lite;
use Data::Dumper;

use lib $FindBin::Bin;

use config;
use proc;
use DB::Util;
use DB::Insert;
use DB::Update;

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
    my $msg = join "\n--\n", (@_, "mirror.pl is quitting now.\n");
	unless ($_[0] =~ /^.* failed at .*proc.pm line \d{1,3}\.$/) {
		&proc::notify("[CVS-mirror] FATAL ERROR", $msg);
	}
	die @_;
};
#sub {
#    my $msg = join "\n--\n", (@_, "mirror.pl is quitting now.\n");
#	unless ($_[0] =~ /^.* failed at .*proc.pm line \d{1,3}\.$/) {
#		&proc::notify("[CVS-mirror] FATAL ERROR", $msg);
#	}
#	die @_;
#};

my $CVS   = "/usr/local/bin/cvs";
my $DIFF  = "/usr/local/bin/diff";
my $DIFF3 = "/usr/local/bin/diff3";
my $PATCH = "/usr/local/bin/patch";
	
my $h = {};

my $paramref = {
	'return' => 'hashref',
#	'noop' => 0,
#	'log_stdout' => 1,
	'log_always' => 1,
	'workdir' => 'tmp',
	'keep_dir' => 1,
#	'nomail' => 1,
};
#
# Get the command line options. do not modify the values in the hash
# instead modify the local scalars
#
GetOptions ($h,
	'mirror_id=i',
	'change_id=i',
	'action=s',
	'user=s',
	'from_branch=s',
	'from_cvsroot=s',
	'to_branch=s',
	'to_cvsroot=s',
	'offset:s',
	'directory:s',
	'file=s',
	'oldrev=s',
	'newrev=s',
	'log:s',
);
#
# I know this appears to be a gratuitious waste of memory, but I want to
# keep the original unmodified values in the %h hash and the munged values
# in local scalars. I don't care if you don't like it and think that it's
# silly.
#
my $mirror_id    = $h->{'mirror_id'};
my $change_id    = $h->{'change_id'};
my $action       = $h->{'action'};
my $user         = $h->{'user'};
my $from_branch  = $h->{'from_branch'};
my $from_cvsroot = $h->{'from_cvsroot'};
my $to_branch    = $h->{'to_branch'};
my $to_cvsroot   = $h->{'to_cvsroot'};
my $offset       = $h->{'offset'};
my $directory    = $h->{'directory'};
my $file         = $h->{'file'};
my $oldrev       = $h->{'oldrev'};
my $newrev       = $h->{'newrev'};
my $log          = $h->{'log'};
#
# Create aggregate variables and quotemeta things that need quoting
# I'm quoting stuff (like mirror_id, rev numbers, and branch) that
# don't technically require it, just in case (however unlikely) CVS
# or bonsai change the way they operate.
#
$mirror_id   = quotemeta($mirror_id);
$change_id   = quotemeta($change_id);
$action      = quotemeta($action);
$user        = quotemeta($user);
$from_branch = quotemeta($from_branch);
$to_branch   = quotemeta($to_branch);
$oldrev      = quotemeta($oldrev);
$newrev      = quotemeta($newrev);
#
# munge the directory/filename using the offset to tweak from/to.
# this allows for inter-repository and inter-module mirroring
# (becareful, inter-x mirroring is *NOT* well tested)
#
my $from_dir_file = $directory ? $directory . "/" . $file : $file;
my $to_dir_file   = $from_dir_file;
$offset = "|" unless $offset;
my ($from_offset, $to_offset) = split /\|/, $offset;
	# remove \Q & \E below to allow from side regex matching; although, that is
	# likely to open a panadora's box of problems for very little benefit.
	# thj sez "don't do it"
$to_dir_file       =~ s/\Q$from_offset\E/$to_offset/;
my $to_directory   = dirname($to_dir_file);
my $to_file        = basename($to_dir_file);
my $uq_to_directory = $to_directory;
$to_directory      = quotemeta($to_directory);
$to_file           = quotemeta($to_file);
my $uq_to_dir_file = $to_dir_file;
$to_dir_file       = quotemeta($to_dir_file);
my $from_directory = quotemeta($directory);
my $from_file      = quotemeta($file);
$from_dir_file     = quotemeta($from_dir_file);
#
# determine the mirror checkin change type
#
my $change_type;
if ($oldrev && $newrev) {
	$change_type = "checkin";
} elsif (!$oldrev && $newrev) {
	$change_type = "add";
} elsif ($oldrev && !$newrev) {
	$change_type = "remove";
} else {
	die "Both and 'oldrev' and 'newrev' are undefined (mirror_id = $mirror_id, ".
		"change_id = $change_id). This is bad. REAL BAD (trust me).\n\n" .
		"If you are getting this error it means that the checkin/change got inserted into " .
		"the database in an extremely bad way. Please to be fixing.\n";
}
#
# munge the log message to indicate this is a mirrored checkin of change_type $change_type
#
$log .= " (mirrored $change_type from $from_branch)";
$log = quotemeta($log);
#
# get the host name and fix cvsroots for local and remote access
# TODO: the remote access parts will require a read-only user on the 
#	    remote repository and also a modified from_cvsroot that includes
#		a conection method and user.
#
# TODO: exit if to_cvsroot != from_cvsroot. do so until I get adds working
#
my $hostname  = Sys::Hostname::hostname();
$to_cvsroot   =~ s/^$hostname://; # this should always match
$from_cvsroot =~ s/^$hostname://; # this should only sometimes match
#
# Oh what a lame ass hack. the old bonsai does stupid shit with
# rlog, and uses $ENV{'CVSROOT'}. um, that's lame.
#
$ENV{'CVSROOT'} = $to_cvsroot;
my $uq_to_cvsroot = $to_cvsroot;
$to_cvsroot   = quotemeta($to_cvsroot);
$from_cvsroot = quotemeta($from_cvsroot);
#
# if we are mirroring to/from the TRUNK branch (TRUNK)
# do not include a -r option on the command line
# (from_branch_arg is probaly wasted since we have rev numbers
# and should therefore never need it, but i like symmetry).
#
my $to_branch_arg   = ($to_branch   && $to_branch   ne "TRUNK") ? "-r $to_branch"   : "" ;
my $from_branch_arg = ($from_branch && $from_branch ne "TRUNK") ? "-r $from_branch" : "" ;
#
# Determine the appropriate merge type (cvs or diff3)
#
my $merge_type;
if ($offset ne "|" || $from_cvsroot ne $to_cvsroot) {
	$merge_type = 'diff3';
} else {
	$merge_type = 'cvs';
}

my $status;
$status = &mirror($merge_type, $change_type);
$status = &diff_patch if $status eq 'conflict';
$status = &mirror($merge_type, $change_type, 1) if $status eq 'conflict';
&error_detected if $status eq "error";
$status = $status eq "merge" ? $merge_type."_".$status : $status;
&update_status($status);

#
# Subroutines
#
sub mirror {
	my ($merge_type, $change_type, $force_ci) = @_;
	my ($cmd, $r, $status) = undef;
	$force_ci = $force_ci ? '-f' : '' ;
	$status = 'merge';
	if ($merge_type eq 'cvs') {
		unlink "tmp/$uq_to_dir_file" if (-f "tmp/$uq_to_dir_file");
		$r = &run_and_log("$CVS -d $to_cvsroot co $to_branch_arg -j $oldrev -j $newrev $to_dir_file");
		&missing_file if (
			$change_type eq 'checkin' &&
			!-f "tmp/$uq_to_dir_file" 
		);
		if ($change_type eq 'add' && defined $r->{'stderr'} && 
			$r->{'stderr'} eq "cvs checkout: file $uq_to_dir_file exists, but has been added in revision $h->{'newrev'}\n") {
				my $diff_to_branch = $to_branch eq "TRUNK" ? "HEAD" : $to_branch;
				$r = &run_and_log("$CVS rdiff -r $newrev -r $diff_to_branch $to_dir_file", {'nomail' => 1});
				&previously_added($r->{'stdout'} ? 'different' : 'same');
		}
		&previously_removed if (
			$change_type eq 'remove' &&
			defined $r->{'stderr'} && 
			$r->{'stderr'} ne "cvs checkout: scheduling $uq_to_dir_file for removal\n"
		);
		$status = 'conflict' if (
			$change_type eq 'checkin' &&
			defined $r->{'stderr'} && 
			$r->{'stderr'} eq "rcsmerge: warning: conflicts during merge\n"
		);
		if ( $change_type eq 'checkin' && defined $r->{'stderr'} && 
			$r->{'stderr'} =~ /\Qcvs checkout: nonmergeable file needs merge\E/) {
				my $diff_to_branch = $to_branch eq "TRUNK" ? "HEAD" : $to_branch;
				my $nmfd = &run_and_log("$CVS rdiff -r $oldrev -r $diff_to_branch $to_dir_file", {'nomail' => 1});
				$status = 'non_merge_overwrite' if $nmfd->{'stdout'};
		}
		if ($status eq 'merge' || $status eq 'non_merge_overwrite' || $force_ci) {
			$r = &run_and_log("$CVS ci $force_ci -m $log $to_dir_file");
			&conflicted if ($status eq 'conflict');
			&non_merge if ($status eq 'non_merge_overwrite');
			&previously_applied unless ($r->{'stdout'} || $r->{'stderr'} || $r->{'exit_value'});
			$status = 'error' if (
				$r->{'exit_value'} ||
				(defined $r->{'stderr'} &&
					$r->{'stderr'} !~ /^(\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] waiting for \E.*?\Q's lock in $uq_to_cvsroot\/$uq_to_directory\E\n)+\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] obtained lock in $uq_to_cvsroot\/$uq_to_directory\E\n$/
				)
			);
		}
		return $status;
	} elsif ($merge_type eq 'diff3') {
		#
		# use diff3 (like cvs does internally) to mirror between modules and repositories.
		# since cvs can't do the magic for us, we need to have separate actions for change, add, and remove.
		#

		# cleanup any cruft that might be left over from the previous attempt (prior to the forced checkin of the conflict)
		unlink "tmp/$uq_to_dir_file" if (-f "tmp/$uq_to_dir_file");

		# check keyword expansion mode of source file
		my ($keywordmode, $option) = undef;
		$r = &run_and_log("$CVS -d $from_cvsroot rlog -hN $from_dir_file | grep '^keyword substitution: '");
		chomp($keywordmode = $r->{'stdout'});
		$keywordmode =~ s/^^keyword substitution: //;

		if ($change_type eq 'checkin') {
			# for changes to existing files use diff3 to merge

			# get the old revision
			$r = &run_and_log("$CVS -q -d $from_cvsroot co -p -r $oldrev $from_dir_file > $from_file,$oldrev");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 

			# get the new revision
			$r = &run_and_log("$CVS -q -d $from_cvsroot co -p -r $newrev $from_dir_file > $from_file,$newrev");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 

			# get the version from the destination module/branch. If it is not there send a missing_file warning
			$r = &run_and_log(
				"$CVS -d $to_cvsroot co $to_branch_arg $to_dir_file",
				{'nomail' => 1}
			);
			&missing_file if (!-f "tmp/$uq_to_dir_file");
			
			# if binary compare dest. with old and change status to "non_merge" and checkin the new source file
			# if not, don't change the status (and thus send the mail), just checkin the new file
			if ($keywordmode eq 'b') {
				$r = &run_and_log("$DIFF -q $from_file,$oldrev $to_dir_file");
				$status = "non_merge_overwrite" if $r->{'stdout'};
				$r = &run_and_log("cp $from_file,$oldrev $to_dir_file");
				# thereshould really be an error check here
			} else {
				# behold the magic that is diff3! (store result in foo,new)
				$r = &run_and_log(
					"$DIFF3 -E -am $to_dir_file $from_file,$oldrev $from_file,$newrev > $to_dir_file,new",
					{'nomail' => 1}
				); 
				$status = 'error' if ($r->{'exit_value'} == 2);
				$status = 'conflict' if ($r->{'exit_value'} == 1);

				# replace with the new file in prep for checkin
				$r = &run_and_log("mv $to_dir_file,new $to_dir_file");
				$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 
			}

			# if we haven't conflicted yet, or we are conflicted and diff_patch couldn't handle it, it's time to checkin
			if ($status eq 'merge' || $status eq 'non_merge_overwrite' || $force_ci) {
				$r = &run_and_log("$CVS ci -m $log $to_dir_file");

				# send bitch mail for conflicts
				&conflicted if ($status eq 'conflict');

				# send mail about binary changes where the files are different
				&non_merge if ($status eq 'non_merge_overwrite');

				# send more naggy mail if the checkin is a noop
				&previously_applied unless ($r->{'stdout'} || $r->{'stderr'} || $r->{'exit_value'});

				# set the status to "error" if we get a non-zero exit value or something unexpected on stderr
				# (the ugly regex is to ignore lock bump messages)
				$status = 'error' if (
					$r->{'exit_value'} ||
					(defined $r->{'stderr'} &&
						$r->{'stderr'} !~ /^(\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] waiting for \E.*?\Q's lock in $uq_to_cvsroot\/$uq_to_directory\E\n)+\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] obtained lock in $uq_to_cvsroot\/$uq_to_directory\E\n$/
					)
				);
			}
		} elsif ($change_type eq 'add') {
			# TODO: Needs mucho error handling and binary foo
			# for added files, bootstrap/spoof some CVS admin directories and add the file

			# check to see if the file is already here and send the appropriate bitch mail
			$r = &run_and_log(
				"$CVS -d $to_cvsroot co -d prev $to_branch_arg $to_dir_file",
				{'nomail' => 1}
			);
			if (-f "tmp/$uq_to_dir_file") {
				$r = &run_and_log("$CVS -q -d $from_cvsroot co -p -r $newrev $from_dir_file > $from_file,$newrev");
				$r = &run_and_log("$DIFF -wB $from_file,$newrev prev/$to_file", {'nomail' => 1});
				&previously_added($r->{'stdout'} ? 'different' : 'same');
			}

			#$r = &run_and_log("$CVS -d $from_cvsroot rlog -hN $from_dir_file | grep '^keyword substitution: '");
			#my $option = $r->{'stdout'};
			#$option =~ s/^^keyword substitution: /-/;

			# create the directory structure for the new file
			$r = &run_and_log("mkdir -p $to_directory");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 

			# get the new file and stash it in it's freshly created directory
			$r = &run_and_log("$CVS -q -d $from_cvsroot co -p -r $newrev $from_dir_file > $to_dir_file");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 

			# spoof an existing checkout by populating a CVS admin directory
			$r = &run_and_log("mkdir CVS");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 
			$r = &run_and_log("echo $to_cvsroot > CVS/Root");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 
			$r = &run_and_log("echo . > CVS/Repository");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 
			$r = &run_and_log("echo D > CVS/Entries");
			$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 
			# you only need CVS/Tag if not on the trunk (also need to prefix with a "T")
			if ($to_branch ne 'TRUNK') {
				$r = &run_and_log("echo T$to_branch > CVS/Tag");
				$status = 'error' if (defined $r->{'stderr'} || $r->{'exit_value'}); 
			}

			# recursively add the subdirs (as described in the `info cvs`) to create the appropriate admin dirs
			my $add = undef;
			for my $element (split("/", $uq_to_dir_file)) {
				$add .= quotemeta($element);

				# don forget to set the keyword expansion the same as the source file
				$option = ($add eq $to_dir_file) ? "-k" . $keywordmode : "";

				# add the dir/file
				$r = &run_and_log("$CVS add $option $add");

				# ignore some expected stderr output
				$status = 'error' if (
					$r->{'exit_value'} ||
					(defined $r->{'stderr'} &&
						$r->{'stderr'} ne 
							"cvs add: scheduling file `$uq_to_dir_file' for addition\n" .
							"cvs add: use 'cvs commit' to add this file permanently\n" &&
						$r->{'stderr'} !~
							/^\Qcvs add: re-adding file $uq_to_dir_file (in place of dead revision \E[0-9\.]+?\)\n\Qcvs add: use 'cvs commit' to add this file permanently\E\n$/
					)
				); 
				$add .= quotemeta("/");
			}

			# checkin the new file
			$r = &run_and_log("$CVS ci -m $log $to_dir_file");

			# again ignore some expected error with big freaky regexs
			$status = 'error' if (
				$r->{'exit_value'} ||
				(defined $r->{'stderr'} &&
					$r->{'stderr'} ne 
						"cvs commit: changing keyword expansion mode to $option\n" &&
					$r->{'stderr'} !~
						/^((\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] waiting for \E.*?\Q's lock in $uq_to_cvsroot\/$uq_to_directory\E\n)+\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] obtained lock in $uq_to_cvsroot\/$uq_to_directory\E\n)?(\Qcvs commit: changing keyword expansion mode to $option\E\n)?$/
				)
			); 
		} elsif ($change_type eq 'remove') {
			# removes are easy, first check to see if the file is even there, and send a message if not
			$r = &run_and_log(
				"$CVS -d $to_cvsroot co $to_branch_arg $to_dir_file",
				{'nomail' => 1}
			);
			&previously_removed if (!-f "tmp/$uq_to_dir_file");

			# remove the file (ignoring expected stderr output)
			$r = &run_and_log("$CVS rm -f $to_dir_file");
			$status = 'error' if (
				$r->{'exit_value'} ||
				(defined $r->{'stderr'} && 
					$r->{'stderr'} ne
						"cvs remove: scheduling `$uq_to_dir_file' for removal\n" .
						"cvs remove: use 'cvs commit' to remove this file permanently\n"
				)
			);

			# and check it in (ignoring expected stderr output)
			$r = &run_and_log("$CVS ci -m $log $to_dir_file");
			$status = 'error' if (
				$r->{'exit_value'} ||
				(defined $r->{'stderr'} &&
					$r->{'stderr'} !~ /^(\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] waiting for \E.*?\Q's lock in $uq_to_cvsroot\/$uq_to_directory\E\n)+\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] obtained lock in $uq_to_cvsroot\/$uq_to_directory\E\n$/
				)
			);
		} else {
			die "Undefined change type ($change_type), Loser.\n\n";
		}
	} else {
		die "Undefined merge type ($merge_type) specified.\n\n" .
			"Your coding is weak and ineffectual.\n\n";
	}
	return $status;
}

#
# patch/diff command
#
sub diff_patch {
	my ($r) = undef;
	unlink "tmp/$uq_to_dir_file" if (-f "tmp/$uq_to_dir_file");
	$r = &run_and_log("$CVS -d $to_cvsroot co $to_branch_arg $to_dir_file");
	$r = &run_and_log(
		"$CVS -d $from_cvsroot rdiff -c -r $oldrev -r $newrev $from_dir_file | $PATCH -c -N -l $to_dir_file",
		{'nomail' => 1}
	);
	if ($r->{'exit_value'} && defined $r->{'stdout'}) {
		if ($r->{'stdout'} =~ /\d+ out of \d+ hunk[s]? FAILED -- saving rejects to( file)? $to_dir_file\.rej/) {
			return 'conflict';
		} elsif ($r->{'stdout'} =~ /\QReversed (or previously applied) patch detected!  Skipping patch.\E/) {
#
# patch lies and says the patch is reversed or previously applied when it is not.
# use mirror_id = 3247 & change_id = 31329 with GNU patch version 2.5.4 as a test case/example.
# Since we can't trust patch, return 'conflict' and force the checkin.
#			&previously_applied;
			return 'conflict';
		}
		return 'error'
	}
	$r = &run_and_log("$CVS ci -m $log $to_dir_file");
	if (
		$r->{'exit_value'} ||
		(defined $r->{'stderr'} &&
			$r->{'stderr'} !~ /^(\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] waiting for \E.*?\Q's lock in $uq_to_cvsroot\/$uq_to_directory\E\n)+\Qcvs commit: [\E([0-9]{2}:){2}[0-9]{2}\Q] obtained lock in $uq_to_cvsroot\/$uq_to_directory\E\n$/
		)
	) {
		return 'error';
	} else {
		return 'diff_patch';
	}
	#return (defined $r->{'stderr'} || $r->{'exit_value'}) ? 'error' : 'diff_patch';
}
#
# Command executor and output logger
#
sub run_and_log {
	my ($cmd, $param) = @_;
	my $r = undef;
	my $new_paramref = {%$paramref};
	while (my($key, $value) = each (%$param)) {
		$new_paramref->{$key} =  $value;
	}
	&DB::Util::connect();
	$r = &proc::run($cmd, $new_paramref);
	&DB::Insert::mirror_change_exec_map(
        $mirror_id,
        $change_id,
        $r->{'log_id'}
    ) if defined $r->{'log_id'};
	&DB::Util::disconnect();
    return $r;
}
#
# Send a message (and update the mirror_change status) if we try to mirror
# and a nonfatal error is detected.
#
sub error_detected {
	my ($subject, $body, $from_root, $to_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	$subject = "error - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your checkin to $h->{'directory'}/$h->{'file'} on the $h->{'from_branch'}$from_root could not be mirrored to the $h->{'to_branch'}$to_root because $uq_to_dir_file an unexpected error was detected.

Whatever is broken (or not quite right) will likely need to be fixed by Release Engineering. This message is just to inform you that the mirror operation did not complete successfully.  If you have any questions, please contact Release Engineering.

Release Engineering: Use the info below to look up the details of the error in the database.
	#;
	&mail($subject, $body);
	&update_status("error");
}
#
# Send a message (and update the mirror_change status) if we try to mirror
# and the destination file doen't exist
#
sub missing_file {
	my ($subject, $body, $from_root, $to_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	$subject = "missing file - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your checkin to $h->{'directory'}/$h->{'file'} on the $h->{'from_branch'}$from_root could not be mirrored to the $h->{'to_branch'}$to_root because $uq_to_dir_file was not found on the $h->{'to_branch'}$to_root.

This could be caused by any of a number of things, such as mirroring being misconfigured, the file might have originally been added to the $h->{'from_branch'} by a tagging operation instead of a \"cvs add\", or maybe it's been removed from the $h->{'to_branch'}$to_root.

If $uq_to_dir_file needs to be on the $h->{'to_branch'}$to_root either add it or contact Release Engineering.
	#;
	&mail($subject, $body);
	&update_status("missing");
}
#
# Send mail if the mirror was to add a file and it already exists
#
sub previously_added {
	my $diff = shift;
	my ($subject, $body, $from_root, $to_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	my $foo = $diff eq "same" ? "the " : "";
	$subject = "previously added ($diff) - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your add of $h->{'directory'}/$h->{'file'} to the $h->{'from_branch'}$from_root could not be mirrored to the $h->{'to_branch'}$to_root because $uq_to_dir_file was already present. The file you added and $uq_to_dir_file on the $h->{'to_branch'}$to_root are $foo$diff.

This may (or may not) indicate a problem. Contact Release Engineering if you have any questions.
	#;
	&mail($subject, $body);
	&update_status("prev_add_$diff");
}
#
# Send mail if the mirror was to remove a file and it was already removed
#
sub previously_removed {
	my ($subject, $body, $from_root, $to_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	$subject = "previously removed - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your remove of $h->{'directory'}/$h->{'file'} from the $h->{'from_branch'}$from_root could not be mirrored to the $h->{'to_branch'}$to_root because $uq_to_dir_file does not exist on the $h->{'to_branch'}$to_root. This file was either previously removed, or never existed on the $h->{'to_branch'}$to_root.

This may (or may not) indicate a problem. Contact Release Engineering if you have any questions.
	#;
	&mail($subject, $body);
	&update_status("prev_rm");
}
#
# Send a message if the mirror results in a NOOP
#
sub previously_applied {
	my ($subject, $body, $from_root, $to_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	$subject = "previously applied - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your checkin to $h->{'directory'}/$h->{'file'} on the $h->{'from_branch'}$from_root was not mirrored to $uq_to_dir_file on the $h->{'to_branch'}$to_root because that change appears to have already been applied.

This may (or may not) indicate a problem. Contact Release Engineering if you have any questions.
	#;
	&mail($subject, $body);
	&update_status("noop");
}
#
# Send mail if the mirror involves a nonmergable file and the source and destination
# were different before the original checkin, since we are overwriting the destination
# file.
#
sub non_merge {
	my ($subject, $body, $from_root, $to_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	$subject = "nonmergeable file - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your checkin to nonmergable file: $h->{'directory'}/$h->{'file'} on the $h->{'from_branch'}$from_root has mirrored to the $h->{'to_branch'}$to_root overwriting the file $uq_to_dir_file. Before your checkin these two files were DIFFERENT; however, now they are the same.

This may (or may not) desirable. Contact Release Engineering if you have any questions.
	#;
	&mail($subject, $body);
	&update_status("non_merge_overwrite");
}
#
# Send email if conflicts were generated during the mirror
#
sub conflicted {
	my ($subject, $body, $count, $conflict, $conflict_text, $to_root, $from_root) = undef;
	if ($h->{'to_cvsroot'} eq $h->{'from_cvsroot'}) {
		$to_root = "";
		$from_root = "";
	} else {
		$to_root = " ($h->{'to_cvsroot'})";
		$from_root = " ($h->{'from_cvsroot'})";
	}
	$conflict_text = "=" x 70 . "\nConflict Detail:\n";
	open (CONFLICTFILE, "tmp/$uq_to_dir_file"); 
	while (<CONFLICTFILE>) {
		$count++;
		if (/^<<<<<<< /) {
			$conflict++;
			if ($conflict == 1) {
				$conflict_text .= "\nAt line $count:\n";
			}
		}
		if ($conflict) {
			$conflict_text .= $_;
		}
		if (/^>>>>>>> /) {
			if ($conflict == 1) {
				$conflict_text .= "\n";
			}
			$conflict--;
		}
	}
	close (CONFLICTFILE);
	$conflict_text .= "=" x 70 . "\n";
	$subject = "CONFLICT - $user - $to_branch - $uq_to_dir_file";
	$body = qq#
Your checkin to $h->{'directory'}/$h->{'file'} on the $h->{'from_branch'}$from_root has mirrored with conflicts (shown below) and the $h->{'to_branch'}$to_root is now broken.

THIS REQUIRES YOUR IMMEDIATE ATTENTION.

You can checkout the conflicted file with the following command:

   cvs co $to_branch_arg $uq_to_dir_file

If you have any questions please contact Release Engineering.
	#;
	&mail($subject, "$body\n$conflict_text");
	&update_status("conflict");
}
#
# handy dandy little mirror_change status updater function
#
sub update_status {
#-debug-#print Dumper(\@_);
	&DB::Util::connect();
	&DB::Update::mirror_change($mirror_id, $change_id, shift);
	&DB::Util::disconnect();
	exit 0;
}
#
# convenient little wrapper for MIME::Lite
# add a cute little message that contains a dump of all the options/values
# passed to this program (might be useful for debugging).
#
sub mail {
	my ($subject, $text) = @_;
	$Data::Dumper::Indent = 1;
	$Data::Dumper::Terse = 1;
	$text .= "\n--\n<jedi_mind_trick>\n" .
			 "This is not the information you're looking for.\n" .
			 Dumper($h) .
			 "</jedi_mind_trick>"
	;
	my $msg = MIME::Lite->new(
		From      => "$default::admin_address",
		To        => "$user\@bluemartini.com",
#		Cc        => "$default::admin_address",
		Bcc       => "$default::pager_address",
		Subject   => "[CVS-mirror] $subject",
		Datestamp => 0,
		Data      => "$text",
	);
	$msg->send();
}
#
# Cleanup after ourselves since the calling script is running as the
# unprivileged mirror user.
#
END { rmtree $paramref->{'workdir'} };

__END__
