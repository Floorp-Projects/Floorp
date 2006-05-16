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

use Bugzilla::Util;
use Bugzilla::Error;

###############################
####    Initialization     ####
###############################

use constant DB_COLUMNS => qw(
   keyworddefs.id
   keyworddefs.name
   keyworddefs.description
);

my $columns = join(", ", DB_COLUMNS);

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

sub _init {
    my $self = shift;
    my ($param) = @_;
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $keyword;

    if (defined $id) {
        detaint_natural($id)
          || ThrowCodeError('param_must_be_numeric',
                            {function => 'Bugzilla::Keyword::_init'});

        $keyword = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM keyworddefs
             WHERE id = ?}, undef, $id);
    } elsif (defined $param->{'name'}) {
        trick_taint($param->{'name'});
        $keyword = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM keyworddefs
             WHERE name = ?}, undef, $param->{'name'});
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Bugzilla::Keyword::_init'});
    }

    return undef unless (defined $keyword);

    foreach my $field (keys %$keyword) {
        $self->{$field} = $keyword->{$field};
    }
    return $self;
}

sub new_from_list {
    my $class = shift;
    my ($id_list) = @_;
    my $dbh = Bugzilla->dbh;

    my $keywords;
    if (@$id_list) {
        my @detainted_ids;
        foreach my $id (@$id_list) {
            detaint_natural($id) ||
                ThrowCodeError('param_must_be_numeric',
                              {function => 'Bugzilla::Keyword::new_from_list'});
            push(@detainted_ids, $id);
        }
        $keywords = $dbh->selectall_arrayref(
            "SELECT $columns FROM keyworddefs WHERE id IN (" 
            . join(',', @detainted_ids) . ")", {Slice=>{}});
    } else {
        return [];
    }

    foreach my $keyword (@$keywords) {
        bless($keyword, $class);
    }
    return $keywords;
}

###############################
####      Accessors      ######
###############################

sub id                { return $_[0]->{'id'};          }
sub name              { return $_[0]->{'name'};        }
sub description       { return $_[0]->{'description'}; }

###############################
####      Subroutines    ######
###############################

sub get_all_keywords {
    my $dbh = Bugzilla->dbh;

    my $ids = $dbh->selectcol_arrayref(q{
        SELECT id FROM keyworddefs ORDER BY name});

    my $keywords = Bugzilla::Keyword->new_from_list($ids);
    return @$keywords;
}

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

 my $keyword = new Bugzilla::Keyword(1);
 my $keyword = new Bugzilla::Keyword({name => 'perf'});

 my $id          = $keyword->id;
 my $name        = $keyword->name;
 my $description = $keyword->description;

=head1 DESCRIPTION

Bugzilla::Keyword represents a keyword that can be added to a bug.

=head1 METHODS

=over

=item C<new($param)>

 Description: The constructor is used to load an existing keyword
              by passing a keyword id or a hash.

 Params:      $param - If you pass an integer, the integer is the
                       keyword id from the database that we want to
                       read in. If you pass in a hash with 'name' key,
                       then the value of the name key is the name of a
                       keyword from the DB.

 Returns:     A Bugzilla::Keyword object.

=item C<new_from_list(\@id_list)>

 Description: Creates an array of Keyword objects, given an
              array of ids.

 Params:      \@id_list - A reference to an array of numbers, keyword ids.
                          If any of these are not numeric, the function
                          will throw an error. If any of these are not
                          valid keyword ids, they will simply be skipped.

 Returns:     A reference to an array of C<Bugzilla::Keyword> objects.

=back

=head1 SUBROUTINES

=over

=item C<get_all_keywords()>

 Description: Returns all keywords from the database.

 Params:      none.

 Returns:     A list of C<Bugzilla::Keyword> objects,
              or an empty list if there are none.

=item C<keyword_count()> 

 Description: A utility function to get the total number
              of keywords defined. Mostly used to see
              if there are any keywords defined at all.
 Params:      none
 Returns:     An integer, the count of keywords.

=back

=cut
