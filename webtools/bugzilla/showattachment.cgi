#!/usr/bonsaitools/bin/perl -wT
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
#                 Jacob Steenhagen <jake@acutex.net>

use strict;

use lib qw(.);

require "CGI.pl";

# Redirect to the new interface for displaying attachments.
detaint_natural($::FORM{'attach_id'}) if defined($::FORM{'attach_id'});
my $id = $::FORM{'attach_id'} || "";
print "Status: 301 Permanent Redirect\n";
print "Location: attachment.cgi?id=$id&action=view\n\n";
exit;
 
