# -*- Mode: perl; indent-tabs-mode: nil -*-

# Persistence::Dumper.pm - A implementation of the Persistance API
# using Data::Dumper.  I suggest using this while debugging Tinderbox
# as the data structures can be viewed in a text editor or even
# modified by hand if need be. However, most of my cpu time during
# testing is going into this function.  This module has an implicit
# eval and should not be used for securty sensitive uses.

# dprofpp says that:

#	%58.0 of user time which is 11.05 seconds 
#	(out of 19.03 User/102.15 Elapsed Seconds)
#	was spend in 32878 calls to Data::Dumper::_dump()


# $Revision: 1.10 $ 
# $Date: 2003/08/17 01:30:15 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Persistence/Dumper.pm,v $ 
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



package Persistence;


# Standard perl libraries
use Data::Dumper;


# The calling structure looks like the call for Storable because the
# arguments for that module are more strict then Data::Dumper.

#    $data: is a scalar (often a list of references to the data) which
#    contains the data we will save. We can only save scalars to be
#    consistant with the storable interface.

#    $data_file: The file where the data will be stored.


sub save_structure {
  my ($data_refs, $data_file,) = @_;

  # This may be the output of a glob, make it taint safe.
  $data_file = main::extract_safe_filename($data_file);

  # Create a text representation of the data we wish to save.  We need
  # only eval this string to get back the data.  We pick the name of
  # the data to be '$r' so that load_structure will know what to
  # return.

  my (@out) = ( 
 	       Data::Dumper->Dump([$data_refs], ["\$r"],).
	       "1;\n"
	      );

  main::overwrite_file($data_file, @out);

  return ;
}



# return the value which was stored.

sub load_structure {
  my ($data_file,) = @_;

  # This may be the output of a glob, make it taint safe.
  $data_file = main::extract_safe_filename($data_file);

  (-r $data_file) || (-R $data_file) ||
    die("data file: $data_file is not readable\n");

  # Try to keep the require's modified variables local to this
  # function.  We know that save_structure saved the data in a
  # variable named '$r'.

  my ($r);

  # It is tempting to filter the file for security reasons only
  # allowing data to be set and no commands run.  This is not a wise
  # idea since we rely on this module for debugging other modules.
  # Should the filtering rule be buggy, which is likely, then
  # debugging is much harder.  If people are worried about security
  # they should use storable.

  require($data_file) ||
    die("Could not eval filename: $data_file: $!\n");

  # For some reason this sometimes makes a difference
  # (Tree='Project-C').  I do not know why, though I think it has to
  # do with the fact that $r is a 'my' variable.

  $r = $Persistence::r;

  # since we have 'required' these files we need to forget that they
  # were loaded or we may not be able  to load them again.

  my $basename = File::Basename::basename($data_file);
  delete $INC{"$basename"};
  delete $INC{$data_file};

  return $r;
}

1;

