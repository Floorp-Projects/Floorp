#!/usr/local/bin/perl -w   
use strict;
use Sys::Hostname;
use Cwd;
use Data::Dumper;
use Time::HiRes;
#use Storable qw(lock_store lock_retrieve);
use vars qw($ch_id_ref);
#
# It's tempting to use environment variables for things like USER
# and CVSROOT; however, don't. Using the builtin CVS variables is
# a better idea, especially if you are using a three entry 
# $CVSROOT/CVSROOT/passwd (i.e., cvs runs as a local user instead of
# the actual user)
#
$::cvsrootdir = shift @ARGV;
#
# I'd really rather have all my "use" and "require" statements before
# anything else, but since I want to keep the bonsai-global.pm module
# checked into $CVSROOT/CVSROOT, I need to do the ugly "parse" the 
# root first, then require the module foo you see here.
#
require "$::cvsrootdir/CVSROOT/bonsai-global.pm";
require "$::cvsrootdir/CVSROOT/bonsai-config.pm";

$::start = Time::HiRes::time;

$::cwd = cwd;
$::user = shift @ARGV;
$::time = time;
$::directory = shift @ARGV ;
#$::directory =~ s/^\"(.*)\"$/$1/;
$::cvsroot = hostname() . ":" . $::cvsrootdir;

#print "CWD: $::cwd\n### USER: $::user\n### TIME: $::time\n### DIR: $::directory\n### CVSROOT: $::cvsroot\n";

$::log = 0;
while (<>) { 
#print " -- $_";
	if (/^Log Message:$/) {
		$::log = 1;
		next;
	}
	next until $::log;
	$::logtext .= $_;
#print " ---- $_";
}
$::logtext =~ s/[\s\n]*$//;
#-debug-#	&debug_msg("LOG: $::logtext", 3);

if (-e "CVS/Entries") {    # if block for first intermodule mirrored add of a new directory
	open (ENTRIES, "<CVS/Entries") || die "Can't open CVS/Entries" ;
	while (<ENTRIES>) {
		chomp;
#-debug-# print "CVS/Entries: $_\n";
		my @line = split /\//;
		next if &get('code', @line);
		my $file = &get('file', @line);
		my $branch = &get('tag', @line);
		$::change_ref->{$branch}->{$file}->{'old'} = &get('rev', @line);
		$::change_ref->{$branch}->{$file}->{'old'} =~ s/^-//;
		undef $file;
		undef $branch;
		undef @line;
	}
	close ENTRIES;
}

if (-e "CVS/Entries.Log") {    # if block for directory adds since CVS/Entries.log doesn't get created for directory adds
	open (ENTRIES, "<CVS/Entries.Log") || die "Can't open CVS/Entries.Log" ;
	while (<ENTRIES>) {
		chomp;
#-debug-# print "CVS/Entries.log: $_\n";
		my @line = split /\//;
		next if (&get('code', @line) eq 'A D'); # the if block isn't enough, this covers cvs add foo foo/* foo/*/* ...
		my $file = &get('file', @line);
		my $branch = &get('tag', @line);
		$::change_ref->{$branch}->{$file}->{'new'} = &get('rev', @line);
#		$::change_ref->{$branch}->{$file}->{'new'} =~ s/^-[1-9][0-9\.]+$/NONE/;
		$::change_ref->{$branch}->{$file}->{'new'} =~ s/^-[1-9][0-9\.]+$/0/;
		undef $file;
		undef $branch;
		undef @line;
	}
	close ENTRIES;
}

&collapse_HOHOH($::change_ref, ['new', 'old']);

&connect();

$::mirror =  eval &retrieve("expanded_mirrorconfig");
#print "\$expanded_mirrorconfig",Dumper($::mirror);

