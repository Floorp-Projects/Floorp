#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;
$|++;

use Litmus;
use Litmus::Auth;

Litmus->init();

my $title = "Log in";

Litmus::Auth::requireLogin("index.cgi");

# if we end up here, it means the user was already logged in 
# for some reason, so we should send a redirect to index.cgi:
print Litmus->cgi()->start_html(-title=>'Please Wait', 
								-head=>Litmus->cgi()->meta({-http_equiv=> 'refresh', -content=>'0;url=index.cgi'})
							   );
print Litmus->cgi()->end_html();	
	
exit;


