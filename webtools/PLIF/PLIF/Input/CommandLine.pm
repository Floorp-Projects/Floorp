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

package PLIF::Input::CommandLine;
use strict;
use vars qw(@ISA);
use PLIF::Input::Arguments;
use Term::ReadLine; # DEPENDENCY
@ISA = qw(PLIF::Input::Arguments);
1;

sub applies {
    return @ARGV > 0;
}

sub defaultOutputProtocol {
    return 'stdout';
}

sub splitArguments {
    my $self = shift;
    # first, make sure the command argument is created even if it is
    # not explicitly given -- this avoids us from asking the user what
    # command they want in interactive mode.
    $self->SUPER::createArgument('');
    # next, parse the arguments provided.
    my $lastArgument;
    foreach my $argument (@ARGV) {
        if ($argument =~ /^-([^-]+)$/os) {
            my @shortArguments = split(//o, $1);
            foreach my $shortArgument (@shortArguments) {
                $self->addArgument($shortArgument, 1);
            }
            $lastArgument = $shortArguments[$#shortArguments];
        } elsif ($argument =~ /^--([^-][^=]*)=(.+)$/os) {
            $self->addArgument($1, $2);
            $lastArgument = undef;
        } elsif ($argument =~ /^--no-([^-].+)/os) {
            $self->addArgument($1, 0);
            $lastArgument = undef;
        } elsif ($argument =~ /^--([^-].+)/os) {
            $self->addArgument($1, 1);
            $lastArgument = $1;
        } else {
            if (defined($lastArgument)) {
                $self->addArgument($lastArgument, $argument);
                $lastArgument = undef;
            } else {
                $self->addArgument('', $argument);
            }
        }
    }
}

sub createArgument {
    my $self = shift;
    # @_ also contains @default, but to save copying it about we don't
    # use it directly in this method
    my($argument) = @_;
    if ($argument eq 'batch') {
        # if --batch was not set, then we assume that means that
        # we are not in --batch mode... no point asking the user,
        # cos if we are, he won't reply, and if he isn't, we know
        # he'd say we aren't! :-)
        $self->setArgument($argument, 0);
    } else {
        if ($self->getArgument('batch')) {
            $self->SUPER::createArgument(@_);
        } else {
            $self->warn(8, "going to request '$argument' from user!");
            $self->app->output->request(@_);
            # get input from user
            my $term = Term::ReadLine->new($self->app->name);
            my $value = $term->readline(''); # (the parameter passed is the prompt, if any)
            # if we cached the input device:
            # $term->addhistory($value);
            if (defined($value)) {
                if ($value eq '') {
                    # use default
                    $self->setArgument(@_);
                } else {
                    $self->setArgument($argument, $value);
                }
            } else {
                # end of file -- give up with this argument and then switch to batch mode
                $self->SUPER::createArgument(@_);
                $self->setArgument($argument, 1);
            }
        }
    }
}


# XXX Grrrr: 

sub UA {
    return '';
}

sub referrer {
    return '';
}

sub host {
    return 'localhost';
}

sub acceptType {
    return 'text/plain';
}

sub acceptCharset {
    return '';
}

sub acceptEncoding {
    return '';
}

sub acceptLanguage {
    return '';
}
