# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::MOTD - the tree specific message of the day, set by
# the administrators.

# $Revision: 1.5 $ 
# $Date: 2003/08/17 01:39:22 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderHeader/MOTD.pm,v $ 
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



package TinderHeader::MOTD;

# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TinderHeader::BasicTxtHeader;

@ISA = qw(TinderHeader::BasicTxtHeader);

$VERSION = ( qw $Revision: 1.5 $ )[1];

# load the simple name of this module into TinderHeader so we can
# track the implementations provided.

# load the simple name of this module into TinderHeader so we can
# track the implementations provided.

$TinderHeader::NAMES2OBJS{ 'MOTD' } = 
  TinderHeader::MOTD->new();


1;
