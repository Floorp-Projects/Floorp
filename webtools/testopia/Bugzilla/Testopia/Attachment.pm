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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Large portions lifted uncerimoniously from Bugzilla::Attachment.pm
# and bugzilla's attachment.cgi
# Which are copyrighted by their respective copyright holders:
#                 Terry Weissman <terry@mozilla.org>
#                 Myk Melez <myk@mozilla.org>
#                 Daniel Raichle <draichle@gmx.net>
#                 Dave Miller <justdave@syndicomm.com>
#                 Alexander J. Vincent <ajvincent@juno.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Greg Hendricks <ghendricks@novell.com>
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Attachment - Attachment object for Testopia

=head1 DESCRIPTION

This module provides support for attachments to Test Cases and Test
Plans in Testopia.

=head1 SYNOPSIS

 $attachment = Bugzilla::Testopia::Attachment->new($attachment_id);
 $attachment = Bugzilla::Testopia::Attachment->new(\%attachment_hash);

=cut

package Bugzilla::Testopia::Attachment;

use strict;

use Bugzilla::Util;
use Bugzilla::Config;

use base qw(Exporter);

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    attachment_id
    submitter_id
    description
    filename
    creation_ts
    mime_type

=cut

use constant DB_COLUMNS => qw(
    attachment_id
    submitter_id
    description
    filename
    creation_ts
    mime_type
);

our $columns = join(", ", DB_COLUMNS);


###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new Attachment object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for attachment class

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_attachments
            WHERE attachment_id = ?}, undef, $id);
    } elsif (ref $param eq 'HASH'){
         $obj = $param; 
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Testopia::Attachment::_init'});
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    return $self;
}

=head2 store

Serializes this attachment to the database

=cut

sub store {
    my ($self) = @_;
    if (!$self->{'case_id'} && !$self->{'plan_id'}){
        ThrowUserError("testopia-missing-attachment-key");
    }
    $self->_validate_data;
    $self->{'filename'} = $self->strip_path($self->{'filename'});
    my $dbh = Bugzilla->dbh;
    my ($timestamp) = Bugzilla::Testopia::Util::get_time_stamp();

         
    $dbh->do("INSERT INTO test_attachments ($columns)
              VALUES (?,?,?,?,?,?)",
              undef, (undef,  
              $self->{'submitter_id'}, $self->{'description'}, 
              $self->{'filename'}, $timestamp, $self->{'mime_type'}));
 
    my $key = $dbh->bz_last_key( 'test_attachments', 'attachment_id' );
    $dbh->do("INSERT INTO test_attachment_data (attachment_id, contents) VALUES(?,?)",
              undef, $key, $self->{'contents'});

    if ($self->{'case_id'}){

        $dbh->do("INSERT INTO test_case_attachments (attachment_id, case_id, case_run_id)
                  VALUES (?,?,?)",
                  undef, ($key, $self->{'case_id'}, $self->{'case_run_id'}));
    }
    elsif ($self->{'plan_id'}){
        $dbh->do("INSERT INTO test_plan_attachments (attachment_id, plan_id)
                  VALUES (?,?)",
                  undef, ($key, $self->{'plan_id'}));
    }

    return $key;    
}

=head2 _validate_data

Private method for validating attachment data. Checks that size 
limit is not exceeded and converts uncompressed BMP to PNG

=cut

sub _validate_data {
    my $self = shift;
    my $maxsize = Param('maxattachmentsize');
    $maxsize *= 1024; # Convert from K
    
    # Windows screenshots are usually uncompressed BMP files which
    # makes for a quick way to eat up disk space. Let's compress them. 
    # We do this before we check the size since the uncompressed version
    # could easily be greater than maxattachmentsize.
    if (Param('convert_uncompressed_images') 
          && $self->{'mime_type'} eq 'image/bmp'){
      require Image::Magick; 
      my $img = Image::Magick->new(magick=>'bmp');
      $img->BlobToImage($self->{'contents'});
      $img->set(magick=>'png');
      my $imgdata = $img->ImageToBlob();
      $self->{'contents'} = $imgdata;
      $self->{'contenttype'} = 'image/png';
    }
        
    # Make sure the attachment does not exceed the maximum permitted size
    my $len = $self->{'contents'} ? length($self->{'contents'}) : 0;
    if ($maxsize && $len > $maxsize) {
        my $vars = { filesize => sprintf("%.0f", $len/1024) };
        ThrowUserError("file_too_large", $vars);
    }
    trick_taint($self->{'contents'});
    
}

