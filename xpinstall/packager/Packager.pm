#!perl -w
package Packager;

require 5.004;

use strict;
use File::stat;
use Cwd;
use File::Basename;
use File::Copy;
use File::Find;
use File::Path;
use File::stat;
require Exporter;

use vars qw(@ISA @EXPORT);

# Package that generates a jar manifest from an input file

@ISA      = qw(Exporter);
@EXPORT   = qw(
                Copy
              );

# initialize variables
my($saved_cwd)        = cwd();
my($component)        = "";		# current component being copied
my(@components)       = ();	  # list of components to copy
my($components)       = "";   # string version of @components
my($PD)               = "";		# file Path Delimiter ( /, \, or :)
my($altdest)          = "";		# alternate file destination
my($line)             = "";		# line being processed
my($srcdir)           = "";		# root directory being copied from
my($destdir)          = "";		# root directory being copied to
my($package)          = "";		# file listing files to copy
my($os)               = "";		# os type (MacOS, MSDOS, Unix)
my($lineno)           = 0;		# line # of package file for error text
my($debug)            = 0;		# controls amount of debug output
my($dirflag)          = 0;		# flag: are we copying a directory?
my($help)             = 0;		# flag: if set, print usage
my($flat)             = 0;    # copy everything into the package dir, not into separate
                              #   component dirs

#
# Copy
#
# Loop over each line in the specified manifest, copying into $destdir
#

sub Copy {
  ($srcdir, $destdir, $package, $os, $flat, $help, $debug, @components) = @_;
  
  check_arguments();
  open (MANIFEST,"<$package") ||
  	die "Error: couldn't open file $package for reading: $!.  Exiting...\n";

  LINE: while (<MANIFEST>) {
  	$line =    "";
  	$altdest = "";
  	$lineno++;

  	s/\\/\//g if ($os eq "MSDOS");  # Convert to posix path
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
  			($debug >= 10) && print "component.\n";
  			$component = $_;
  			do_component();
  			next LINE;
  	};

  	# if we find a file before we have a component and we are in flat mode,
  	# copy it - allows for flat only files (installed-chrome.txt)
  	if (( $component eq "" ) && ($components eq "" ) && (!$flat)) {
  		next LINE;
  	}

  	# skip line if we're only copying specific components and outside
  	# those components
  	if (( $component eq "" ) && ($components ne "" )) {
  		($debug >= 10) && print "Not in specifed component.  Skipping $_\n";
  		next LINE;
  	}
  	if ($line eq "") {
  		$line = $_;				# if $line not set, set it.
  	}

  	if ($os ne "MSDOS") {		# hack - need to fix for dos
  		$line =~ s/^$PD//;		# strip any leading path delimiter
  	}

  	# delete the file or directory following the '-'
  	/^-/	&& do {
  			$line =~ s/^-//;		# strip leading '-'
  			($debug >= 10) && print "delete: $destdir$PD$component$PD$line\n";
  			do_delete ("$destdir", "$component", "$line");
  			next LINE;
  	};

  	# file/directory being copied to different target location
  	/\,/	&& do {
  			/.*\,.*\,.*/ &&
  				die "Error: multiple commas not allowed ($package, $lineno): $_.\n";
  			($line, $altdest) = split (/\s*\,\s*/, $line, 2);
  			$line =~ s/$PD*$//;	# strip any trailing path delimiters
  			$altdest =~ s/$PD*$//;	# strip any trailing delimiter
  			($debug >= 10) && print "relocate: $line => $altdest.\n";
  	};

  	# if it has wildcards, do recursive copy.
  	/(?:\*|\?)/	&& do {
  		($debug >= 10) && print "wildcard copy.\n";
  		if ($os eq "MacOS") {
  			if (/:\*$/) {
  				($debug >= 4) && print "Mac and wildcard is \":*\" so strip and copy entire directory.\n";
  				$line =~ s/:\*$//;		# Mac doesn't like wildcards, so strip off any ":*" at the end of the line
  										# and copy the whole directory.
  				do_copydir ("$srcdir$PD$line");
  			}
  		} else {
  				do_wildcard ("$srcdir$PD$line");
  		}
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
  		do_copydir ("$srcdir$PD$line");
  		next LINE;
  	};

  	# if we hit this, it's either a file in the package file that is
  	# not in the src directory, or it is not a valid entry.
    warn "Warning: package error or possible missing or unnecessary file: $line ($package, $lineno).\n";

  } # LINE

  close (MANIFEST);
  chdir ($saved_cwd);

}

