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
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;

# If you have an Apache2::Status handler in your Apache configuration,
# you need to load Apache2::Status *here*, so that Apache::DBI can
# report information to Apache2::Status.
#use Apache2::Status ();

# We don't want to import anything into the global scope during
# startup, so we always specify () after using any module in this
# file.

use Apache::DBI ();
use Bugzilla ();
use Bugzilla::Constants ();
use Bugzilla::CGI ();
use Bugzilla::Mailer ();
use Bugzilla::Template ();
use Bugzilla::Util ();
use CGI ();
CGI->compile(qw(:cgi -no_xhtml -oldstyle_urls :private_tempfiles
                :unique_headers SERVER_PUSH :push));
use Template::Config ();
Template::Config->preload();

# ModPerl::RegistryLoader can pre-compile all CGI scripts.
use ModPerl::RegistryLoader ();
my $rl = new ModPerl::RegistryLoader();
# Note that $cgi_path will be wrong if somebody puts the libraries
# in a different place than the CGIs.
my $cgi_path = Bugzilla::Constants::bz_locations()->{'libpath'};
foreach my $file (glob "$cgi_path/*.cgi") {
    Bugzilla::Util::trick_taint($file);
    $rl->handler($file, $file);
}

1;
