# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::Build - the TinderDB::Build module needs to put some
# information into the top of the status page outside of the main
# table. 

# $Revision: 1.5 $ 
# $Date: 2003/08/17 16:03:58 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderHeader/Build.pm,v $ 
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



package TinderHeader::Build;


# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TinderDB::Build;

@ISA = qw(TinderDB::Build);

$VERSION = ( qw $Revision: 1.5 $ )[1];

$TinderHeader::NAMES2OBJS{ 'Build' } = 
  TinderHeader::Build->new();

1;
