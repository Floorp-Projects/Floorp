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
# The Original Code is NewsBot
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
#
# Contributor(s): Dawn Endico <endico@mozilla.org>

# wraps raw html file with the mozilla.org logo and menu.

require 5.00397;
use strict;

require "/e/stage-docs/mozilla-org/tools/wrap.pl";

dowrap("/e/stage-docs/mozilla-org/html/template.html", 
       "/opt/newsbot/newsbot.html", 
       "/opt/newsbot/wrapped.html");
