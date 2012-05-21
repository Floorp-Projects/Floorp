#!/usr/bin/perl -w
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# xptlink.pl -
#
# traverse directories created by pkgcp.pl and merge multiple .xpt files into
# a single .xpt file to improve startup performance.
#

use Getopt::Long;

# initialize variables
$srcdir           = "";		# root directory being copied from
$destdir          = "";		# root directory being copied to
$finaldir         = "";   # where to put the final linked XPT
$verbose          = 0;		# shorthand for --debug 1
$debug            = 0;		# controls amount of debug output
$help             = 0;		# flag: if set, print usage
$xptlink          = "";   # path to the xpt_link binary

# get command line options
$return = GetOptions(	"source|s=s",           \$srcdir,
			"destination|d=s",      \$destdir,
      "final|f=s",            \$finaldir,
			"help|h",               \$help,
			"debug=i",              \$debug,
			"verbose|v",            \$verbose,
			"xptlink|x=s",          \$xptlink,
			"<>",                   \&do_badargument
			);

if ($finaldir eq "") {
  die "Error: -f is required";
}

my $bindir = "";

# remove extra slashes from $destdir
$destdir =~ s:/+:/:g;

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

($debug >= 1) && print "\nLinking .xpt files...\n";
($debug >= 2) && print "do_xptlink():\n";

# get list of directories on which to run xptlink
opendir (DESTDIR, "$destdir") ||
	die "Error: could not open directory $destdir.  Exiting...\n";
@xptdirs = sort ( grep (!/^\./, readdir (DESTDIR) ) );
($debug >= 4) && print "xptdirs: @xptdirs\n";
closedir (DESTDIR);

foreach my $component (@xptdirs) {
	($debug >= 1) && print "[$component]\n";

  print ("Checking for '$destdir/$component/$bindir"."components'\n") if $debug >= 3;

  if (-d "$destdir/$component/$bindir"."components") {
    warn "File '$destdir/$component/$bindir"."components/$component.xpt' already exists."
        if -f "$destdir/$component/$bindir"."components/$component.xpt";

		# create list of .xpt files in cwd
   my @xptfiles;

		($debug >= 4) && print "opendir: $destdir/$component/$bindir"."components\n";
		opendir (COMPDIR, "$destdir/$component/$bindir"."components") ||
			die "Error: cannot open $destdir/$component/$bindir"."components.  Exiting...\n";
		($debug >= 3) && print "Creating list of .xpt files...\n";
		my @files = sort ( grep (!/^\./, readdir (COMPDIR)));
		foreach my $file (@files) {
			($debug >= 6) && print "$file\n";
			if ( $file =~ /\.xpt$/ ) {
                            push @xptfiles, "$destdir/$component/$bindir"."components/$file";
				($debug >= 8) && print "xptfiles:\t@xptfiles\n";
			}
		}
		closedir (COMPDIR);

		# merge .xpt files into one if we found any in the dir
		if ( scalar(@xptfiles) ) {
      my ($merged, $manifest);
      $merged = "$finaldir/$component.xpt";
      $manifest = "$finaldir/interfaces.manifest";

      my @realxptfiles;
      my $realmerged;
      if ($^O eq "cygwin") {
          @realxptfiles = map {my $file = `cygpath -t mixed $_`;
                               chomp $file;
                               $file} @xptfiles;
	  $realmerged = `cygpath -t mixed $merged`;
	  chomp $realmerged;
      }
      else {
          @realxptfiles = @xptfiles;
	  $realmerged = $merged;
      }

      my $cmdline = "$xptlink $realmerged @realxptfiles";
			($debug >= 4) && print "$cmdline\n";
			system($cmdline) == 0 || die ("'$cmdline' failed");

      print "Manifest file: $manifest";
      open MANIFEST, '>>', $manifest;
      print MANIFEST "interfaces $component.xpt\n";
      close MANIFEST;
		}
  }
}
($debug >= 1) && print "Linking .xpt files completed.\n";

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

	if ($exitval) {
		print "See \'$0 --help\' for more information.\n";
		print "Exiting...\n";
		exit ($exitval);
	}

	if ($xptlink eq "") {
		$xptlink = "$srcdir/bin/xpt_link";
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
