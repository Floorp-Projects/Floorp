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

############################################################################
# Module Initialization
############################################################################

use diagnostics;
use strict;

package Attachment;

# This module requires that its caller have said "require CGI.pl" to import
# relevant functions from that script and its companion globals.pl.

############################################################################
# Functions
############################################################################

sub query
{
  # Retrieves and returns an array of attachment records for a given bug. 
  # This data should be given to attachment/list.atml in an
  # "attachments" variable.
  my ($bugid) = @_;

  my $in_editbugs = &::UserInGroup("editbugs");

  # Retrieve a list of attachments for this bug and write them into an array
  # of hashes in which each hash represents a single attachment.
  &::SendSQL("
              SELECT attach_id, creation_ts, mimetype, description, ispatch, 
               isobsolete, isprivate, submitter_id
              FROM attachments WHERE bug_id = $bugid ORDER BY attach_id
            ");
  my @attachments = ();
  while (&::MoreSQLData()) {
    my %a;
    my $submitter_id;
    ($a{'attachid'}, $a{'date'}, $a{'contenttype'}, $a{'description'},
     $a{'ispatch'}, $a{'isobsolete'}, $a{'isprivate'}, $submitter_id) 
        = &::FetchSQLData();

    # Format the attachment's creation/modification date into a standard
    # format (YYYY-MM-DD HH:MM)
    if ($a{'date'} =~ /^(\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
        $a{'date'} = "$1-$2-$3 $4:$5";
    }

    # Retrieve a list of status flags that have been set on the attachment.
    &::PushGlobalSQLState();
    &::SendSQL(" 
                SELECT   name 
                FROM     attachstatuses, attachstatusdefs 
                WHERE    attach_id = $a{'attachid'} 
                AND      attachstatuses.statusid = attachstatusdefs.id
                ORDER BY sortkey
              ");
    my @statuses = ();
    while (&::MoreSQLData()) { 
      my ($status) = &::FetchSQLData(); 
      push @statuses , $status;
    }
    $a{'statuses'} = \@statuses;
    &::PopGlobalSQLState();

    # We will display the edit link if the user can edit the attachment;
    # ie the are the submitter, or they have canedit.
    # Also show the link if the user is not logged in - in that cae,
    # They'll be prompted later
    $a{'canedit'} = ($::userid == 0 || $submitter_id == $::userid ||
                     $in_editbugs);
    push @attachments, \%a;
  }
  
  return \@attachments;  
}

1;
