#!/usr/local/bin/perl -w    
use strict;
use Sys::Hostname;
use Cwd;

#use diagnostics ;
#use Data::Dumper;
#use Time::HiRes;

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

#$::start = Time::HiRes::time;

$::cwd = cwd;
$::user = shift @ARGV;
$::time = time;
$::directory = shift @ARGV ;
$::directory =~ s/^$::cvsrootdir\/?(.*)$/$1/;
$::cvsroot = hostname() . ":" . $::cvsrootdir;

#print "#" x 80, "\n", Dumper(\@ARGV), "#" x 80, "\n";

###
### directory/file specific actions/checks
###
if ($::directory eq "CVSROOT") {
	$::modulesfile = "./modules";

	if (-e $::modulesfile ) {
		$::modules = `cat $::modulesfile`;
		$::modules_hash = &BuildModuleHash($::modules) ;
		&CheckCircularity($::modules_hash);
		print "\nno circular references found in CVSROOT/modules\n";
	} else {
		print "\nno changes to CVSROOT/modules\n";
	}

	if (-e "./bonsai-mirrorconfig.pm") {
		require "./bonsai-mirrorconfig.pm";
		print "CVSROOT/bonsai-mirrorconfig.pm has changed and appears to be OK\n";
	} else {
		print "no changes to CVSROOT/bonsai-mirrorconfig.pm\n";
	}

	if (-e "./bonsai-accessconfig.pm") {
		require "./bonsai-accessconfig.pm";
		print "CVSROOT/bonsai-accessconfig.pm has changed and appears to be OK\n";
	} else {
		print "no changes to CVSROOT/bonsai-accessconfig.pm\n";
	}

	print "\n";
}
###
### Log checkin to database
###
open (ENTRIES, "<CVS/Entries") || die "Can't open CVS/Entries" ;
while (<ENTRIES>) {
	chomp;
	my @line = split /\//;
	next if &get('code', @line);
	my $branch = &get('tag', @line);
	my $oldrev = &get('rev', @line);
	my $file = &get('file', @line);
	if (&intersect([$file], \@ARGV)) {
#	for my $f (@ARGV) {  # Sometimes I really hate CVS
#		if ($file eq $f) {
			$::files .= $branch.":".$oldrev.":".$file." | ";
			push @{$::change_ref->{$branch}}, $file;
#		}
	}
}
close ENTRIES;
$::files =~ s/^(.*) \| $/$1/;
#print "\$files -- $::files\n";

&connect();

#print Time::HiRes::time - $::start, "\n";

my $ac = eval &retrieve("expanded_accessconfig");
&log_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'checking permissions', $::files);

#print Dumper($::change_ref);

for my $i (0..$#{$ac}) {
	if (&rule_applies($ac->[$i], $::change_ref)) {
		if ( $ac->[$i]->{'close'} && !&included($::user, $ac->[$i]->{'bless'}) ) { push @::closed, $i }
		if ( &included($::user, $ac->[$i]->{'deny'}) ) { push @::deny, $i }
		if ( $ac->[$i]->{'permit'} && !&included($::user, $ac->[$i]->{'permit'}) ) { push @::deny, $i }
	}
}

@::eol = @{&branch_eol($::cvsroot, keys(%$::change_ref))};
if (scalar @::eol) {
	my $branch = join ", ", @::eol;
	$::msg->{'denied'}->{'eol'} =~ s/#-branch-#/$branch/;

	&print_deny_header('eol');
	map { print "branch: $_\nfiles:\n"; map { print "   $::directory/$_\n" } @{$::change_ref->{$_}} } @::eol;
	print &center("", "#"), "\n";

	&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'branch eol');
	exit 1;
}

if (scalar @::closed) {
	my $branch = join ", ", &uniq(map{ $ac->[$_]->{'branch'} } @::closed);
	$::msg->{'denied'}->{'closed'} =~ s/#-branch-#/$branch/;

	&print_deny_header('closed');
	&print_blocking_rules('close', @::closed);

	&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'branch closed');
	exit 1;
}

if (scalar @::deny) {
	&print_deny_header('access');
	&print_blocking_rules('deny_msg', @::deny);

	&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'permission denied');
	exit 1;
}

&update_commit($::cwd, $::user, $::time, $::directory, $::cvsroot, 'checkin permitted');

&disconnect();

#print Time::HiRes::time - $::start, "\n\n";

###############
# subroutines #
###############
sub print_blocking_rules {
	my ($key, @array) = @_;
	my $rules = join ", ", @array;
	$rules =~ s/^([0-9, ]*[0-9]+), ([0-9]+)$/$1 and $2/;
	print "access denied by rule", $#array?"s":"" , " $rules.\n\n";
	map { print "$_. ", $ac->[$_]->{$key}?$ac->[$_]->{$key}:'<no reason given>', "\n" } @array;
	print &center("", "#"), "\n";
}

sub print_deny_header {
	my ($x) = @_;
	print &center("", "#"), "\n";
	print &center($::msg->{'denied'}->{'generic'}), "\n";
	print &center("", "="), "\n";
	print &center($::msg->{'denied'}->{$x}), "\n";
	print &center("", "-"), "\n";
}

sub center {
	my ($string, $chr, $width) = @_;
	$chr = " " unless $chr;
	$width = 50 unless $width;
	my $half = ($width -length($string))/2;
	$string = $chr x $half . $string . $chr x $half;
	return $string;
}

sub line {
	my ($chr, $width) = @_;
	$chr = "-" unless $chr;
	$width = 50 unless $width;
	return $chr x $width;
}

sub included {
	my ($user, $ph) = @_;
	my $bga = &bonsai_groups($user);
	my $uga = &unix_groups($user);
	if (&intersect($bga, $ph->{'bonsai_group'}) || 
		&intersect($uga, $ph->{'unix_group'}) ||
		&intersect([$user, "#-all-#"], $ph->{'user'})) {
		return 1;
	}
	return 0;
}

sub intersect { # find the intersection of N LIST references and return as a LIST
    my %h;
    map { map { $h{$_}++ } @$_ } @_;
    return grep { $h{$_} > $#_ } keys %h;
}

sub rule_applies {
	my ($ah, $ch_ref) = @_;
	my $return = 0;
	while (my ($b, $fa) = each (%$ch_ref)) {
		if (($::cvsroot eq $ah->{'cvsroot'} || $ah->{'cvsroot'} eq "#-all-#") &&
			($b eq $ah->{'branch'} || $ah->{'branch'} eq "#-all-#")) {
			for my $f (@$fa) {                              # I would have like to have returned out of this  
				$return += &allowed($f, $ah->{'location'}); # loop at the first &allowed file, but when i did
			}                                               # the next call to the sub had ch_ref messed up and the each failed.
		}
	}
return $return;
}
