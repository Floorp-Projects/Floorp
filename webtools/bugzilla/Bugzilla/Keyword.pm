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
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;

package Bugzilla::Keyword;

use base qw(Bugzilla::Object);

###############################
####    Initialization     ####
###############################

use constant DB_COLUMNS => qw(
   keyworddefs.id
   keyworddefs.name
   keyworddefs.description
);

use constant DB_TABLE => 'keyworddefs';

###############################
####      Accessors      ######
###############################

sub description       { return $_[0]->{'description'}; }

###############################
####      Subroutines    ######
###############################

sub keyword_count {
    my ($count) = 
        Bugzilla->dbh->selectrow_array('SELECT COUNT(*) FROM keyworddefs');
    return $count;
}

1;

__END__

=head1 NAME

Bugzilla::Keyword - A Keyword that can be added to a bug.

=head1 SYNOPSIS

 use Bugzilla::Keyword;

 my $count = Bugzilla::Keyword::keyword_count;

 my $description = $keyword->description;

=head1 DESCRIPTION

Bugzilla::Keyword represents a keyword that can be added to a bug.

This implements all standard C<Bugzilla::Object> methods. See 
L<Bugzilla::Object> for more details.

=head1 SUBROUTINES

This is only a list of subroutines specific to C<Bugzilla::Keyword>.
See L<Bugzilla::Object> for more subroutines that this object 
implements.

=over

=item C<keyword_count()> 

 Description: A utility function to get the total number
              of keywords defined. Mostly used to see
              if there are any keywords defined at all.
 Params:      none
 Returns:     An integer, the count of keywords.

=back

=cut