#
# Delete the given file or directory
#
sub do_delete
{
	my ($targetpath) = $_[0];
	my ($targetcomp) = $_[1];
	my ($targetfile) = $_[2];
	my ($target) = ($flat) ? "$targetpath$PD$targetfile" : "$targetpath$PD$targetcomp$PD$targetfile";

	($debug >= 2) && print "do_delete():\n";
	($debug >= 1) && print "-$targetfile\n";

	if ( -f $target ) {
		(! -w $target ) &&
			die "Error: delete failed: $target not writeable ($package, $component, $lineno).  Exiting...\n";
		($debug >= 4) && print " unlink($target)\n";
		unlink ($target) ||
			die "Error: unlink() failed: $!.  Exiting...\n";
	} elsif ( -d $target ) {
		(! -w $target ) &&
			die "Error: delete failed: $target not writeable ($package, $component, $lineno).  Exiting...\n";
		($debug >= 4) && print " rmtree($target)\n";
		rmtree ($target, 0, 0) ||
			die "Error: rmtree() failed: $!.  Exiting...\n";
	} else {
		warn "Warning: delete failed: $target is not a file or directory ($package, $component, $lineno).\n";
	}
}


#
# Copy an individual file from the srcdir to the destdir.
#
# This is called by both the individual and batch/recursive copy routines,
# using $dirflag to check if called from do_copydir.  Batch copy can pass in
# directories, so be sure to check first and break if it isn't a file.
#
sub do_copyfile
{
	my ($destpath)     = "";  # destination directory path
	my ($destpathcomp) = "";  # ditto, but possibly including component dir
	my ($destname)     = "";  # destination file name
	my ($destsuffix)   = "";  # destination file name suffix
	my ($altpath)      = "";  # alternate destination directory path
	my ($altname)      = "";  # alternate destination file name
	my ($altsuffix)    = "";  # alternate destination file name suffix
	my ($srcpath)      = "";  # source file directory path
	my ($srcname)      = "";  # source file name
	my ($srcsuffix)    = "";  # source file name suffix
	
	($debug >= 2) && print "do_copyfile():\n";

	# set srcname correctly depending on how called
	if ( $dirflag ) {
		($srcname, $srcpath, $srcsuffix) = fileparse("$File::Find::name", '\..*?$');
	} else {
		($srcname, $srcpath, $srcsuffix) = fileparse("$srcdir$PD$line", '\..*?$');
	}

	($debug >= 4) && print " fileparse(src): $srcpath $srcname $srcsuffix\n";

	# return if srcname is a directory from do_copydir
	if ( -d "$srcpath$srcname$srcsuffix" ) {
		($debug >= 10) && print " return: $srcpath$srcname$srcsuffix is a directory\n";
		return;
	}

	# set the destination path, if alternate destination given, use it.
	if ($flat) {
	  $destpathcomp = "$destdir";
	} else {
	  # double-check that we have a valid component before appending the path
	  # delimiter. If $component is empty, we'd end up placing an extra : at 
	  # the end, which unix ignores, but on mac sends us up an extra level.
	  if ( $component ne "" ) {
	    $destpathcomp = "$destdir$PD$component";
	  }
	  else {
	    $destpathcomp = "$destdir";
	  }
 	}
	if ( $altdest ne "" ) {
		if ( $dirflag ) {	# directory copy to altdest
			($destname, $destpath, $destsuffix) = fileparse("$destpathcomp$PD$altdest$PD$File::Find::name", '\..*?$');
			# Todo: add MSDOS hack
			$destpath =~ s/$srcdir$PD$line$PD//;	# rm info added by find
			($debug >= 5) &&
				print " dir copy to altdest: $destpath $destname $destsuffix\n";
		} else {	# single file copy to altdest
			($destname, $destpath, $destsuffix) = fileparse("$destpathcomp$PD$altdest", '\..*?$');
			($debug >= 5) &&
				print " file copy to altdest: $destpath $destname $destsuffix\n";
		}
	} else {
		if ( $dirflag ) {	# directory copy, no altdest
			($destname, $destpath, $destsuffix) = fileparse("$destpathcomp$PD$File::Find::name", '\..*?$');

			# avert your eyes now, butt-ugly hack
			if ( $os eq "MSDOS" ) {
				$destpath =~ s/\\/\//g;
				$srcdir =~ s/\\/\//g;
				$PD = "/";
				$destpath =~ s/$srcdir$PD//g;
				$destpath =~ s/\\/\//g;
				$srcdir =~ s/\\/\//g;
				$PD = "/";
			} else {
				$destpath =~ s/$srcdir$PD//;
			}
			# end stupid MSDOS hack

			($debug >= 5) &&
				print " dir copy w/o altdest: $destpath $destname $destsuffix\n";
		} else {	# single file copy, no altdest
			($destname, $destpath, $destsuffix) = fileparse("$destpathcomp$PD$line", '\..*?$');
			($debug >= 5) &&
				print " file copy w/o altdest: $destpath $destname $destsuffix\n";
		}
	}

	# create the destination path if it doesn't exist
	if (! -d "$destpath" ) {
		($debug >= 5) && print " mkpath($destpath)\n";
		# For OS/2 - remove trailing '/'
		chop($destpath);
		mkpath ($destpath, 0, 0755) ||
			die "Error: mkpath() failed: $!.  Exiting...\n";
		# Put delimeter back for copying...
		$destpath = "$destpath$PD"; 
	}

	# path exists, source and destination known, time to copy
	if ((-f "$srcpath$srcname$srcsuffix") && (-r "$srcpath$srcname$srcsuffix")) {
		if ( $debug >= 1 ) {
			if ( $dirflag ) {
				print "$destname$destsuffix\n";	# from unglob
			} else {
				print "$line\n";		# from single file
			}
			if ( $debug >= 3 ) {
				print " copy\t$srcpath$srcname$srcsuffix =>\n\t\t$destpath$destname$destsuffix\n";
			}
		}
		unlink("$destpath$destname$destsuffix") if ( -e "$destpath$destname$destsuffix");
		copy ("$srcpath$srcname$srcsuffix", "$destpath$destname$destsuffix") ||
			die "Error: copy of file $srcpath$srcname$srcsuffix failed ($package, $component, $lineno): $!.  Exiting...\n";

		# if this is unix, set the dest file permissions
		# read permissions
		my($st) = stat("$srcpath$srcname$srcsuffix") ||
		    die "Error: can't stat $srcpath$srcname$srcsuffix: $!  Exiting...\n";
		# set permissions
		($debug >= 2) && print " chmod ".$st->mode." $destpath$destname$destsuffix\n";
		chmod ($st->mode, "$destpath$destname$destsuffix") ||
		    warn "Warning: chmod of $destpath$destname$destsuffix failed: $!.  Exiting...\n";
	    } else {
		warn "Error: file $srcpath$srcname$srcsuffix is not a file or is not readable ($package, $component, $lineno).\n";
	    }
}


