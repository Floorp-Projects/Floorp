# -*- Mode: perl; indent-tabs-mode: nil -*-

# VCDisplay::Perforce_P4DB - This is the implemenation to use if you
# have installed perforce with P4DB to let your webserver render html
# pages of your VC repository.

# $Revision: 1.4 $ 
# $Date: 2003/08/17 01:29:26 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/VCDisplay/Perforce_P4DB.pm,v $ 
# $Name:  $ 



# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 





package VCDisplay::Perforce_P4DB;


# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use HTMLPopUp;
use TreeData;

# Remember the HTMLPopUp.pm function Link accepts these arguments:

# Link(
#      "statuslinetxt"=>"", 
#      "windowtxt"=>"", 
#      "linktxt"=>"", 
#      "name"=>"", 
#      "href"=>"",
#     );

# We will pass these values through to Link (except for href which is
# created using additional arguments as needed by the specific
# function).

$VERSION = '#tinder_version#';



# This is how we create a URL to the various CGI programs.


$P4DB_URL = ( $TinderConfig::P4DB_URL || 
                "http://public.perforce.com/cgi-bin/p4db");

$QUERY = $P4DB_URL."/changeList.cgi";
$TIMEQUERY = $P4DB_URL."/filesChangedSince.cgi";

$BLAME = $P4DB_URL."/fileViewer.cgi";
$GUESS = $P4DB_URL."/fileSearch.cgi";


# convert time() format to form that p4db uses in
#    filesChangedSince.cgi

sub time2p4db {
  my ($time) = @_;
  my ($weeks, $hours, $seconds);

  {
      use integer;
      
      $time = $main::TIME - $time; 

      # fudge time a bit so that this rounding error does not happen:

      # x time2p4db( time() - $main::SECONDS_PER_WEEK )
      #     0  'WEEKS=0'
      #     1  'DAYS=6'
      #     2  'HOURS=23'
      #     3  'ignore=GO%21'

      $time -= 100;

      $weeks = $time / $main::SECONDS_PER_WEEK;
      $time = $time - ($weeks * $main::SECONDS_PER_WEEK);
      
      $days = $time / $main::SECONDS_PER_DAY;
      $time = $time - ($days * $main::SECONDS_PER_DAY);

      $hours = $time / $main::SECONDS_PER_HOUR;
      $time = $time - ($hours * $main::SECONDS_PER_HOUR);

      $seconds = $time;      
  }

  my @out;

  push @out, "WEEKS=$weeks";
  push @out, "DAYS=$days";
  push @out, "HOURS=$hours";

  # Is this necessary? I saw it during testing and 
  # I am afraid to take it out.

  push @out, 'ignore=GO!';
  
  return @out;
}


sub prepare_perforce_args {
    my (%args) = @_;

    my @url_args = ();

    ($args{'who'}) &&
        (push @url_args, "USER=".HTMLPopUp::escapeURL($args{'who'}) );

    # this is a global contraint, leave this assertion test in even
    # though it does not apply to p4db

    ($args{'mindate'} && $args{'maxdate'}) && 
    ($args{'mindate'} > $args{'maxdate'}) && 
        (die (
              "Bonsai mindate is GREATER then maxdate.\n".
              "Bonsai requires that mindate be an earlier time then maxdate.\n".
              "mindate: $args{'mindate'}: ".time2bonsai($args{'mindate'})."\n".
              "maxdate: $args{'maxdate'}: ".time2bonsai($args{'maxdate'})."\n".
              ""));

    # Perforce does not seem to allow us to specify a time interval.
    # We ignore maxdate and specify mindate only.

    ($args{'mindate'}) &&
        (push @url_args, time2p4db($args{'mindate'}) );
    
    # I am not sure of the calling sequence for FSPC, I think this is
    # correct but someone who runs this should confirm it.

    my $filespec =TreeData::Tree2Filespec($args{'tree'});

    if ($args{'file'}) {
        $filespec .= $args{'file'};
    }    

    push @url_args, "FSPC=".HTMLPopUp::escapeURL($filespec) ;

    return @url_args;
}

# create a Link to a VC file and its line number

