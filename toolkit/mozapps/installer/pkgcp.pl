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
