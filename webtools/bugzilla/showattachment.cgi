#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 David Gardiner <david.gardiner@unisa.edu.au>

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();

my @row;
if (defined $::FORM{'attach_id'}) {
    SendSQL("select mimetype, filename, thedata from attachments where attach_id = $::FORM{'attach_id'}");
    @row = FetchSQLData();
}
if (!@row) {
    print "Content-type: text/html\n\n";
    PutHeader("Bad ID");
    print "Please hit back and try again.\n";
    exit;
}
print qq{Content-type: $row[0]; name="$row[1]"; \n\n$row[2]};

    
