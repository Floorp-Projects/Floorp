#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Application Registry.
#
# The Initial Developer of the Original Code is Mozilla Corporation. Portions
# created by Mozilla Corporation are Copyright (C) 2007 Mozilla Foundation. All
# Rights Reserved.
#
# Contributor(s): Dave Miller <justdave@mozilla.com>
#
# ***** END LICENSE BLOCK *****

use CGI (qw(-oldstyle_urls));
use Template;

my $bonsai_root = "../bonsai";

my $cgi = new CGI;
$cgi->charset('UTF-8');

my $template = new Template;

print $cgi->header("text/html");
$template->process("who.html.tmpl", { bonsai_root => $bonsai_root }) || die "Couldn't process template: $@";
