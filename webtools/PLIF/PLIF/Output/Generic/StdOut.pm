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

package PLIF::Output::Generic::StdOut;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'output.generic.http' or
            $service eq 'output.generic.stdout' or
            $class->SUPER::provides($service));
}

sub output {
    my $self = shift;
    my($app, $session, $string) = @_;
    $| = 1; # flush output even if no newline
    print $string;
}

sub hash {
    # Includes today's date as an RFC822 compliant string with the
    # exception that the year is returned as four digits. IMHO RFC822
    # was wrong to specify the year as two digits. Many email systems
    # generate four-digit years.
    my ($tsec, $tmin, $thour, $tmday, $tmon, $tyear, $twday, $tyday, $tisdst) = gmtime(time());
    $tyear += 1900; # as mentioned above, this is not RFC822 compliant, but is Y2K-safe.
    $tsec = "0$tsec" if $tsec < 10;
    $tmin = "0$tmin" if $tmin < 10;
    $thour = "0$thour" if $thour < 10;
    $tmon = ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec')[$tmon];
    $twday = ('Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat')[$twday];
    return {
        'date' => "$twday, $tmday $tmon $tyear $thour:$tmin:$tsec GMT",
    };
}
