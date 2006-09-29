# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::WebService::Bugzilla;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla::Constants;

use Time::Zone;

sub version {
    return BUGZILLA_VERSION;
}

sub timezone {
    my $offset = tz_offset();
    $offset = (($offset / 60) / 60) * 100;
    $offset = sprintf('%+05d', $offset);
    return 'GMT' . $offset;
}

1;

__END__

=head1 NAME

Bugzilla::WebService::Bugzilla - Global functions for the webservice interface.

=head1 SYNOPSIS

 my $version = Bugzilla.version;
 my $tz = Bugzilla.timezone;

=head1 DESCRIPTION

This provides functions that tell you about Bugzilla in general.

=head1 METHODS

See L<Bugzilla::WebService> for a description of what B<STABLE>, B<UNSTABLE>,
and B<EXPERIMENTAL> mean.

=over

=item C<version>

Returns the current version of Bugzilla, as a string.

=item C<timezone>

Returns the timezone of the server Bugzilla is running on, in GMT(+/-)XXXX 
format. This is important because all dates/times that the webservice
interface returns will be in this timezone.

=back
