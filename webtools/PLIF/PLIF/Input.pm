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

package PLIF::Input;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return (($service eq 'input' and $class->applies) or $class->SUPER::provides($service));
}

sub applies {
    my $self = shift;
    $self->notImplemented(); # this must be overriden by descendants
}

sub defaultOutputProtocol {
    my $self = shift;
    $self->notImplemented(); # this must be overriden by descendants
}

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    $self->app($app); # only safe because input services are created as service instances not pure services!!!
    $self->fetchArguments();
}

sub next {
    return 0;
}

sub fetchArguments {} # stub

# returns the argument, potentially after asking the user or whatever.
sub getArgument {
    my $self = shift;
    $self->notImplemented();
}

# returns the argument if it has been provided, otherwise undef.
sub peekArgument {
    return undef;
}

# 'username' and 'password' are two out of band arguments that may be
# provided as well, they are accessed as properties of the input
# object (e.g., |if (defined($input->username)) {...}|).  Input
# services that have their own username syntaxes (e.g. AIM, ICQ)
# should have a username syntax of "SERVICE: <username>" e.g., my AIM
# username would be "AIM: HixieDaPixie". Other services, which get the
# username from the user (e.g. HTTP), should pass the username
# directly. See the user service for more details.


# XXX I don't like having these here:

sub UA {
    my $self = shift;
    $self->notImplemented();
}

sub referrer {
    my $self = shift;
    $self->notImplemented();
}

sub host {
    my $self = shift;
    $self->notImplemented();
}

sub acceptType {
    my $self = shift;
    $self->notImplemented();
}

sub acceptCharset {
    my $self = shift;
    $self->notImplemented();
}

sub acceptEncoding {
    my $self = shift;
    $self->notImplemented();
}

sub acceptLanguage {
    my $self = shift;
    $self->notImplemented();
}
