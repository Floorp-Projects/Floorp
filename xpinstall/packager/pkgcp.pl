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
use File::stat;
use Getopt::Long;

# initialize variables
$saved_cwd        = cwd();
$component        = "";		# current component being copied
$PD               = "";		# file Path Delimiter ( /, \, or :)
$altdest          = "";		# alternate file destination
$line             = "";		# line being processed
$srcdir           = "";		# directory being copied from
$destdir          = "";		# destination being copied to
$package          = "";		# file listing files to copy
$os               = "";  	# os type (MacOS, MSDOS, Unix)
$verbose          = 0;		# shorthand for --debug 1
$lineno           = 0;		# line # of package file for error text
$debug            = 0;		# controls amount of debug output
$batch            = 0;		# flag: are we in batch copy mode?
$help             = 0;		# flag: if set, print usage


# get command line options
$return = GetOptions(	"source|s=s",           \$srcdir,
			"destination|d=s",      \$destdir,
			"file|f=s",             \$package,
			"os|o=s",               \$os,
			"help|h",               \$help,
			"debug=i",              \$debug,
			"verbose|v",            \$verbose,
			"<>",                   \&do_badargument
			);

# set debug level
if ($verbose && !($debug)) {
	$debug = 1;
} elsif ($debug != 0) {
	$debug = abs ($debug);
	($debug >= 2) && print "debug level: $debug\n";
}

# check usage
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
	$line =    "";
	$altdest = "";
	$lineno++;

	s/\;.*//;			# it's a comment, kill it.
	s/^\s+//;			# nuke leading whitespace
	s/\s+$//;			# nuke trailing whitespace

	($debug >= 2) && print "\n";
	($debug >= 8) && print "line $lineno:$_\n";

	# it's a blank line, skip it.
	/^$/	&& do {
			($debug >= 10) && print "blank line.\n";
			next LINE;
	};

	# it's a new component
	/^\[/	&& do {
			$component = $_;
			($debug >= 10) && print "component.\n";
			do_component ();
			next LINE;
	};

	# make sure a component is defined before doing any copies or deletes.
	( $component eq "" ) &&
		die "Error: item $_ outside a component ($package, $lineno).  Exiting...\n";

	# delete the file or directory following the '-'
	/^-/	&& do {
			s/^-//;		# strip leading '-'
			($debug >= 10) && print "delete: $destdir$PD$component$PD$_\n";
			do_delete ("$destdir$PD$component$PD$_");
			next LINE;
	};

	# file/directory being copied to different target location
	/\,/	&& do {
			/.*\,.*\,.*/ &&
				die "Error: multiple commas not allowed ($package, $lineno): $_.\n";
			($line, $altdest) = split (/\s*\,\s*/, $_, 2);
			$line =~ s/$PD*$//;	# strip any trailing delimiter
			$altdest =~ s/$PD*$//;	# strip any trailing delimiter
			($debug >= 10) && print "relocate: $line, $altdest.\n";
		};

	if ($line eq "") {
		$line = $_;	# if $line not set, set it.
	}

	# if it has wildcards, do recursive copy.
	/(?:\*|\?)/	&& do {
		($debug >= 10) && print "wildcard copy.\n";
		do_batchcopy ("$srcdir$PD$line");
		next LINE;
	};

	# if it's a single file, copy it.
	( -f "$srcdir$PD$line" ) && do {
		($debug >= 10) && print "file copy.\n";
		do_copyfile ();
		next LINE;
	};

	# if it's a directory, do recursive copy.
 	(-d "$srcdir$PD$line") && do {
		($debug >= 10) && print "directory copy.\n";
		do_batchcopy ("$srcdir$PD$line");
		next LINE;
	};

	# if we hit this, it's either a file in the package file that is
	# not in the src directory, or it is not a valid entry.
	warn "Warning: package error or possible missing file: $_ ($package, $lineno).\n";

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

	($debug >= 2) && print "do_delete():\n";

	if (-f $target) {
		!(-w $target) &&
			die "Error: delete failed: $target not writeable ($package, $component, $lineno).  Exiting...\n";
		if ($debug >= 1) {
			print "-$target (file)\n";
		}
		unlink ($target) ||
			die "Error: unlink() failed: $!.  Exiting...\n";
	} elsif (-d $target) {
		!(-w $target) &&
			die "Error: delete failed: $target not writeable ($package, $component, $lineno).  Exiting...\n";
		if ($debug >= 1) {
			print "-$target (directory)\n";
		}
		rmtree ($target, 0, 0) ||
			die "Error: rmtree() failed: $!.  Exiting...\n";
	} else {
		die "Error: delete failed: $target is not a file or directory ($package, $component, $lineno).  Exiting...\n";
	}
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
	
	($debug >= 2) && print "do_copyfile():\n";

	# set srcfile correctly depending on how called
	if ($batch) {
		$srcfile = $File::Find::name;
	} else {
		$srcfile = "$srcdir$PD$line";
	}
	# check that source file is readable
	(!( -r $srcfile )) &&
		die "Error: file $srcfile is not readable ($package, $component, $lineno).\n";

	# set the destination path, if alternate destination given, use it.
	if ($altdest ne "") {
		if ($batch) {
			$path = "$destdir$PD$component$PD$altdest$PD$File::Find::dir";
			$path =~ s/$srcdir$PD$line$PD//;	# rm info added by find
			$basefile = basename ($File::Find::name);
			($debug >= 5) &&
				print "recursive find w/altdest: $path $basefile\n";
		} else {
			$path = dirname ("$destdir$PD$component$PD$altdest");
			$basefile = basename ($altdest);
			($debug >= 5) &&
				print "recursive find w/altdest: $path $basefile\n";
		}
	} else {
		if ($batch) {
			$path = "$destdir$PD$component$PD$File::Find::dir";

			# avert your eyes now, butt-ugly hack
			if ($os eq "MSDOS") {
				$path =~ s/\\/\//g;
				$srcdir =~ s/\\/\//g;
				$PD = "/";
				$path =~ s/$srcdir$PD//g;
				$path =~ s/\//\\/g;
				$srcdir =~ s/\//\\/g;
				$PD = "\\";
			} else {
				$path =~ s/$srcdir$PD//;
			}
			# end stupid MSDOS hack

			$basefile = basename ($File::Find::name);
			($debug >= 5) &&
				print "recursive find w/o altdest: $path $basefile\n";
		} else {
			$path = dirname ("$destdir$PD$component$PD$line");
			$basefile = basename ($line);
			($debug >= 5) &&
				print "recursive find w/o altdest: $path $basefile\n";
		}
	}

	# create the directory path to the file if not there yet
	if (!( -d $path)) {
		mkpath ($path, 0, 0755) ||
			die "Error: mkpath() failed: $!.  Exiting...\n";
	}

	if (-f $srcfile) {	# don't copy if it's a directory
		if ($debug >= 1) {
			if ($batch) {
				print "$basefile\n";	# from unglob
			} else {
				print "$line\n";	# from single file
			}
			if ($debug >= 3) {
				print "copy\t$srcfile =>\n\t\t$path$PD$basefile\n";
			}
		}
		copy ("$srcfile", "$path$PD$basefile") ||
			die "Error: copy of file $srcdir$PD$line failed ($package, $component, $lineno): $!.  Exiting...\n";

		# if this is unix, set the dest file permissions
		if ($os eq "") {
			# read permissions
			$st = stat($srcfile) ||
				die "Error: can't stat $srcfile: $!  Exiting...\n";
			# set permissions
			($debug >= 2) && print "chmod ".$st->mode."\n";
			chmod ($st->mode, "$path$PD$basefile");
		}
	}
}


