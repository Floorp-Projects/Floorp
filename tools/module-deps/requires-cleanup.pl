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
# Currently needs File::List
# http://www.cpan.org/modules/by-module/File/File-List-0.1.tar.gz
# We can probably get rid of this dependency

# If you can't place File::List in your perl library path,
# point this to where you've placed File::List:
# use lib "/home/jag/utils/";
use strict;
use File::List;

my $search = new File::List(".");
my @files  = @{ $search->find("\\.pp\$") };

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
  open MAKE, "<$dir/Makefile" or die "huh? $dir/Makefile";
  while ( <MAKE> ) {
    if ( m#^\s*srcdir\s*=\s*(.*?mozilla.*?)\s*$# ) {
      $srcdir = $1;
      last;
    }
  }
  close MAKE;

  $srcdir or die "No srcdir found";

  open MAKE, "<$srcdir/Makefile.in" or die "huh? $dir/Makefile, $srcdir/Makefile.in";

  my @lastLines; # buffer to store last three lines in (to emulate diff -B3)
  my $i;
  my $j = 3;
  my $lastLine = "";
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
      foreach my $item (@reqs) {
        if ( $item =~ m#^\$\(.*\)$# ) { # always copy "$(FOO_REQUIRES)"
          $line = $line.$item." ";
        }
        elsif ( $req->{$item} ) {     # copy if the module was referenced in .deps
          $line = $line.$item." ";
          delete($req{$item});    # this filters duplicate entries
        }
      }
      $line =~ s/ $//;
      if ( $line eq $modules ) {
        last;
      }

      # And here's the start of making it look like a cvs diff -u
      # I chose this approach because it allows a final manual editing pass over
      # the generated diff before actually modifying the Makefile.ins. We may want
      # to replace this at some point with direct editing.
      print "Index: $dir/Makefile.in\n";
      print "===================================================================\n";
      print "--- $dir/Makefile.in\t2001/01/01 00:00:00\n";
      print "+++ $dir/Makefile.in\t2001/01/01 00:00:00\n";
      print "@@ -".($i-2).",".(7+$#requiresLines)." +".($i-2).",7 @@\n";

      while ( $j ) {
        print " ".$lastLines[($i - $j--) % 3]; 
      }
      foreach my $i ( @requiresLines ) {
        print "-".$i;
      } 
      print "+$requires$line\n";

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