#
# Expand any wildcards and copy files and/or directories
#
# todo: pass individual files to do_copyfile, not do_copydir
#
sub do_wildcard
{
	my ($entry) = $_[0];
	my (@list)  = ();
	my ($item)  = "";

	($debug >= 2) && print "do_wildcard():\n";

	if ( $entry =~ /(?:\*|\?)/ ) {		# it's a wildcard,
		if ($os eq "MacOS") {
			warn "Warning: globbing on Mac not supported.\nWorkaround is to copy entire directory.\n";
			@list = <$entry>;
		} else {
			@list = glob($entry);			# expand it
		}
		($debug >= 4) && print " glob: $entry => @list\n";

		foreach $item ( @list ) {			# now copy each item in list
			if ( -f $item ) {
				($debug >= 10) && print " do_copyfile: $item\n";

				# glob adds full path to item like find() in copydir so
				# take advantage of existing code in copyfile by using
				# $dirflag and $File::Find::name.

				$File::Find::name = $item;
				$dirflag = 1;
				do_copyfile();
				$dirflag = 0;
				$File::Find::name = "";
			} elsif ( -d $item ) {
				($debug >= 10) && print " do_copydir($item)\n";
				do_copydir ($item);
			} else {
				warn "Warning: $item is not a file or directory ($package, $component, $lineno).  Skipped...\n";
			}
		}
	}
}

