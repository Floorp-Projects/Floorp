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

package PLIF::Output;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'output.'.$class->protocol or $class->SUPER::provides($service));
}

sub protocol {
    my $self = shift;
    $self->notImplemented(); # this must be overriden by descendants
}

__DATA__

sub serviceInstanceInit {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    $self->{app} = $app;
    # output classes disable implied property creation, so we use
    # propertySet() here instead of just $self->{app} = $app.
}

# if we don't implement the output handler directly, let's see if some
# output dispatcher service for this protocol does
sub implyMethod {
    my $self = shift;
    my($method, @arguments) = @_;
    if (not $self->{app}->dispatchMethod('dispatcher.output.'.$self->protocol, 'output', $method, $self, @arguments)) {
        $self->SUPER::implyMethod(@_);
    }
}
