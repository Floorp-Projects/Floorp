#!/usr/bin/perl -w
# 
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
# Jonathan Granrose (granrose@netscape.com)
# 

#
# pkgcp.pl -
#
# Parse a package file and copy the specified files for a component
# from the given source directory into the given destination directory
# for packaging by the install builder.
#
#
# Things to do:
# - use options to specify arguments (i.e.  --s <sourcedir>)
#

use Cwd;
use File::Basename;
use File::Copy;
use File::Find;
use File::Path;

# check Usage
if (($#ARGV != 2) || ($ARGV[0] =~ /^-{1,2}h/i))
{
	print_usage();
	exit (1);
}

# check arguments
($srcdir, $destdir, $manifest) = @ARGV;
check_arguments();

$saved_cwd        = cwd();
$component        = "";
$altdest          = "";
$file             = "";
$lineno           = 0;
$batch            = 0;

#
# main loop
#

open (MANIFEST,"<$manifest") ||
	die "Error: couldn't open file $manifest for reading: $!.  Exiting...\n";

LINE: while (<MANIFEST>) {
	$file =    "";
	$altdest = "";
	$lineno++;

	s/\;.*//;			# it's a comment, kill it.
	s/^\s+//;			# nuke leading whitespace
	s/\s+$//;			# nuke trailing whitespace
	s/\/$//;			# strip any trailing '/'


	# it's a blank line, skip it.
	/^$/	&& do {
			next LINE;
		};

	# it's a new component
	/^\[/	&& do {
			$component = $_;
			do_component ();
			next LINE;
		};

	# make sure a component is defined before doing any copies or deletes.
	( $component eq "") &&
		die "Error: item $_ outside a component ($manifest, $lineno).  Exiting...\n";

	# delete the file or directory following the '-'
	/^-/	&& do {
			s/^-//;		# strip leading '-'
			do_delete ("$destdir/$component/$_");
			next LINE;
		};

	# file/directory being copied to different target location
	/\,/	&& do {
			/.*\,.*\,.*/ &&
				die "Error: multiple commas not allowed ($manifest, $lineno): $_.\n";
			($file, $altdest) = split (/\s*\,\s*/, $_, 2);
			$file =~ s/\/*$//;	# strip any trailing '/'
			$altdest =~ s/\/*$//;	# strip any trailing '/'
		};

	($file eq "") && ($file = $_);	# if $file not set, set it.

	# if it has wildcards, do recursive copy.
	/(?:\*|\?)/	&& do {
		do_batchcopy ("$srcdir/$file");
		next LINE;
	};

	# if it's a directory, do recursive copy.
 	(-d "$srcdir/$file") && do {
		do_batchcopy ("$srcdir/$file");
		next LINE;
	};

	# if it's a single file, copy it.
	( -f "$srcdir/$file" ) && do {
		do_copyfile ();
		$file = "";
		next LINE;
	};

	# if we hit this, dunno what it is.  abort! abort!
	die "Error: $_ unrecognized ($manifest, $lineno).  Exiting...\n";

} # LINE

close (MANIFEST);
chdir ($saved_cwd);

exit (0);


#
# Delete the given file or directory
#
sub do_delete
{
	local ($target) = $_[0];

	if (-f $target) {
		!(-w $target) &&
			die "Error: delete failed: $target not writeable ($manifest, $component, $lineno).  Exiting...\n";
		unlink ($target) ||
			die "Error: unlink() failed: $!.  Exiting...\n";
	} elsif (-d $target) {
		!(-w $target) &&
			die "Error: delete failed: $target not writeable ($manifest, $component, $lineno).  Exiting...\n";
		rmtree ($target, 0, 0) ||
			die "Error: rmtree() failed: $!.  Exiting...\n";
	} else {
		die "Error: delete failed: $target is not a file or directory ($manifest, $component, $lineno).  Exiting...\n";
	};
}


#
# Copy an individual file from the srcdir to the destdir.
# This is called by both the individual and batch/recursive copy routines,
# using $batch to check if called from do_batchcopy.
# 
sub do_copyfile
{
	local ($path)  = "";
	my ($srcfile) = "";
	
	# set srcfile correctly depending on how called
	if ($batch) {
		$srcfile = $File::Find::name;
	} else {
		$srcfile = "$srcdir/$file";
	}
	# check that source file is readable
	(!( -r $srcfile )) &&
		die "Error: file $srcfile is not readable ($manifest, $component, $lineno).\n";

	# set the destination path, if alternate destination given, use it.
	if ($altdest ne "") {
		if ($batch) {
			$path = "$destdir/$component/$altdest/$File::Find::dir";
			$path =~ s/$srcdir\/$file\///;	# rm info added by find
			$basefile = basename ($File::Find::name);
#			print "recursive find w/altdest: $path $basefile\n";
		} else {
			$path = dirname ("$destdir/$component/$altdest");
			$basefile = basename ($altdest);
#			print "recursive find w/altdest: $path $basefile\n";
		};
	} else {
		if ($batch) {
			$path = "$destdir/$component/$File::Find::dir";
			$path =~ s/$srcdir\///;	# rm $srcdir added by find()
			$basefile = basename ($File::Find::name);
#			print "recursive find w/o altdest: $path $basefile\n";
		} else {
			$path = dirname ("$destdir/$component/$file");
			$basefile = basename ($file);
#			print "recursive find w/o altdest: $path $basefile\n";
		};
	};

	# create the directory path to the file if not there yet
	if (!( -d $path)) {
		mkpath ($path, 0, 0755) ||
			die "Error: mkpath() failed: $!.  Exiting...\n";
	};

	if (-f $srcfile) {	# don't copy if it's a directory
		copy ("$srcfile", "$path/$basefile") ||
			die "Error: copy of file $srcdir/$file failed ($manifest, $component, $lineno): $!.  Exiting...\n";
	};
}

#
# Expand any wildcards, and recursively copy files and directories specified.
#
sub do_batchcopy
{
	my ($entry) = $_[0];
	my (@list) = ();

	if ($entry =~ /(?:\*|\?)/) {		# it's a wildcard,
		@list = glob($entry);		# expand it, and
		foreach $entry (@list) {
			do_batchcopy($entry);	# recursively copy results.
		}
	} else {
		$batch = 1;			# flag for do_copyfile
		find (\&do_copyfile, $entry);
		$batch = 0;
	};
}


#
# Handle new component
#
sub do_component
{
	( $component =~ /^\[.*(?:\s|\[|\])+.*\]/ ) &&	# no brackets or ws
		die "Error: malformed component $component.  Exiting...\n";
	$component =~ s/^\[(.*)\]/$1/;	# strip []

	# create component directory
	( -d "$destdir/$component" ) &&
		die "Error: component directory \"$component\" already exists.  Exiting...\n";
	mkdir ("$destdir/$component", 0755) ||
		die "Error: couldn't create component directory \"$component\": $!.  Exiting...\n";
}


#
# Check that arguments to script are valid.
#
sub check_arguments
{
	my ($exitval) = 0;	# like chmod: src==x, dest==w, manifest==r

	$srcdir =~ s/\/$//;	# strip trailing /
	$destdir =~ s/\/$//;	# strip trailing /

	if (!(-d $srcdir) || !(-r $srcdir))
	{
		print "Error: source directory $srcdir is not a directory or is unreadable.\n";
		$exitval = 1;
	}

	if (!(-d $destdir) || !(-w $destdir))
	{
		print "Error: destination directory $destdir is not a directory or is not writeable.\n";
		$exitval += 2;
	}

	if (!(-f $manifest) || !(-r $manifest))
	{
		print "Error: manifest file $manifest is not a file or is unreadable.\n";
		$exitval += 4;
	}

	if ($exitval) {
		print "Exiting...\n";
		exit ($exitval);
	}
}


#
# display usage information
#
sub print_usage
{
	print <<EOC


Usage:	$0 <source directory> <destination directory> <manifest file>
e.g.	$0 /builds/mozilla/dist /h/lithium/install packages-win

NOTE: All arguments are required.

EOC

}

# EOF
