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
# Contributor(s): Dawn Endico    <endico@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>
#                 Chris Yeh      <cyeh@bluemartini.com>
#                 Bradley Baetz  <bbaetz@acm.org>
#                 Dave Miller    <justdave@bugzilla.org>
#                 Max Kanat-Alexander <mkanat@kerio.com>

package Bugzilla::Bug;

use strict;

use vars qw($legal_keywords @legal_platform
            @legal_priority @legal_severity @legal_opsys @legal_bugs_status
            @settable_resolution %components %versions %target_milestone
            @enterable_products %milestoneurl %prodmaxvotes);

use CGI::Carp qw(fatalsToBrowser);

use Bugzilla;
use Bugzilla::Attachment;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Error;

use base qw(Exporter);
@Bugzilla::Bug::EXPORT = qw(
    bug_alias_to_id
    ValidateComment
);

use constant MAX_COMMENT_LENGTH => 65535;

sub fields {
    # Keep this ordering in sync with bugzilla.dtd
    my @fields = qw(bug_id alias creation_ts short_desc delta_ts
                    reporter_accessible cclist_accessible
                    classification_id classification
                    product component version rep_platform op_sys
                    bug_status resolution
                    bug_file_loc status_whiteboard keywords
                    priority bug_severity target_milestone
                    dependson blocked votes
                    reporter assigned_to cc
                   );

    if (Param('useqacontact')) {
        push @fields, "qa_contact";
    }

    if (Param('timetrackinggroup')) {
        push @fields, qw(estimated_time remaining_time actual_time deadline);
    }

    return @fields;
}

my %ok_field;
foreach my $key (qw(error groups
                    longdescs milestoneurl attachments
                    isopened isunconfirmed
                    flag_types num_attachment_flag_types
                    show_attachment_flags use_keywords any_flags_requesteeble
                   ),
                 fields()) {
    $ok_field{$key}++;
}

# create a new empty bug
#
sub new {
  my $type = shift();
  my %bug;

  # create a ref to an empty hash and bless it
  #
  my $self = {%bug};
  bless $self, $type;

  # construct from a hash containing a bug's info
  #
  if ($#_ == 1) {
    $self->initBug(@_);
  } else {
    confess("invalid number of arguments \($#_\)($_)");
  }

  # bless as a Bug
  #
  return $self;
}

# dump info about bug into hash unless user doesn't have permission
# user_id 0 is used when person is not logged in.
#
sub initBug  {
  my $self = shift();
  my ($bug_id, $user_id) = (@_);
  my $dbh = Bugzilla->dbh;

  $bug_id = trim($bug_id);

  my $old_bug_id = $bug_id;

  # If the bug ID isn't numeric, it might be an alias, so try to convert it.
  $bug_id = bug_alias_to_id($bug_id) if $bug_id !~ /^0*[1-9][0-9]*$/;

  if ((! defined $bug_id) || (!$bug_id) || (!detaint_natural($bug_id))) {
      # no bug number given or the alias didn't match a bug
      $self->{'bug_id'} = $old_bug_id;
      $self->{'error'} = "InvalidBugId";
      return $self;
  }

# default userid 0, or get DBID if you used an email address
  unless (defined $user_id) {
    $user_id = 0;
  }
  else {
     if ($user_id =~ /^\@/) {
        $user_id = login_to_id($user_id); 
     }
  }

  $self->{'who'} = new Bugzilla::User($user_id);

  my $query = "
    SELECT
      bugs.bug_id, alias, products.classification_id, classifications.name,
      bugs.product_id, products.name, version,
      rep_platform, op_sys, bug_status, resolution, priority,
      bug_severity, bugs.component_id, components.name, 
      assigned_to AS assigned_to_id, reporter AS reporter_id,
      bug_file_loc, short_desc, target_milestone,
      qa_contact AS qa_contact_id, status_whiteboard, " .
      $dbh->sql_date_format('creation_ts', '%Y.%m.%d %H:%i') . ",
      delta_ts, COALESCE(SUM(votes.vote_count), 0),
      reporter_accessible, cclist_accessible,
      estimated_time, remaining_time, " .
      $dbh->sql_date_format('deadline', '%Y-%m-%d') . "
    FROM bugs LEFT JOIN votes using(bug_id),
      classifications, products, components
    WHERE bugs.bug_id = ?
      AND classifications.id = products.classification_id
      AND products.id = bugs.product_id
      AND components.id = bugs.component_id
    GROUP BY bugs.bug_id";

  my $bug_sth = $dbh->prepare($query);
  $bug_sth->execute($bug_id);
  my @row;

  if ((@row = $bug_sth->fetchrow_array()) 
      && $self->{'who'}->can_see_bug($bug_id)) {
    my $count = 0;
    my %fields;
    foreach my $field ("bug_id", "alias", "classification_id", "classification",
                       "product_id", "product", "version", 
                       "rep_platform", "op_sys", "bug_status", "resolution", 
                       "priority", "bug_severity", "component_id", "component",
                       "assigned_to_id", "reporter_id", 
                       "bug_file_loc", "short_desc",
                       "target_milestone", "qa_contact_id", "status_whiteboard",
                       "creation_ts", "delta_ts", "votes",
                       "reporter_accessible", "cclist_accessible",
                       "estimated_time", "remaining_time", "deadline")
      {
        $fields{$field} = shift @row;
        if (defined $fields{$field}) {
            $self->{$field} = $fields{$field};
        }
        $count++;
    }
  } elsif (@row) {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotPermitted";
      return $self;
  } else {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotFound";
      return $self;
  }

  $self->{'isunconfirmed'} = ($self->{bug_status} eq 'UNCONFIRMED');
  $self->{'isopened'} = &::IsOpenedState($self->{bug_status});
  
  return $self;
}

