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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Joel Peshkin <bugreport@peshkin.net>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Tiago R. Mello <timello@async.com.br>

use strict;

package Bugzilla::Group;

use Bugzilla;
use Bugzilla::Config;
use Bugzilla::Util;
use Bugzilla::Error;

###############################
##### Module Initialization ###
###############################

use constant DB_COLUMNS => qw(
    groups.id
    groups.name
    groups.description
    groups.isbuggroup
    groups.last_changed
    groups.userregexp
    groups.isactive
);

our $columns = join(", ", DB_COLUMNS);

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $group;

    if (defined $id && detaint_natural($id)) {

        $group = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM groups
            WHERE id = ?}, undef, $id);

    } elsif (defined $param->{'name'}) {

        trick_taint($param->{'name'});

        $group = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM groups
            WHERE name = ?}, undef, $param->{'name'});

    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Bugzilla::Group::_init'});
    }

    return undef unless (defined $group);

    foreach my $field (keys %$group) {
        $self->{$field} = $group->{$field};
    }
    return $self;
}

###############################
####      Accessors      ######
###############################

sub id           { return $_[0]->{'id'};           }
sub name         { return $_[0]->{'name'};         }
sub description  { return $_[0]->{'description'};  }
sub is_bug_group { return $_[0]->{'isbuggroup'};   }
sub last_changed { return $_[0]->{'last_changed'}; }
sub user_regexp  { return $_[0]->{'userregexp'};   }
sub is_active    { return $_[0]->{'isactive'};     }

################################
#####  Module Subroutines    ###
################################

sub ValidateGroupName {
    my ($name, @users) = (@_);
    my $dbh = Bugzilla->dbh;
    my $query = "SELECT id FROM groups " .
                "WHERE name = ?";
    if (Param('usevisibilitygroups')) {
        my @visible = (-1);
        foreach my $user (@users) {
            $user && push @visible, @{$user->visible_groups_direct};
        }
        my $visible = join(', ', @visible);
        $query .= " AND id IN($visible)";
    }
    my $sth = $dbh->prepare($query);
    $sth->execute($name);
    my ($ret) = $sth->fetchrow_array();
    return $ret;
}

1;

__END__

=head1 NAME

Bugzilla::Group - Object for a Bugzilla group.

=head1 SYNOPSIS

    use Bugzilla::Group;

    my $group = new Bugzilla::Group(1);
    my $group = new Bugzilla::Group({name => 'AcmeGroup'});

    my $id           = $group->id;
    my $name         = $group->name;
    my $description  = $group->description;
    my $last_changed = $group->last_changed;
    my $user_reg_exp = $group->user_reg_exp;
    my $is_active    = $group->is_active;

=head1 DESCRIPTION

Group.pm represents a Bugzilla Group object.

=head1 METHODS

=over

=item C<new($param)>

The constructor is used to load an existing group by passing
a group id or a hash with the group name.

=back

=head1 SUBROUTINES

=over

=item C<ValidateGroupName($group_name, @users)>

ValidateGroupName checks to see if ANY of the users in the provided list 
of user objects can see the named group.  It returns the group id if
successful and undef otherwise.

=back

=head1 AUTHOR
    
    Joel Peshkin <bugreport@peshkin.net>
    Erik Stambaugh <erik@dasbistro.com>
    Tiago R. Mello <timello@async.com.br>

=cut
