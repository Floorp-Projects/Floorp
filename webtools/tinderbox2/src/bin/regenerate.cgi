#!#perl# #perlflags# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# regenerate.cgi - the webform used by administrators to close the
#		 tree, set the message of the day and stop build 
#		 columns from being shown on the default pages.


# $Revision: 1.14 $ 
# $Date: 2003/08/17 00:44:05 $ 
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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 



# Standard perl libraries

# Tinderbox libraries

use lib '#tinder_libdir#';

use TinderConfig;
use TreeData;
use Utils;
use HTMLPopUp;
use FileStructure;



#       Main        
{
    # must call set_static_vars() to ensure that we are taint safe.
    set_static_vars();

    my (%form) = HTMLPopUp::split_cgi_args(
                                           'cgi_remove_args' => ['daemon-mode'],
                                           );

    my ($tree) = $form{'tree'};

    my ($url) = (
               FileStructure::get_filename($tree, 'tree_URL').
                 '/'.
                 $FileStructure::DEFAULT_HTML_PAGE
                 );

    my ($link) = HTMLPopUp::Link(
                                 "linktxt"=>"Tinderbox page",
                                 "href"=>$url,
                                 );

    my ($out) = <<EOF;
Content-type: text/html

<TITLE>tinderbox</TITLE>
<META HTTP-EQUIV="Refresh" CONTENT="0; URL=$url">
<BODY   BGCOLOR="#FFFFFF" TEXT="#000000"
        LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
<CENTER>
<TABLE BORDER=0 WIDTH="100%" HEIGHT="100%"><TR><TD ALIGN=CENTER VALIGN=CENTER>
<FONT SIZE="+2">
Regenerating HTML now.<br>
Please refresh the page when the redirect is complete.<br>
Sending you back to the regenerated $link.<br>
</FONT>
</TD></TR></TABLE>
</CENTER>

EOF

;

    print $out;

    HTMLPopUp::regenerate_HTML_pages();

    exit 0;		
}
