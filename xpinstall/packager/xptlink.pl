#!/usr/bin/perl -w
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Granrose (granrose@netscape.com)
#   Jean-Jacques Enser (jj@netscape.com)
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# xptlink.pl -
#
# traverse directories created by pkgcp.pl and merge multiple .xpt files into
# a single .xpt file to improve startup performance.
#

# load modules
use Cwd;
#use File::Basename;
#use File::Copy;
#use File::Find;
#use File::Path;
#use File::stat;
use Getopt::Long;

# initialize variables
$saved_cwd        = cwd();
$component        = "";		# current component being copied
$PD               = "";		# file Path Delimiter ( /, \, or :)
$bindir           = "";		# directory for xpt files ("bin" or "viewer")
$srcdir           = "";		# root directory being copied from
$destdir          = "";		# root directory being copied to
$os               = "";  	# os type (MSDOS, Unix)
$verbose          = 0;		# shorthand for --debug 1
$debug            = 0;		# controls amount of debug output
$help             = 0;		# flag: if set, print usage

# get command line options
$return = GetOptions(	"source|s=s",           \$srcdir,
			"destination|d=s",      \$destdir,
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

$xptdirs = ();	# directories in the destination directory
$xptfiles = ();	# list of xpt files found in current directory
$file = "";		# file from file list
$files = ();	# file list for removing .xpt files
$cmdline = "";	# command line passed to system() call
$i = 0;			# generic counter var

($debug >= 1) && print "\nLinking .xpt files...\n";
($debug >= 2) && print "do_xptlink():\n";

# get list of directories on which to run xptlink
opendir (DESTDIR, "$destdir") ||
	die "Error: could not open directory $destdir.  Exiting...\n";
@xptdirs = sort ( grep (!/^\./, readdir (DESTDIR) ) );
($debug >= 4) && print "xptdirs: @xptdirs\n";
closedir (DESTDIR);

foreach $component (@xptdirs) {
	($debug >= 1) && print "[$component]\n";
	chdir ("$destdir$PD$component");
	if (( $os eq "MacOS" && -d "$PD$bindir$PD"."components") || ( -d "$bindir$PD"."components")) {
		if ( $os eq "MacOS" ) {
			chdir (":$bindir$PD"."components") ;
		} else {
			chdir ("$bindir$PD"."components") ;
		}	
		if (( -f "$component".".xpt" ) || ( -f ":$component".".xpt" )) {
			warn "Warning:  file ".$component.".xpt already exists.\n";
		}
		
		# create list of .xpt files in cwd
		($debug >= 4) && print "opendir: $destdir$PD$component$PD$bindir$PD"."components\n";
		opendir (COMPDIR, "$destdir$PD$component$PD$bindir$PD"."components") ||
			die "Error: cannot open $destdir$PD$component$PD$bindir$PD"."components.  Exiting...\n";
		($debug >= 3) && print "Creating list of .xpt files...\n";
		@files = sort ( grep (!/^\./, readdir (COMPDIR)));
		foreach $file (@files) {
			($debug >= 6) && print "$file\n";
			if ( $file =~ /\.xpt$/ ) {
				$xptfiles[$i] = $file;
				$i++;
				($debug >= 8) && print "xptfiles:\t@xptfiles\n";
			}
		}
		closedir (COMPDIR);

		# merge .xpt files into one if we found any in the dir
		if ( $#xptfiles >=1 ) {
			if ( $os eq "MacOS" ) {
				$cmdline = "'$srcdir$PD"."xpt_link' $component".".xpt.new"." @xptfiles";
			} else {
				$cmdline = "$srcdir$PD$bindir$PD"."xpt_link $component".".xpt.new"." @xptfiles";
			}
			($debug >= 4) && print "$cmdline\n";
			system($cmdline);

			# remove old .xpt files in the component directory if xpt_link succeeded
			if	(( -f "$component".".xpt.new" ) ||
				(( -f "$PD$component".".xpt.new" ) && ( $os eq "MacOS" ))) {
				($debug >= 2) && print "Deleting individual xpt files.\n";
				foreach $file (@xptfiles) {
					($debug >= 4) && print "\t$file";
					unlink ("$destdir$PD$component$PD$bindir$PD"."components"."$PD$file") ||
						warn "Warning: couldn't unlink file $file.\n";
					($debug >= 4) && print "\t\tdeleted\n";
				}
				($debug >= 4) && print "\n";
			} else {
				die "Error: xpt_link failed.  Exiting...\n";
			}
			($debug >= 2) && print "Renaming $component".".xpt.new as $component".".xpt.\n";
			if ( $os eq "MacOS" ) {
				rename (":$component".".xpt.new", ":$component".".xpt") ||
					warn "Warning: rename of $component".".xpt.new as ".$component.".xpt failed.\n";
			} else {
				rename ("$component".".xpt.new", "$component".".xpt") ||
					warn "Warning: rename of $component".".xpt.new as ".$component.".xpt failed.\n";
			}
		}
	}
	# reinitialize vars for next component
	@xptfiles = ();
	$i = 0;
}
($debug >= 1) && print "Linking .xpt files completed.\n";

chdir ($saved_cwd);

exit (0);


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
	if ( $srcdir eq "" ) {
		print "Error: source directory (--source) not specified.\n";
		$exitval += 8;
	} elsif ((! -d $srcdir) || (! -r $srcdir)) {
		print "Error: source directory \"$srcdir\" is not a directory or is unreadable.\n";
		$exitval = 1;
	}

	# check directory
	if ( $destdir eq "" ) {
		print "Error: destination directory (--destdir) not specified.\n";
		$exitval += 8;
	} elsif ((! -d $destdir) || (! -w $destdir)) {
		print "Error: destination directory \"$destdir\" is not a directory or is not writeable.\n";
		$exitval += 2;
	}

	# check OS == {mac|unix|dos}
	if ($os eq "") {
		print "Error: OS type (--os) not specified.\n";
		$exitval += 8;
	} elsif ( $os =~ /mac/i ) {
		$os = "MacOS";
		$PD = ":";
		$bindir = "viewer";
		($debug >= 4) && print " OS: $os\n";
	} elsif ( $os =~ /dos/i ) {
		$os = "MSDOS";
		$PD = "/";
		$bindir = "bin";
		($debug >= 4) && print " OS: $os\n";
	} elsif ( $os =~ /unix/i ) {
		$os = "Unix";       # can be anything but MacOS, MSDOS, or VMS
		$PD = "/";
		$bindir = "bin";
		($debug >= 4) && print " OS: Unix\n";
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
	Traverse component directory specified and merge multiple existing
	.xpt files into single new .xpt files for improved startup time.

Options:

	-s, --source <directory>
		Specifies the directory from which the component files were
		copied.  Typically, this will be the same directory used by
		pkgcp.pl.
		Required.

	-d, --destination <directory>
		Specifies the directory in which the component directories are
		located.  Typically, this will be the same directory used by
		pkgcp.pl.
		Required.

	-o, --os [dos|unix]
		Specifies which type of system this is.  Used for setting path
		delimiters correctly.
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

$0 --os unix -source /builds/mozilla/dist --destination /h/lithium/install --os unix --verbose

Note: options can be specified by either a leading '--' or '-'.

EOC
}

# EOF
