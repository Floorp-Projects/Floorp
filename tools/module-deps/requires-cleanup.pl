#!/usr/bin/perl
# ex: set tabstop=4 expandtab :
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is this file as it was released on
# September 12, 2001.
# 
# The Initial Developer of the Original Code is Michel C. C. Buijsman.
# Portions created by Michel C. C. Buijsman are Copyright (C) 2001
# Michel C. C. Buijsman.  All Rights Reserved.
# 
# Contributor(s):
#   Michel Buijsman <michel@rubberchicken.nl> (Original Author)
#   Peter Annema <jag@tty.nl>
#   Chris Seawood <seawood@netscape.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.

# Usage (Linux only for now):
#
# Do your normal Mozilla build, preferably a clobber one since
# that tends to give you more accurate .deps/*.pp files.
#
# cd objdir
# requires-cleanup.pl > req.diff
#
# The objdir is the directory where you ran |make| to build Mozilla
#
use strict;
use IO::Handle;

my @files = rfind(".", "\\.pp\$");

my $curdir = "";
my %req;
foreach my $file ( @files ) {
  $file =~ m#^(.*?)/\.deps#;
  if ( $curdir ne $1 ) {
    domakefile(\%req,$curdir) if $curdir;
    undef %req;
    $curdir = $1;
  }

  # Find and store all dist/include/<module>
  my $content;
  open FILE, "<$file" or die "can't open file: $!";
  $content .= $_ while ( <FILE> );
  close FILE;
  $content =~ s/\s+/\n/g;
  foreach my $line ( split /\n/, $content ) {
    $req{$2} = 1 if ( $line =~ m#(../)*dist/include/(.*)/.*?.h# );
  }
}
domakefile(\%req,$curdir);

sub domakefile {
  my $req = shift;
  my $dir = shift;

  # find the Makefile.in by scanning Makefile for the srcdir
  my $srcdir;
  open MAKE, "<$dir/Makefile" or die "Couldn't find file 'Makefile' in $dir";
  while ( <MAKE> ) {
    if ( m#^\s*srcdir\s*=\s*(\S*\/(mozilla|ns)\/\S*)# ) {
      $srcdir = $1;
      last;
    }
  }
  close MAKE;

  $srcdir or die "No srcdir found in file $dir/Makefile";

  open MAKE, "<$curdir/$srcdir/Makefile.in" or die "Couldn't find file 'Makefile.in' in $srcdir";

  my @lastLines; # buffer to store last three lines in (to emulate diff -B3)
  my $i;
  my $j = 3;
  my $lastLine = "";
  my $modified = 0;
  my @requiresLines = (); # array to contain REQUIRES line(s) we're removing
  while ( <MAKE> ) {
    $lastLine = $lastLine.$_;
    if ( $lastLine =~ m#^REQUIRES\s*=\s*.*\\$# ) {
      # The REQUIRES line ended with a \ which means it's continued on the next line
      $requiresLines[$#requiresLines+1] = $_;
      $lastLine =~ s/\\$//;
      chomp($lastLine);
    }
    elsif ( $lastLine =~ m#(REQUIRES\s*=\s*)(.*)$# ) {
      $requiresLines[$#requiresLines+1] = $_;
      $lastLine = "";
      my $requires = $1;
      my $modules = $2;
      my @reqs = split /\s+/, $modules;
      my $line = "";

      if ( $reqs[$#reqs] ne '$(NULL)' ) {
        $reqs[$#reqs + 1] = '$(NULL)';
      }

      for ( my $index = 0; $index < $#reqs; $index++ ) {
        my $item = $reqs[$index];
        if ( $item =~ m#^\$\(.*\)$# ) { # always copy "$(XXX)"
          # keep it
        }
        elsif ( $req->{$item} ) {       # copy if the module was referenced in .deps
          # keep it
          delete($req{$item});          # this filters duplicate entries
        }
        else {
          splice(@reqs, $index, 1);     # if it's not in .deps, don't write it out
          $modified = 1;
        }
      }

      if ( $modified == 0 ) {
        last;
      }

      foreach my $index ( 0 .. $#reqs ) {
        if ( $index == 0 ) {
          $reqs[$index] = "REQUIRES\t= ".$reqs[$index];
        }
        else {
          $reqs[$index] = "\t\t  ".$reqs[$index];
        }

        if ( $index < $#reqs ) {
          $reqs[$index] = $reqs[$index]." \\\n";
        }
        else {
          $reqs[$index] = $reqs[$index]."\n";
        }
      }

      # And here's the start of making it look like a cvs diff -u
      # I chose this approach because it allows a final manual editing pass over
      # the generated diff before actually modifying the Makefile.ins. We may want
      # to replace this at some point with direct editing.
      # Instead of replicating the diff algorithm, this code just removes all of
      # the old REQUIRES and emits the new REQUIRES, use cvs diff to see the
      # actual changes.

      print "Index: $dir/Makefile.in\n";
      print "===================================================================\n";
      print "--- $dir/Makefile.in\t2001/01/01 00:00:00\n";
      print "+++ $dir/Makefile.in\t2001/01/01 00:00:00\n";
      print "@@ -".($i-2).",".(7+$#requiresLines)." +".($i-2).",".(7+$#reqs)." @@\n";

      while ( $j ) {
        print " ".$lastLines[($i - $j--) % 3]; 
      }
      foreach my $i ( @requiresLines ) {
        print "-".$i;
      } 
      foreach my $i ( @reqs ) {
        print "+".$i;
      } 

      while ( <MAKE> ) {
        if ( $j++ == 3 ) {
          last;
        }
        print " ";
        print $_;
      }
      last;
    }
    else {
      if ( $lastLine =~ m#^\s*MODULE\s*=\s*(.*)\s*$# ) {
        # If it's on the MODULE line, no need for it on the REQUIRES line.
        # XXX if a MODULE line appears after a REQUIRES line we'll not reach this
        delete($req->{$1});
      }
      $lastLines[$i++ % 3] = $_;
      $lastLine = "";
    }
  }
  close MAKE;
}
exit(0);


sub rfind($,$) {
    my ($dirname, $regexp) = @_;
    my (@dirlist, @filelist, @sublist, $file, $dir);

    # Create lists of current files and subdirectories
    my $SDIR = new IO::Handle;
    opendir($SDIR, "$dirname") || die "opendir($dirname): $!\n";
    while ($file = readdir($SDIR)) {
	next if ($file eq "." || $file eq "..");
	if ( -d "$dirname/$file" ) {
	    push @dirlist, "$file";
	} else {
	    push @filelist, "$dirname/$file" if ($file =~ m/$regexp/);
	}
    }
    closedir($SDIR);

    # Call rfind recursively
    foreach $dir (@dirlist) {
	#print "rfind(\"$dirname/$dir\") \n";
	@sublist = rfind("$dirname/$dir", $regexp);
	foreach $file (@sublist) {
	    push @filelist, $file;
	}
    }

    return @filelist;

}
