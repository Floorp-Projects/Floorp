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

package PLIF::Program;
use strict;
use vars qw(@ISA);
use PLIF::Controller;
use PLIF::Exception;
@ISA = qw(PLIF::Controller);

# the center of the PLIF-based application:
my $app = 'main'->create();
$app->run();

1;

__DATA__

# setup everything (automatically called by the constructor, above)
sub init {
    my $self = shift;
    $self->dump(8, '', '');
    $self->dump(10, '********************************');
    $self->dump(5, '*** Started PLIF Application ***');
    $self->dump(9, '********************************');
    $self->SUPER::init(@_);
    $self->initInput();
}

# called after the constructor (see above)
# this is the core of the application
sub run {
    my $self = shift;
    do {
        try {
            # the input device is the same throughout the application
            # see constructor above
            if ($self->verifyInput()) {
                if ($self->input->command) {
                    $self->dump(8, 'Command: ' . ($self->input->command));
                    $self->{command} = $self->input->command;
                    $self->dispatch($self->input->command);
                } else {
                    $self->dump(8, 'Command: (none)');
                    $self->{command} = '';
                    $self->noCommand();
                }
                $self->commandDone();
            } # verifyInput should deal with the errors
        } except {
            $self->dump(3, "previous command didn't go over well: @_");
            $self->output->reportFatalError(@_);
        };
        # command has been completed, reset it
        $self->{command} = undef;
        # In case we used a progressive output device, let it shut
        # down.  It's important to do this, because it holds a
        # reference to us and we wouldn't want a memory leak...
        $self->{defaultOutput} = undef;
        # empty the session objects list
        $self->{objects} = [];
    } while ($self->input->next());
    # clear the objects hash here, so that objects are removed before
    # us, otherwise they can't refer back to us during shutdown.
    # don't need to do the same to services as services should never
    # use the application object during shutdown. (They shouldn't be
    # able to. If they can, there is a circular reference.)
    $self->{objects} = [];
    $self->input(undef); # shutdown the input service instance
    $self->dump(5, 'PLIF application completed normally.');
}

# takes the first applicable input method.
sub initInput {
    my $self = shift;
    my $input = $self->getServiceInstance('input');
    if ($input) {
        $self->dump(8, "Input: $input");
        $self->input($input);
    } else {
        $self->noInput();
    }
}

sub input {
    my $self = shift;
    if (@_) {
        return $self->{'_input'} = shift;
    } else {
        return $self->{'_input'};
    }
}

# Returns an applicable output method. If you need a particular
# protocol, pass it as a parameter. To get the default output class
# given the current objects, do not pass any parameters. The output
# object is a one-off and is not (and should not) be cached; once you
# have called the relevant output method on it let it go out of scope
# and that should be it.
# You may also pass a session argument (typically the object
# representing a user, for example). If you don't pass any, a the
# first session object that was created by the input verifiers is used
# instead (e.g. during authentication).
sub output {
    my $self = shift;
    my($protocol, $session) = @_;
    my $default = 0;
    if (not defined($protocol)) {
        if (defined($self->{defaultOutput})) {
            return $self->{defaultOutput};
        }
        if ($session) {
            $self->warn(3, 'Tried to use default output method for a specific session object');
            $session = undef;
        }
        $default = 1;
        $protocol = $self->selectOutputProtocol();
    }
    if (not defined($session)) {
        $session = $self->getObject('session');
    }
    # There are two output models in PLIF. The first is the protocol-
    # specific-code model, the second is the string-expander
    # model. The string expander model is still protocol specific to
    # some extent, but it gives greater flexibility for exactly what
    # is output... so long as it can be represented by a single string
    # that is then passed to protocol-specific code. 
    # First, see if a full protocol-specific-code handler exists:
    my $output = $self->getServiceInstance("output.$protocol", $session);
    if (not defined($output)) {
        # ...and, since we failed to find one, fall back on the
        # generic string expander model:
        $output = $self->getServiceInstance('output.generic', $session, $protocol);
        if (not defined($output)) {
            # oops, no string expander model either :-/
            $self->error(0, 'Could not find an applicable output class');
        }
    }
    if ($default) {
        # now add the objects that have hooked in.
        # * hooks have to be registered by the time the default output
        #   device is picked; the hooks are not rescanned once a
        #   default output is picked.
        # * hooks are run in reverse order of being registered.
        # * output.hook objects have to provide a getOutputHook method
        #   which returns a reference which will be treated just as a
        #   normal output service. In particular, this means that any
        #   method could be called. So most output hooks should use
        #   implyMethod much like PLIF::Output::Generic.
        # * passthrough hooks should then call the original method
        #   again on the argument of the getOutputHook method (which
        #   is the next object). Override hooks (like the XML RPC one)
        #   can call specific methods on the next object, or they can
        #   do whatever they like. Note, though, that not using the
        #   default output object is a bad idea, since it could leave
        #   the user with nothing.
        my @hooks = $self->getObjectList('output.hook');
        foreach my $hook (@hooks) {
            $output = $hook->getOutputHook($output);
        }
        $self->{defaultOutput} = $output;
    }
    return $output;
}

sub verifyInput {
    my $self = shift;
    # we invoke all the input verifiers until one fails
    my $result = $self->getSelectingServiceList('input.verify')->verifyInput($self);
    if (defined($result)) { 
        # if one failed, then the result will be the object that should report the error
        $result->reportInputVerificationError($self);
        return 0;
    } else {
        return 1;
    }
}

sub selectOutputProtocol {
    my $self = shift;
    return $self->input->defaultOutputProtocol;
}

sub hash {
    my $self = shift;
    return { 'name' => $self->name };
}

sub command {
    my $self = shift;
    syntaxError 'command() called with arguments' if @_;
    return $self->{command};
}


# Implementation Specific Methods
# At least some of these should be overriden by real applications

# If you override this one, only call $self->SUPER::dispatch(@_) if
# you couldn't dispatch the command.
# Note: Application.pm overrides this to forward commands to
# services implementing the 'dispatcher.commands' service.
sub dispatch {
    my $self = shift;
    my($command) = @_;
    # the \u makes the first letter of the $command uppercase
    if (not $self->SUPER::dispatch($self, "cmd\u$command")) {
        $self->unknownCommand();
    }
}

sub noInput {
    my $self = shift;
    $self->error(0, 'Could not find an applicable input method');
}

sub unknownCommand {
    my $self = shift;
    $self->error(0, 'The command given was not recognised');
}

sub noCommand {
    my $self = shift;
    $self->unknownCommand(@_);
}

sub commandDone {
    my $self = shift;
    $self->getCollectingServiceList('dispatcher.commandDone')->commandDone($self);
}

sub name {
    my $self = shift;
    # default our app name to be the name of the executable
    # may be overridden by descendants
    return $0;
}
