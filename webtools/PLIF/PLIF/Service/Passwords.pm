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

package PLIF::Service::Passwords;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'service.passwords' or $class->SUPER::provides($service));
}

__DATA__

# sanity checking: |$self->checkPassword($self->newPassword())| should
# return a true value if everything is behaving itself

sub newPassword {
    my $self = shift;
    # Returns first an opaque key (the one-way encrypted version of
    # the generated password) and second the actual plain-text
    # password for a one-time sending to the user.
    my $password = $self->generatePassword();
    my $crypt = $self->crypt($password);
    return ($crypt, $password);
}

sub encryptPassword {
    my $self = shift;
    my($password) = @_;
    my $crypt = $self->crypt($password);
    return $crypt;
}

sub checkPassword {
    my $self = shift;
    my($key, $userData) = @_;
    # $key is the first value returned from newPassword, 
    # $userData is the password given by the user.
    return ($key eq $self->crypt($userData, $key));
}


# internal routines

sub crypt {
    my $self = shift;
    my($string, $salt) = @_;
    if (not defined($salt)) {
        $salt = $self->generateSalt();
    }
    return crypt($string, $salt);
}

sub generatePassword {
    # Generates a pseudo-random password that is not predictable, but
    # which is memorable. XXX this should be more secure... :-)

    my(@adjectives) = ('adorable', 'amazing', 'amusing', 'artistic',
    'azure', 'bouncy', 'cheeky', 'cheerful', 'cheery', 'cuddly',
    'cute', 'dynamic', 'excited', 'exciting', 'female', 'flash',
    'fluffy', 'funny', 'giant', 'good', 'great', 'happy', 'joyful',
    'lovely', 'majestic', 'mauve', 'mellow', 'meowing', 'miaowing',
    'poetic', 'puffing', 'purring', 'purry', 'relaxed', 'silver',
    'sleepy', 'soft', 'special', 'spicy', 'squeaky', 'sweet',
    'pink', 'yellow', ); # XXX more (pleasant) suggestions welcome...

    my(@nouns) = ('angel', 'bee', 'book', 'bunny', 'cat', 'color',
    'colour', 'comedian', 'computer', 'cube', 'date', 'dawn', 'duck',
    'ferret', 'fig', 'flower', 'forest', 'fox', 'frog', 'fun', 'girl',
    'grass', 'heaven', 'humor', 'humour', 'island', 'jester', 'joy',
    'kitten', 'life', 'love', 'ludwig', 'mowmow', 'mouse', 'music',
    'party', 'pencil', 'pleasure', 'poem', 'puffin', 'rainbow',
    'raindrop', 'science', 'sphere', 'star', 'student', 'summer',
    'sunshine', 'tautology', 'teddy', 'time', 'tree', 'voice', );

    my $adjective = $adjectives[int(rand($#adjectives))];
    my $noun = $nouns[int(rand($#nouns))];
    return "$adjective $noun";
}

sub generateSalt {
    my $self = shift;
    my @salts = ('0'..'9', 'a'..'z', 'A'..'Z', '.', '/');
    my $salt = '';
    foreach (1..2) {
        $salt .= $salts[int(rand($#salts))];
    }
    return $salt;
}
