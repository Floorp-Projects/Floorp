# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::Service::WWW;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'service.www' or $class->SUPER::provides($service));
}

__DATA__

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    require HTML::Entities; import HTML::Entities; # DEPENDENCY
}

sub get {
    my $self = shift;
    my($app, $uri, $referrer) = @_;
    if (not exists $self->{ua}) {
        require LWP::UserAgent; import LWP::UserAgent; # DEPENDENCY
        require HTTP::Request; import HTTP::Request; # DEPENDENCY
        my $ua = LWP::UserAgent->new();
        $ua->agent($ua->agent . ' (' . $app->name . ')');
        $ua->timeout(5); # XXX HARDCODED CONSTANT ALERT
        $ua->env_proxy();
        $self->{ua} = $ua;
    }
    my $request = HTTP::Request->new('GET', $uri);
    if (defined($referrer)) {
        $request->referer($referrer);
    }
    my $response = $self->{ua}->request($request);
    if (wantarray) {
        return ($response->content, $response);
    } else {
        return $response->content;
    }
}

sub unescapeHTML {
    my $self = shift;
    my($value) = @_;
    if (ref($value)) {
        decode_entities($$value);
    } else {
        return decode_entities($value);
    }
}
