# -*- Mode: perl; indent-tabs-mode: nil -*-

# The VCDisplay module describes how tinderbox should prepeare URLS to
# show the source tree allow VC queries.  Currently there are two
# implementations one for users of the bonsai system (cvsquery,
# cvsguess, cvsblame) and one for users who have no web based access
# to their version control repository.  In the future I will make a
# VCDisplay module for CVSWeb.


# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:13:59 $ 
# $Author: mcafee%netscape.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/VCDisplay.pm,v $ 
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




package VCDisplay;



# pick the VCDisplay module that you wish to use. 

#use VCDisplay::None;
use VCDisplay::Bonsai;

1;
