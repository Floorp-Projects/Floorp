# -*- Mode: perl; indent-tabs-mode: nil -*-

# Persistence::Storable.pm - A implementation of the Persistance API
# using Storable.  Storable is a commonly used Perl CPAN Module which
# writes/recovers perl datastructures to files using a binary format.
# This implemenation of Persistence should be faster then Dumper.pm
# but will be harder to debug Tinderbox if it is in use as it is not
# possible to read the data structures which are generated with a
# browser.


# $Revision: 1.4 $ 
# $Date: 2001/03/26 14:03:35 $ 
# $Author: kestes%tradinglinx.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Persistence/Storable.pm,v $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@tradinglinx.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 





package Persistence;


# Load standard perl libraries
use Storable;

# Load Tinderbox libraries

use lib '#tinder_libdir#';


# The calling structure looks like the call for Storable because the
# arguments for that module are more strict then Data::Dumper.

#    $data: is a scalar (often a list of references to the data) which
#    contains the data we will save. We can only save scalars to be
#    consistant with the storable interface.

#    $data_file: The file where the data will be stored.


sub save_structure {
  my ($data_refs, $data_file,) = @_;

  # This may be the output of a glob, make it taint safe.
  $data_file = main::extract_filename_chars($data_file);

  my ($tmpfile) = "$data_file.$main::UID";

  store($data_refs, $tmpfile);
  main::atomic_rename_file($tmpfile, $data_file);

  return ;
}



# return the value which was stored.

sub load_structure {
  my ($data_file,) = @_;

  # This may be the output of a glob, make it taint safe.
  $data_file = main::extract_filename_chars($data_file);

  (-r $data_file) || (-R $data_file) ||
    die("data file: $data_file is not readable\n");

  my ($r) = retrieve($data_file);

  return $r;
}

1;

