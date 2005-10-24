#!/usr/bin/perl -wT
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
#                 Frédéric Buclin <LpSolit@gmail.com>

use strict;
use lib ".";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT :admin :params $datadir);
use Bugzilla::Config::Common;

require "globals.pl";
use vars qw(@parampanels);

my $user = Bugzilla->login(LOGIN_REQUIRED);
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

print $cgi->header();

$user->in_group('tweakparams')
  || ThrowUserError("auth_failure", {group  => "tweakparams",
                                     action => "modify",
                                     object => "parameters"});

my $action = trim($cgi->param('action') || '');
my $current_panel = $cgi->param('section') || 'core';
$current_panel =~ /^([A-Za-z0-9_-]+)$/;
$current_panel = $1;

GetVersionTable();

my $current_module;
my @panels = ();
foreach my $panel (@parampanels) {
    next if ($panel eq 'Common');
    require "Bugzilla/Config/$panel.pm";
    my @module_param_list = "Bugzilla::Config::${panel}"->get_param_list();
    my $item = { name => lc($panel),
                 current => ($current_panel eq lc($panel)) ? 1 : 0,
                 param_list => \@module_param_list,
                 sortkey => eval "\$Bugzilla::Config::${panel}::sortkey;"
               };
    push(@panels, $item);
    $current_module = $panel if ($current_panel eq lc($panel));
}

$vars->{panels} = \@panels;

if ($action eq 'save' && $current_module) {
    my @changes = ();
    my @module_param_list = "Bugzilla::Config::${current_module}"->get_param_list();

    foreach my $i (@module_param_list) {
        my $name = $i->{'name'};
        my $value = $cgi->param($name);

        if (defined $cgi->param("reset-$name")) {
            $value = $i->{'default'};
        } else {
            if ($i->{'type'} eq 'm') {
                # This simplifies the code below
                $value = [ $cgi->param($name) ];
            } else {
                # Get rid of windows/mac-style line endings.
                $value =~ s/\r\n?/\n/g;
                # assume single linefeed is an empty string
                $value =~ s/^\n$//;
            }
        }

        my $changed;
        if ($i->{'type'} eq 'm') {
            my @old = sort @{Param($name)};
            my @new = sort @$value;
            if (scalar(@old) != scalar(@new)) {
                $changed = 1;
            } else {
                $changed = 0; # Assume not changed...
                for (my $cnt = 0; $cnt < scalar(@old); ++$cnt) {
                    if ($old[$cnt] ne $new[$cnt]) {
                        # entry is different, therefore changed
                        $changed = 1;
                        last;
                    }
                }
            }
        } else {
            $changed = ($value eq Param($name))? 0 : 1;
        }

        if ($changed) {
            if (exists $i->{'checker'}) {
                my $ok = $i->{'checker'}->($value, $i);
                if ($ok ne "") {
                    ThrowUserError('invalid_parameter', { name => $name, err => $ok });
                }
            }
            push(@changes, $name);
            SetParam($name, $value);
            if (($name eq "shutdownhtml") && ($value ne "")) {
                $vars->{'shutdown_is_active'} = 1;
            }
        }
    }
    $vars->{'message'} = 'parameters_updated';
    $vars->{'param_changed'} = \@changes;

    WriteParams();
    unlink "$datadir/versioncache";
}

$template->process("admin/params/editparams.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
