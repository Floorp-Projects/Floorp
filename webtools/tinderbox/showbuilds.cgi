#!/usr/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
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
# Contributor(s): 

use strict;
use lib "@TINDERBOX_DIR@";
require 'tbglobals.pl';
require 'imagelog.pl';
require 'showbuilds.pl';

umask 002;

# Process the form arguments
my %form = &split_cgi_args();

# Show 12 hours by default
#
$::nowdate = time; 
if (not defined($::maxdate = $form{maxdate})) {
    $::maxdate = $::nowdate;
}
if ($form{showall}) {
    $::mindate = 0;
} else {
    $::default_hours = 12;
    $::hours = $::default_hours;
    $::hours = $form{hours} if $form{hours};
    $::mindate = $::maxdate - ($::hours*60*60);
}

# $rel_path is the relative path to webtools/tinderbox used for links.
# It changes to "../" if the page is generated statically, because then
# it is placed in tinderbox/$tree.
$::rel_path = ''; 

&show_tree_selector(\%form),  exit if $form{tree} eq '';
&do_quickparse(\%form),       exit if $form{quickparse};
&do_express(\%form),          exit if $form{express};
&do_rdf(\%form),              exit if $form{rdf};
&do_static(\%form),           exit if $form{static};
&do_flash(\%form),            exit if $form{flash};
&do_panel(\%form),            exit if $form{panel};
&do_hdml(\%form),             exit if $form{hdml};
&do_vxml(\%form),             exit if $form{vxml};
&do_wml(\%form),              exit if $form{wml}; 
&do_tinderbox(\%form),        exit;

# end of main
#=====================================================================
