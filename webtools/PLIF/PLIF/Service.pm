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

package PLIF::Service;
use strict;
use vars qw(@ISA);
use PLIF;
@ISA = qw(PLIF);
1;

# what services the module provides as a service
sub provides { return 0; } # stub

# what services the module provides as an object
sub objectProvides { 
    my $class = shift;
    return $class->provides(@_);
} 

sub dispatch {
    my $self = shift;
    my($app, $name, @arguments) = @_;
    my $method = $self->can($name);
    local $" = '\', \'';
    if ($method) {
        $self->dump(10, "Attempting to dispatch method: $self->$name('$app', ...)"); # can't mention @arguments in string since it might contain undefs
        &$method($self, $app, @arguments);
        return 1;
    } else {
        $self->dump(10, "There is no method '$name' in object '$self', skipping...");
        return;
    }
}
