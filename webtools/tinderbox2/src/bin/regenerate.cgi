#!#perl# #perlflags# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# regenerate.cgi - the webform used by administrators to close the
#		 tree, set the message of the day and stop build 
#		 columns from being shown on the default pages.


# $Revision: 1.1 $ 
# $Date: 2002/05/01 01:56:59 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/bin/regenerate.cgi,v $ 
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
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 



# Standard perl libraries

# Tinderbox libraries

use lib '#tinder_libdir#';

use HTMLPopUp;



#       Main        
{

    $DEFAULT_HTML_PAGE = ( ($DEFAULT_HTML_PAGE::DEFAULT_HTML_PAGE) ||
			   ("status.html"));


  HTMLPopUp::regenerate_HTML_pages();

    $out = <<EOF;
<TITLE>tinderbox</TITLE>
<META HTTP-EQUIV="Refresh" CONTENT="0; URL=$DEFAULT_HTML_PAGE">
<BODY   BGCOLOR="#FFFFFF" TEXT="#000000"
        LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
<CENTER>
<TABLE BORDER=0 WIDTH="100%" HEIGHT="100%"><TR><TD ALIGN=CENTER VALIGN=CENTER>
<FONT SIZE="+2">
Regenerating HTML now.
</FONT>
</TD></TR></TABLE>
</CENTER>

    print $out;

    exit 0;		
}
