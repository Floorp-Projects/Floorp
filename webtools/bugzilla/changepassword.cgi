#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

use strict;

print q{Content-type: text/html

<HTML>
<HEAD>
<META HTTP-EQUIV="Refresh"
  CONTENT="0; URL=userprefs.cgi">
</HEAD>
<BODY>
This URL is obsolete.  Forwarding you to the correct one.
<P>
Going to <A HREF="userprefs.cgi">userprefs.cgi</A>
<BR>
</BODY>
</HTML>
}
