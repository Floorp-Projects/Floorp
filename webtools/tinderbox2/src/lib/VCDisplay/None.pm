# -*- Mode: perl; indent-tabs-mode: nil -*-

# VCDisplay::None - This is the implemenation to use if there is no
# means of displaying source code via the web.  The interface is
# highly influcenced by the Bonsai system using cvsblame cvsguess and
# cvsquery an we may need to generalize the interfaces in the future
# to accomidate more VC systems.

# $Revision: 1.10 $ 
# $Date: 2003/08/17 01:29:26 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/VCDisplay/None.pm,v $ 
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




package VCDisplay::None;

$VERSION = '#tinder_version#';



# Load standard perl libraries

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TreeData;
use HTMLPopUp;
use Utils;


# create a Link to a VC file and its line number

sub source {
  my ($self, %args) = @_;

  if ($DEBUG) {

    # we should check the data is correct, even though this
    # implementation does not use it.

    ($args{'tree'} && $args{'file'} && $args{'line'} && $args{'linktxt'}) ||
      die("function VCDisplay::source, not called with enough arguments ");
    
    (TreeData::tree_exists($args{'tree'})) ||
      die("function VCDisplay::source, tree: $args{'tree'} does not exist\n");
  }

  my $output = '';
  if ($args{"linktxt"}) {
      $output = $args{"linktxt"};
  }

  return $output;
};


# Create a Link to a VC file and its line number.

# This function is used when only the basename of the file is provided
# and some additional work must be done to figure out the file which
# was meant.  Most compilers only give the basename in their error
# messages and leave the determination of the directory to the user
# who is looking at the error log.

sub guess {
  my ($self, %args) = @_;

  if ($DEBUG) {

    # we should check the data is correct, even though this
    # implementation does not use it.

    ($args{'tree'} && $args{'file'} && $args{'line'} && $args{'linktxt'}) ||
      die("function VCDisplay::guess, not called with enough arguments ");
    
    (TreeData::tree_exists($args{'tree'})) ||
      die("function VCDisplay::guess, tree: $args{'tree'} does not exist\n");
  }

  my $output = '';
  if ($args{"linktxt"}) {
      $output = $args{"linktxt"};
  }

  return $output;
};


# Query VC about checkins by a user or during a particular time or
# with a particular file.


sub query {
  my ($self, %args) = @_;

  if ($DEBUG) {

    # we should check the data is correct, even though this
    # implementation does not use it.

    ($args{'tree'} && $args{'linktxt'}) ||
      die("function VCDisplay::query, not called with enough arguments ");
    
    (TreeData::tree_exists($args{'tree'})) ||
      die("function VCDisplay::query, tree: $args{'tree'} does not exist\n");
  }

  my $checkin_page_reference = ($args{'maxdate'} || $args{'mindate'});
  
  my ($href) = (FileStructure::get_filename($args{'tree'}, 'tree_URL').
                "/all_vc.html");

  if ($checkin_page_reference) {

      # we round the time, since there are only names in the page for
      # rounded times.
      $checkin_page_reference = main::round_time($checkin_page_reference);
      $href .= "\#$checkin_page_reference";
  }

  $args{'href'} = $href;

  my ($output) = HTMLPopUp::Link(%args);

  return $output;
};


1;
