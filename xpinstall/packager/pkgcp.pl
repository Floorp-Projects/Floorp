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

# load modules
use Cwd;
use File::Basename;
use File::Copy;
use File::Find;
use File::Path;
use Getopt::Long;

# initialize variables
$saved_cwd        = cwd();
$component        = "";
$altdest          = "";
$file             = "";
$srcdir           = "";
$destdir          = "";
$package          = "";
$os               = "";
$lineno           = 0;
$batch            = 0;
$help             = 0;
$debug            = 0;


# get command line options
$return = GetOptions(	"source|src|s=s",       \$srcdir,
			"destination|dest|d=s", \$destdir,
			"file|f=s",             \$package,
			"os|o=s",               \$os,
			"help|h",               \$help,
			"debug=i",              \$debug,
			"<>",                   \&do_badargument
			);

# set debug level
$debug = abs ($debug);
if ($debug) {
	print "debug level: $debug\n";
}

# check Usage
if (! $return)
{
	die "Error: couldn't parse command line options.  See \'$0 --help' for options.\nExiting...\n";
} else {
	check_arguments();
}


#
# main loop
#

open (MANIFEST,"<$package") ||
	die "Error: couldn't open file $package for reading: $!.  Exiting...\n";

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
		die "Error: item $_ outside a component ($package, $lineno).  Exiting...\n";

	# delete the file or directory following the '-'
	/^-/	&& do {
			s/^-//;		# strip leading '-'
			do_delete ("$destdir/$component/$_");
			next LINE;
		};

	# file/directory being copied to different target location
	/\,/	&& do {
			/.*\,.*\,.*/ &&
				die "Error: multiple commas not allowed ($package, $lineno): $_.\n";
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
	die "Error: $_ unrecognized ($package, $lineno).  Exiting...\n";

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

	($debug >= 2) && print "do_delete:\n";

	if (-f $target) {
		!(-w $target) &&
			die "Error: delete failed: $target not writeable ($package, $component, $lineno).  Exiting...\n";
		unlink ($target) ||
			die "Error: unlink() failed: $!.  Exiting...\n";
	} elsif (-d $target) {
		!(-w $target) &&
			die "Error: delete failed: $target not writeable ($package, $component, $lineno).  Exiting...\n";
		rmtree ($target, 0, 0) ||
			die "Error: rmtree() failed: $!.  Exiting...\n";
	} else {
		die "Error: delete failed: $target is not a file or directory ($package, $component, $lineno).  Exiting...\n";
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
	
	($debug >= 2) && print "do_copyfile:\n";

	# set srcfile correctly depending on how called
	if ($batch) {
		$srcfile = $File::Find::name;
	} else {
		$srcfile = "$srcdir/$file";
	}
	# check that source file is readable
	(!( -r $srcfile )) &&
		die "Error: file $srcfile is not readable ($package, $component, $lineno).\n";

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
		($debug >= 5) && print "\tcopy\t$srcfile => $path/$basefile\n";
		copy ("$srcfile", "$path/$basefile") ||
			die "Error: copy of file $srcdir/$file failed ($package, $component, $lineno): $!.  Exiting...\n";
	};
}


#
# Expand any wildcards, and recursively copy files and directories specified.
#
sub do_batchcopy
{
	my ($entry) = $_[0];
	my (@list) = ();

	($debug >= 2) && print "do_batchcopy:\n";

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
	($debug >= 2) && print "do_component:\n";

	( $component =~ /^\[.*(?:\s|\[|\])+.*\]/ ) &&	# no brackets or ws
		die "Error: malformed component $component.  Exiting...\n";
	$component =~ s/^\[(.*)\]/$1/;	# strip []

	# create component directory
	if ( -d "$destdir/$component" ) {
		warn "Warning: component directory \"$component\" already exists in \"$destdir\".\n";
	} else {
		mkdir ("$destdir/$component", 0755) ||
			die "Error: couldn't create component directory \"$component\": $!.  Exiting...\n";
	};
}


#
# Check that arguments to script are valid.
#
sub check_arguments
{
	my ($exitval) = 0;

	($debug >= 2) && print "check_arguments:\n";

	# if --help print usage
	if ($help) {
		print_usage();
		exit (1);
	}

	# make sure required variables are set:
	# check source directory
	if ($srcdir eq "") {
		print "Error: source directory (--source) not specified.\n";
		$exitval += 8;
	} elsif (!(-d $srcdir) || !(-r $srcdir)) {
		print "Error: source directory \"$srcdir\" is not a directory or is unreadable.\n";
		$exitval = 1;
	}

	# check destination directory
	if ($destdir eq "") {
		print "Error: destination directory (--destination) not specified.\n";
		$exitval += 8;
	} elsif (!(-d $destdir) || !(-w $destdir)) {
		print "Error: destination directory \"$destdir\" is not a directory or is not writeable.\n";
		$exitval += 2;
	}

	# check package file
	if ($package eq "") {
		print "Error: package file (--file) not specified.\n";
		$exitval += 8;
	} elsif (!(-f $package) || !(-r $package)) {
		print "Error: package file \"$package\" is not a file or is unreadable.\n";
		$exitval += 4;
	}

	# check OS == {mac|unix|dos}
	if ($os eq "") {
		print "Error: OS type (--os) not specified.\n";
		$exitval += 8;
	} elsif ( $os =~ /mac/i ) {
		$os = "MacOS";
	} elsif ( $os =~ /dos/i ) {
		$os = "MSDOS";
	} elsif ( $os =~ /unix/i ) {	# null because Unix is default for
		$os = "";		# fileparse_set_fstype()
	} else {
		print "Error: OS type \"$os\" unknown.\n";
		$exitval += 16;
	}

	if ($exitval) {
		print "\See '$0 --help\' for more information.\n";
		print "Exiting...\n";
		exit ($exitval);
	}
}


#
# This is called by GetOptions when there are extra command line arguments
# it doesn't understand.
#
sub do_badargument
{
	warn "Warning: unknown command line option specified: @_.\n";
}


#
# display usage information
#
sub print_usage
{
	($debug >= 2) && print "print_usage:\n";

	print <<EOC

$0
	Copy files from the source directory to component directories
	in the destination directory as specified by the package file.

Options:
	--source <source directory>
		Specifies the directory from which to copy the files
		specified in the file passed via --file.  Can also be
		abbreviated to: --src, --s.
		Required.

	--destination <destination directory>
		Specifies the directory in which to create the component
		directories and copy the files specified in the file passed
		via --file.  Can also be abbreviated to: --dest, --d.
		Required.

	--file <package file>
		Specifies the file listing the components to be created in
		the destination directory and the files to copy from the
		source directory to each component directory in the
		destination directory.  Can also be abbreviated to: --f.
		Required.

	--os [dos|mac|unix]
		Specifies which type of system this is.  Used for parsing file
		specifications from the package file.  Can also be abbreviated
		to: --o.
		Required.

	--help
		Prints this information.  Can also be abbreviated to: --h
		Optional.

	--debug [1-10]
		Controls verbosity of debugging output, 10 being most verbose.
		Optional.

e.g.

$0 --os unix --source /builds/mozilla/dist --destination /h/lithium/install --file packages-win

Note: options can be specified by either a leading '--' or '-'.

EOC
}

# EOF