=head2 strip_path

Strips the path from a filename, everything up to the last / or \.
Note: this was copied directly from bugzilla.

=cut

sub strip_path {
    my $self = shift;
    my ($filename) = @_;

    # Remove path info (if any) from the file name.  The browser should do this
    # for us, but some are buggy.  This may not work on Mac file names and could
    # mess up file names with slashes in them, but them's the breaks.  We only
    # use this as a hint to users downloading attachments anyway, so it's not 
    # a big deal if it munges incorrectly occasionally.
    $filename =~ s/^.*[\/\\]//;

    # Truncate the filename to 100 characters, counting from the end of the string
    # to make sure we keep the filename extension.
    $filename = substr($filename, -100, 100);

    trick_taint($filename);
    return $filename;    
    
}

=head2 isViewable

Returns true if the content type (mime-type) is viewable in a browser
text/* and img for the most part are viewable, All others are not.

=cut

# Returns 1 if the parameter is a content-type viewable in this browser
# Note that we don't use $cgi->Accept()'s ability to check if a content-type
# matches, because this will return a value even if it's matched by the generic
# */* which most browsers add to the end of their Accept: headers.
sub isViewable
{
  my $self = shift;
  my $cgi = shift;
  my $contenttype = $self->mime_type;
    
  # We assume we can view all text and image types  
  if ($contenttype =~ /^(text|image)\//) {
    return 1;
  }
  
  # Mozilla can view XUL. Note the trailing slash on the Gecko detection to
  # avoid sending XUL to Safari.
  if (($contenttype =~ /^application\/vnd\.mozilla\./) &&
      ($cgi->user_agent() =~ /Gecko\//))
  {
    return 1;
  }

  # If it's not one of the above types, we check the Accept: header for any 
  # types mentioned explicitly.
  my $accept = join(",", $cgi->Accept());
  
  if ($accept =~ /^(.*,)?\Q$contenttype\E(,.*)?$/) {
    return 1;
  }
  
  return 0;
}

=head2 update

Updates an existing attachment object in the database.
Takes a reference to a hash, the keys of which must match
the fields of an attachment and the values representing the
new data.

=cut

sub update {
    my $self = shift;
    my ($newvalues) = @_;
    my $dbh = Bugzilla->dbh;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();

    $dbh->bz_lock_tables('test_attachments WRITE');
    foreach my $field (keys %{$newvalues}){
        if ($self->{$field} ne $newvalues->{$field}){
            trick_taint($newvalues->{$field});
            $dbh->do("UPDATE test_attachments 
                      SET $field = ? WHERE attachment_id = ?",
                      undef, $newvalues->{$field}, $self->{'attachment_id'});
            $self->{$field} = $newvalues->{$field};
        }
    }
    $dbh->bz_unlock_tables();
}

=head2 obliterate

Completely removes an attachment from the database. This is the only
safe way to do this.

=cut

sub obliterate {
    my $self = shift;
    return 0 unless $self->candelete;
    my $dbh = Bugzilla->dbh;
    
    $dbh->do("DELETE FROM test_attachment_data 
              WHERE attachment_id = ?", undef, $self->{'attachment_id'});
    $dbh->do("DELETE FROM test_case_attachments 
              WHERE attachment_id = ?", undef, $self->{'attachment_id'});
    $dbh->do("DELETE FROM test_plan_attachments 
              WHERE attachment_id = ?", undef, $self->{'attachment_id'});
    $dbh->do("DELETE FROM test_attachments 
              WHERE attachment_id = ?", undef, $self->{'attachment_id'});
    return 1;
}

sub link_plan {
    my $self = shift;
    my ($plan_id) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_plan_attachments WRITE');
    my ($is) = $dbh->selectrow_array(
            "SELECT 1 
               FROM test_plan_attachments
              WHERE attachment_id = ?
                AND plan_id = ?",
               undef, ($self->id, $plan_id));
    if ($is) {
        $dbh->bz_unlock_tables();
        return;
    }

    $dbh->do("INSERT INTO test_plan_attachments (attachment_id, plan_id)
              VALUES (?,?)",
              undef, ($self->id, $plan_id));
    $dbh->bz_unlock_tables(); 
}

sub link_case {
    my $self = shift;
    my ($case_id) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_case_attachments WRITE');
    my ($is) = $dbh->selectrow_array(
            "SELECT 1 
               FROM test_case_attachments
              WHERE attachment_id = ?
                AND case_id = ?",
               undef, ($self->id, $case_id));
    if ($is) {
        $dbh->bz_unlock_tables();
        return;
    }

    $dbh->do("INSERT INTO test_case_attachments (attachment_id, plan_id)
              VALUES (?,?)",
              undef, ($self->id, $case_id));
    $dbh->bz_unlock_tables();
}
    
sub unlink_plan {
    my $self = shift;
    my ($plan_id) = @_;
    my $dbh = Bugzilla->dbh;
    my ($refcount) = $dbh->selectrow_array(
        "SELECT COUNT(*) 
           FROM test_plan_attachments 
          WHERE attachment_id = ?", undef, $self->id);
    if ($refcount > 1){
        $dbh->do("DELETE FROM test_plan_attachments 
                  WHERE plan_id = ? AND attachment_id = ?",
                  undef, ($plan_id, $self->id));
    }
    else {
        $self->obliterate;
    }
}

sub unlink_case {
    my $self = shift;
    my ($case_id) = @_;
    my $dbh = Bugzilla->dbh;
    
    my ($refcount) = $dbh->selectrow_array(
        "SELECT COUNT(*) 
           FROM test_case_attachments 
          WHERE attachment_id = ?", undef, $self->id);
    if ($refcount > 1){
        $dbh->do("DELETE FROM test_case_attachments 
                  WHERE case_id = ? AND attachment_id = ?",
                  undef, ($case_id, $self->id));
    }
    else {
        $self->obliterate;
    }
}

=head2 canview

Returns true if the logged in user has rights to view this attachment

=cut

sub canview {
    my $self = shift;
# TODO: Check for private attachments
    return 1;
}

=head2 canedit

Returns true if the logged in user has rights to edit this attachment

=cut

sub canedit {
    my $self = shift;
    my $obj;
    if ($self->plan_id){
        $obj = Bugzilla::Testopia::TestPlan->new($self->plan_id);
    }
    else {
        $obj = Bugzilla::Testopia::TestCase->new($self->case_id);
    }

    return $obj->canedit && $self->canview; $obj->canedit;
}

=head2 candelete

Returns true if the logged in user has rights to delete this attachment

=cut

sub candelete {
    my $self = shift;
    return 1 if Bugzilla->user->in_group("admin");
    return 0 unless $self->canedit && Param("allow-test-deletion");
    return 1 if Bugzilla->user->id == $self->submitter->id;
    return 0;
}

###############################
####      Accessors        ####
###############################

sub id             { return $_[0]->{'attachment_id'};    }
sub plan_id        { return $_[0]->{'plan_id'};          }
sub case_id        { return $_[0]->{'case_id'};          }
sub submitter      { return Bugzilla::User->new($_[0]->{'submitter_id'});      }
sub description    { return $_[0]->{'description'};      }
sub filename       { return $_[0]->{'filename'};         }
sub creation_ts    { return $_[0]->{'creation_ts'};      }
sub mime_type      { return $_[0]->{'mime_type'};        }

=head2 contents

Returns the attachment data

=cut

sub contents {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'contents'} if exists $self->{'contents'};
    my ($contents) = $dbh->selectrow_array("SELECT contents 
                                           FROM test_attachment_data
                                           WHERE attachment_id = ?",
                                           undef, $self->{'attachment_id'});

    $self->{'contents'} = $contents;
    return $self->{'contents'};
}

=head2 datasize

Returns the size of the attachment data

=cut

sub datasize {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'datasize'} if exists $self->{'datasize'};
    my ($datasize) = $dbh->selectrow_array("SELECT LENGTH(contents) 
                                           FROM test_attachment_data
                                           WHERE attachment_id = ?",
                                           undef, $self->{'attachment_id'});
    $self->{'datasize'} = $datasize;
    return $self->{'datasize'};
}

=head1 SEE ALSO

Bugzilla::Attachment

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
