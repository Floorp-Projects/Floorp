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

package PLIF::Output::Generic::Email;
use strict;
use vars qw(@ISA);
use PLIF::Service;
use Net::SMTP; # DEPENDENCY
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'output.generic.email' or
            $service eq 'protocol.smtp' or
            $service eq 'setup.configure' or
            $service eq 'dataSource.configuration.client' or
            $class->SUPER::provides($service));
}

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    eval {
        $app->getService('dataSource.configuration')->getSettings($app, $self, 'protocol.smtp');
    };
    if ($@) {
        $self->dump(9, "failed to get the SMTP configuration, not going to bother to connect: $@");
        $self->handle(undef);
    } else {
        $self->open();
    }
}

sub open {
    my $self = shift;
    { local $^W = 0; # XXX shut up warnings in Net::SMTP
    $self->handle(Net::SMTP->new($self->host)); }
    if (not defined($self->handle)) {
        $self->warn(4, 'Could not create the SMTP handle');
    }
}

sub close {
    my $self = shift;
    if (defined($self->handle)) {
        $self->handle->quit();
    }
}

# output.generic.email
sub output {
    my $self = shift;
    my($app, $session, $string) = @_;
    $self->assert(defined($self->handle), 1, 'No SMTP handle, can\'t send mail');
    $self->assert($self->handle->mail($self->from), 1, 'Could not start sending mail');
    $self->assert($self->handle->to($session->getAddress('email')), 1, 'Could not set mail recipient (was going to send to '.($session->getAddress('email')).')');
    $self->assert($self->handle->data($string), 1, 'Could not send mail body');
}

# protocol.smtp
sub checkAddress {
    my $self = shift;
    my($app, $username) = @_;
    $self->assert(defined($self->handle), 1, 'No SMTP handle, can\'t check address');
    my $result = $self->handle->verify($username);
    return $result;
}

sub DESTROY {
    my $self = shift;
    $self->SUPER::DESTROY(@_);
}

# dataSource.configuration.client
sub settings {
    return qw(host address);
}

# setup.configure
sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure protocol.smtp...');
    $app->output->setupProgress('protocol.smtp');
    $self->close();

    my $value;

    $value = $self->host;
    if (not defined($value)) {
        $value = 'localhost';
    }
    $value = $app->input->getArgument('protocol.smtp.host', $value);
    if (not defined($value)) {
        return 'protocol.smtp.host';
    }
    $self->host($value);

    $value = $self->address;
    if (defined($value)) {
        # default to existing value
        $value = $app->input->getArgument('protocol.smtp.address', $value);
    } else {
        # don't default to anything as our guess is most likely going
        # to be wrong: "$USER@`hostname`" is rarely correct.
        $value = $app->input->getArgument('protocol.smtp.address');
    }
    if (not defined($value)) {
        return 'protocol.smtp.address';
    }
    $self->address($value);

    $self->open();
    $app->getService('dataSource.configuration')->setSettings($app, $self, 'protocol.smtp');
    $self->dump(9, 'done configuring protocol.smtp');
    return;
}

sub hash {
    my $self = shift;
    return {
            'address' => $self->address,
           };
}
