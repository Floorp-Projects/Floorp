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

&show_tree_selector(\%form),  exit if $form{tree} eq '';

my $mode_count=0;
foreach my $mode ('quickparse', 'express', 'rdf', 'flash',
                  'static', 'panel', 'hdml', 'vxml', 'wml') {
    $mode_count++ if defined($form{$mode});
}

if ($mode_count > 1) {
    print "Content-type: text/plain\n\n";
    print "Error: Only one mode type can be specified at a time.\n";
    exit(0);
}

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
