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
# Contributor(s): Myk Melez <myk@mozilla.org>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Bradley Baetz <bbaetz@acm.org>
#                 Joel Peshkin <bugreport@peshkin.net> 
#                 Byron Jones <bugzilla@glob.com.au>
#                 Shane H. W. Travis <travis@sedsystems.ca>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Gervase Markham <gerv@gerv.net>
#                 Lance Larsh <lance.larsh@oracle.com>
#                 Justin C. De Vries <judevries@novell.com>
#                 Dennis Melentyev <dennis.melentyev@infopulse.com.ua>
#                 Frédéric Buclin <LpSolit@gmail.com>

################################################################################
# Module Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# This module implements utilities for dealing with Bugzilla users.
package Bugzilla::User;

use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::User::Setting;
use Bugzilla::Product;
use Bugzilla::Classification;
use Bugzilla::Field;

use base qw(Exporter);
@Bugzilla::User::EXPORT = qw(insert_new_user is_available_username
    login_to_id user_id_to_login validate_password
    UserInGroup
    USER_MATCH_MULTIPLE USER_MATCH_FAILED USER_MATCH_SUCCESS
    MATCH_SKIP_CONFIRM
);

#####################################################################
# Constants
#####################################################################

use constant USER_MATCH_MULTIPLE => -1;
use constant USER_MATCH_FAILED   => 0;
use constant USER_MATCH_SUCCESS  => 1;

use constant MATCH_SKIP_CONFIRM  => 1;

################################################################################
# Functions
################################################################################

sub new {
    my $invocant = shift;
    my $user_id = shift;

    if ($user_id) {
        my $uid = $user_id;
        detaint_natural($user_id)
          || ThrowCodeError('invalid_numeric_argument',
                            {argument => 'userID',
                             value    => $uid,
                             function => 'Bugzilla::User::new'});
        return $invocant->_create("userid=?", $user_id);
    }
    else {
        return $invocant->_create;
    }
}

# This routine is sort of evil. Nothing except the login stuff should
# be dealing with addresses as an input, and they can get the id as a
# side effect of the other sql they have to do anyway.
# Bugzilla::BugMail still does this, probably as a left over from the
# pre-id days. Provide this as a helper, but don't document it, and hope
# that it can go away.
# The request flag stuff also does this, but it really should be passing
# in the id it already had to validate (or the User.pm object, of course)
sub new_from_login {
    my $invocant = shift;
    my $login = shift;

    my $dbh = Bugzilla->dbh;
    return $invocant->_create($dbh->sql_istrcmp('login_name', '?'), $login);
}

# Internal helper for the above |new| methods
# $cond is a string (including a placeholder ?) for the search
# requirement for the profiles table
sub _create {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;

    my $cond = shift;
    my $val = shift;

    # Allow invocation with no parameters to create a blank object
    my $self = {
        'id'             => 0,
        'name'           => '',
        'login'          => '',
        'showmybugslink' => 0,
        'disabledtext'   => '',
        'flags'          => {},
    };
    bless ($self, $class);
    return $self unless $cond && $val;

    # We're checking for validity here, so any value is OK
    trick_taint($val);

    my $dbh = Bugzilla->dbh;

    my ($id, $login, $name, $disabledtext, $mybugslink) =
        $dbh->selectrow_array(qq{SELECT userid, login_name, realname,
                                        disabledtext, mybugslink
                                 FROM profiles WHERE $cond},
                                 undef, $val);

    return undef unless defined $id;

    $self->{'id'}             = $id;
    $self->{'name'}           = $name;
    $self->{'login'}          = $login;
    $self->{'disabledtext'}   = $disabledtext;
    $self->{'showmybugslink'} = $mybugslink;

    return $self;
}

# Accessors for user attributes
sub id { $_[0]->{id}; }
sub login { $_[0]->{login}; }
sub email { $_[0]->{login} . Bugzilla->params->{'emailsuffix'}; }
sub name { $_[0]->{name}; }
sub disabledtext { $_[0]->{'disabledtext'}; }
sub is_disabled { $_[0]->disabledtext ? 1 : 0; }
sub showmybugslink { $_[0]->{showmybugslink}; }

sub set_authorizer {
    my ($self, $authorizer) = @_;
    $self->{authorizer} = $authorizer;
}
sub authorizer {
    my ($self) = @_;
    if (!$self->{authorizer}) {
        require Bugzilla::Auth;
        $self->{authorizer} = new Bugzilla::Auth();
    }
    return $self->{authorizer};
}

# Generate a string to identify the user by name + login if the user
# has a name or by login only if she doesn't.
sub identity {
    my $self = shift;

    return "" unless $self->id;

    if (!defined $self->{identity}) {
        $self->{identity} = 
          $self->{name} ? "$self->{name} <$self->{login}>" : $self->{login};
    }

    return $self->{identity};
}

sub nick {
    my $self = shift;

    return "" unless $self->id;

    if (!defined $self->{nick}) {
        $self->{nick} = (split(/@/, $self->{login}, 2))[0];
    }

    return $self->{nick};
}

sub queries {
    my $self = shift;

    return $self->{queries} if defined $self->{queries};
    return [] unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $used_in_whine_ref = $dbh->selectall_hashref('
                    SELECT DISTINCT query_name
                      FROM whine_events we
                INNER JOIN whine_queries wq
                        ON we.id = wq.eventid
                     WHERE we.owner_userid = ?',
                     'query_name', undef, $self->id);

    # If the user is in any group, there may be shared queries to be included.
    my $or_nqgm_group_id_in_usergroups = '';
    if ($self->groups_as_string) {
        $or_nqgm_group_id_in_usergroups =
            'OR MAX(nqgm.group_id) IN (' . $self->groups_as_string . ') ';
    }

    my $queries_ref = $dbh->selectall_arrayref('
                    SELECT nq.id, MAX(userid) AS userid, name, query, query_type,
                           MAX(nqgm.group_id) AS shared_with_group,
                           COUNT(nql.namedquery_id) AS link_in_footer
                      FROM namedqueries AS nq
                      LEFT JOIN namedquery_group_map nqgm
                             ON nqgm.namedquery_id = nq.id
                      LEFT JOIN namedqueries_link_in_footer AS nql
                             ON nql.namedquery_id = nq.id
                            AND nql.user_id = ? ' .
                      $dbh->sql_group_by('nq.id', 'name, query, query_type') .
                  ' HAVING MAX(nq.userid) = ? ' .
                           $or_nqgm_group_id_in_usergroups .
                ' ORDER BY UPPER(name)',
                {'Slice'=>{}}, $self->id, $self->id);

    foreach my $queries_hash (@$queries_ref) {
        # For each query, determine whether it's being used in a whine.
        if (exists($$used_in_whine_ref{$queries_hash->{'name'}})) {
            $queries_hash->{'usedinwhine'} = 1;
        }

        # For shared queries, provide the sharer's user object.
        if ($queries_hash->{'userid'} != $self->id) {
            $queries_hash->{'user'} = new Bugzilla::User($queries_hash->{'userid'});
        }
    }
    $self->{queries} = $queries_ref;

    return $self->{queries};
}

sub settings {
    my ($self) = @_;

    return $self->{'settings'} if (defined $self->{'settings'});

    # IF the user is logged in
    # THEN get the user's settings
    # ELSE get default settings
    if ($self->id) {
        $self->{'settings'} = get_all_settings($self->id);
    } else {
        $self->{'settings'} = get_defaults();
    }

    return $self->{'settings'};
}

