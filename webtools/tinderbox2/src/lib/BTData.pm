# -*- Mode: perl; indent-tabs-mode: nil -*-

# BTData.pm - the configuration file which describes the local Bug
# Tracking system and its relationship to the tinderbox trees.


# $Revision: 1.2 $ 
# $Date: 2000/10/18 20:26:04 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Attic/BTData.pm,v $ 
# $Name:  $ 


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 




package BTData;

$VERSION = '#tinder_version#';




# Ticket variable names match this pattern

# All variables seem to start with a capital letter.

# Note: there are some standard bugzilla variables with unusual
# characters in them:
#
#       Bug#: 1999
#       OS/Version: 

# Some AIM variable names have spaces in them, we will conert these
# into '_' after the mail is parsed.

$VAR_PATTERN = '[A-Z][a-zA-Z0-9._/ \#\-]*'; 



# the name of the bug tracking field which shows bug_id

#$BUGID_FIELD_NAME = 'Ticket_#';
$BUGID_FIELD_NAME = 'Bug#';


# the name of the bug tracking field which shows progress/slippage.

$STATUS_FIELD_NAME = 'Status';


# The values of the status field wich denote that the ticket is moving
# forward.  Notice that this list may not be complete as we are only
# interested in displaying Developer progress. If the ticket is moving
# through QA tinderbox may not be the correct place to see that
# change.  In particular newly opend tickets are not particularly
# interesting when monitoring the development process.

# All status values are converted to lower case for ease of
# processing.  Each value of this table corresponds to a bug column in
# the tinderbox status page. You may have as many bug columns as you
# like.

%STATUS_PROGRESS = (
		    'ASSIGNED' => 'Progress',
		    'VERIFIED' => 'Progress',

		    'REOPENED' => 'Slipage',
		    'FAILED' => 'Slipage',
		    'OPENED' => 'Slipage',
		    'NEW' => 'Slipage',
		   );


# Uncomment only the fields you wish displayed in the popup window,
# The fields will be displayed in the order they are listed here.
# Only uncomment fields which are interesting. Fields which are empty
# will still be displayed.

@DISPLAY_FIELDS = (

		   # Tinderbox created fields
		   
		   #'tinderbox_status',
		   #'tinderbox_bug_id',
		   #'tinderbox_tree',
		   #'tinderbox_bug_url',
		   
		   # Bugzilla fields
		   
		   'Bug#',
		   'Product',
		   'Version',
		   'Platform',
		   'OS/Version',
		   'Status',
		   'Resolution',
		   'Severity',
		   'Priority',
		   'Component',
		   'AssignedTo',
		   'ReportedBy',
		   #'URL',
		   #'Cc',
		   'Summary',
		   
		   
		   # AIM Fields 

		   # (these names are configurable and come from the
		   #  appearance of mail messages. We always convert
		   #  spaces to '_'.)

		   
#		   'Ticket_#',
#		   'Date_Open',
#		   'Product_Name',
#		   'Product_Sub_System',
#		   'Product_Version',
#		   'Severity',
#		   'Status',
#		   'Closed_Date',
#		   'User_Login',
#		   'E-Mail',
#		   'Support_Staff_Login',
#		   'Support_Staff_E-Mail',
#		   'Last_Modified_Date',
#		   'Short_Description',

		  );


# Given a pointer to a bug update hash return the name of the tree to
# which this bug report belongs.  Typically this will be the contents
# of a field like 'Product', however some projects may be more
# compicated.  One example of a complex function would be if each of
# the product product types listed in the bug tracking data base
# refers to one development project except for a particular
# feature/platform of one particular project which is being developed
# by a separate group of developers.  So the version control notion of
# tress (a set of modules on a branch) may not have a direct map into
# the bug tracking database at all times.

# This function should return 'undef' if the bug report should be
# ignored by the tinderbox server.


sub update2tree {
  my ($tinderbox_ref) = @_;

  $out = (
	  $tinderbox_ref->{'Product'}.
	  "");

  # It might be a good idea to call TreeData::tree_exists() and ensure
  # that this tree is valid, but this would make it harder for testing
  # using genbugs.

  return $out;
}


# Given a pointer to a bug_update_hash return a URL ('href') to the
# bug. If the bug tracker does not support URL's to a bug number,
# return a 'mailto: ' to someone who cares about the bug.

sub update2bug_url {
  my ($tinderbox_ref) = @_;

  # AIM can not accept bug numbers as URLS without encoding lots of
  # other junk. Just give a mailto:

  #  $out = (
  #	  'mailto: '.
  #	  $tinderbox_ref{'User_Login'}.
  #	  "");

  # Bugzilla has an easy format for making URL's of bugs

  $out = (
	  'http://bugzilla.mozilla.org/show_bug.cgi?id='.
	  $tinderbox_ref->{$BTData::BUGID_FIELD_NAME}.
	  "");

  return $out;
}


sub get_all_progress_states {

  my (@progress_states) = main::uniq( values %BTData::STATUS_PROGRESS );

  return @progress_states;
}

sub is_status_valid {
  my ($status) = @_;

  my $out = defined($STATUS_PROGRESS{$status});

  return $out;
}

1;
