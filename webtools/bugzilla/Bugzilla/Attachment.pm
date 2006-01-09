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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Myk Melez <myk@mozilla.org>

use strict;

package Bugzilla::Attachment;

=head1 NAME

Bugzilla::Attachment - a file related to a bug that a user has uploaded
                       to the Bugzilla server

=head1 SYNOPSIS

  use Bugzilla::Attachment;

  # Get the attachment with the given ID.
  my $attachment = Bugzilla::Attachment->get($attach_id);

  # Get the attachments with the given IDs.
  my $attachments = Bugzilla::Attachment->get_list($attach_ids);

=head1 DESCRIPTION

This module defines attachment objects, which represent files related to bugs
that users upload to the Bugzilla server.

=cut

# This module requires that its caller have said "require globals.pl"
# to import relevant functions from that script.

use Bugzilla::Flag;
use Bugzilla::Config qw(:locations);
use Bugzilla::User;

sub get {
    my $invocant = shift;
    my $id = shift;

    my $attachments = _retrieve([$id]);
    my $self = $attachments->[0];
    bless($self, ref($invocant) || $invocant) if $self;

    return $self;
}

sub get_list {
    my $invocant = shift;
    my $ids = shift;

    my $attachments = _retrieve($ids);
    foreach my $attachment (@$attachments) {
        bless($attachment, ref($invocant) || $invocant);
    }

    return $attachments;
}

sub _retrieve {
    my ($ids) = @_;

    return [] if scalar(@$ids) == 0;

    my @columns = (
        'attachments.attach_id AS id',
        'attachments.bug_id AS bug_id',
        'attachments.description AS description',
        'attachments.mimetype AS contenttype',
        'attachments.submitter_id AS _attacher_id',
        Bugzilla->dbh->sql_date_format('attachments.creation_ts',
                                       '%Y.%m.%d %H:%i') . " AS attached",
        'attachments.filename AS filename',
        'attachments.ispatch AS ispatch',
        'attachments.isurl AS isurl',
        'attachments.isobsolete AS isobsolete',
        'attachments.isprivate AS isprivate'
    );
    my $columns = join(", ", @columns);

    my $records = Bugzilla->dbh->selectall_arrayref("SELECT $columns
                                                     FROM attachments
                                                     WHERE attach_id IN (" .
                                                     join(",", @$ids) . ")
                                                     ORDER BY attach_id",
                                                    { Slice => {} });
    return $records;
}

=pod

=head2 Instance Properties

=over

=item C<id>

the unique identifier for the attachment

=back

=cut

sub id {
    my $self = shift;
    return $self->{id};
}

=over

=item C<bug_id>

the ID of the bug to which the attachment is attached

=back

=cut

# XXX Once Bug.pm slims down sufficiently this should become a reference
# to a bug object.
sub bug_id {
    my $self = shift;
    return $self->{bug_id};
}

=over

=item C<description>

user-provided text describing the attachment

=back

=cut

sub description {
    my $self = shift;
    return $self->{description};
}

=over

=item C<contenttype>

the attachment's MIME media type

=back

=cut

sub contenttype {
    my $self = shift;
    return $self->{contenttype};
}

=over

=item C<attacher>

the user who attached the attachment

=back

=cut

sub attacher {
    my $self = shift;
    return $self->{attacher} if exists $self->{attacher};
    $self->{attacher} = new Bugzilla::User($self->{_attacher_id});
    return $self->{attacher};
}

=over

=item C<attached>

the date and time on which the attacher attached the attachment

=back

=cut

sub attached {
    my $self = shift;
    return $self->{attached};
}

=over

=item C<filename>

the name of the file the attacher attached

=back

=cut

sub filename {
    my $self = shift;
    return $self->{filename};
}

=over

=item C<ispatch>

whether or not the attachment is a patch

=back

=cut

sub ispatch {
    my $self = shift;
    return $self->{ispatch};
}

=over

=item C<isurl>

whether or not the attachment is a URL

=back

=cut

sub isurl {
    my $self = shift;
    return $self->{isurl};
}

=over

=item C<isobsolete>

whether or not the attachment is obsolete

=back

=cut

sub isobsolete {
    my $self = shift;
    return $self->{isobsolete};
}

=over

=item C<isprivate>

whether or not the attachment is private

=back

=cut

sub isprivate {
    my $self = shift;
    return $self->{isprivate};
}

=over

=item C<data>

the content of the attachment

=back

=cut

sub data {
    my $self = shift;
    return $self->{data} if exists $self->{data};

    # First try to get the attachment data from the database.
    ($self->{data}) = Bugzilla->dbh->selectrow_array("SELECT thedata
                                                      FROM attach_data
                                                      WHERE id = ?",
                                                     undef,
                                                     $self->{id});

    # If there's no attachment data in the database, the attachment is stored
    # in a local file, so retrieve it from there.
    if (length($self->{data}) == 0) {
        if (open(AH, $self->_get_local_filename())) {
            local $/;
            binmode AH;
            $self->{data} = <AH>;
            close(AH);
        }
    }
    
    return $self->{data};
}

=over

=item C<datasize>

the length (in characters) of the attachment content

=back

=cut

# datasize is a property of the data itself, and it's unclear whether we should
# expose it at all, since you can easily derive it from the data itself: in TT,
# attachment.data.size; in Perl, length($attachment->{data}).  But perhaps
# it makes sense for performance reasons, since accessing the data forces it
# to get retrieved from the database/filesystem and loaded into memory,
# while datasize avoids loading the attachment into memory, calling SQL's
# LENGTH() function or stat()ing the file instead.  I've left it in for now.

sub datasize {
    my $self = shift;
    return $self->{datasize} if exists $self->{datasize};

    # If we have already retrieved the data, return its size.
    return length($self->{data}) if exists $self->{data};

    ($self->{datasize}) =
        Bugzilla->dbh->selectrow_array("SELECT LENGTH(thedata)
                                        FROM attach_data
                                        WHERE id = ?",
                                       undef,
                                       $self->{id});

    # If there's no attachment data in the database, the attachment
    # is stored in a local file, so retrieve its size from the file.
    if ($self->{datasize} == 0) {
        if (open(AH, $self->_get_local_filename())) {
            binmode AH;
            $self->{datasize} = (stat(AH))[7];
            close(AH);
        }
    }

    return $self->{datasize};
}

=over

=item C<flags>

flags that have been set on the attachment

=back

=cut

sub flags {
    my $self = shift;
    return $self->{flags} if exists $self->{flags};

    $self->{flags} = Bugzilla::Flag::match({ attach_id => $self->id,
                                             is_active => 1 });
    return $self->{flags};
}

# Instance methods; no POD documentation here yet because the only one so far
# is private.

sub _get_local_filename {
    my $self = shift;
    my $hash = ($self->id % 100) + 100;
    $hash =~ s/.*(\d\d)$/group.$1/;
    return "$attachdir/$hash/attachment." . $self->id;
}

=pod

=head2 Class Methods

=over

=item C<get_attachments_by_bug($bug_id)>

Description: retrieves and returns the attachments for the given bug.

Params:     C<$bug_id> - integer - the ID of the bug for which
            to retrieve and return attachments.

Returns:    a reference to an array of attachment objects.

=back

=cut

sub get_attachments_by_bug {
    my ($class, $bug_id) = @_;
    my $attach_ids = Bugzilla->dbh->selectcol_arrayref("SELECT attach_id
                                                        FROM attachments
                                                        WHERE bug_id = ?",
                                                       undef, $bug_id);
    my $attachments = Bugzilla::Attachment->get_list($attach_ids);
    return $attachments;
}

1;
