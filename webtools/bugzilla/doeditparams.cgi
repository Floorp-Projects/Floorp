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
#                 J. Paul Reed <preed@sigkill.com>

use diagnostics;
use strict;

use lib qw(.);

require "CGI.pl";
require "defparams.pl";

# Shut up misguided -w warnings about "used only once":
use vars %::param,
    %::param_default,
    @::param_list;

ConnectToDatabase();
confirm_login();

print "Content-type: text/html\n\n";

if (!UserInGroup("tweakparams")) {
    print "<H1>Sorry, you aren't a member of the 'tweakparams' group.</H1>\n";
    print "And so, you aren't allowed to edit the parameters.\n";
    PutFooter();
    exit;
}


PutHeader("Saving new parameters");

foreach my $i (@::param_list) {
#    print "Processing $i...<BR>\n";
    if (exists $::FORM{"reset-$i"}) {
        if ($::param_type{$i} eq "s") {
            my $index = get_select_param_index($i, $::param_default{$i}->[1]);
            die "Param not found for '$i'" if ($index eq undef);
            $::FORM{$i} = $index; 
        }
        elsif ($::param_type{$i} eq "m") {
            # For 'multi' selects, default is the 2nd anon array of the default
            @{$::MFORM{$i}} = ();
            foreach my $defaultPrm (@{$::param_default{$i}->[1]}) {
                my $index = get_select_param_index($i, $defaultPrm); 
                die "Param not found for '$i'" if ($index eq undef);
                push(@{$::MFORM{$i}}, $index); 
            }
        }
        else {
            $::FORM{$i} = $::param_default{$i};
        }
    }
    $::FORM{$i} =~ s/\r\n?/\n/g;   # Get rid of windows/mac-style line endings.
    $::FORM{$i} =~ s/^\n$//;      # assume single linefeed is an empty string
    if ($::FORM{$i} ne Param($i)) {
        if (defined $::param_checker{$i}) {
            my $ref = $::param_checker{$i};
            my $ok = &$ref($::FORM{$i}, $i);
            if ($ok ne "") {
                print "New value for $i is invalid: $ok<p>\n";
                print "Please hit <b>Back</b> and try again.\n";
                PutFooter();
                exit;
            }
        }
        print "Changed $i.<br>\n";
#      print "Old: '" . url_quote(Param($i)) . "'<BR>\n";
#      print "New: '" . url_quote($::FORM{$i}) . "'<BR>\n";
        if ($::param_type{$i} eq "s") {
            $::param{$i} = $::param_default{$i}->[0]->[$::FORM{$i}];
        }
        elsif ($::param_type{$i} eq "m") {
            my $multiParamStr = "[ ";
            foreach my $chosenParam (@{$::MFORM{$i}}) {
                $multiParamStr .= 
                 "'$::param_default{$i}->[0]->[$chosenParam]', ";
            }
            $multiParamStr .= " ]";

            $::param{$i} = $multiParamStr;
        }
        else {
            $::param{$i} = $::FORM{$i};
        }
    }
}


WriteParams();

unlink "data/versioncache";
print "<PRE>";
system("./syncshadowdb", "-v");
print "</PRE>";

print "OK, done.<p>\n";
print "<a href=editparams.cgi>Edit the params some more.</a><p>\n";
print "<a href=query.cgi>Go back to the query page.</a>\n";
    
PutFooter();
