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
use PLIF::Exception;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'output.generic.email' or
            $service eq 'protocol.email' or
            $service eq 'setup.configure' or
            $service eq 'dataSource.configuration.client' or
            $class->SUPER::provides($service));
}

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    require Net::SMTP; import Net::SMTP; # DEPENDENCY
    try {
        $app->getService('dataSource.configuration')->getSettings($app, $self, 'protocol.email');
    } except {
        $self->dump(9, "failed to get the SMTP configuration, not going to bother to connect: $@");
        $self->handle(undef);
    } otherwise {
        $self->open();
    }
}

sub open {
    my $self = shift;
    my($app, $session, $string) = @_;
    try {
        local $SIG{ALRM} = sub { raise PLIF::Exception::Alarm };
        local $^W = 0; # XXX shut up warnings in Net::SMTP
        $self->handle(Net::SMTP->new($self->host, 'Timeout' => $self->timeout));
        alarm(0);
    } catch PLIF::Exception::Alarm with {
        # timed out -- ignore
    };
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
    try {
        local $SIG{ALRM} = sub { raise PLIF::Exception::Alarm };
        $self->assert($self->handle->mail($self->from), 1, 'Could not start sending mail');
        $self->assert($self->handle->to($session->getAddress('email')), 1, 'Could not set mail recipient (was going to send to '.($session->getAddress('email')).')');
        $self->assert($self->handle->data($string), 1, 'Could not send mail body');
        alarm(0);
    } catch PLIF::Exception::Alarm with {
        $self->error(1, 'Timed out while trying to send e-mail');
    }
}

# protocol.email
sub checkAddress {
    my $self = shift;
    my($app, $username) = @_;
    return (defined($username) and $username =~ m/^[^@\s]+@[^@\s]+\.[^@.\s]+$/os);
    # XXX this doesn't seem to be working:
    # $self->assert(defined($self->handle), 1, 'No SMTP handle, can\'t check address');
    # $self->assert(defined($username), 1, 'Internal error: no username passed to checkAddress');
    # my $result = $self->handle->verify($username);
    # return $result;
}

sub DESTROY {
    my $self = shift;
    $self->close();
    $self->SUPER::DESTROY(@_);
}

# dataSource.configuration.client
sub settings {
    return qw(host address timeout);
}

# setup.configure
sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure protocol.email...');
    $app->output->setupProgress('protocol.email');
    $self->close();

    my $value;

    $value = $self->host;
    if (not defined($value)) {
        $value = 'localhost';
    }
    $value = $app->input->getArgument('protocol.email.host', $value);
    if (not defined($value)) {
        return 'protocol.email.host';
    }
    $self->host($value);

    $value = $self->address;
    if (defined($value)) {
        # default to existing value
        $value = $app->input->getArgument('protocol.email.address', $value);
    } else {
        # don't default to anything as our guess is most likely going
        # to be wrong: "$USER@`hostname`" is rarely correct.
        $value = $app->input->getArgument('protocol.email.address');
    }
    if (not defined($value)) {
        return 'protocol.email.address';
    }
    $self->address($value);

    $value = $self->timeout;
    if (not defined($value)) {
        $value = 5;
    }
    $value = $app->input->getArgument('protocol.email.timeout', $value);
    if (not defined($value)) {
        return 'protocol.email.timeout';
    }
    $self->timeout($value);

    $self->open();
    $app->getService('dataSource.configuration')->setSettings($app, $self, 'protocol.email');
    $self->dump(9, 'done configuring protocol.email');
    return;
}

sub hash {
    my $self = shift;
    return {
        'address' => $self->address,
        # XXX RFC822 date -- need to provide this WITHOUT duplicating code in StdOut outputter
    };
}