#####################################################################
# Accessors
#####################################################################

# These subs are in alphabetical order, as much as possible.
# If you add a new sub, please try to keep it in alphabetical order
# with the other ones.

sub dup_id {
    my ($self) = @_;

    return $self->{'dup_id'} if exists $self->{'dup_id'};

    $self->{'dup_id'} = undef;
    if ($self->{'resolution'} eq 'DUPLICATE') { 
        my $dbh = Bugzilla->dbh;
        $self->{'dup_id'} =
          $dbh->selectrow_array(q{SELECT dupe_of 
                                  FROM duplicates
                                  WHERE dupe = ?},
                                undef,
                                $self->{'bug_id'});
    }
    return $self->{'dup_id'};
}

sub actual_time {
    my ($self) = @_;

    return $self->{'actual_time'} if exists $self->{'actual_time'};

    return undef unless Bugzilla->user->in_group(Param("timetrackinggroup"));

    my $sth = Bugzilla->dbh->prepare("SELECT SUM(work_time)
                                      FROM longdescs 
                                      WHERE longdescs.bug_id=?");
    $sth->execute($self->{bug_id});
    $self->{'actual_time'} = $sth->fetchrow_array();
    return $self->{'actual_time'};
}

sub any_flags_requesteeble () {
    my ($self) = @_;
    return $self->{'any_flags_requesteeble'} 
        if exists $self->{'any_flags_requesteeble'};

    $self->{'any_flags_requesteeble'} = 
        grep($_->{'is_requesteeble'}, @{$self->flag_types});

    return $self->{'any_flags_requesteeble'};
}

sub attachments () {
    my ($self) = @_;
    return $self->{'attachments'} if exists $self->{'attachments'};
    $self->{'attachments'} = Bugzilla::Attachment::query($self->{bug_id});
    return $self->{'attachments'};
}

sub assigned_to () {
    my ($self) = @_;
    return $self->{'assigned_to'} if exists $self->{'assigned_to'};
    $self->{'assigned_to'} = new Bugzilla::User($self->{'assigned_to_id'});
    return $self->{'assigned_to'};
}

sub blocked () {
    my ($self) = @_;
    return $self->{'blocked'} if exists $self->{'blocked'};
    $self->{'blocked'} = EmitDependList("dependson", "blocked", $self->bug_id);
    return $self->{'blocked'};
}

sub bug_id { $_[0]->{'bug_id'}; }

sub cc () {
    my ($self) = @_;
    return $self->{'cc'} if exists $self->{'cc'};

    my $dbh = Bugzilla->dbh;
    $self->{'cc'} = $dbh->selectcol_arrayref(
        q{SELECT profiles.login_name FROM cc, profiles
           WHERE bug_id = ?
             AND cc.who = profiles.userid
        ORDER BY profiles.login_name},
      undef, $self->bug_id);

    $self->{'cc'} = undef if !scalar(@{$self->{'cc'}});

    return $self->{'cc'};
}

sub dependson () {
    my ($self) = @_;
    return $self->{'dependson'} if exists $self->{'dependson'};
    $self->{'dependson'} = 
        EmitDependList("blocked", "dependson", $self->bug_id);
    return $self->{'dependson'};
}

sub flag_types () {
    my ($self) = @_;
    return $self->{'flag_types'} if exists $self->{'flag_types'};

    # The types of flags that can be set on this bug.
    # If none, no UI for setting flags will be displayed.
    my $flag_types = Bugzilla::FlagType::match(
        {'target_type'  => 'bug',
         'product_id'   => $self->{'product_id'}, 
         'component_id' => $self->{'component_id'} });

    foreach my $flag_type (@$flag_types) {
        $flag_type->{'flags'} = Bugzilla::Flag::match(
            { 'bug_id'      => $self->bug_id,
              'type_id'     => $flag_type->{'id'},
              'target_type' => 'bug',
              'is_active'   => 1 });
    }

    $self->{'flag_types'} = $flag_types;

    return $self->{'flag_types'};
}

sub keywords () {
    my ($self) = @_;
    return $self->{'keywords'} if exists $self->{'keywords'};

    my $dbh = Bugzilla->dbh;
    my $list_ref = $dbh->selectcol_arrayref(
         "SELECT keyworddefs.name
            FROM keyworddefs, keywords
           WHERE keywords.bug_id = ?
             AND keyworddefs.id = keywords.keywordid
        ORDER BY keyworddefs.name",
        undef, ($self->bug_id));

    $self->{'keywords'} = join(', ', @$list_ref);
    return $self->{'keywords'};
}

sub longdescs {
    my ($self) = @_;

    return $self->{'longdescs'} if exists $self->{'longdescs'};

    $self->{'longdescs'} = GetComments($self->{bug_id});

    return $self->{'longdescs'};
}

sub milestoneurl () {
    my ($self) = @_;
    return $self->{'milestoneurl'} if exists $self->{'milestoneurl'};
    $self->{'milestoneurl'} = $::milestoneurl{$self->{product}};
    return $self->{'milestoneurl'};
}

sub qa_contact () {
    my ($self) = @_;
    return $self->{'qa_contact'} if exists $self->{'qa_contact'};

    if (Param('useqacontact') && $self->{'qa_contact_id'} > 0) {
        $self->{'qa_contact'} = new Bugzilla::User($self->{'qa_contact'});
    } else {
        # XXX - This is somewhat inconsistent with the assignee/reporter 
        # methods, which will return an empty User if they get a 0. 
        # However, we're keeping it this way now, for backwards-compatibility.
        $self->{'qa_contact'} = undef;
    }
    return $self->{'qa_contact'};
}

sub reporter () {
    my ($self) = @_;
    return $self->{'reporter'} if exists $self->{'reporter'};
    $self->{'reporter'} = new Bugzilla::User($self->{'reporter_id'});
    return $self->{'reporter'};
}


sub show_attachment_flags () {
    my ($self) = @_;
    return $self->{'show_attachment_flags'} 
        if exists $self->{'show_attachment_flags'};

    # The number of types of flags that can be set on attachments to this bug
    # and the number of flags on those attachments.  One of these counts must be
    # greater than zero in order for the "flags" column to appear in the table
    # of attachments.
    my $num_attachment_flag_types = Bugzilla::FlagType::count(
        { 'target_type'  => 'attachment',
          'product_id'   => $self->{'product_id'},
          'component_id' => $self->{'component_id'} });
    my $num_attachment_flags = Bugzilla::Flag::count(
        { 'target_type'  => 'attachment',
          'bug_id'       => $self->bug_id,
          'is_active'    => 1 });

    $self->{'show_attachment_flags'} =
        ($num_attachment_flag_types || $num_attachment_flags);

    return $self->{'show_attachment_flags'};
}


sub use_keywords {
    return @::legal_keywords;
}

sub use_votes {
    my ($self) = @_;

    return Param('usevotes')
      && $::prodmaxvotes{$self->{product}} > 0;
}

sub groups {
    my $self = shift;

    return $self->{'groups'} if exists $self->{'groups'};

    my $dbh = Bugzilla->dbh;
    my @groups;

    # Some of this stuff needs to go into Bugzilla::User

    # For every group, we need to know if there is ANY bug_group_map
    # record putting the current bug in that group and if there is ANY
    # user_group_map record putting the user in that group.
    # The LEFT JOINs are checking for record existence.
    #
    my $sth = $dbh->prepare(
             "SELECT DISTINCT groups.id, name, description," .
             " bug_group_map.group_id IS NOT NULL," .
             " user_group_map.group_id IS NOT NULL," .
             " isactive, membercontrol, othercontrol" .
             " FROM groups" . 
             " LEFT JOIN bug_group_map" .
             " ON bug_group_map.group_id = groups.id" .
             " AND bug_id = ?" .
             " LEFT JOIN user_group_map" .
             " ON user_group_map.group_id = groups.id" .
             " AND user_id = ?" .
             " AND isbless = 0" .
             " LEFT JOIN group_control_map" .
             " ON group_control_map.group_id = groups.id" .
             " AND group_control_map.product_id = ? " .
             " WHERE isbuggroup = 1");
    $sth->execute($self->{'bug_id'}, Bugzilla->user->id,
                  $self->{'product_id'});

    while (my ($groupid, $name, $description, $ison, $ingroup, $isactive,
            $membercontrol, $othercontrol) = $sth->fetchrow_array()) {

        $membercontrol ||= 0;

        # For product groups, we only want to use the group if either
        # (1) The bit is set and not required, or
        # (2) The group is Shown or Default for members and
        #     the user is a member of the group.
        if ($ison ||
            ($isactive && $ingroup
                       && (($membercontrol == CONTROLMAPDEFAULT)
                           || ($membercontrol == CONTROLMAPSHOWN))
            ))
        {
            my $ismandatory = $isactive
              && ($membercontrol == CONTROLMAPMANDATORY);

            push (@groups, { "bit" => $groupid,
                             "name" => $name,
                             "ison" => $ison,
                             "ingroup" => $ingroup,
                             "mandatory" => $ismandatory,
                             "description" => $description });
        }
    }

    $self->{'groups'} = \@groups;

    return $self->{'groups'};
}

sub user {
    my $self = shift;
    return $self->{'user'} if exists $self->{'user'};

    my @movers = map { trim $_ } split(",", Param("movers"));
    my $canmove = Param("move-enabled") && Bugzilla->user->id && 
                  (lsearch(\@movers, Bugzilla->user->login) != -1);

    # In the below, if the person hasn't logged in, then we treat them
    # as if they can do anything.  That's because we don't know why they
    # haven't logged in; it may just be because they don't use cookies.
    # Display everything as if they have all the permissions in the
    # world; their permissions will get checked when they log in and
    # actually try to make the change.
    my $unknown_privileges = !Bugzilla->user->id
                             || Bugzilla->user->in_group("editbugs");
    my $canedit = $unknown_privileges
                  || Bugzilla->user->id == $self->{assigned_to_id}
                  || (Param('useqacontact')
                      && $self->qa_contact
                      && Bugzilla->user->id == $self->{qa_contact_id});
    my $canconfirm = $unknown_privileges
                     || Bugzilla->user->in_group("canconfirm");
    my $isreporter = Bugzilla->user->id
                     && Bugzilla->user->id == $self->{reporter_id};

    $self->{'user'} = {canmove    => $canmove,
                       canconfirm => $canconfirm,
                       canedit    => $canedit,
                       isreporter => $isreporter};
    return $self->{'user'};
}

sub choices {
    my $self = shift;
    return $self->{'choices'} if exists $self->{'choices'};

    &::GetVersionTable();

    $self->{'choices'} = {};

    # Fiddle the product list.
    my $seen_curr_prod;
    my @prodlist;

    foreach my $product (@::enterable_products) {
        if ($product eq $self->{'product'}) {
            # if it's the product the bug is already in, it's ALWAYS in
            # the popup, period, whether the user can see it or not, and
            # regardless of the disallownew setting.
            $seen_curr_prod = 1;
            push(@prodlist, $product);
            next;
        }

        if (!&::CanEnterProduct($product)) {
            # If we're using bug groups to restrict entry on products, and
            # this product has an entry group, and the user is not in that
            # group, we don't want to include that product in this list.
            next;
        }

        push(@prodlist, $product);
    }

    # The current product is part of the popup, even if new bugs are no longer
    # allowed for that product
    if (!$seen_curr_prod) {
        push (@prodlist, $self->{'product'});
        @prodlist = sort @prodlist;
    }

    # Hack - this array contains "". See bug 106589.
    my @res = grep ($_, @::settable_resolution);

    $self->{'choices'} =
      {
       'product' => \@prodlist,
       'rep_platform' => \@::legal_platform,
       'priority' => \@::legal_priority,
       'bug_severity' => \@::legal_severity,
       'op_sys' => \@::legal_opsys,
       'bug_status' => \@::legal_bugs_status,
       'resolution' => \@res,
       'component' => $::components{$self->{product}},
       'version' => $::versions{$self->{product}},
       'target_milestone' => $::target_milestone{$self->{product}},
      };

    return $self->{'choices'};
}

# Convenience Function. If you need speed, use this. If you need
# other Bug fields in addition to this, just create a new Bug with
# the alias.
# Queries the database for the bug with a given alias, and returns
# the ID of the bug if it exists or the undefined value if it doesn't.
sub bug_alias_to_id ($) {
    my ($alias) = @_;
    return undef unless Param("usebugaliases");
    my $dbh = Bugzilla->dbh;
    trick_taint($alias);
    return $dbh->selectrow_array(
        "SELECT bug_id FROM bugs WHERE alias = ?", undef, $alias);
}

#####################################################################
# Subroutines
#####################################################################

sub EmitDependList {
    my ($myfield, $targetfield, $bug_id) = (@_);
    my $dbh = Bugzilla->dbh;
    my $list_ref =
        $dbh->selectcol_arrayref(
          "SELECT dependencies.$targetfield
             FROM dependencies, bugs
            WHERE dependencies.$myfield = ?
              AND bugs.bug_id = dependencies.$targetfield
         ORDER BY dependencies.$targetfield",
         undef, ($bug_id));
    return $list_ref;
}

sub ValidateTime {
    my ($time, $field) = @_;

    # regexp verifies one or more digits, optionally followed by a period and
    # zero or more digits, OR we have a period followed by one or more digits
    # (allow negatives, though, so people can back out errors in time reporting)
    if ($time !~ /^-?(?:\d+(?:\.\d*)?|\.\d+)$/) {
        ThrowUserError("number_not_numeric",
                       {field => "$field", num => "$time"});
    }

    # Only the "work_time" field is allowed to contain a negative value.
    if ( ($time < 0) && ($field ne "work_time") ) {
        ThrowUserError("number_too_small",
                       {field => "$field", num => "$time", min_num => "0"});
    }

    if ($time > 99999.99) {
        ThrowUserError("number_too_large",
                       {field => "$field", num => "$time", max_num => "99999.99"});
    }
}

sub GetComments {
    my ($id) = (@_);
    my $dbh = Bugzilla->dbh;
    my @comments;
    my $sth = $dbh->prepare(
            "SELECT  profiles.realname AS name, profiles.login_name AS email,
            " . $dbh->sql_date_format('longdescs.bug_when', '%Y.%m.%d %H:%i') . "
               AS time, longdescs.thetext AS body, longdescs.work_time,
                     isprivate, already_wrapped,
            " . $dbh->sql_date_format('longdescs.bug_when', '%Y%m%d%H%i%s') . "
               AS bug_when
             FROM    longdescs, profiles
            WHERE    profiles.userid = longdescs.who
              AND    longdescs.bug_id = ?
            ORDER BY longdescs.bug_when");
    $sth->execute($id);

    while (my $comment_ref = $sth->fetchrow_hashref()) {
        my %comment = %$comment_ref;

        # Can't use "when" as a field name in MySQL
        $comment{'when'} = $comment{'bug_when'};
        delete($comment{'bug_when'});

        $comment{'email'} .= Param('emailsuffix');
        $comment{'name'} = $comment{'name'} || $comment{'email'};

        push (@comments, \%comment);
    }

    return \@comments;
}

# CountOpenDependencies counts the number of open dependent bugs for a
# list of bugs and returns a list of bug_id's and their dependency count
# It takes one parameter:
#  - A list of bug numbers whose dependencies are to be checked
sub CountOpenDependencies {
    my (@bug_list) = @_;
    my @dependencies;
    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare(
          "SELECT blocked, count(bug_status) " .
            "FROM bugs, dependencies " .
           "WHERE blocked IN (" . (join "," , @bug_list) . ") " .
             "AND bug_id = dependson " .
             "AND bug_status IN ('" . (join "','", &::OpenStates())  . "') " .
        "GROUP BY blocked ");
    $sth->execute();

    while (my ($bug_id, $dependencies) = $sth->fetchrow_array()) {
        push(@dependencies, { bug_id       => $bug_id,
                              dependencies => $dependencies });
    }

    return @dependencies;
}

sub ValidateComment ($) {
    my ($comment) = @_;

    if (defined($comment) && length($comment) > MAX_COMMENT_LENGTH) {
        ThrowUserError("comment_too_long");
    }
}

sub AUTOLOAD {
  use vars qw($AUTOLOAD);
  my $attr = $AUTOLOAD;

  $attr =~ s/.*:://;
  return unless $attr=~ /[^A-Z]/;
  confess ("invalid bug attribute $attr") unless $ok_field{$attr};

  no strict 'refs';
  *$AUTOLOAD = sub {
      my $self = shift;
      if (defined $self->{$attr}) {
          return $self->{$attr};
      } else {
          return '';
      }
  };

  goto &$AUTOLOAD;
}

1;