#
# Recursively copy directories specified.
#
sub do_copydir
{
	my ($entry) = $_[0];

	$dirflag = 1;			# flag indicating directory copy in progress

	($debug >= 2) && print "do_copydir():\n";

	if (! -d "$entry" ) {
		warn "Warning: $entry is not a directory ($package, $component, $lineno).  Skipped...\n";
	}

	($debug >= 4) && print " find($entry)\n";

	find (\&do_copyfile, $entry);

	$dirflag = 0;
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

	if ( $components ne "") {
		if ( $components =~ /$component/ ) {
			($debug >= 10) && print "Component $component is in $components.\n";
		} else {
			($debug >= 10) && print "Component $component not in $components.\n";
			$component = "";
			return;		# named specific components and this isn't it
		}
	}

	if ($debug >= 1) {
		print "[$component]\n";
	}
	# create component directory
        if (!$flat) {
	  if ( -d "$destdir$PD$component" ) {
	    warn "Warning: component directory \"$component\" already exists in \"$destdir\".\n";
	  } else {
	    ($debug >= 4) && print " mkdir $destdir$PD$component\n";
	    mkdir ("$destdir$PD$component", 0755) ||
	      die "Error: couldn't create component directory \"$component\": $!.  Exiting...\n";
	  }
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
	if ( $srcdir eq "" ) {
		print "Error: source directory (--source) not specified.\n";
		$exitval += 8;
	} elsif ((! -d $srcdir) || (! -r $srcdir)) {
		print "Error: source directory \"$srcdir\" is not a directory or is unreadable.\n";
		$exitval = 1;
	}

	# check destination directory
	if ( $destdir eq "" ) {
		print "Error: destination directory (--destination) not specified.\n";
		$exitval += 8;
	} elsif ((! -d $destdir) || (! -w $destdir)) {
		print "Error: destination directory \"$destdir\" is not a directory or is not writeable.\n";
		$exitval += 2;
	}

	# check destdir not a subdir of srcdir
	# hack - workaround for bug 14558 that should be fixed eventually.
	if (0) {	# todo - write test
		print "Error: destination directory must not be subdirectory of the source directory.\n";
		$exitval += 32;
	}

	# check package file
	if ( $package eq "" ) {
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
	} elsif ( $os =~ /dos/i ) {
		$os = "MSDOS";
		$PD = "/";
		fileparse_set_fstype ($os);
	} elsif ( $os =~ /unix/i ) {
		$os = "Unix";       # can be anything but MacOS, MSDOS, or VMS
		$PD = "/";
		fileparse_set_fstype ($os);
	} else {
		print "Error: OS type \"$os\" unknown.\n";
		$exitval += 16;
	}

	# turn components array into a string for regexp
	if ( @components > 0 ) {
		$components = join (",",@components);
	} else {
		$components = "";
	}

	if ($debug > 4) {
		print ("source dir:\t$srcdir\ndest dir:\t$destdir\npackage:\t$package\nOS:\t$os\ncomponents:\t$components\n");
	}

	if ($exitval) {
		print "See \'$0 --help\' for more information.\n";
		print "Exiting...\n";
		exit ($exitval);
	}

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
		Required.

NOTE:	Source and destination directories must be absolute paths.
	Relative paths will NOT work.  Also, the destination directory
	must NOT be a subdirectory of the source directory.

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

	-c, --component <component name>
		Specifies a specific component in the package file to copy
		rather than copying all the components in the package file.
		Can be used more than once for multiple components (e.g.
		"-c browser -c mail" to copy mail and news only).
		Optional.

	-l, --flat
                Suppresses creation of components dirs, but stuffes everything
                directly into the package destination dir. This is useful
                for creating tarballs.

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
