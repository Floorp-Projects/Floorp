# -*- Mode: perl; indent-tabs-mode: nil -*-

# Persistence - an abstract interface to give our objects persistance.
# An effort is made to ensure files are updated atomically.  This
# interface allows us to easily change the perl module we use
# (Data::Dumper, Storable, etc).  You will have choices between
# Data::Dumper which is slow but text files allows great debugging
# capabilities and Storable (not yet implemented) which is much faster
# but binary format.


# $Revision: 1.3 $ 
# $Date: 2000/11/09 19:30:32 $ 
# $Author: kestes%staff.mail.com $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
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
# storable has not been tested yet
            # 'Persistence::Storable',
           )
         );

main::require_modules($IMPLS);


1;
