# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
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
 #   Max Kanat-Alexander <mkanat@bugzilla.org>
 #
 # ***** END LICENSE BLOCK *****

=cut

# Global object store and function library for Litmus

package Litmus;

use strict;

use Litmus::Template;
use Litmus::Config;
use Litmus::Error;
use Litmus::Auth;
use Litmus::CGI;

our $_request_cache = {};

# each cgi _MUST_ call Litmus->init() prior to doing anything else.
# init() ensures that the installation has not been disabled, deals with pending 
# login requests, and other essential tasks.
sub init() {
	if ($Litmus::Config::disabled) {
  	  	my $c = new CGI();
    	print $c->header();
    	print "Litmus has been shutdown by the administrator. Please try again later.";
    	exit;
	}
	# check for pending logins:
	my $c = cgi();
	if ($c->param("login_type")) {
		Litmus::Auth::processLoginForm();
	}
}

# Global Template object
sub template() {
    my $class = shift;
    request_cache()->{template} ||= Litmus::Template->create();
    return request_cache()->{template};
}

# Global CGI object
sub cgi() {
    my $class = shift;
    request_cache()->{cgi} ||= new Litmus::CGI();
    return request_cache()->{cgi};
}

sub getCurrentUser {
	return Litmus::Auth::getCurrentUser();
}

# cache of global variables for a single request only
# use me like: Litmus->request_cache->{'var'} = 'foo';
# entries here are guarenteed to get flushed when the request ends, 
# even when running under mod_perl
# from Bugzilla.pm:
sub request_cache {
    if ($ENV{MOD_PERL}) {
        my $request = Apache->request();
        my $cache = $request->pnotes();
        # Sometimes mod_perl doesn't properly call DESTROY on all
        # the objects in pnotes(), so we register a cleanup handler
        # to make sure that this happens.
        if (!$cache->{cleanup_registered}) {
             $request->push_handlers(PerlCleanupHandler => sub {
                 my $r = shift;
                 foreach my $key (keys %{$r->pnotes}) {
                     delete $r->pnotes->{$key};
                 }
             });
             $cache->{cleanup_registered} = 1;
        }
        return $cache;
    }
    return $_request_cache;
}

1;