sub source {
  my ($self, %args) = @_;

  if ($DEBUG) {
    ($args{'tree'} && $args{'file'} && $args{'line'} && $args{'linktxt'}) ||
      die("function VCDisplay::source, not called with enough arguments ");
    
    (TreeData::tree_exists($args{'tree'})) ||
      die("function VCDisplay::source, tree: $args{'tree'} does not exist\n");
  }

  my $line = $args{'line'} || 1;
  my ($goto_line) = ($line > 10 ? $line - 10 : 1 );
  
  my @url_args = prepare_perforce_args(%args);


  $args{'href'} = ("$BLAME?".join('&', @url_args));

  ($args{'line'}) &&
      ($args{'href'} .= '#L'.HTMLPopUp::escapeURL($goto_line) );

  my $output = HTMLPopUp::Link(%args);
  
  return $output;
}


# Create a Link to a VC file and its line number.

# This function is used when only the basename of the file is provided
# and some additional work must be done to figure out the file which
# was meant.  Most compilers only give the basename in their error
# messages and leave the determination of the directory to the user
# who is looking at the error log.

sub guess {
  my ($self, %args) = @_;

  if ($DEBUG) {
    ($args{'tree'} && $args{'file'} && $args{'line'} && $args{'linktxt'}) ||
      die("function VCDisplay::guess, not called with enough arguments ");
    
    (TreeData::tree_exists($args{'tree'})) ||
      die("function VCDisplay::guess, tree: $args{'tree'} does not exist\n");
  }

  my $line = $args{'line'} || 1;
  my ($goto_line) = ($line > 10 ? $line : 1 );
  
  my @url_args = prepare_perforce_args(%args);

  my ($tree) = ($args{'tree'} ||
                'default');

  $args{'href'} = ("$GUESS?".join('&', @url_args));

  # there is no way to pass a line number to the fileSearch.cgi so
  # that when the user picks a file he is transfered to the
  # fileViewer.cgi with that line number in the URL

#  ($args{'line'}) &&
#      ($args{'href'} .= '#L'.HTMLPopUp::escapeURL($goto_line) );

  my $output = HTMLPopUp::Link(%args);
  
  return $output;
}


# Query VC about checkins by a user or during a particular time or
# with a particular file.


sub query {
  my ($self, %args) = @_;

  if ($DEBUG) {
    ($args{'tree'} && $args{'linktxt'}) ||
      die("function VCDisplay::query, not called with enough arguments ");
    
    ($args{'mindate'} || $args{'who'}) || 
      die("function VCDisplay::query, not called with enough arguments ");
    
    (TreeData::tree_exists($args{'tree'})) ||
      die("function VCDisplay::query, tree: $args{'tree'} does not exist\n");
  }

  my @url_args = prepare_perforce_args(%args);

  # Near as I can tell, we can not specify BOTH a user and a time
  # since, we try and make sure that the timesince query works
  # OK. This query is the important query.

  if ($args{'mindate'}) {
      $args{'href'} = ("$TIMEQUERY?".join('&', @url_args));
  } else {
      $args{'href'} = ("$QUERY?".join('&', @url_args));
  }

  my ($output) = HTMLPopUp::Link(%args);

  return $output;
}


1;


__END__

Here are the results of my experiments with P4DB


http://public.perforce.com/cgi-bin/p4db/fileSearch.cgi?FSPC=%2F%2F...Makefile

http://public.perforce.com/cgi-bin/p4db/fileSearch.cgi?FSPC=Makefile


http://public.perforce.com/cgi-bin/p4db/filesChangedSince.cgi?FSPC=%2F%2F...&WEEKS=4&DAYS=3&HOURS=2&ignore=GO%21


http://public.perforce.com/cgi-bin/p4db/changeList.cgi?USER=aaron%5fzor&FSPC=//...

http://public.perforce.com/cgi-bin/p4db/changeList.cgi?USER=dave%5fhildebrandt&FSPC=//...

http://public.perforce.com/cgi-bin/p4db/changeList.cgi?FSPC=%2F%2F...&SEARCHDESC=always&ignore=GO%21

http://public.perforce.com/cgi-bin/p4db/fileViewer.cgi?FSPC=//guest/craig%5fmcpheeters/jam/src/Jamfile&REV=12


http://public.perforce.com/cgi-bin/p4db/fileViewer.cgi?FSPC=//guest/craig%5fmcpheeters/jam/src/Jamfile


