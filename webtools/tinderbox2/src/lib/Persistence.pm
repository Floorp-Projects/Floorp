# -*- Mode: perl; indent-tabs-mode: nil -*-

# Persistence - an abstract interface to save the data structures to
# disk and load the data back.  An effort is made to ensure files are
# updated atomically.  This interface allows us to easily change the
# perl module we use to store the data (Data::Dumper, Storable, etc).
# You will have choices between Data::Dumper which is slow but text
# files allows great debugging capabilities and Storable (not yet
# implemented) which is much faster but binary format.  testing is
# going into this function.  Data::Dumper module has an implicit eval
# and should not be used for securty sensitive uses.



# $Revision: 1.8 $ 
# $Date: 2003/08/17 01:44:07 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Persistence.pm,v $ 
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



# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use Utils;

$VERSION = '#tinder_version#';


# Pick how you wish to the Tinderbox Persistence to be implemented:
# Uncomment only one Persistence implementation.

$IMPLS = ( ($TinderConfig::PersistenceImpl) ||
           (
            'Persistence::Dumper',
            # 'Persistence::Storable',
           )
         );

main::require_modules($IMPLS);


1;
