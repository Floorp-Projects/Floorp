# -*- Mode: perl; indent-tabs-mode: nil -*-

# Persistence::Dumper.pm - A implementation of the Persistance API
# using Data::Dumper.  I suggest using this while debugging Tinderbox
# as the data structures can be viewed in a text editor or even
# modified by hand if need be. However, most of my cpu time during
# testing is going into this function.

# dprofpp says that:

#	%58.0 of user time which is 11.05 seconds 
#	(out of 19.03 User/102.15 Elapsed Seconds)
#	was spend in 32878 calls to Data::Dumper::_dump()


# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:16:18 $ 
# $Author: mcafee%netscape.com $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 



# We need this empty package namespace for our dependency analysis, it
# gets confused if there is not a package name which matches the file
# name and in this case the file is one of several possible
# implementations.

package Persistence::Dumper;


package Persistence;


# Standard perl libraries
use Data::Dumper;


# The main function in this module, the calling structure looks like
# the call for Data::Dumper because the arguments for that module are
# more strict then Storable.

#    $data_refs: is a list of references to the data which we will save
#    $data_names: is the names of the data


sub save_structure {
  my ($data_refs, $data_names, $data_file,) = @_;

  # Create a text representation of the data we wish to save.
  # We need only eval this string to get back the data.

  my (@out) = ( 
 	       Data::Dumper->Dump( $data_refs, $data_names).
	       "1;\n"
	      );
  
  main::overwrite_file($data_file, @out);
  
  return ;
}

# I need a better abstraction for loading rather then just evaling the
# files.  This will not work when storable is an alternate method.

sub load_structure {
  my ($data_file,) = @_;

  # try to keep the require's modified variables local to this
  # function.

  my ($r);

  (-r $data_file) || (-R $data_file) ||
    die("data file: $data_file is not readable\n");

  require($data_file) ||
    die("Could not eval filename: $data_file: $!\n");


  # since we have 'required' these files we need to forget that they
  # were loaded or we may not be able  to load them again.

  delete $INC{"$dir/$file"};

  return $r;
}

1;