#
# Expand any wildcards, and recursively copy files and directories specified.
#
sub do_batchcopy
{
	my ($entry) = $_[0];
	my (@list) = ();

	($debug >= 2) && print "do_batchcopy():\n";

	if ($entry =~ /(?:\*|\?)/) {		# it's a wildcard,
		@list = glob($entry);		# expand it, and
		foreach $entry (@list) {
			do_batchcopy($entry);	# recursively copy results.
		}
	} else {
		$batch = 1;			# flag for do_copyfile
		find (\&do_copyfile, $entry);
		$batch = 0;
	}
}


#
# Handle new component
#
sub do_component
{
	($debug >= 2) && print "do_component():\n";

	( $component =~ /^\[.*(?:\s|\[|\])+.*\]/ ) &&	# no brackets or ws
		die "Error: malformed component $component.  Exiting...\n";
	$component =~ s/^\[(.*)\]/$1/;	# strip []

	if ($debug >= 1) {
		print "[$component]\n";
	}
	# create component directory
	if ( -d "$destdir$PD$component" ) {
		warn "Warning: component directory \"$component\" already exists in \"$destdir\".\n";
	} else {
		($debug >= 4) && print "mkdir $destdir$PD$component\n";
		mkdir ("$destdir$PD$component", 0755) ||
			die "Error: couldn't create component directory \"$component\": $!.  Exiting...\n";
	}
}


#
# Check that arguments to script are valid.
#
sub check_arguments
{
	my ($exitval) = 0;

	($debug >= 2) && print "check_arguments():\n";

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
		$PD = ":";
		fileparse_set_fstype ($os);
		($debug >= 4) && print "OS: $os\n";
		warn "Warning: MacOS not fully implemented/tested.\n";
	} elsif ( $os =~ /dos/i ) {
		$os = "MSDOS";
		$PD = "\\";
		fileparse_set_fstype ($os);
		($debug >= 4) && print "OS: $os\n";
		warn "Warning: MSDOS not fully implemented/tested.\n";
	} elsif ( $os =~ /unix/i ) {
		$os = "Unix";       # can be anything but MacOS, MSDOS, or VMS
		$PD = "/";
		fileparse_set_fstype ($os);
		($debug >= 4) && print "OS: Unix\n";
	} else {
		print "Error: OS type \"$os\" unknown.\n";
		$exitval += 16;
	}

	if ($exitval) {
		print "See \'$0 --help\' for more information.\n";
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
	($debug >= 2) && print "print_usage():\n";

	print <<EOC

$0
	Copy files from the source directory to component directories
	in the destination directory as specified by the package file.

Options:
	-s, --source <source directory>
		Specifies the directory from which to copy the files
		specified in the file passed via --file.
		Required.

	-d, --destination <destination directory>
		Specifies the directory in which to create the component
		directories and copy the files specified in the file passed
		via --file.
		NOTE:	This MUST be an absolute path, relative paths
			will not work!
		Required.

	-f, --file <package file>
		Specifies the file listing the components to be created in
		the destination directory and the files to copy from the
		source directory to each component directory in the
		destination directory.
		Required.

	-o, --os [dos|mac|unix]
		Specifies which type of system this is.  Used for parsing
		file specifications from the package file.
		Required.

	-h, --help
		Prints this information.
		Optional.

	--debug [1-10]
		Controls verbosity of debugging output, 10 being most verbose.
			1 : same as --verbose.
			2 : includes function calls.
			3 : includes source and destination for each copy.
		Optional.

	-v, --verbose
		Print component names and files copied/deleted.
		Optional. 


e.g.

$0 --os unix --source /builds/mozilla/dist --destination /h/lithium/install --file packages-win --os unix --verbose

Note: options can be specified by either a leading '--' or '-'.

EOC
}

# EOF
