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

# returns the argument if it has been provided, otherwise an empty list.
sub peekArgument {
    my $self = shift;
    $self->notImplemented();
}

# returns all the arguments in a form of a hash:
#  key => value
sub getArguments {
    my $self = shift;
    $self->notImplemented();
}

# escape semicolons as #s and hashes as #h, etc.
sub escapeString {
    my $self = shift;
    my($substring) = @_;
    # this is a simple escaping mechanism which gets rid of all
    # semicolons without introducing any ambiguities and without
    # requiring much thought when unescaping. (If you try to escape
    # the separator, say ';', by doubling it, e.g. ';;', then you lose
    # the possibility of a blank value, and if you escape by using one
    # escape for the separator and doubling for the escape character,
    # e.g. '\;' and '\\', then you get all kinds of confusion when the
    # string contains lots of '\' and ';' characters.
    $substring =~ s/  \#  /  \#h  /gosx;
    $substring =~ s/  \|  /  \#b  /gosx;
    $substring =~ s/  \;  /  \#s  /gosx;
    $substring =~ s/  \   /  \#w  /gosx;
    $substring =~ s/  \n  /  \#n  /gosx;
    $substring =~ s/  \r  /  \#r  /gosx;
    $substring =~ s/  \t  /  \#t  /gosx;
    return $substring;
}

# escape semicolons as #s and hashes as #h, etc.
sub unescapeString {
    my $self = shift;
    my($substring) = @_;
    $substring =~ s/  \#b  /  \|  /gosx;
    $substring =~ s/  \#s  /  \;  /gosx;
    $substring =~ s/  \#w  /  \   /gosx;
    $substring =~ s/  \#n  /  \n  /gosx;
    $substring =~ s/  \#r  /  \r  /gosx;
    $substring =~ s/  \#t  /  \t  /gosx;
    $substring =~ s/  \#h  /  \#  /gosx;
    return $substring;
}

# returns all the arguments in a form of a string
sub getArgumentsAsString {
    my $self = shift;
    my $hash = $self->getArguments();
    my $string = '';
    foreach my $key (keys %$hash) {
        $key = $self->escapeString($key);
        if (ref($hash->{$key}) eq 'ARRAY') {
            $string .= "$key;";
            foreach my $substring (@{$hash->{$key}}) {
                $substring = $self->escapeString($substring);
                $string .= "$substring|";
            }
            chop $string;
            $string .= ";";
        } else {
            $string .= "$key;".($hash->{$key}).';';
        }
    }
    chop $string;
    return $string;
}

# turns a string from getArgumentsAsString() back into a hash
# you can also pass an arrayref of strings
sub getArgumentsFromString {
    my $self = shift;
    my($string) = @_;
    if (not defined($string)) {
        # no string, no arguments
        return {};
    }
    if (ref($string) eq 'ARRAY') {
        # concatenate strings (possibly containing semicolons
        # themselves) together to form one long string
        $string = join(';', @$string);
    }
    my @rawHash = split(/;/, $string);
    if (not @rawHash or @rawHash % 2) {
        # nope! Something in this data is screwed up, let's bail out.
        return {};
    } else {
        my $isKey = 1;
        my @hash;
        foreach my $substring (@rawHash) {
            if ($isKey) {
                push(@hash, $self->unescapeString($substring));
            } else {
                my @values;
                foreach my $value (split(/\|/, $substring)) {
                    push(@values, $self->unescapeString($value));
                }
                push(@hash, \@values);
            }
            $isKey = not $isKey;
        }
        return { @hash };
    }
}

# returns all the arguments present that begin with a specific string
# followed by a dot (the keys in the hash returned do not start with
# the prefix)
sub getArgumentsBranch {
    my $self = shift;
    $self->notImplemented();
}

sub hash {
    my $self = shift;
    return {
        'arguments' => $self->getArguments(),
        'protocol' => $self->defaultOutputProtocol(),
        # XXX also UA, etc...
    };
}

# 'username' and 'password' are two out of band arguments that may be
# provided as well, they are accessed as properties of the input
# object (e.g., |if (defined($input->username)) {...}|).  Input
# services that have their own username syntaxes (e.g. AIM, ICQ)
# should have a username syntax of "SERVICE: <username>" e.g., my AIM
# username would be "AIM: HixieDaPixie". Other services, which get the
# username from the user (e.g. HTTP), should pass the username
# directly. See the user service for more details.
#
# 'username' should only be provided if the user attempted to log in.
#
# 'address' is an out of band argument that should only be provided
# for input devices that know the address of the user (and can thus
# construct the username).


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