sub flush_queries_cache {
    my $self = shift;

    delete $self->{queries};
}

sub groups {
    my $self = shift;

    return $self->{groups} if defined $self->{groups};
    return {} unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $groups = $dbh->selectcol_arrayref(q{SELECT DISTINCT groups.name, group_id
                                              FROM groups, user_group_map
                                             WHERE groups.id=user_group_map.group_id
                                               AND user_id=?
                                               AND isbless=0},
                                          { Columns=>[1,2] },
                                          $self->{id});

    # The above gives us an arrayref [name, id, name, id, ...]
    # Convert that into a hashref
    my %groups = @$groups;
    my @groupidstocheck = values(%groups);
    my %groupidschecked = ();
    my $rows = $dbh->selectall_arrayref(
                "SELECT DISTINCT groups.name, groups.id, member_id
                            FROM group_group_map
                      INNER JOIN groups
                              ON groups.id = grantor_id
                           WHERE grant_type = " . GROUP_MEMBERSHIP);
    my %group_names = ();
    my %group_membership = ();
    foreach my $row (@$rows) {
        my ($member_name, $grantor_id, $member_id) = @$row; 
        # Just save the group names
        $group_names{$grantor_id} = $member_name;
        
        # And group membership
        push (@{$group_membership{$member_id}}, $grantor_id);
    }
    
    # Let's walk the groups hierarchy tree (using FIFO)
    # On the first iteration it's pre-filled with direct groups 
    # membership. Later on, each group can add its own members into the
    # FIFO. Circular dependencies are eliminated by checking
    # $groupidschecked{$member_id} hash values.
    # As a result, %groups will have all the groups we are the member of.
    while ($#groupidstocheck >= 0) {
        # Pop the head group from FIFO
        my $member_id = shift @groupidstocheck;
        
        # Skip the group if we have already checked it
        if (!$groupidschecked{$member_id}) {
            # Mark group as checked
            $groupidschecked{$member_id} = 1;
            
            # Add all its members to the FIFO check list
            # %group_membership contains arrays of group members 
            # for all groups. Accessible by group number.
            foreach my $newgroupid (@{$group_membership{$member_id}}) {
                push @groupidstocheck, $newgroupid 
                    if (!$groupidschecked{$newgroupid});
            }
            # Note on if clause: we could have group in %groups from 1st
            # query and do not have it in second one
            $groups{$group_names{$member_id}} = $member_id 
                if $group_names{$member_id} && $member_id;
        }
    }
    $self->{groups} = \%groups;

    return $self->{groups};
}

sub groups_as_string {
    my $self = shift;
    return (join(',',values(%{$self->groups})) || '-1');
}

sub bless_groups {
    my $self = shift;

    return $self->{'bless_groups'} if defined $self->{'bless_groups'};
    return [] unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $query;
    my $connector;
    my @bindValues;

    if ($self->in_group('editusers')) {
        # Users having editusers permissions may bless all groups.
        $query = 'SELECT DISTINCT id, name, description FROM groups';
        $connector = 'WHERE';
    }
    else {
        # Get all groups for the user where:
        #    + They have direct bless privileges
        #    + They are a member of a group that inherits bless privs.
        $query = q{
            SELECT DISTINCT groups.id, groups.name, groups.description
                       FROM groups, user_group_map, group_group_map AS ggm
                      WHERE user_group_map.user_id = ?
                        AND ((user_group_map.isbless = 1
                              AND groups.id=user_group_map.group_id)
                             OR (groups.id = ggm.grantor_id
                                 AND ggm.grant_type = ?
                                 AND ggm.member_id IN(} .
                                 $self->groups_as_string . 
                               q{)))};
        $connector = 'AND';
        @bindValues = ($self->id, GROUP_BLESS);
    }

    # If visibilitygroups are used, restrict the set of groups.
    if (!$self->in_group('editusers')
        && Bugzilla->params->{'usevisibilitygroups'}) 
    {
        # Users need to see a group in order to bless it.
        my $visibleGroups = join(', ', @{$self->visible_groups_direct()})
            || return $self->{'bless_groups'} = [];
        $query .= " $connector id in ($visibleGroups)";
    }

    $query .= ' ORDER BY name';

    return $self->{'bless_groups'} =
        $dbh->selectall_arrayref($query, {'Slice' => {}}, @bindValues);
}

sub in_group {
    my ($self, $group) = @_;
    return exists $self->groups->{$group} ? 1 : 0;
}

sub in_group_id {
    my ($self, $id) = @_;
    my %j = reverse(%{$self->groups});
    return exists $j{$id} ? 1 : 0;
}

sub can_see_user {
    my ($self, $otherUser) = @_;
    my $query;

    if (Bugzilla->params->{'usevisibilitygroups'}) {
        # If the user can see no groups, then no users are visible either.
        my $visibleGroups = $self->visible_groups_as_string() || return 0;
        $query = qq{SELECT COUNT(DISTINCT userid)
                    FROM profiles, user_group_map
                    WHERE userid = ?
                    AND user_id = userid
                    AND isbless = 0
                    AND group_id IN ($visibleGroups)
                   };
    } else {
        $query = qq{SELECT COUNT(userid)
                    FROM profiles
                    WHERE userid = ?
                   };
    }
    return Bugzilla->dbh->selectrow_array($query, undef, $otherUser->id);
}

sub can_edit_product {
    my ($self, $prod_id) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth = $self->{sthCanEditProductId};
    my $userid = $self->{id};
    my $query = q{SELECT group_id FROM group_control_map 
                   WHERE product_id =? 
                     AND canedit != 0 };
    if (%{$self->groups}) {
        my $groups = join(',', values(%{$self->groups}));
        $query .= qq{AND group_id NOT IN($groups)};
    }
    unless ($sth) { $sth = $dbh->prepare($query); }
    $sth->execute($prod_id);
    $self->{sthCanEditProductId} = $sth;
    my $result = $sth->fetchrow_array();
    
    return (!defined($result));
}

sub can_see_bug {
    my ($self, $bugid) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth  = $self->{sthCanSeeBug};
    my $userid  = $self->{id};
    # Get fields from bug, presence of user on cclist, and determine if
    # the user is missing any groups required by the bug. The prepared query
    # is cached because this may be called for every row in buglists or
    # every bug in a dependency list.
    unless ($sth) {
        $sth = $dbh->prepare("SELECT 1, reporter, assigned_to, qa_contact,
                             reporter_accessible, cclist_accessible,
                             COUNT(cc.who), COUNT(bug_group_map.bug_id)
                             FROM bugs
                             LEFT JOIN cc 
                               ON cc.bug_id = bugs.bug_id
                               AND cc.who = $userid
                             LEFT JOIN bug_group_map 
                               ON bugs.bug_id = bug_group_map.bug_id
                               AND bug_group_map.group_ID NOT IN(" .
                               $self->groups_as_string .
                               ") WHERE bugs.bug_id = ? 
                               AND creation_ts IS NOT NULL " .
                             $dbh->sql_group_by('bugs.bug_id', 'reporter, ' .
                             'assigned_to, qa_contact, reporter_accessible, ' .
                             'cclist_accessible'));
    }
    $sth->execute($bugid);
    my ($ready, $reporter, $owner, $qacontact, $reporter_access, $cclist_access,
        $isoncclist, $missinggroup) = $sth->fetchrow_array();
    $sth->finish;
    $self->{sthCanSeeBug} = $sth;
    return ($ready
            && ((($reporter == $userid) && $reporter_access)
                || (Bugzilla->params->{'useqacontact'} 
                    && $qacontact && ($qacontact == $userid))
                || ($owner == $userid)
                || ($isoncclist && $cclist_access)
                || (!$missinggroup)));
}

sub can_see_product {
    my ($self, $product_name) = @_;

    return scalar(grep {$_->name eq $product_name} @{$self->get_selectable_products});
}

sub get_selectable_products {
    my $self = shift;
    my $classification_id = shift;

    if (defined $self->{selectable_products}) {
        return $self->{selectable_products};
    }

    my $dbh = Bugzilla->dbh;
    my @params = ();

    my $query = "SELECT id " .
                "FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id ";
    if (Bugzilla->params->{'useentrygroupdefault'}) {
        $query .= "AND group_control_map.entry != 0 ";
    } else {
        $query .= "AND group_control_map.membercontrol = " .
                  CONTROLMAPMANDATORY . " ";
    }
    $query .= "AND group_id NOT IN(" . 
               $self->groups_as_string . ") " .
              "WHERE group_id IS NULL ";

    if (Bugzilla->params->{'useclassification'} && $classification_id) {
        $query .= "AND classification_id = ? ";
        detaint_natural($classification_id);
        push(@params, $classification_id);
    }

    $query .= "ORDER BY name";

    my $prod_ids = $dbh->selectcol_arrayref($query, undef, @params);
    my @products;
    foreach my $prod_id (@$prod_ids) {
        push(@products, new Bugzilla::Product($prod_id));
    }
    $self->{selectable_products} = \@products;
    return $self->{selectable_products};
}

sub get_selectable_classifications {
    my ($self) = @_;

    if (defined $self->{selectable_classifications}) {
        return $self->{selectable_classifications};
    }

    my $products = $self->get_selectable_products;

    my $class;
    foreach my $product (@$products) {
        $class->{$product->classification_id} ||= 
            new Bugzilla::Classification($product->classification_id);
    }
    my @sorted_class = sort {lc($a->name) cmp lc($b->name)} (values %$class);
    $self->{selectable_classifications} = \@sorted_class;
    return $self->{selectable_classifications};
}

sub can_enter_product {
    my ($self, $product_name, $warn) = @_;
    my $dbh = Bugzilla->dbh;

    if (!defined($product_name)) {
        return unless $warn;
        ThrowUserError('no_products');
    }
    trick_taint($product_name);

    # Checks whether the user has access to the product.
    my $has_access = $dbh->selectrow_array('SELECT CASE WHEN group_id IS NULL
                                                        THEN 1 ELSE 0 END
                                              FROM products
                                         LEFT JOIN group_control_map
                                                ON group_control_map.product_id = products.id
                                               AND group_control_map.entry != 0
                                               AND group_id NOT IN (' . $self->groups_as_string . ')
                                             WHERE products.name = ? ' .
                                             $dbh->sql_limit(1),
                                            undef, $product_name);

    if (!$has_access) {
        return unless $warn;
        ThrowUserError('entry_access_denied', { product => $product_name });
    }

    # Checks whether the product is open for new bugs and
    # has at least one component and one version.
    my ($is_open, $has_version) = 
        $dbh->selectrow_array('SELECT CASE WHEN disallownew = 0
                                           THEN 1 ELSE 0 END,
                                      CASE WHEN versions.value IS NOT NULL
                                           THEN 1 ELSE 0 END
                                 FROM products
                           INNER JOIN components
                                   ON components.product_id = products.id
                            LEFT JOIN versions
                                   ON versions.product_id = products.id
                                WHERE products.name = ? ' .
                               $dbh->sql_limit(1), undef, $product_name);

    # Returns undef if the product has no components
    # Returns 0 if the product has no versions, or is closed for bug entry
    # Returns 1 if the user can enter bugs into the product
    return ($is_open && $has_version) unless $warn;

    # (undef, undef): the product has no components,
    # (0,     ?)    : the product is closed for new bug entry,
    # (?,     0)    : the product has no versions,
    # (1,     1)    : the user can enter bugs into the product,
    if (!defined $is_open) {
        ThrowUserError('missing_component', { product => $product_name });
    } elsif (!$is_open) {
        ThrowUserError('product_disabled', { product => $product_name });
    } elsif (!$has_version) {
        ThrowUserError('missing_version', { product => $product_name });
    }
    return 1;
}

sub get_enterable_products {
    my $self = shift;

    if (defined $self->{enterable_products}) {
        return $self->{enterable_products};
    }

    my @products;
    foreach my $product (Bugzilla::Product::get_all_products()) {
        if ($self->can_enter_product($product->name)) {
            push(@products, $product);
        }
    }
    $self->{enterable_products} = \@products;
    return $self->{enterable_products};
}

# visible_groups_inherited returns a reference to a list of all the groups
# whose members are visible to this user.
sub visible_groups_inherited {
    my $self = shift;
    return $self->{visible_groups_inherited} if defined $self->{visible_groups_inherited};
    return [] unless $self->id;
    my @visgroups = @{$self->visible_groups_direct};
    @visgroups = @{$self->flatten_group_membership(@visgroups)};
    $self->{visible_groups_inherited} = \@visgroups;
    return $self->{visible_groups_inherited};
}

# visible_groups_direct returns a reference to a list of all the groups that
# are visible to this user.
sub visible_groups_direct {
    my $self = shift;
    my @visgroups = ();
    return $self->{visible_groups_direct} if defined $self->{visible_groups_direct};
    return [] unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $glist = join(',',(-1,values(%{$self->groups})));
    my $sth = $dbh->prepare("SELECT DISTINCT grantor_id
                                FROM group_group_map
                               WHERE member_id IN($glist)
                                 AND grant_type=" . GROUP_VISIBLE);
    $sth->execute();

    while (my ($row) = $sth->fetchrow_array) {
        push @visgroups,$row;
    }
    $self->{visible_groups_direct} = \@visgroups;

    return $self->{visible_groups_direct};
}

sub visible_groups_as_string {
    my $self = shift;
    return join(', ', @{$self->visible_groups_inherited()});
}

# This function defines the groups a user may share a query with.
# More restrictive sites may want to build this reference to a list of group IDs
# from bless_groups instead of mirroring visible_groups_inherited, perhaps.
sub queryshare_groups {
    my $self = shift;
    if ($self->in_group(Bugzilla->params->{'querysharegroup'})) {
        return $self->visible_groups_inherited();
    }
    else {
        return [];
    }
}

sub queryshare_groups_as_string {
    my $self = shift;
    return join(', ', @{$self->queryshare_groups()});
}

sub derive_regexp_groups {
    my ($self) = @_;

    my $id = $self->id;
    return unless $id;

    my $dbh = Bugzilla->dbh;

    my $sth;

    # avoid races, we are only up to date as of the BEGINNING of this process
    my $time = $dbh->selectrow_array("SELECT NOW()");

    # add derived records for any matching regexps

    $sth = $dbh->prepare("SELECT id, userregexp, user_group_map.group_id
                            FROM groups
                       LEFT JOIN user_group_map
                              ON groups.id = user_group_map.group_id
                             AND user_group_map.user_id = ?
                             AND user_group_map.grant_type = ?");
    $sth->execute($id, GRANT_REGEXP);

    my $group_insert = $dbh->prepare(q{INSERT INTO user_group_map
                                       (user_id, group_id, isbless, grant_type)
                                       VALUES (?, ?, 0, ?)});
    my $group_delete = $dbh->prepare(q{DELETE FROM user_group_map
                                       WHERE user_id = ?
                                         AND group_id = ?
                                         AND isbless = 0
                                         AND grant_type = ?});
    while (my ($group, $regexp, $present) = $sth->fetchrow_array()) {
        if (($regexp ne '') && ($self->{login} =~ m/$regexp/i)) {
            $group_insert->execute($id, $group, GRANT_REGEXP) unless $present;
        } else {
            $group_delete->execute($id, $group, GRANT_REGEXP) if $present;
        }
    }

    $dbh->do(q{UPDATE profiles SET refreshed_when = ? WHERE userid = ?},
             undef, ($time, $id));
}

sub product_responsibilities {
    my $self = shift;

    return $self->{'product_resp'} if defined $self->{'product_resp'};
    return [] unless $self->id;

    my $h = Bugzilla->dbh->selectall_arrayref(
        qq{SELECT products.name AS productname,
                  components.name AS componentname,
                  initialowner,
                  initialqacontact
           FROM products, components
           WHERE products.id = components.product_id
             AND ? IN (initialowner, initialqacontact)
          },
        {'Slice' => {}}, $self->id);
    $self->{'product_resp'} = $h;

    return $h;
}

sub can_bless {
    my $self = shift;

    if (!scalar(@_)) {
        # If we're called without an argument, just return 
        # whether or not we can bless at all.
        return scalar(@{$self->bless_groups}) ? 1 : 0;
    }

    # Otherwise, we're checking a specific group
    my $group_id = shift;
    return (grep {$$_{'id'} eq $group_id} (@{$self->bless_groups})) ? 1 : 0;
}

sub flatten_group_membership {
    my ($self, @groups) = @_;

    my $dbh = Bugzilla->dbh;
    my $sth;
    my @groupidstocheck = @groups;
    my %groupidschecked = ();
    $sth = $dbh->prepare("SELECT member_id FROM group_group_map
                             WHERE grantor_id = ? 
                               AND grant_type = " . GROUP_MEMBERSHIP);
    while (my $node = shift @groupidstocheck) {
        $sth->execute($node);
        my $member;
        while (($member) = $sth->fetchrow_array) {
            if (!$groupidschecked{$member}) {
                $groupidschecked{$member} = 1;
                push @groupidstocheck, $member;
                push @groups, $member unless grep $_ == $member, @groups;
            }
        }
    }
    return \@groups;
}

sub match {
    # Generates a list of users whose login name (email address) or real name
    # matches a substring or wildcard.
    # This is also called if matches are disabled (for error checking), but
    # in this case only the exact match code will end up running.

    # $str contains the string to match, while $limit contains the
    # maximum number of records to retrieve.
    my ($str, $limit, $exclude_disabled) = @_;
    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    my @users = ();
    return \@users if $str =~ /^\s*$/;

    # The search order is wildcards, then exact match, then substring search.
    # Wildcard matching is skipped if there is no '*', and exact matches will
    # not (?) have a '*' in them.  If any search comes up with something, the
    # ones following it will not execute.

    # first try wildcards
    my $wildstr = $str;

    if ($wildstr =~ s/\*/\%/g # don't do wildcards if no '*' in the string
        # or if we only want exact matches
        && Bugzilla->params->{'usermatchmode'} ne 'off') 
    {

        # Build the query.
        trick_taint($wildstr);
        my $query  = "SELECT DISTINCT login_name FROM profiles ";
        if (Bugzilla->params->{'usevisibilitygroups'}) {
            $query .= "INNER JOIN user_group_map
                               ON user_group_map.user_id = profiles.userid ";
        }
        $query .= "WHERE ("
            . $dbh->sql_istrcmp('login_name', '?', "LIKE") . " OR " .
              $dbh->sql_istrcmp('realname', '?', "LIKE") . ") ";
        if (Bugzilla->params->{'usevisibilitygroups'}) {
            $query .= "AND isbless = 0 " .
                      "AND group_id IN(" .
                      join(', ', (-1, @{$user->visible_groups_inherited})) . ") ";
        }
        $query    .= " AND disabledtext = '' " if $exclude_disabled;
        $query    .= " ORDER BY login_name ";
        $query    .= $dbh->sql_limit($limit) if $limit;

        # Execute the query, retrieve the results, and make them into
        # User objects.
        my $user_logins = $dbh->selectcol_arrayref($query, undef, ($wildstr, $wildstr));
        foreach my $login_name (@$user_logins) {
            push(@users, Bugzilla::User->new_from_login($login_name));
        }
    }
    else {    # try an exact match
        # Exact matches don't care if a user is disabled.
        trick_taint($str);
        my $user_id = $dbh->selectrow_array('SELECT userid FROM profiles
                                             WHERE ' . $dbh->sql_istrcmp('login_name', '?'),
                                             undef, $str);

        push(@users, new Bugzilla::User($user_id)) if $user_id;
    }

    # then try substring search
    if ((scalar(@users) == 0)
        && (Bugzilla->params->{'usermatchmode'} eq 'search')
        && (length($str) >= 3))
    {
        $str = lc($str);
        trick_taint($str);

        my $query   = "SELECT DISTINCT login_name FROM profiles ";
        if (Bugzilla->params->{'usevisibilitygroups'}) {
            $query .= "INNER JOIN user_group_map
                               ON user_group_map.user_id = profiles.userid ";
        }
        $query     .= " WHERE (" .
                $dbh->sql_position('?', 'LOWER(login_name)') . " > 0" . " OR " .
                $dbh->sql_position('?', 'LOWER(realname)') . " > 0) ";
        if (Bugzilla->params->{'usevisibilitygroups'}) {
            $query .= " AND isbless = 0" .
                      " AND group_id IN(" .
                join(', ', (-1, @{$user->visible_groups_inherited})) . ") ";
        }
        $query     .= " AND disabledtext = '' " if $exclude_disabled;
        $query    .= " ORDER BY login_name ";
        $query     .= $dbh->sql_limit($limit) if $limit;

        my $user_logins = $dbh->selectcol_arrayref($query, undef, ($str, $str));
        foreach my $login_name (@$user_logins) {
            push(@users, Bugzilla::User->new_from_login($login_name));
        }
    }
    return \@users;
}

# match_field() is a CGI wrapper for the match() function.
#
# Here's what it does:
#
# 1. Accepts a list of fields along with whether they may take multiple values
# 2. Takes the values of those fields from the first parameter, a $cgi object 
#    and passes them to match()
# 3. Checks the results of the match and displays confirmation or failure
#    messages as appropriate.
#
# The confirmation screen functions the same way as verify-new-product and
# confirm-duplicate, by rolling all of the state information into a
# form which is passed back, but in this case the searched fields are
# replaced with the search results.
#
# The act of displaying the confirmation or failure messages means it must
# throw a template and terminate.  When confirmation is sent, all of the
# searchable fields have been replaced by exact fields and the calling script
# is executed as normal.
#
# You also have the choice of *never* displaying the confirmation screen.
# In this case, match_field will return one of the three USER_MATCH 
# constants described in the POD docs. To make match_field behave this
# way, pass in MATCH_SKIP_CONFIRM as the third argument.
#
# match_field must be called early in a script, before anything external is
# done with the form data.
#
# In order to do a simple match without dealing with templates, confirmation,
# or globals, simply calling Bugzilla::User::match instead will be
# sufficient.

# How to call it:
#
# Bugzilla::User::match_field($cgi, {
#   'field_name'    => { 'type' => fieldtype },
#   'field_name2'   => { 'type' => fieldtype },
#   [...]
# });
#
# fieldtype can be either 'single' or 'multi'.
#

sub match_field {
    my $cgi          = shift;   # CGI object to look up fields in
    my $fields       = shift;   # arguments as a hash
    my $behavior     = shift || 0; # A constant that tells us how to act
    my $matches      = {};      # the values sent to the template
    my $matchsuccess = 1;       # did the match fail?
    my $need_confirm = 0;       # whether to display confirmation screen
    my $match_multiple = 0;     # whether we ever matched more than one user

    my $params = Bugzilla->params;

    # prepare default form values

    # What does a "--do_not_change--" field look like (if any)?
    my $dontchange = $cgi->param('dontchange');

    # Fields can be regular expressions matching multiple form fields
    # (f.e. "requestee-(\d+)"), so expand each non-literal field
    # into the list of form fields it matches.
    my $expanded_fields = {};
    foreach my $field_pattern (keys %{$fields}) {
        # Check if the field has any non-word characters.  Only those fields
        # can be regular expressions, so don't expand the field if it doesn't
        # have any of those characters.
        if ($field_pattern =~ /^\w+$/) {
            $expanded_fields->{$field_pattern} = $fields->{$field_pattern};
        }
        else {
            my @field_names = grep(/$field_pattern/, $cgi->param());
            foreach my $field_name (@field_names) {
                $expanded_fields->{$field_name} = 
                  { type => $fields->{$field_pattern}->{'type'} };
                
                # The field is a requestee field; in order for its name 
                # to show up correctly on the confirmation page, we need 
                # to find out the name of its flag type.
                if ($field_name =~ /^requestee-(\d+)$/) {
                    my $flag = Bugzilla::Flag::get($1);
                    $expanded_fields->{$field_name}->{'flag_type'} = 
                      $flag->{'type'};
                }
                elsif ($field_name =~ /^requestee_type-(\d+)$/) {
                    $expanded_fields->{$field_name}->{'flag_type'} = 
                      Bugzilla::FlagType::get($1);
                }
            }
        }
    }
    $fields = $expanded_fields;

    for my $field (keys %{$fields}) {

        # Tolerate fields that do not exist.
        #
        # This is so that fields like qa_contact can be specified in the code
        # and it won't break if the CGI object does not know about them.
        #
        # It has the side-effect that if a bad field name is passed it will be
        # quietly ignored rather than raising a code error.

        next if !defined $cgi->param($field);

        # Skip it if this is a --do_not_change-- field
        next if $dontchange && $dontchange eq $cgi->param($field);

        # We need to move the query to $raw_field, where it will be split up,
        # modified by the search, and put back into the CGI environment
        # incrementally.

        my $raw_field = join(" ", $cgi->param($field));

        # When we add back in values later, it matters that we delete
        # the field here, and not set it to '', so that we will add
        # things to an empty list, and not to a list containing one
        # empty string.
        # If the field accepts only one match (type eq "single") and
        # no match or more than one match is found for this field,
        # we will set it back to '' so that the field remains defined
        # outside this function (it was if we came here; else we would
        # have returned earlier above).
        # If the field accepts several matches (type eq "multi") and no match
        # is found, we leave this field undefined (= empty array).
        $cgi->delete($field);

        my @queries = ();

        # Now we either split $raw_field by spaces/commas and put the list
        # into @queries, or in the case of fields which only accept single
        # entries, we simply use the verbatim text.

        $raw_field =~ s/^\s+|\s+$//sg;  # trim leading/trailing space

        # single field
        if ($fields->{$field}->{'type'} eq 'single') {
            @queries = ($raw_field) unless $raw_field =~ /^\s*$/;

        # multi-field
        }
        elsif ($fields->{$field}->{'type'} eq 'multi') {
            @queries =  split(/[\s,]+/, $raw_field);

        }
        else {
            # bad argument
            ThrowCodeError('bad_arg',
                           { argument => $fields->{$field}->{'type'},
                             function =>  'Bugzilla::User::match_field',
                           });
        }

        my $limit = 0;
        if ($params->{'maxusermatches'}) {
            $limit = $params->{'maxusermatches'} + 1;
        }

        for my $query (@queries) {

            my $users = match(
                $query,   # match string
                $limit,   # match limit
                1         # exclude_disabled
            );

            # skip confirmation for exact matches
            if ((scalar(@{$users}) == 1)
                && (lc(@{$users}[0]->{'login'}) eq lc($query)))
            {
                $cgi->append(-name=>$field,
                             -values=>[@{$users}[0]->{'login'}]);

                next;
            }

            $matches->{$field}->{$query}->{'users'}  = $users;
            $matches->{$field}->{$query}->{'status'} = 'success';

            # here is where it checks for multiple matches

            if (scalar(@{$users}) == 1) { # exactly one match

                $cgi->append(-name=>$field,
                             -values=>[@{$users}[0]->{'login'}]);

                $need_confirm = 1 if $params->{'confirmuniqueusermatch'};

            }
            elsif ((scalar(@{$users}) > 1)
                    && ($params->{'maxusermatches'} != 1)) {
                $need_confirm = 1;
                $match_multiple = 1;

                if (($params->{'maxusermatches'})
                   && (scalar(@{$users}) > $params->{'maxusermatches'}))
                {
                    $matches->{$field}->{$query}->{'status'} = 'trunc';
                    pop @{$users};  # take the last one out
                }

            }
            else {
                # everything else fails
                $matchsuccess = 0; # fail
                $matches->{$field}->{$query}->{'status'} = 'fail';
                $need_confirm = 1;  # confirmation screen shows failures
            }
        }
        # Above, we deleted the field before adding matches. If no match
        # or more than one match has been found for a field expecting only
        # one match (type eq "single"), we set it back to '' so
        # that the caller of this function can still check whether this
        # field was defined or not (and it was if we came here).
        if (!defined $cgi->param($field)
            && $fields->{$field}->{'type'} eq 'single') {
            $cgi->param($field, '');
        }
    }

    my $retval;
    if (!$matchsuccess) {
        $retval = USER_MATCH_FAILED;
    }
    elsif ($match_multiple) {
        $retval = USER_MATCH_MULTIPLE;
    }
    else {
        $retval = USER_MATCH_SUCCESS;
    }

    # Skip confirmation if we were told to, or if we don't need to confirm.
    return $retval if ($behavior == MATCH_SKIP_CONFIRM || !$need_confirm);

    my $template = Bugzilla->template;
    my $vars = {};

    $vars->{'script'}        = Bugzilla->cgi->url(-relative => 1); # for self-referencing URLs
    $vars->{'fields'}        = $fields; # fields being matched
    $vars->{'matches'}       = $matches; # matches that were made
    $vars->{'matchsuccess'}  = $matchsuccess; # continue or fail

    print Bugzilla->cgi->header();

    $template->process("global/confirm-user-match.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;

}

# Changes in some fields automatically trigger events. The 'field names' are
# from the fielddefs table. We really should be using proper field names 
# throughout.
our %names_to_events = (
    'Resolution'             => EVT_OPENED_CLOSED,
    'Keywords'               => EVT_KEYWORD,
    'CC'                     => EVT_CC,
    'Severity'               => EVT_PROJ_MANAGEMENT,
    'Priority'               => EVT_PROJ_MANAGEMENT,
    'Status'                 => EVT_PROJ_MANAGEMENT,
    'Target Milestone'       => EVT_PROJ_MANAGEMENT,
    'Attachment description' => EVT_ATTACHMENT_DATA,
    'Attachment mime type'   => EVT_ATTACHMENT_DATA,
    'Attachment is patch'    => EVT_ATTACHMENT_DATA,
    'BugsThisDependsOn'      => EVT_DEPEND_BLOCK,
    'OtherBugsDependingOnThis' => EVT_DEPEND_BLOCK);

# Returns true if the user wants mail for a given bug change.
# Note: the "+" signs before the constants suppress bareword quoting.
sub wants_bug_mail {
    my $self = shift;
    my ($bug_id, $relationship, $fieldDiffs, $commentField, $changer) = @_;

    # Don't send any mail, ever, if account is disabled 
    # XXX Temporary Compatibility Change 1 of 2:
    # This code is disabled for the moment to make the behaviour like the old
    # system, which sent bugmail to disabled accounts.
    # return 0 if $self->{'disabledtext'};
    
    # Make a list of the events which have happened during this bug change,
    # from the point of view of this user.    
    my %events;    
    foreach my $ref (@$fieldDiffs) {
        my ($who, $fieldName, $when, $old, $new) = @$ref;
        # A change to any of the above fields sets the corresponding event
        if (defined($names_to_events{$fieldName})) {
            $events{$names_to_events{$fieldName}} = 1;
        }
        else {
            # Catch-all for any change not caught by a more specific event
            # XXX: Temporary Compatibility Change 2 of 2:
            # This code is disabled, and replaced with the code a few lines
            # below, in order to make the behaviour more like the original, 
            # which only added this event if _all_ changes were of "other" type.
            # $events{+EVT_OTHER} = 1;            
        }

        # If the user is in a particular role and the value of that role
        # changed, we need the ADDED_REMOVED event.
        if (($fieldName eq "AssignedTo" && $relationship == REL_ASSIGNEE) ||
            ($fieldName eq "QAContact" && $relationship == REL_QA)) 
        {
            $events{+EVT_ADDED_REMOVED} = 1;
        }
        
        if ($fieldName eq "CC") {
            my $login = $self->login;
            my $inold = ($old =~ /^(.*,)?\Q$login\E(,.*)?$/);
            my $innew = ($new =~ /^(.*,)?\Q$login\E(,.*)?$/);
            if ($inold != $innew)
            {
                $events{+EVT_ADDED_REMOVED} = 1;
            }
        }
    }

    if ($commentField =~ /Created an attachment \(/) {
        $events{+EVT_ATTACHMENT} = 1;
    }
    elsif ($commentField ne '') {
        $events{+EVT_COMMENT} = 1;
    }
    
    my @event_list = keys %events;
    
    # XXX Temporary Compatibility Change 2 of 2:
    # See above comment.
    if (!scalar(@event_list)) {
      @event_list = (EVT_OTHER);
    }
    
    my $wants_mail = $self->wants_mail(\@event_list, $relationship);

    # The negative events are handled separately - they can't be incorporated
    # into the first wants_mail call, because they are of the opposite sense.
    # 
    # We do them separately because if _any_ of them are set, we don't want
    # the mail.
    if ($wants_mail && $changer && ($self->{'login'} eq $changer)) {
        $wants_mail &= $self->wants_mail([EVT_CHANGED_BY_ME], $relationship);
    }    
    
    if ($wants_mail) {
        my $dbh = Bugzilla->dbh;
        # We don't create a Bug object from the bug_id here because we only
        # need one piece of information, and doing so (as of 2004-11-23) slows
        # down bugmail sending by a factor of 2. If Bug creation was more
        # lazy, this might not be so bad.
        my $bug_status = $dbh->selectrow_array('SELECT bug_status
                                                FROM bugs WHERE bug_id = ?',
                                                undef, $bug_id);

        if ($bug_status eq "UNCONFIRMED") {
            $wants_mail &= $self->wants_mail([EVT_UNCONFIRMED], $relationship);
        }
    }
    
    return $wants_mail;
}

# Returns true if the user wants mail for a given set of events.
sub wants_mail {
    my $self = shift;
    my ($events, $relationship) = @_;
    
    # Don't send any mail, ever, if account is disabled 
    # XXX Temporary Compatibility Change 1 of 2:
    # This code is disabled for the moment to make the behaviour like the old
    # system, which sent bugmail to disabled accounts.
    # return 0 if $self->{'disabledtext'};
    
    # No mail if there are no events
    return 0 if !scalar(@$events);
    
    my $dbh = Bugzilla->dbh;
    
    # If a relationship isn't given, default to REL_ANY.
    if (!defined($relationship)) {
        $relationship = REL_ANY;
    }
    
    my $wants_mail = 
        $dbh->selectrow_array('SELECT 1
                                 FROM email_setting
                                WHERE user_id = ?
                                  AND relationship = ?
                                  AND event IN (' . join(',', @$events) . ') ' .
                                      $dbh->sql_limit(1),
                              undef, ($self->{'id'}, $relationship));

    return defined($wants_mail) ? 1 : 0;
}

sub is_mover {
    my $self = shift;

    if (!defined $self->{'is_mover'}) {
        my @movers = map { trim($_) } split(',', Bugzilla->params->{'movers'});
        $self->{'is_mover'} = ($self->id
                               && lsearch(\@movers, $self->login) != -1);
    }
    return $self->{'is_mover'};
}

sub get_userlist {
    my $self = shift;

    return $self->{'userlist'} if defined $self->{'userlist'};

    my $dbh = Bugzilla->dbh;
    my $query  = "SELECT DISTINCT login_name, realname,";
    if (Bugzilla->params->{'usevisibilitygroups'}) {
        $query .= " COUNT(group_id) ";
    } else {
        $query .= " 1 ";
    }
    $query     .= "FROM profiles ";
    if (Bugzilla->params->{'usevisibilitygroups'}) {
        $query .= "LEFT JOIN user_group_map " .
                  "ON user_group_map.user_id = userid AND isbless = 0 " .
                  "AND group_id IN(" .
                  join(', ', (-1, @{$self->visible_groups_inherited})) . ")";
    }
    $query    .= " WHERE disabledtext = '' ";
    $query    .= $dbh->sql_group_by('userid', 'login_name, realname');

    my $sth = $dbh->prepare($query);
    $sth->execute;

    my @userlist;
    while (my($login, $name, $visible) = $sth->fetchrow_array) {
        push @userlist, {
            login => $login,
            identity => $name ? "$name <$login>" : $login,
            visible => $visible,
        };
    }
    @userlist = sort { lc $$a{'identity'} cmp lc $$b{'identity'} } @userlist;

    $self->{'userlist'} = \@userlist;
    return $self->{'userlist'};
}

sub insert_new_user {
    my ($username, $realname, $password, $disabledtext) = (@_);
    my $dbh = Bugzilla->dbh;

    $disabledtext ||= '';

    # If not specified, generate a new random password for the user.
    # If the password is '*', do not encrypt it; we are creating a user
    # based on the ENV auth method.
    $password ||= generate_random_password();
    my $cryptpassword = ($password ne '*') ? bz_crypt($password) : $password;

    # XXX - These should be moved into is_available_username or validate_email_syntax
    #       At the least, they shouldn't be here. They're safe for now, though.
    trick_taint($username);
    trick_taint($realname);

    # Insert the new user record into the database.
    $dbh->do("INSERT INTO profiles 
                          (login_name, realname, cryptpassword, disabledtext,
                           refreshed_when) 
                   VALUES (?, ?, ?, ?, '1901-01-01 00:00:00')",
             undef, 
             ($username, $realname, $cryptpassword, $disabledtext));

    # Turn on all email for the new user
    my $new_userid = $dbh->bz_last_key('profiles', 'userid');

    foreach my $rel (RELATIONSHIPS) {
        foreach my $event (POS_EVENTS, NEG_EVENTS) {
            # These "exceptions" define the default email preferences.
            # 
            # We enable mail unless the change was made by the user, or it's
            # just a CC list addition and the user is not the reporter.
            next if ($event == EVT_CHANGED_BY_ME);
            next if (($event == EVT_CC) && ($rel != REL_REPORTER));

            $dbh->do('INSERT INTO email_setting (user_id, relationship, event)
                      VALUES (?, ?, ?)', undef, ($new_userid, $rel, $event));
        }
    }

    foreach my $event (GLOBAL_EVENTS) {
        $dbh->do('INSERT INTO email_setting (user_id, relationship, event)
                  VALUES (?, ?, ?)', undef, ($new_userid, REL_ANY, $event));
    }

    my $user = new Bugzilla::User($new_userid);
    $user->derive_regexp_groups();

    # Add the creation date to the profiles_activity table.
    # $who is the user who created the new user account, i.e. either an
    # admin or the new user himself.
    my $who = Bugzilla->user->id || $user->id;
    my $creation_date_fieldid = get_field_id('creation_ts');

    $dbh->do('INSERT INTO profiles_activity
                          (userid, who, profiles_when, fieldid, newvalue)
                   VALUES (?, ?, NOW(), ?, NOW())',
                   undef, ($user->id, $who, $creation_date_fieldid));

    # Return the password to the calling code so it can be included
    # in an email sent to the user.
    return $password;
}

sub is_available_username {
    my ($username, $old_username) = @_;

    if(login_to_id($username) != 0) {
        return 0;
    }

    my $dbh = Bugzilla->dbh;
    # $username is safe because it is only used in SELECT placeholders.
    trick_taint($username);
    # Reject if the new login is part of an email change which is
    # still in progress
    #
    # substring/locate stuff: bug 165221; this used to use regexes, but that
    # was unsafe and required weird escaping; using substring to pull out
    # the new/old email addresses and sql_position() to find the delimiter (':')
    # is cleaner/safer
    my $sth = $dbh->prepare(
        "SELECT eventdata FROM tokens WHERE tokentype = 'emailold'
        AND SUBSTRING(eventdata, 1, (" 
        . $dbh->sql_position(q{':'}, 'eventdata') . "-  1)) = ?
        OR SUBSTRING(eventdata, (" 
        . $dbh->sql_position(q{':'}, 'eventdata') . "+ 1)) = ?");
    $sth->execute($username, $username);

    if (my ($eventdata) = $sth->fetchrow_array()) {
        # Allow thru owner of token
        if($old_username && ($eventdata eq "$old_username:$username")) {
            return 1;
        }
        return 0;
    }

    return 1;
}

sub login_to_id {
    my ($login, $throw_error) = @_;
    my $dbh = Bugzilla->dbh;
    # $login will only be used by the following SELECT statement, so it's safe.
    trick_taint($login);
    my $user_id = $dbh->selectrow_array("SELECT userid FROM profiles WHERE " .
                                        $dbh->sql_istrcmp('login_name', '?'),
                                        undef, $login);
    if ($user_id) {
        return $user_id;
    } elsif ($throw_error) {
        ThrowUserError('invalid_username', { name => $login });
    } else {
        return 0;
    }
}

sub user_id_to_login {
    my $user_id = shift;
    my $dbh = Bugzilla->dbh;

    return '' unless ($user_id && detaint_natural($user_id));

    my $login = $dbh->selectrow_array('SELECT login_name FROM profiles
                                       WHERE userid = ?', undef, $user_id);
    return $login || '';
}

sub validate_password {
    my ($password, $matchpassword) = @_;

    if (length($password) < USER_PASSWORD_MIN_LENGTH) {
        ThrowUserError('password_too_short');
    } elsif (length($password) > USER_PASSWORD_MAX_LENGTH) {
        ThrowUserError('password_too_long');
    } elsif ((defined $matchpassword) && ($password ne $matchpassword)) {
        ThrowUserError('passwords_dont_match');
    }
    return 1;
}

sub UserInGroup {
    return exists Bugzilla->user->groups->{$_[0]} ? 1 : 0;
}

1;

__END__

=head1 NAME

Bugzilla::User - Object for a Bugzilla user

=head1 SYNOPSIS

  use Bugzilla::User;

  my $user = new Bugzilla::User($id);

  my @get_selectable_classifications = 
      $user->get_selectable_classifications;

  # Class Functions
  $password = insert_new_user($username, $realname, $password, $disabledtext);

=head1 DESCRIPTION

This package handles Bugzilla users. Data obtained from here is read-only;
there is currently no way to modify a user from this package.

Note that the currently logged in user (if any) is available via
L<Bugzilla-E<gt>user|Bugzilla/"user">.

=head1 CONSTANTS

=over

=item C<USER_MATCH_MULTIPLE>

Returned by C<match_field()> when at least one field matched more than 
one user, but no matches failed.

=item C<USER_MATCH_FAILED>

Returned by C<match_field()> when at least one field failed to match 
anything.

=item C<USER_MATCH_SUCCESS>

Returned by C<match_field()> when all fields successfully matched only one
user.

=item C<MATCH_SKIP_CONFIRM>

Passed in to match_field to tell match_field to never display a 
confirmation screen.

=back

=head1 METHODS

=over 4

=item C<new($userid)>

Creates a new C<Bugzilla::User> object for the given user id.  If no user
id was given, a blank object is created with no user attributes.

If an id was given but there was no matching user found, undef is returned.

=begin undocumented

=item C<new_from_login($login)>

Creates a new C<Bugzilla::User> object given the provided login. Returns
C<undef> if no matching user is found.

This routine should not be required in general; most scripts should be using
userids instead.

=end undocumented

=item C<id>

Returns the userid for this user.

=item C<login>

Returns the login name for this user.

=item C<email>

Returns the user's email address. Currently this is the same value as the
login.

=item C<name>

Returns the 'real' name for this user, if any.

=item C<showmybugslink>

Returns C<1> if the user has set his preference to show the 'My Bugs' link in
the page footer, and C<0> otherwise.

=item C<identity>

Returns a string for the identity of the user. This will be of the form
C<name E<lt>emailE<gt>> if the user has specified a name, and C<email>
otherwise.

=item C<nick>

Returns a user "nickname" -- i.e. a shorter, not-necessarily-unique name by
which to identify the user. Currently the part of the user's email address
before the at sign (@), but that could change, especially if we implement
usernames not dependent on email address.

=item C<authorizer>

This is the L<Bugzilla::Auth> object that the User logged in with.
If the user hasn't logged in yet, a new, empty Bugzilla::Auth() object is
returned.

=item C<set_authorizer($authorizer)>

Sets the L<Bugzilla::Auth> object to be returned by C<authorizer()>.
Should only be called by C<Bugzilla::Auth::login>, for the most part.

=item C<queries>

Returns an array of the user's named queries, sorted in a case-insensitive
order by name. Each entry is a hash with five keys:

=over

=item *

id - The ID of the query

=item *

userid - The query owner's user ID

=item *

name - The name of the query

=item *

query - The text for the query

=item *

link_in_footer - Whether or not the query should be displayed in the footer.

=back

=item C<disabledtext>

Returns the disable text of the user, if any.

=item C<settings>

Returns a hash of hashes which holds the user's settings. The first key is
the name of the setting, as found in setting.name. The second key is one of:
is_enabled     - true if the user is allowed to set the preference themselves;
                 false to force the site defaults
                 for themselves or must accept the global site default value
default_value  - the global site default for this setting
value          - the value of this setting for this user. Will be the same
                 as the default_value if the user is not logged in, or if 
                 is_default is true.
is_default     - a boolean to indicate whether the user has chosen to make
                 a preference for themself or use the site default.

=item C<flush_queries_cache>

Some code modifies the set of stored queries. Because C<Bugzilla::User> does
not handle these modifications, but does cache the result of calling C<queries>
internally, such code must call this method to flush the cached result.

=item C<groups>

Returns a hashref of group names for groups the user is a member of. The keys
are the names of the groups, whilst the values are the respective group ids.
(This is so that a set of all groupids for groups the user is in can be
obtained by C<values(%{$user-E<gt>groups})>.)

=item C<groups_as_string>

Returns a string containing a comma-seperated list of numeric group ids.  If
the user is not a member of any groups, returns "-1". This is most often used
within an SQL IN() function.

=item C<in_group>

Determines whether or not a user is in the given group by name. 

=item C<in_group_id>

Determines whether or not a user is in the given group by id. 

=item C<bless_groups>

Returns an arrayref of hashes of C<groups> entries, where the keys of each hash
are the names of C<id>, C<name> and C<description> columns of the C<groups>
table.
The arrayref consists of the groups the user can bless, taking into account
that having editusers permissions means that you can bless all groups, and
that you need to be aware of a group in order to bless a group.

=item C<can_see_user(user)>

Returns 1 if the specified user account exists and is visible to the user,
0 otherwise.

=item C<can_edit_product(prod_id)>

Determines if, given a product id, the user can edit bugs in this product
at all.

=item C<can_see_bug(bug_id)>

Determines if the user can see the specified bug.

=item C<can_see_product(product_name)>

Returns 1 if the user can access the specified product, and 0 if the user
should not be aware of the existence of the product.

=item C<derive_regexp_groups>

Bugzilla allows for group inheritance. When data about the user (or any of the
groups) changes, the database must be updated. Handling updated groups is taken
care of by the constructor. However, when updating the email address, the
user may be placed into different groups, based on a new email regexp. This
method should be called in such a case to force reresolution of these groups.

=item C<get_selectable_products>

 Description: Returns all products the user is allowed to access. This list
              is restricted to some given classification if $classification_id
              is given.

 Params:      $classification_id - (optional) The ID of the classification
                                   the products belong to.

 Returns:     An array of product objects, sorted by the product name.

=item C<get_selectable_classifications>

 Description: Returns all classifications containing at least one product
              the user is allowed to view.

 Params:      none

 Returns:     An array of Bugzilla::Classification objects, sorted by
              the classification name.

=item C<can_enter_product($product_name, $warn)>

 Description: Returns 1 if the user can enter bugs into the specified product.
              If the user cannot enter bugs into the product, the behavior of
              this method depends on the value of $warn:
              - if $warn is false (or not given), a 'false' value is returned;
              - if $warn is true, an error is thrown.

 Params:      $product_name - a product name.
              $warn         - optional parameter, indicating whether an error
                              must be thrown if the user cannot enter bugs
                              into the specified product.

 Returns:     1 if the user can enter bugs into the product,
              0 if the user cannot enter bugs into the product and if $warn
              is false (an error is thrown if $warn is true).

=item C<get_enterable_products>

 Description: Returns an array of product objects into which the user is
              allowed to enter bugs.

 Params:      none

 Returns:     an array of product objects.

=item C<get_userlist>

Returns a reference to an array of users.  The array is populated with hashrefs
containing the login, identity and visibility.  Users that are not visible to this
user will have 'visible' set to zero.

=item C<flatten_group_membership>

Accepts a list of groups and returns a list of all the groups whose members 
inherit membership in any group on the list.  So, we can determine if a user
is in any of the groups input to flatten_group_membership by querying the
user_group_map for any user with DIRECT or REGEXP membership IN() the list
of groups returned.

=item C<visible_groups_inherited>

Returns a list of all groups whose members should be visible to this user.
Since this list is flattened already, there is no need for all users to
be have derived groups up-to-date to select the users meeting this criteria.

=item C<visible_groups_direct>

Returns a list of groups that the user is aware of.

=item C<visible_groups_as_string>

Returns the result of C<visible_groups_direct> as a string (a comma-separated
list).

=item C<product_responsibilities>

Retrieve user's product responsibilities as a list of hashes.
One hash per Bugzilla component the user has a responsibility for.
These are the hash keys:

=over

=item productname

Name of the product.

=item componentname

Name of the component.

=item initialowner

User ID of default assignee.

=item initialqacontact

User ID of default QA contact.

=back

=item C<can_bless>

When called with no arguments:
Returns C<1> if the user can bless at least one group, returns C<0> otherwise.

When called with one argument:
Returns C<1> if the user can bless the group with that id, returns
C<0> otherwise.

=item C<wants_bug_mail>

Returns true if the user wants mail for a given bug change.

=item C<wants_mail>

Returns true if the user wants mail for a given set of events. This method is
more general than C<wants_bug_mail>, allowing you to check e.g. permissions
for flag mail.

=item C<is_mover>

Returns true if the user is in the list of users allowed to move bugs
to another database. Note that this method doesn't check whether bug
moving is enabled.

=back

=head1 CLASS FUNCTIONS

These are functions that are not called on a User object, but instead are
called "statically," just like a normal procedural function.

=over 4

=item C<insert_new_user>

Creates a new user in the database.

Params: $username (scalar, string) - The login name for the new user.
        $realname (scalar, string) - The full name for the new user.
        $password (scalar, string) - Optional. The password for the new user;
                                     if not given, a random password will be
                                     generated.
        $disabledtext (scalar, string) - Optional. The disable text for the new
                                         user; if not given, it will be empty.
                                         If given, the user will be disabled,
                                         meaning the account will be
                                         unavailable for login.

Returns: The password for this user, in plain text, so it can be included
         in an e-mail sent to the user.

=item C<is_available_username>

Returns a boolean indicating whether or not the supplied username is
already taken in Bugzilla.

Params: $username (scalar, string) - The full login name of the username 
            that you are checking.
        $old_username (scalar, string) - If you are checking an email-change
            token, insert the "old" username that the user is changing from,
            here. Then, as long as it's the right user for that token, he 
            can change his username to $username. (That is, this function
            will return a boolean true value).

=item C<login_to_id($login, $throw_error)>

Takes a login name of a Bugzilla user and changes that into a numeric
ID for that user. This ID can then be passed to Bugzilla::User::new to
create a new user.

If no valid user exists with that login name, then the function returns 0.
However, if $throw_error is set, the function will throw a user error
instead of returning.

This function can also be used when you want to just find out the userid
of a user, but you don't want the full weight of Bugzilla::User.

However, consider using a Bugzilla::User object instead of this function
if you need more information about the user than just their ID.

=item C<user_id_to_login($user_id)>

Returns the login name of the user account for the given user ID. If no
valid user ID is given or the user has no entry in the profiles table,
we return an empty string.

=item C<validate_password($passwd1, $passwd2)>

Returns true if a password is valid (i.e. meets Bugzilla's
requirements for length and content), else returns false.

If a second password is passed in, this function also verifies that
the two passwords match.

=item C<UserInGroup($groupname)>

Takes a name of a group, and returns 1 if a user is in the group, 0 otherwise.

=back

=head1 SEE ALSO

L<Bugzilla|Bugzilla>
