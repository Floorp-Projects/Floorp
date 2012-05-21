#!/usr/bin/perl -w
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# pkgcp.pl -
#
# Parse a package file and copy the specified files for a component
# from the given source directory into the given destination directory
# for packaging by the install builder.
#
# Todo:
#	- port to MacPerl
#	- change warn()s to die()s to enforce updating package files.
#	- change var names to standard form

# load modules
use Getopt::Long;
use File::Basename;
use Cwd;

# initialize variables
%components        = ();	# list of components to copy
$srcdir           = "";		# root directory being copied from
$destdir          = "";		# root directory being copied to
$package          = "";		# file listing files to copy
$os               = "";  	# os type (MacOS, MSDOS, Unix, OS/2)
$verbose          = 0;		# shorthand for --debug 1
$debug            = 0;		# controls amount of debug output
$help             = 0;		# flag: if set, print usage


# get command line options
$return = GetOptions(
			"source|s=s",           \$srcdir,
			"destination|d=s",      \$destdir,
			"file|f=s",             \$package,
			"os|o=s",               \$os,
			"component|c=s",        \@components,
			"help|h",               \$help,
			"debug=i",              \$debug,
			"verbose|v",            \$verbose,
			"flat|l",               \$flat,
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
}

# ensure that Packager.pm is in @INC, since we might not be called from
# mozilla/toolkit/mozapps/installer.
$top_path = $0;
if ( $os eq "dos" ) {
  $top_path =~ s/\\/\//g;
}
push(@INC, dirname($top_path));
require Packager;

if ( $os eq "os2" ) {
  $cwd = cwd();
  if ($srcdir !~ /^.:+/) {
    $srcdir = $cwd."/".$srcdir;
  }
  $os = "unix";
}
Packager::Copy($srcdir, $destdir, $package, $os, $flat, $help, $debug, @components);

#
# This is called by GetOptions when there are extra command line arguments
# it doesn't understand.
#
sub do_badargument
{
	warn "Warning: unknown command line option specified: @_.\n";
}


# EOF
