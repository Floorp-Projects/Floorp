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

package PLIF::MagicPipingArray;
use strict;
use vars qw($AUTOLOAD);  # it's a package global
use Carp qw(cluck confess); # stack trace versions of warn and die
1;

# This can be used separate from PLIF, and so does not inherit from
# the PLIF core. Calling any method except 'create' will result in the
# method call being forwarded to the wrapped objects. Calling 'create'
# will create a new MagicPipingArray object, see the AUTOLOAD function
# below for an example.

sub create {
    my $class = shift;
    if (ref($class)) {
        $class = ref($class);
    }
    my $self = [@_];
    bless($self, $class);
    return $self;
}

sub AUTOLOAD {
    my $self = shift;
    my $name = $AUTOLOAD;
    $name =~ s/^.*://o; # strip fully-qualified portion
    my @allResults = ();
    foreach my $object (@$self) {
        my $method = $object->can($name);
        if ($method) {
            push(@allResults, [&$method($object, @_)]);
        } else {
            confess("Failed to find method or property '$name' in object '$object' of MagicPipingArray '$self', aborting"); # die with stack trace
        }
    }
    return $self->create(@allResults);
}

sub DESTROY {} # stub to not cause infinite loop with AUTOLOAD :-)
