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

package PLIF::Output::Generic::AIM;
use strict;
use vars qw(@ISA);
use PLIF::Service;
use Net::AIM; # DEPENDENCY
@ISA = qw(PLIF::Service);

# work around a bug in some releases of Net::AIM::Connection
if (not Net::AIM::Connection->can('handler')) {
    eval {
        package Net::AIM::Connection;
        sub handler { }
    }
}

1;

# XXX This protocol should check if the user is actually online, and
# if not, should refuse to help. This will probably require some
# architectural changes.

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'output.generic.aim' or
            $service eq 'protocol.aim' or
            $service eq 'setup.configure' or
            $service eq 'dataSource.configuration.client' or
            $class->SUPER::provides($service));
}

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    eval {
        $app->getService('dataSource.configuration')->getSettings($app, $self, 'protocol.aim');
    };
    if ($@) {
        $self->dump(9, "failed to get the AIM configuration, not going to bother to connect: $@");
        $self->handle(undef);
    } else {
        $self->open();
    }
}

sub open {
    my $self = shift;
    # try to connect
    $self->dump(9, 'opening AIM connection');
    $self->handle(undef);
    {
        # The Net::AIM code sprouts warning like there's no tomorrow
        # Let's mute them. :-)
        local $^W = 0;
        my $aim = Net::AIM->new();
        # $aim->debug(${$self->getDebugLevel} > 4);
        if ($aim->newconn('Screenname' => $self->address,
                          'Password' => $self->password,
                          'AutoReconnect' => 1)) {
            # wow, we did it
            # add a buddy first of all (seem to need this, not sure why)
            $aim->add_buddy(0, 'Buddies', $self->address);

            # this is dodgy; protocol specs don't guarentee that this
            # message will arrive
            $aim->getconn->set_handler('nick', sub {
                                           my $conn = shift;
                                           my($evt, $from, $to) = @_;
                                           my $nick = $evt->args()->[0];
                                           $self->handle($aim);
                                           $self->dump(9, "opened AIM connection to $from as $nick");
                                       });

            # while we're at it, here's an error handler for completeness
            $aim->getconn->set_handler('error', sub {
                                           my $conn = shift;
                                           my($evt) = @_;
                                           my($error, @stuff) = @{$evt->args()};
                                           my $errstr = $evt->trans($error);
                                           $errstr =~ s/\$(\d+)/$stuff[$1]/ge;
                                           $self->warn(4, "error occured while opening AIM connection: $errstr");
                                       });

            while (not defined($self->handle) and $aim->do_one_loop()) { }
        }
    }

    if (not defined($self->handle)) {
        $self->warn(4, 'Could not create the AIM handle');
    }
}

sub close {
    my $self = shift;
    if (defined($self->handle)) {
        my $conn = $self->handle->getconn;
        if (defined($conn)) {
            $conn->disconnect();
        }
    }
}

# output.generic.aim
sub output {
    my $self = shift;
    my($app, $session, $string) = @_;
    $self->assert(defined($self->handle), 1, 'No AIM handle, can\'t send IM');
    $self->handle->send_im($session->getAddress('aim'), $string);
}

# protocol.aim
sub checkAddress {
    my $self = shift;
    my($app, $username) = @_;
    $self->assert(defined($self->handle), 1, 'No AIM handle, can\'t check address');
    # my $result = $self->handle->XXX;
    # return $result;
    return 1;
}

sub DESTROY {
    my $self = shift;
    $self->close();
    $self->SUPER::DESTROY(@_);
}

# dataSource.configuration.client
sub settings {
    return qw(address password);
}

# setup.configure
sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure protocol.aim...');
    $app->output->setupProgress('protocol.aim');
    $self->close();

    foreach my $property ($self->settings) {
        my $value = $self->propertyGet($property);
        if (defined($value)) {
            # default to existing value
            $value = $app->input->getArgument("protocol.aim.$property", $value);
        } else {
            # don't default to anything as our guess is most likely
            # going to be wrong: "$USER" is rarely the AIM screen name
            # and there's no way we can guess the password.
            $value = $app->input->getArgument("protocol.aim.$property");
        }
        if (not defined($value)) {
            return "protocol.aim.$property";
        }
        $self->propertySet($property, $value);
    }

    $self->open();
    $app->getService('dataSource.configuration')->setSettings($app, $self, 'protocol.aim');
    $self->dump(9, 'done configuring protocol.aim');
    return;
}

sub hash {
    my $self = shift;
    return {
            'address' => $self->address,
           };
}
