# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::TreeState - The current state of the tree as set by
# the adminstrators.  This is used as an informational message only it
# does not directly effect the operation of the version control
# system.



# $Revision: 1.2 $ 
# $Date: 2000/11/09 19:27:20 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderHeader/TreeState.pm,v $ 
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




package TinderHeader::TreeState;

# this file is only used by TreeState_Basic.pm the package
# TreeState_Bonsai.pm uses something hard coded.

# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TinderHeader::BasicTxtHeader;

@ISA = qw(TinderHeader::BasicTxtHeader);

$VERSION = ( qw $Revision: 1.2 $ )[1];

# load the simple name of this module into TinderHeader so we can
# track the implementations provided.

$TinderHeader::NAMES2OBJS{ 'TreeState' } = 
  TinderHeader::TreeState->new();

1;
