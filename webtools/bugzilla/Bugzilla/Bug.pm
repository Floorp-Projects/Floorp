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

package Bugzilla::Bug;

use strict;

use Bugzilla::RelationSet;
use vars qw($unconfirmedstate $legal_keywords @legal_platform
            @legal_priority @legal_severity @legal_opsys @legal_bugs_status
            @settable_resolution %components %versions %target_milestone
            @enterable_products %milestoneurl %prodmaxvotes);

use CGI::Carp qw(fatalsToBrowser);

use Bugzilla::Attachment;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Error;

sub fields {
    # Keep this ordering in sync with bugzilla.dtd
    my @fields = qw(bug_id alias creation_ts short_desc delta_ts
                    reporter_accessible cclist_accessible
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
        push @fields, qw(estimated_time remaining_time actual_time);
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

  $bug_id = trim($bug_id);

  my $old_bug_id = $bug_id;

  # If the bug ID isn't numeric, it might be an alias, so try to convert it.
  $bug_id = &::BugAliasToID($bug_id) if $bug_id !~ /^0*[1-9][0-9]*$/;

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
        $user_id = &::DBname_to_id($user_id); 
     }
  }

  $self->{'whoid'} = $user_id;

  my $query = "
    SELECT
      bugs.bug_id, alias, bugs.product_id, products.name, version,
      rep_platform, op_sys, bug_status, resolution, priority,
      bug_severity, bugs.component_id, components.name, assigned_to,
      reporter, bug_file_loc, short_desc, target_milestone,
      qa_contact, status_whiteboard,
      DATE_FORMAT(creation_ts,'%Y.%m.%d %H:%i'),
      delta_ts, COALESCE(SUM(votes.vote_count), 0),
      reporter_accessible, cclist_accessible,
      estimated_time, remaining_time
    from bugs left join votes using(bug_id),
      products, components
    where bugs.bug_id = $bug_id
      AND products.id = bugs.product_id
      AND components.id = bugs.component_id
    group by bugs.bug_id";

  &::SendSQL($query);
  my @row = ();

  if ((@row = &::FetchSQLData()) && &::CanSeeBug($bug_id, $self->{'whoid'})) {
    my $count = 0;
    my %fields;
    foreach my $field ("bug_id", "alias", "product_id", "product", "version", 
                       "rep_platform", "op_sys", "bug_status", "resolution", 
                       "priority", "bug_severity", "component_id", "component",
                       "assigned_to", "reporter", "bug_file_loc", "short_desc",
                       "target_milestone", "qa_contact", "status_whiteboard", 
                       "creation_ts", "delta_ts", "votes",
                       "reporter_accessible", "cclist_accessible",
                       "estimated_time", "remaining_time")
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

  $self->{'assigned_to'} = new Bugzilla::User($self->{'assigned_to'});
  $self->{'reporter'} = new Bugzilla::User($self->{'reporter'});

  if (Param('useqacontact') && $self->{'qa_contact'} > 0) {
      $self->{'qa_contact'} = new Bugzilla::User($self->{'qa_contact'});
  } else {
      $self->{'qa_contact'} = undef;
  }

  my $ccSet = new Bugzilla::RelationSet;
  $ccSet->mergeFromDB("select who from cc where bug_id=$bug_id");
  my @cc = $ccSet->toArrayOfStrings();
  if (@cc) {
    $self->{'cc'} = \@cc;
  }

  if (@::legal_keywords) {
    &::SendSQL("SELECT keyworddefs.name 
              FROM keyworddefs, keywords
             WHERE keywords.bug_id = $bug_id 
               AND keyworddefs.id = keywords.keywordid
          ORDER BY keyworddefs.name");
    my @list;
    while (&::MoreSQLData()) {
        push(@list, &::FetchOneColumn());
    }
    if (@list) {
      $self->{'keywords'} = join(', ', @list);
    }
  }

  $self->{'attachments'} = Bugzilla::Attachment::query($self->{bug_id});

  # The types of flags that can be set on this bug.
  # If none, no UI for setting flags will be displayed.
  my $flag_types = 
    Bugzilla::FlagType::match({ 'target_type'  => 'bug', 
                                'product_id'   => $self->{'product_id'}, 
                                'component_id' => $self->{'component_id'} });
  foreach my $flag_type (@$flag_types) {
      $flag_type->{'flags'} = 
        Bugzilla::Flag::match({ 'bug_id'      => $self->{bug_id},
                                'type_id'     => $flag_type->{'id'},
                                'target_type' => 'bug',
                                'is_active'   => 1 });
  }
  $self->{'flag_types'} = $flag_types;
  $self->{'any_flags_requesteeble'} = grep($_->{'is_requesteeble'}, @$flag_types);

  # The number of types of flags that can be set on attachments to this bug
  # and the number of flags on those attachments.  One of these counts must be
  # greater than zero in order for the "flags" column to appear in the table
  # of attachments.
  my $num_attachment_flag_types =
    Bugzilla::FlagType::count({ 'target_type'  => 'attachment',
                                'product_id'   => $self->{'product_id'},
                                'component_id' => $self->{'component_id'} });
  my $num_attachment_flags =
    Bugzilla::Flag::count({ 'target_type'  => 'attachment',
                            'bug_id'       => $self->{bug_id},
                            'is_active'    => 1 });

  $self->{'show_attachment_flags'}
    = $num_attachment_flag_types || $num_attachment_flags;

  $self->{'milestoneurl'} = $::milestoneurl{$self->{product}};

  $self->{'isunconfirmed'} = ($self->{bug_status} eq $::unconfirmedstate);
  $self->{'isopened'} = &::IsOpenedState($self->{bug_status});
  
  my @depends = EmitDependList("blocked", "dependson", $bug_id);
  if (@depends) {
      $self->{'dependson'} = \@depends;
  }
  my @blocked = EmitDependList("dependson", "blocked", $bug_id);
  if (@blocked) {
    $self->{'blocked'} = \@blocked;
  }

  return $self;
}

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

    if (&::UserInGroup(Param("timetrackinggroup"))) {
        &::SendSQL("SELECT SUM(work_time)
               FROM longdescs WHERE longdescs.bug_id=$self->{bug_id}");
        $self->{'actual_time'} = &::FetchSQLData();
    }

    return $self->{'actual_time'};
}

sub longdescs {
    my ($self) = @_;

    return $self->{'longdescs'} if exists $self->{'longdescs'};

    $self->{'longdescs'} = &::GetComments($self->{bug_id});

    return $self->{'longdescs'};
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

    my @groups;

    # Some of this stuff needs to go into Bugzilla::User

    # For every group, we need to know if there is ANY bug_group_map
    # record putting the current bug in that group and if there is ANY
    # user_group_map record putting the user in that group.
    # The LEFT JOINs are checking for record existence.
    #
    &::SendSQL("SELECT DISTINCT groups.id, name, description," .
             " bug_group_map.group_id IS NOT NULL," .
             " user_group_map.group_id IS NOT NULL," .
             " isactive, membercontrol, othercontrol" .
             " FROM groups" . 
             " LEFT JOIN bug_group_map" .
             " ON bug_group_map.group_id = groups.id" .
             " AND bug_id = $self->{'bug_id'}" .
             " LEFT JOIN user_group_map" .
             " ON user_group_map.group_id = groups.id" .
             " AND user_id = $::userid" .
             " AND isbless = 0" .
             " LEFT JOIN group_control_map" .
             " ON group_control_map.group_id = groups.id" .
             " AND group_control_map.product_id = " . $self->{'product_id'} .
             " WHERE isbuggroup = 1");

    while (&::MoreSQLData()) {
        my ($groupid, $name, $description, $ison, $ingroup, $isactive,
            $membercontrol, $othercontrol) = &::FetchSQLData();

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

    use Bugzilla;

    my @movers = map { trim $_ } split(",", Param("movers"));
    my $canmove = Param("move-enabled") && Bugzilla->user->id && 
                  (lsearch(\@movers, Bugzilla->user->login) != -1);

    # In the below, if the person hasn't logged in, then we treat them
    # as if they can do anything.  That's because we don't know why they
    # haven't logged in; it may just be because they don't use cookies.
    # Display everything as if they have all the permissions in the
    # world; their permissions will get checked when they log in and
    # actually try to make the change.
    my $privileged = (!Bugzilla->user->id)
                     || Bugzilla->user->in_group("editbugs")
                     || Bugzilla->user->id == $self->{'assigned_to'}{'id'}
                     || (Param('useqacontact') && $self->{'qa_contact'} &&
                         Bugzilla->user->id == $self->{'qa_contact'}{'id'});
    my $isreporter = Bugzilla->user->id && 
                     Bugzilla->user->id == $self->{'reporter'}{'id'};

    my $canedit = $privileged || $isreporter;
    my $canconfirm = $privileged || Bugzilla->user->in_group("canconfirm");

    $self->{'user'} = {canmove    => $canmove, 
                       canconfirm => $canconfirm, 
                       canedit    => $canedit,};
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

sub EmitDependList {
  my ($myfield, $targetfield, $bug_id) = (@_);
  my @list;
  &::SendSQL("select dependencies.$targetfield, bugs.bug_status
           from dependencies, bugs
           where dependencies.$myfield = $bug_id
             and bugs.bug_id = dependencies.$targetfield
           order by dependencies.$targetfield");
  while (&::MoreSQLData()) {
    my ($i, $stat) = (&::FetchSQLData());
    push @list, $i;
  }
  return @list;
}

sub ValidateTime{
  my ($time, $field) = @_;
    if ($time > 99999.99 || $time < 0 || !($time =~ /^(?:\d+(?:\.\d*)?|\.\d+)$/)){
      ThrowUserError("need_positive_number", {field => "$field"}, "abort");
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
