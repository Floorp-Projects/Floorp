#!/usr/bonsaitools/bin/perl -w
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

# Use the template toolkit (http://www.template-toolkit.org/) to generate
# the user interface (HTML pages and mail messages) using templates in the
# "templates/" subdirectory.
use Template;

# This is the global template object that gets used one or more times by
# the script when it needs to process a template and return the results.
# Configuration parameters can be specified here that apply to all templates
# processed in this file.
my $template = Template->new(
  {
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => 'template/default' ,
    # Allow templates to be specified with relative paths.
    RELATIVE => 1 
  }
);

# This module requires that its caller have said "require CGI.pl" to import
# relevant functions from that script and its companion globals.pl.

############################################################################
# Functions
############################################################################

sub list
{
  # Displays a table of attachments for a given bug along with links for
  # viewing, editing, or making requests for each attachment.

  my ($bugid) = @_;


  # Retrieve a list of attachments for this bug and write them into an array
  # of hashes in which each hash represents a single attachment.
  &::SendSQL("
              SELECT attach_id, creation_ts, mimetype, description, ispatch, isobsolete 
              FROM attachments WHERE bug_id = $bugid ORDER BY attach_id
            ");
  my @attachments = ();
  while (&::MoreSQLData()) {
    my %a;
    ($a{'attachid'}, $a{'date'}, $a{'contenttype'}, $a{'description'}, $a{'ispatch'}, $a{'isobsolete'}) = &::FetchSQLData();

    # Format the attachment's creation/modification date into something readable.
    if ($a{'date'} =~ /^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
        $a{'date'} = "$3/$4/$2&nbsp;$5:$6";
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

    push @attachments, \%a;
  }

  my $vars = 
    {
      'bugid' => $bugid , 
      'attachments' => \@attachments , 
      'Param' => \&::Param , # for retrieving global parameters
      'PerformSubsts' => \&::PerformSubsts # for processing global parameters
    };

  $template->process("attachment/list.atml", $vars)
    || &::DisplayError("Template process failed: " . $template->error())
    && exit;

}
