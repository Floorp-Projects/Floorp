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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

# Persistant object store for various objects used by 
# different modules within Litmus

package Litmus;

use strict;

use Litmus::Template;
use Litmus::Config;
use CGI;

if ($Litmus::Config::disabled) {
    my $c = new CGI();
    print $c->header();
    print "Litmus has been shutdown by the administrator. Please try again later.";
    exit;
}

my $_template;
sub template() {
    my $class = shift;
    $_template ||= Litmus::Template->create();
    return $_template;
}

1;