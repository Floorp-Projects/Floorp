# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::Build - the TinderDB::Build module needs to put some
# information into the top of the status page outside of the main
# table. 

# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:17:18 $ 
# $Author: mcafee%netscape.com $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 



package TinderHeader::Build;

use TinderDB::Build;

@ISA = qw(TinderDB::Build);

$VERSION = ( qw $Revision: 1.1 $ )[1];

$TinderHeader::NAMES2OBJS{ 'Build' } = 
  TinderHeader::Build->new();

1;