###
### directory/file specific actions
###
if ($::directory eq "CVSROOT") {
	my $modulesfile = "./modules";
	if (-e $modulesfile ) {    # create an expanded modules file and store it in the database
		my $modules = `cat $modulesfile`;
		my $modules_hash = &BuildModuleHash($modules) ;
		&ExpandHash($modules_hash);    # Expand the modules file in terms of itself
		&FlattenHash($modules_hash);   # Remove dulicate entries from expansion
		my $formatted_modules = &FormatModules($modules_hash, $::cvsrootdir);    # Convert to regexs suitable for matching
		&store("modules", $formatted_modules, {"cvsroot_id" => $::cvsroot, "rev" => $::change_ref->{'TRUNK'}->{'modules'}->{'new'}});
		#
		# update expanded_mirrorconfig & expanded_accessconfig
		#
		unless ($::change_ref->{'TRUNK'}->{'bonsai-mirrorconfig.pm'} &&
			    $::change_ref->{'TRUNK'}->{'bonsai-mirrorconfig.pm'}->{'new'}) {
			my $mc = eval &retrieve("mirrorconfig");
			&expand_mirror_modules($mc);
			&store("expanded_mirrorconfig", $mc);
		}
		unless ($::change_ref->{'TRUNK'}->{'bonsai-accessconfig.pm'} &&
			    $::change_ref->{'TRUNK'}->{'bonsai-accessconfig.pm'}->{'new'}) {
			my $ac = eval &retrieve("accessconfig");
			&expand_access_modules($ac);
			&store("expanded_accessconfig", $ac);
		}
	}

	if ($::change_ref->{'TRUNK'}->{'bonsai-mirrorconfig.pm'} &&
		$::change_ref->{'TRUNK'}->{'bonsai-mirrorconfig.pm'}->{'new'}) {
        require "./bonsai-mirrorconfig.pm";
#print Dumper($::mirrorconfig), "#" x 80, "\n";
		&format_mirrorconfig($::mirrorconfig);   # Convert to regexs suitable for matching
#print Dumper($::mirrorconfig), "#" x 80, "\n";
		&store("mirrorconfig", $::mirrorconfig, {"cvsroot_id" => $::cvsroot, "rev" => $::change_ref->{'TRUNK'}->{'bonsai-mirrorconfig.pm'}->{'new'}});
		#
		# update expanded_mirrorconfig
		#
#$Data::Dumper::Indent=2;
#$Data::Dumper::Terse=0;
#print Dumper($::mirrorconfig), "#" x 80, "\n";
		&expand_mirror_modules($::mirrorconfig);
#print Dumper($::mirrorconfig);
		&store("expanded_mirrorconfig", $::mirrorconfig);
    }

	if ($::change_ref->{'TRUNK'}->{'bonsai-accessconfig.pm'} &&
		$::change_ref->{'TRUNK'}->{'bonsai-accessconfig.pm'}->{'new'}) {
		require "./bonsai-accessconfig.pm";
#print Dumper($::accessconfig), "#" x 80, "\n";
		&format_accessconfig($::accessconfig);   # Convert to regexs suitable for matching
		&store("accessconfig", $::accessconfig, {"cvsroot_id" => $::cvsroot, "rev" => $::change_ref->{'TRUNK'}->{'bonsai-accessconfig.pm'}->{'new'}});
		#
		# update expanded_accessconfig
		#
#$Data::Dumper::Indent=2;
#$Data::Dumper::Terse=0;
#print Dumper($::accessconfig), "#" x 80, "\n";
		&expand_access_modules($::accessconfig);
#print Dumper($::accessconfig);
		&store("expanded_accessconfig", $::accessconfig);
	}
}

###
### Create checkin and mirror objects in database
###
#-debug-#	&debug_msg("logging checkins in $::directory to database...", 1);
&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'creating checkin object');
($::id, $::ch_id_ref) = &checkin($::cwd, $::user, $::time, $::directory, $::cvsroot, $::logtext, $::change_ref);
#print Dumper($::change_ref);
#-debug-# &debug_msg("\ncheckin id: $::id\n", 0, { prefix => 'none' });
unless (&mirrored_checkin($::logtext)) {
#print "\n--> creating mirror objects <--\n\n";
#unless (&mirrored_checkin($::logtext) || &nomirrored($::logtext)) {
	&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'creating mirror object(s)');
	&create_mirrors($::change_ref, $::mirror);
}

#print "FINAL-" x 12, "FINAL\n", Dumper(\%::mirror_object) if %::mirror_object;

&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'checkin complete');
&delete_commit($::cwd, $::user, $::time, $::directory, $::cvsroot);
&log_performance("loginfo_performance", $::id, Time::HiRes::time - $::start);
&disconnect();
#while (my ($file, $change_id) = each %$::ch_id_ref) { print "--> $change_id -- $file\n" }
